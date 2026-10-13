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

#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QGLContext>
#include <QOpenGLContext>
#include <QSettings>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <map>

// OpenGL 1.2 to 1.4 and EXT_texture_filter_anisotropic, missing from some gl.h
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif
#ifndef GL_CLAMP_TO_BORDER
#define GL_CLAMP_TO_BORDER 0x812D
#endif
#ifndef GL_GENERATE_MIPMAP
#define GL_GENERATE_MIPMAP 0x8191
#endif
#ifndef GL_TEXTURE_MAX_ANISOTROPY_EXT
#define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF
#endif


namespace hud {


namespace {

struct Vertex
{
    float x, y, z;
    float u, v;
};

// triangles closer than this to the eye's plane get no HUD texture (it would be mirrored behind)
const float DecalNear = 0.03f;

}


QString Cockpit3D::directory()
{
    QSettings settings;
    const QString directory = settings.value("cockpit/directory").toString();
    if (!directory.isEmpty())
        return directory;
    return QDir(settings.value("datadir").toString()).filePath("3d-cockpits");
}


QString Cockpit3D::find(const QString &cockpit)
{
    const QFileInfo file(QDir(directory()).filePath(QString("cockpit_%1.glb").arg(cockpit)));
    return file.isFile() ? file.absoluteFilePath() : QString();
}


std::shared_ptr<Cockpit3D> Cockpit3D::get(const QString &fileName)
{
    static std::map<QString, std::weak_ptr<Cockpit3D>> cache;
    std::shared_ptr<Cockpit3D> cockpit = cache[fileName].lock();
    if (!cockpit)
    {
        cockpit = std::make_shared<Cockpit3D>(fileName);
        cache[fileName] = cockpit;
    }
    return cockpit;
}


Cockpit3D::Cockpit3D(const QString &fileName) :
    m_fileName(fileName),
    m_vertices(QOpenGLBuffer::VertexBuffer),
    m_baseIndices(QOpenGLBuffer::IndexBuffer),
    m_decalIndices(QOpenGLBuffer::IndexBuffer)
{
}


Cockpit3D::~Cockpit3D()
{
    // the buffers free themselves with their context; the texture needs one to be deleted
    if (m_texture != 0 && QOpenGLContext::currentContext())
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
        m_baseIndices.destroy();
        m_decalIndices.destroy();
        m_vertices.destroy();
        glDeleteTextures(1, &m_texture);
        m_texture = 0;
        m_state = State::Unloaded;
    }
    m_cameraChanged = false;
    if (m_state != State::Unloaded)
        return m_state == State::Ready;
    m_state = State::Failed;

    QElapsedTimer timer;
    timer.start();
    gfx::GlbModel model;
    if (!model.load(m_fileName))
    {
        std::cerr << "3D cockpit: " << model.error().toUtf8().constData() << std::endl;
        return false;
    }

    // one vertex buffer; the model's textured meshes are drawn, the window glass only receives
    // the HUD (the 2D art shows no glass either)
    std::vector<Vertex> vertices;
    std::vector<quint32> base, decal;
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
        const bool glass = mesh.blend && mesh.extras.value("role").toString() != "hud";
        if (!textured && !glass)
            continue;
        if (textured)
        {
            if (image >= 0 && mesh.image != image)
                std::cerr << "3D cockpit: " << mesh.name.toUtf8().constData() << " uses a second texture, which isn't supported" << std::endl;
            image = image < 0 ? mesh.image : image;
        }
        const quint32 first = quint32(vertices.size());
        for (size_t i = 0; i < mesh.positions.size(); i++)
        {
            const Vector3D &p = mesh.positions[i];
            const Vector2D uv = textured ? mesh.texCoords[i] : Vector2D(0);
            vertices.push_back({ p.x, p.y, p.z, uv.x, uv.y });
        }
        for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3)
        {
            const quint32 a = mesh.indices[i], b = mesh.indices[i + 1], c = mesh.indices[i + 2];
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
        std::cerr << "3D cockpit: " << m_fileName.toUtf8().constData() << " has no textured meshes" << std::endl;
        return false;
    }

    if (!m_vertices.create() || !m_baseIndices.create() || !m_decalIndices.create())
    {
        std::cerr << "3D cockpit: cannot create vertex buffers" << std::endl;
        return false;
    }
    m_vertices.bind();
    m_vertices.allocate(vertices.data(), int(vertices.size()*sizeof(Vertex)));
    m_vertices.release();
    m_baseIndices.bind();
    m_baseIndices.allocate(base.data(), int(base.size()*sizeof(quint32)));
    m_baseIndices.release();
    m_decalIndices.bind();
    m_decalIndices.allocate(decal.data(), int(decal.size()*sizeof(quint32)));
    m_decalIndices.release();
    m_baseCount = int(base.size());
    m_decalCount = int(decal.size());

    // the baked texture: trilinear and anisotropic, as most surfaces are seen at an angle
    QImage texture = model.images()[image].convertToFormat(QImage::Format_RGB888);
    GLint maxSize = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxSize);
    if (maxSize > 0 && (texture.width() > maxSize || texture.height() > maxSize))
        texture = texture.scaled(std::min(texture.width(), int(maxSize)), std::min(texture.height(), int(maxSize)), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    glGenTextures(1, &m_texture);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    const char *extensions = reinterpret_cast<const char *>(glGetString(GL_EXTENSIONS));
    if (extensions && strstr(extensions, "GL_EXT_texture_filter_anisotropic"))
    {
        GLfloat anisotropy = 1;
        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &anisotropy);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, std::min(8.0f, anisotropy));
    }
    // QImage pads lines to 4 bytes, GL's default unpack alignment
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, texture.width(), texture.height(), 0, GL_RGB, GL_UNSIGNED_BYTE, texture.constBits());
    glBindTexture(GL_TEXTURE_2D, 0);

    std::cout << "3D cockpit: " << QFileInfo(m_fileName).fileName().toUtf8().constData() << ", " << m_baseCount/3 << " triangles, "
              << texture.width() << "x" << texture.height() << " texture, loaded in " << timer.elapsed() << " ms" << std::endl;
    m_state = State::Ready;
    return true;
}


void Cockpit3D::bindArrays()
{
    m_vertices.bind();
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3, GL_FLOAT, sizeof(Vertex), reinterpret_cast<void *>(offsetof(Vertex, x)));
}


void Cockpit3D::releaseArrays()
{
    // the rest of the game draws from client memory, which a bound buffer would break
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    m_vertices.release();
    QOpenGLBuffer::release(QOpenGLBuffer::IndexBuffer);
}


bool Cockpit3D::draw(const Matrix &projection, const Matrix &view)
{
    if (!create())
        return false;

    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(glm::value_ptr(projection));
    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(glm::value_ptr(view));

    // unlit: the lighting is painted into the texture
    glDisable(GL_LIGHTING);
    glDisable(GL_FOG);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glDisableClientState(GL_NORMAL_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);

    bindArrays();
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glTexCoordPointer(2, GL_FLOAT, sizeof(Vertex), reinterpret_cast<void *>(offsetof(Vertex, u)));
    m_baseIndices.bind();
    glDrawElements(GL_TRIANGLES, m_baseCount, GL_UNSIGNED_INT, nullptr);
    releaseArrays();

    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    return true;
}


void Cockpit3D::drawDecal(const Matrix &projection, const Matrix &view, unsigned int texture)
{
    if (m_state != State::Ready || m_decalCount == 0)
        return;

    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(glm::value_ptr(projection));
    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(glm::value_ptr(view));

    // keep the HUD off anything behind the eye's plane, where the projection mirrors
    const GLdouble nearPlane[4] = { 0, 0, -1, -DecalNear };
    glClipPlane(GL_CLIP_PLANE0, nearPlane);
    glEnable(GL_CLIP_PLANE0);

    // projective texture coordinates from the eye at rest: s = x/640 and t = 1 - y/480 of the
    // pixel a model point covers in the 640x480 view, divided by q = -z per fragment
    const GLfloat planeS[4] = { m_focal/640, 0, -m_center.x/640, 0 };
    const GLfloat planeT[4] = { 0, m_focal/480, -(480 - m_center.y)/480, 0 };
    const GLfloat planeQ[4] = { 0, 0, -1, 0 };
    glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
    glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
    glTexGeni(GL_Q, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
    glTexGenfv(GL_S, GL_OBJECT_PLANE, planeS);
    glTexGenfv(GL_T, GL_OBJECT_PLANE, planeT);
    glTexGenfv(GL_Q, GL_OBJECT_PLANE, planeQ);
    glEnable(GL_TEXTURE_GEN_S);
    glEnable(GL_TEXTURE_GEN_T);
    glEnable(GL_TEXTURE_GEN_Q);

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texture);
    // nothing outside the view
    const GLfloat transparent[4] = { 0, 0, 0, 0 };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, transparent);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

    glDisable(GL_LIGHTING);
    glDisable(GL_FOG);
    glDisable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER, 0.0f);

    bindArrays();
    m_decalIndices.bind();
    glDrawElements(GL_TRIANGLES, m_decalCount, GL_UNSIGNED_INT, nullptr);
    releaseArrays();

    glDisable(GL_ALPHA_TEST);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_TRUE);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glDisable(GL_TEXTURE_GEN_S);
    glDisable(GL_TEXTURE_GEN_T);
    glDisable(GL_TEXTURE_GEN_Q);
    glDisable(GL_CLIP_PLANE0);
}


} // namespace hud
