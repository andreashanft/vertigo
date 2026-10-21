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
#include "cockpitpaint.h"
#include "gfx/glbmodel.h"
#include "gfx/pipeline.h"
#include "util/datapath.h"
#include "util/path.hpp"

#include "gfx/gl.hpp"
#include "util/settings.hpp"
#include <SDL3/SDL_filesystem.h>
#include <algorithm>
#include <chrono>
#include <cstdio>
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

// setDirectory()'s, for this run
std::string runDirectory;

#if defined(SDL_PLATFORM_IOS)
// The iPad's screen is no larger than the texture at 4096x4096, which loads in half the time and
// takes 0.5 GB less (the game takes 1.4 GB on the desktop with 8192x8192, 0.1 GB without the 3D
// cockpit), also on iPads with less memory; the kit's texture is painted at that size, with the
// art's coverage at 4x (a quarter of the time and memory of 8x)
const int MaxTextureSize = 4096;
const int EyeScale = 4;
#else
const int MaxTextureSize = 1 << 30;
const int EyeScale = 8;
#endif

// raised when CockpitPaint paints differently, so that the cached textures are painted again
const int PaintVersion = 1;
const int CacheQuality = 92;

std::uint64_t fnv1a(const void *data, std::size_t size, std::uint64_t hash = 14695981039346656037ull)
{
    const std::uint8_t *p = static_cast<const std::uint8_t *>(data);
    for (std::size_t i = 0; i < size; i++)
        hash = (hash ^ p[i])*1099511628211ull;
    return hash;
}


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
    if (!runDirectory.empty())
        return util::toPath(runDirectory).is_absolute() ? runDirectory : util::joinPath(util::DataPath::directory(), runDirectory);
    const std::string directory = util::Settings::value("cockpit/directory");
    if (!directory.empty())
        return directory;
    return util::joinPath(util::DataPath::directory(), "3d-cockpits");
}


void Cockpit3D::setDirectory(const std::string &directory)
{
    runDirectory = directory;
}


std::string Cockpit3D::kitDirectory()
{
    static const std::string found = []() {
        const char *base = SDL_GetBasePath();
        if (!base)
            return std::string();
        for (const char *candidate : { "cockpits", "../share/vertigo/cockpits" }) {
            const std::filesystem::path path = util::toPath(util::joinPath(base, candidate));
            std::error_code error;
            if (std::filesystem::is_directory(path, error))
                return util::fromPath(std::filesystem::absolute(path, error).lexically_normal());
        }
        return std::string();
    }();
    return found;
}


std::string Cockpit3D::find(const String &cockpit)
{
    const std::string name = String("cockpit_%1.glb").arg(cockpit).toUtf8();
    for (const std::string &directory : { directory(), kitDirectory() }) {
        if (directory.empty())
            continue;
        const std::filesystem::path file = util::toPath(util::joinPath(directory, name));
        std::error_code error;
        if (std::filesystem::is_regular_file(file, error))
            return util::fromPath(std::filesystem::absolute(file, error).lexically_normal());
    }
    return std::string();
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
    // (a texture being painted is not needed any more)
    m_cancel = true;
    if (m_painted.valid())
        m_painted.wait();
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


void Cockpit3D::prepare(const String &cockpit, bool wait)
{
    if (m_state == State::Ready && m_cameraChanged)
    {
        m_mesh.destroy();
        glDeleteTextures(1, &m_texture);
        m_texture = 0;
        m_state = State::Unloaded;
    }
    m_cameraChanged = false;
    if (m_state == State::Unloaded)
    {
        m_state = State::Failed;
        m_started = std::chrono::steady_clock::now();
        m_model.reset(new gfx::GlbModel);
        if (!m_model->load(m_fileName))
        {
            std::cerr << "3D cockpit: " << m_model->error() << std::endl;
            m_model.reset();
            return;
        }
        if (m_model->extras()["vertigo_kit"].isUndefined())
        {
            // a finished model with its texture
            if (create(gfx::Image()))
                m_state = State::Ready;
            m_model.reset();
            return;
        }
        startPainting(cockpit);
        m_state = State::Painting;
    }
    if (m_state == State::Painting)
    {
        if (!wait && m_painted.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
            return;
        const Painted painted = m_painted.get();
        m_state = State::Failed;
        if (painted.texture.isNull())
            std::cerr << "3D cockpit: cannot paint the texture of " << util::fileName(m_fileName) << std::endl;
        else if (create(painted.texture))
        {
            m_state = State::Ready;
            std::cout << "3D cockpit: the texture "
                      << (painted.cached ? "from the cache, " : "painted from the kit and the cockpit art, ") << painted.file << std::endl;
        }
        m_model.reset();
    }
}


// The kit's texture on a worker thread: from the cache, where the kit, the art, its camera and the
// size make the name, else painted and kept there
void Cockpit3D::startPainting(const String &cockpit)
{
    CockpitPaint::Input input;
    input.kit = m_model.get();
    const gfx::Image art = gfx::Image::load(String("vfx:cockpit/%1/cockpit.r16").arg(cockpit), 640, 480, true);
    input.see = CockpitPaint::seeThrough(art);
    input.art = art.convertToFormat(gfx::Image::RGB888);
    input.focal = m_focal;
    input.center = m_center;
    GLint maxSize = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxSize);
    input.size = std::min(m_model->extras()["vertigo_kit"]["atlas"].toInt(8192), MaxTextureSize);
    if (maxSize > 0)
        input.size = std::min(input.size, int(maxSize));
    input.eyeScale = EyeScale;

    std::vector<char> kit;
    util::readFile(m_fileName, kit);
    std::uint64_t hash = fnv1a(kit.data(), kit.size());
    hash = fnv1a(input.art.constBits(), std::size_t(input.art.bytesPerLine())*input.art.height(), hash);
    const float camera[3] = { input.focal, input.center.x, input.center.y };
    const int settings[3] = { input.size, input.eyeScale, PaintVersion };
    hash = fnv1a(camera, sizeof(camera), hash);
    hash = fnv1a(settings, sizeof(settings), hash);
    char hex[17];
    std::snprintf(hex, sizeof(hex), "%016llx", static_cast<unsigned long long>(hash));
    const std::string directory = util::joinPath(util::cacheLocation(), "3d-cockpits");
    const std::string prefix = String("cockpit_%1_%2_").arg(cockpit).arg(input.size).toUtf8();
    const std::string file = util::joinPath(directory, prefix + hex + ".jpg");

    m_cancel = false;
    m_painted = std::async(std::launch::async, [input = std::move(input), directory, prefix, file, cancel = &m_cancel]() {
        Painted painted;
        painted.file = file;
        std::vector<char> data;
        if (util::exists(file) && util::readFile(file, data))
        {
            painted.texture = gfx::Image::fromData(reinterpret_cast<const std::uint8_t *>(data.data()), data.size());
            painted.cached = painted.texture.width() == input.size && painted.texture.height() == input.size;
            if (painted.cached)
                return painted;
        }
        painted.texture = CockpitPaint::paint(input, cancel);
        if (!painted.texture.isNull() && util::makeDirectory(directory))
        {
            // (the textures of other kits, art or versions are of no use any more)
            for (const std::string &name : util::files(directory))
                if (name.compare(0, prefix.size(), prefix) == 0)
                {
                    std::error_code error;
                    std::filesystem::remove(util::toPath(util::joinPath(directory, name)), error);
                }
            if (!painted.texture.saveJpeg(file, CacheQuality))
                std::cerr << "3D cockpit: cannot write " << file << std::endl;
        }
        return painted;
    });
}


bool Cockpit3D::create(const gfx::Image &painted)
{
    const gfx::GlbModel &model = *m_model;
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

    // the texture: trilinear and anisotropic, as most surfaces are seen at an angle; a finished
    // model's halved until it fits (without copies of the full size, which take hundreds of MB)
    const gfx::Image &baked = painted.isNull() ? model.images()[image] : painted;
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

    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - m_started);
    std::cout << "3D cockpit: " << util::fileName(m_fileName) << (painted.isNull() ? "" : " (kit)") << ", " << m_baseCount/3
              << " triangles, " << pixels->width() << "x" << pixels->height() << " texture, loaded in " << elapsed.count() << " ms" << std::endl;
    return true;
}


bool Cockpit3D::draw(const Matrix &projection, const Matrix &view)
{
    if (m_state != State::Ready)
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
