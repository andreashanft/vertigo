/***************************************************************************
 *  Copyright (C) 2010  Philipp Nordhus                                    *
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

#include "billboard.h"
#include "txt/desfile.h"
#include "gfx/texturemanager.h"
#include "fight/boundingbox.h"
#include "fight/collisionmesh.h"
#include "gfx/pipeline.h"

#include "gfx/gl.hpp"
#include "gfx/image.h"
#include <algorithm>
#include <glm/gtx/norm.hpp>
#include <glm/trigonometric.hpp>

namespace fight {


Vector3D square[4] = { Vector3D(-1, 1, 0), Vector3D(1, 1, 0), Vector3D(1, -1, 0), Vector3D(-1, -1, 0) };


Billboard::Billboard(gfx::TextureManager &texMan, txt::DesFile &file, int index, bool blendColor) :
    m_blendColor(blendColor)
{
    file.setSection(String("size%1").arg(index));
    m_scale = file.value("scale").toFloat();

    file.setSection(String("collision%1").arg(index));
    m_collisionRadius = file.value("radiusscale").toFloat();

    file.setSection(String("noise%1").arg(index));
    m_noiseLevel = file.value("level").toFloat();

    file.setSection(String("defense%1").arg(index));
    m_kineticShield = file.value("kineticshield0").toInt();

    file.setSection(String("offense%1").arg(index));
    m_kineticStrength = file.value("kineticstrength").toInt();
    m_shockStrength = file.value("shockstrength").toInt();

    file.setSection(String("kinematics%1").arg(index));
    if (file.contains("range"))
    {
        m_range = file.value("range").toInt() / 32.0f;
        m_velocity = file.value("velocity").toInt() / 32.0f;
    }
    m_lifeTime = file.value("lifetime", 0).toFloat();
    m_friction = file.value("friction", 0).toFloat();
    m_lift = file.value("lift", 0).toFloat();
    m_minVelocity = file.value("minvelocity", 0).toFloat() / 32.0f;
    m_maxVelocity = file.value("maxvelocity", 0).toFloat() / 32.0f;


    file.setSection(String("anim%1").arg(index));
    float scale = file.value("scale").toFloat();

    m_displayTime = file.value("displaytime").toFloat() + 0.02f;
    int numOfStages = file.value("numofstages").toInt();
    int numInX = 0;

    int i, j, k;
    float width, height;
    Vector2D center;
    for (i = -1; ; i++)
    {
        if (i < 0)
        {
            file.setSection(String("anim%1raster").arg(index));
            if (!file.contains("imgblockname"))
                i++;
        }
        if (i >= 0)
        {
            file.setSection(String("anim%1stage%2").arg(index).arg(i));
            if (!file.contains("imgblockname"))
                break;
        }

        m_stages.emplace_back();
        Stage &stage = m_stages.back();
        stage.texture = texMan.getModule(file.value("imgblockname").toString(), true);
        if (m_stages.size() == 1)
            m_imageName = file.value("imgblockname").toString();

        Vector2D t0(file.value("x1").toInt() / 255.0f, file.value("y1").toInt() / 255.0f);
        stage.texCoords[0] = t0;
        if (file.contains("width"))
        {
            numInX = file.value("numinx").toInt();
            width = file.value("width").toInt() / 255.0f;
            height = file.value("height").toInt() / 255.0f;
            center = Vector2D(width/2, height/2);
            stage.texCoords[1] = t0 + Vector2D(width - 1/255.0f, 0);
            stage.texCoords[2] = t0 + Vector2D(width - 1/255.0f, height - 1/255.0f);
            stage.texCoords[3] = t0 + Vector2D(0, height - 1/255.0f);
        }
        else
        {
            stage.texCoords[1] = Vector2D(file.value("x2").toInt() / 255.0f, file.value("y2").toInt() / 255.0f);
            stage.texCoords[2] = Vector2D(file.value("x3").toInt() / 255.0f, file.value("y3").toInt() / 255.0f);
            stage.texCoords[3] = Vector2D(file.value("x4").toInt() / 255.0f, file.value("y4").toInt() / 255.0f);

            width = (stage.texCoords[1] - t0).x + 1/255.0f;
            height = (stage.texCoords[3] - t0).y + 1/255.0f;
            center = Vector2D(file.value("xcenter").toInt() / 255.0f, file.value("ycenter").toInt() / 255.0f);
            center -= t0;
        }

        stage.scale = Vector2D(width, height)*scale*2.0f; // TODO: test scale
        stage.offset = Vector2D(1, -1) - Vector2D(center.x/width, -center.y/height)*2.0f;
    }

    if (m_stages.size() < numOfStages)
    {
        if (numInX == 0)
            numInX = (int)(1.0f/width);
        i = m_stages.size() - 1;
        for (j = 1; j < numOfStages - i; j++)
        {
            m_stages.push_back(m_stages[i]);
            Stage &stage = m_stages.back();
            int x = (int)(stage.texCoords[0].x/width + 1e-5);
            for (k = 0; k < 4; k++)
                stage.texCoords[k] += Vector2D(width*((x + j)%numInX - x), height*((x + j)/numInX));
        }
    }
}



void Billboard::quad(const Vector3D &position, float angle, float scale, float time, const glm::mat3 &camera,
                     Vector3D corners[4], const Vector2D *&texCoords, gfx::Texture *&texture)
{
    Stage &stage = m_stages[static_cast<int>(time/m_displayTime)%m_stages.size()];
    const float c = std::cos(glm::radians(angle)), s = std::sin(glm::radians(angle));
    const float size = m_scale*scale;
    for (int i = 0; i < 4; i++)
    {
        // the frame's offset and size, turned by the angle, 2 m in front of the position
        const Vector2D v = (Vector2D(square[i]) + stage.offset)*stage.scale;
        corners[i] = position + (camera[0]*(c*v.x - s*v.y) + camera[1]*(s*v.x + c*v.y))*size + camera[2]*2.0f;
    }
    texCoords = stage.texCoords;
    texture = &stage.texture;
}


void Billboard::draw(const Vector3D &position, float angle, float scale, float time, const glm::mat3 &camera)
{
    Vector3D corners[4];
    const Vector2D *texCoords;
    gfx::Texture *texture;
    quad(position, angle, scale, time, camera, corners, texCoords, texture);

    gfx::Pipeline::Vertex vertices[4];
    for (int i = 0; i < 4; i++)
        vertices[i] = { corners[i], texCoords[i] };

    if (m_blendColor)
        glBlendFunc(GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR);
    gfx::Pipeline::setAlphaTest(true);

    texture->bind();
    gfx::Pipeline::drawQuads(vertices, 4);

    if (m_blendColor)
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    gfx::Pipeline::setAlphaTest(false);
}


Vector3D Billboard::lightColor()
{
    if (m_lightColorKnown)
        return m_lightColor;
    m_lightColorKnown = true;
    if (m_stages.empty())
        return m_lightColor;

    const String basename = String("vfx:texture/%1").arg(m_imageName);
    const gfx::Image image = gfx::Image::load(basename + ".imb", gfx::ColorTable(basename + ".s16"));
    // the frame's pixels; the texture coordinates are pixel/255, and index 0 is the colour key
    const Stage &stage = m_stages.front();
    const int x0 = int(stage.texCoords[0].x*255 + 0.5f), y0 = int(stage.texCoords[0].y*255 + 0.5f);
    const int x1 = int(stage.texCoords[2].x*255 + 0.5f), y1 = int(stage.texCoords[2].y*255 + 0.5f);
    Vector3D sum(0);
    float weight = 0;
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++)
        {
            if (!image.valid(x, y) || image.pixelIndex(x, y) == 0)
                continue;
            const gfx::Rgb c = image.pixel(x, y);
            const float brightness = gfx::gray(c)/255.0f;
            sum += Vector3D(gfx::red(c), gfx::green(c), gfx::blue(c))*(brightness/255.0f);
            weight += brightness;
        }
    if (weight > 0)
    {
        sum /= weight;
        m_lightColor = sum/std::max({ sum.x, sum.y, sum.z });
    }
    return m_lightColor;
}


BoundingBox Billboard::box()
{
    float scale = m_scale*glm::length(m_stages[0].scale);
    return BoundingBox(Vector3D(-1, -1, -1)*scale, Vector3D(1, 1, 1)*scale);
}


bool Billboard::intersect(const Vector3D &start, const Vector3D &dir, float &distance)
{
    return CollisionMesh::intersectSphereLine(start, dir, m_scale*m_scale*glm::length2(m_stages[0].scale), distance);
}


} // namespace fight
