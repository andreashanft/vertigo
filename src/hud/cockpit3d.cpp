/***************************************************************************
 *  Copyright (C) 2026  Andreas Hanft                                      *
 *                                                                         *
 *  This program is free software: you can redistribute it and/or modify   *
 *  it under the terms of the GNU General Public License as published by   *
 *  the Free Software Foundation, either version 3 of the License, or      *
 *  (at your option) any later version.                                    *
 *                                                                         *
 *  This program is distributed in the hope that it will be useful,        *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
 *  GNU General Public License for more details.                           *
 *                                                                         *
 *  You should have received a copy of the GNU General Public License      *
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.  *
 ***************************************************************************/

#include "cockpit3d.h"
#include "gfx/glbmodel.h"
#include "gfx/pipeline.h"
#include "util/datapath.h"
#include "util/path.hpp"

#include "util/timer.hpp"
#include "gfx/gl.hpp"
#include "util/settings.hpp"
#include <algorithm>
#include <iostream>
#include <map>

// OpenGL 1.2 and EXT_texture_filter_anisotropic, missing from some gl.h
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif
#ifndef GL_TEXTURE_MAX_ANISOTROPY_EXT
#define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF
#endif


namespace hud {


namespace {

// triangles closer than this to the eye's plane get no HUD texture (it would be mirrored behind)
const float DecalNear = 0.03f;

#if defined(SDL_PLATFORM_IOS)
// The iPad's screen is no larger than the texture at 4096x4096, which loads in half the time and
// takes 0.5 GB less (the game takes 1.4 GB on the desktop with 8192x8192, 0.1 GB without the 3D
// cockpit), also on iPads with less memory
const int MaxTextureSize = 4096;
#else
const int MaxTextureSize = 1 << 30;
#endif


// half the size, each pixel the mean of four (the baked textures are squares of powers of two)
gfx::Image halved(const gfx::Image &image)
{
    gfx::Image half(image.width()/2, image.height()/2, gfx::Image::RGB888);
    for (int y = 0; y < half.height(); y++)
    {
        const std::uint8_t *a = image.constScanLine(2*y), *b = image.constScanLine(2*y + 1);
        std::uint8_t *out = half.scanLine(y);
        for (int i = 0; i < 3*half.width(); i++)
        {
            const int x = 6*(i/3) + i%3;
            out[i] = std::uint8_t((a[x] + a[x + 3] + b[x] + b[x + 3] + 2)/4);
        }
    }
    return half;
}

}


std::string Cockpit3D::directory()
{
    const std::string directory = util::Settings::value("cockpit/directory");
    if (!directory.empty())
        return directory;
    return util::joinPath(util::DataPath::directory(), "3d-cockpits");
}


std::string Cockpit3D::find(const String &cockpit)
{
    const std::filesystem::path file = util::toPath(util::joinPath(directory(), String("cockpit_%1.glb").arg(cockpit).toUtf8()));
    std::error_code error;
    return std::filesystem::is_regular_file(file, error) ? util::fromPath(std::filesystem::absolute(file, error).lexically_normal()) : std::string();
}


std::shared_ptr<Cockpit3D> Cockpit3D::get(const std::string &fileName)
{
    static std::map<std::string, std::weak_ptr<Cockpit3D>> cache;
    std::shared_ptr<Cockpit3D> cockpit = cache[fileName].lock();
    if (!cockpit)
    {
        cockpit = std::make_shared<Cockpit3D>(fileName);
        cache[fileName] = cockpit;
    }
    return cockpit;
}


Cockpit3D::Cockpit3D(const std::string &fileName) :
    m_fileName(fileName)
{
}


Cockpit3D::~Cockpit3D()
{
    // the texture went with the context otherwise
    if (m_texture != 0 && gfx::gl::hasContext())
        glDeleteTextures(1, &m_texture);
}


void Cockpit3D::setCamera(float focal, const Vector2D &center)
{
    if (focal == m_focal && center == m_center)
        return;
    m_focal = focal;
    m_center = center;
    // the triangles in view depend on the camera: rebuilt on the next draw, with a GL context
    m_cameraChanged = true;
}


bool Cockpit3D::create()
{
    if (m_state == State::Ready && m_cameraChanged)
    {
        m_mesh.destroy();
        glDeleteTextures(1, &m_texture);
        m_texture = 0;
        m_state = State::Unloaded;
    }
    m_cameraChanged = false;
    if (m_state != State::Unloaded)
        return m_state == State::Ready;
    m_state = State::Failed;

    util::ElapsedTimer timer;
    timer.start();
    gfx::GlbModel model;
    if (!model.load(m_fileName))
    {
        std::cerr << "3D cockpit: " << model.error() << std::endl;
        return false;
    }

    // one vertex buffer; the model's textured meshes are drawn, the window glass only receives
    // the HUD (the 2D art shows no glass either)
    std::vector<Vector3D> positions;
    std::vector<Vector2D> texCoords;
    std::vector<std::uint32_t> base, decal;
    int image = -1;
    auto inView = [](const Vector3D &p) {
        return -p.z > DecalNear;
    };
    auto pixel = [this](const Vector3D &p) {
        return Vector2D(m_center.x + m_focal*p.x/-p.z, m_center.y - m_focal*p.y/-p.z);
    };
    for (const gfx::GlbModel::Mesh &mesh : model.meshes())
    {
        const bool textured = mesh.image >= 0 && !mesh.blend;
        const bool glass = mesh.blend && mesh.extras["role"].toString() != "hud";
        if (!textured && !glass)
            continue;
        if (textured)
        {
            if (image >= 0 && mesh.image != image)
                std::cerr << "3D cockpit: " << mesh.name << " uses a second texture, which isn't supported" << std::endl;
            image = image < 0 ? mesh.image : image;
        }
        const std::uint32_t first = std::uint32_t(positions.size());
        for (size_t i = 0; i < mesh.positions.size(); i++)
        {
            positions.push_back(mesh.positions[i]);
            texCoords.push_back(textured ? mesh.texCoords[i] : Vector2D(0));
        }
        for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3)
        {
            const std::uint32_t a = mesh.indices[i], b = mesh.indices[i + 1], c = mesh.indices[i + 2];
            if (textured)
                base.insert(base.end(), { first + a, first + b, first + c });
            // the HUD covers the 640x480 view: only triangles in front of the eye that reach into it
            const Vector3D &pa = mesh.positions[a], &pb = mesh.positions[b], &pc = mesh.positions[c];
            if (!inView(pa) || !inView(pb) || !inView(pc))
                continue;
            const Vector2D qa = pixel(pa), qb = pixel(pb), qc = pixel(pc);
            const Vector2D low = glm::min(qa, glm::min(qb, qc)), high = glm::max(qa, glm::max(qb, qc));
            if (high.x >= -8 && low.x <= 648 && high.y >= -8 && low.y <= 488)
                decal.insert(decal.end(), { first + a, first + b, first + c });
        }
    }
    if (base.empty() || image < 0)
    {
        std::cerr << "3D cockpit: " << m_fileName << " has no textured meshes" << std::endl;
        return false;
    }

    m_baseCount = int(base.size());
    m_decalCount = int(decal.size());
    base.insert(base.end(), decal.begin(), decal.end());
    m_mesh.create(int(positions.size()), positions.data(), nullptr, texCoords.data(), int(base.size()), base.data());

    // the baked texture: trilinear and anisotropic, as most surfaces are seen at an angle; halved
    // until it fits (without copies of the full size, which take hundreds of MB)
    const gfx::Image &baked = model.images()[image];
    gfx::Image texture = baked.format() == gfx::Image::RGB888 ? gfx::Image() : baked.convertToFormat(gfx::Image::RGB888);
    const gfx::Image *pixels = texture.isNull() ? &baked : &texture;
    GLint maxSize = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxSize);
    maxSize = maxSize > 0 ? std::min(int(maxSize), MaxTextureSize) : MaxTextureSize;
    while (pixels->width() > maxSize || pixels->height() > maxSize)
    {
        texture = halved(*pixels);
        pixels = &texture;
    }
    glGenTextures(1, &m_texture);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    if (gfx::gl::hasExtension("GL_EXT_texture_filter_anisotropic") || gfx::gl::hasExtension("GL_ARB_texture_filter_anisotropic"))
    {
        GLfloat anisotropy = 1;
        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &anisotropy);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, std::min(8.0f, anisotropy));
    }
    // the rows follow each other without padding to 4 bytes, GL's default
    // (errors from before, at most a few)
    for (int i = 0; i < 8 && glGetError() != GL_NO_ERROR; i++)
        ;
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, pixels->width(), pixels->height(), 0, GL_RGB, GL_UNSIGNED_BYTE, pixels->constBits());
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    if (const GLenum error = glGetError())
        std::cerr << "3D cockpit: OpenGL error 0x" << std::hex << error << std::dec << " with the texture" << std::endl;

    std::cout << "3D cockpit: " << util::fileName(m_fileName) << ", " << m_baseCount/3 << " triangles, "
              << pixels->width() << "x" << pixels->height() << " texture, loaded in " << timer.elapsed() << " ms" << std::endl;
    m_state = State::Ready;
    return true;
}


bool Cockpit3D::draw(const Matrix &projection, const Matrix &view)
{
    if (!create())
        return false;

    gfx::Pipeline::setProjection(projection);
    gfx::Pipeline::setModelView(view);

    // unlit: the lighting is painted into the texture
    gfx::Pipeline::setLighting(false);
    gfx::Pipeline::setFog(false);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    const Vector4D color = gfx::Pipeline::color();
    gfx::Pipeline::setColor(Vector4D(1));
    m_mesh.draw(0, m_baseCount);
    gfx::Pipeline::setColor(color);
    if (!m_drawn)
    {
        m_drawn = true;
        if (const GLenum error = glGetError())
            std::cerr << "3D cockpit: OpenGL error 0x" << std::hex << error << std::dec << " in the first frame" << std::endl;
    }
    return true;
}


void Cockpit3D::drawDecal(const Matrix &projection, const Matrix &view, unsigned int texture)
{
    if (m_state != State::Ready || m_decalCount == 0)
        return;

    gfx::Pipeline::setProjection(projection);
    gfx::Pipeline::setModelView(view);

    // projective texture coordinates from the eye at rest: s = x/640 and t = 1 - y/480 of the
    // pixel a model point covers in the 640x480 view, divided by q = -z per fragment; nothing
    // behind the eye's plane, where the projection mirrors
    const Vector4D planeS(m_focal/640, 0, -m_center.x/640, 0);
    const Vector4D planeT(0, m_focal/480, -(480 - m_center.y)/480, 0);
    const Vector4D planeQ(0, 0, -1, 0);
    gfx::Pipeline::setTextureProjection(true, glm::transpose(Matrix(planeS, planeT, Vector4D(0), planeQ)), DecalNear);

    glBindTexture(GL_TEXTURE_2D, texture);
    const Vector4D color = gfx::Pipeline::color();
    gfx::Pipeline::setColor(Vector4D(1));
    gfx::Pipeline::setLighting(false);
    gfx::Pipeline::setFog(false);
    gfx::Pipeline::setAlphaTest(true);
    glDisable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    m_mesh.draw(m_baseCount, m_decalCount);

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_TRUE);
    gfx::Pipeline::setAlphaTest(false);
    gfx::Pipeline::setColor(color);
    gfx::Pipeline::setTextureProjection(false);
}


} // namespace hud
