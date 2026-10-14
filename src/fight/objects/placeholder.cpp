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

#include "placeholder.h"
#include "fight/scenario.h"
#include "gfx/pipeline.h"
#include "gfx/gl.hpp"

#include <algorithm>
#include <limits>


namespace fight {


Placeholder::Placeholder(Scenario *scenario, txt::DesFile &file, const ObjectInfo &info, float size) :
    ActiveObject(scenario, file, info),
    m_halfSize(0.5f*size)
{
    m_box = BoundingBox(Vector3D(-m_halfSize), Vector3D(m_halfSize));
    scenario->collisionManager().addObject(this);
}


void Placeholder::draw()
{
    // the six faces of a cube, each with a fixed shade so the box reads as 3D without lighting
    static const float faces[6][4][3] = {
        { { 1, -1, -1 }, { 1,  1, -1 }, { 1,  1,  1 }, { 1, -1,  1 } },
        { {-1, -1, -1 }, {-1, -1,  1 }, {-1,  1,  1 }, {-1,  1, -1 } },
        { {-1,  1, -1 }, {-1,  1,  1 }, { 1,  1,  1 }, { 1,  1, -1 } },
        { {-1, -1, -1 }, { 1, -1, -1 }, { 1, -1,  1 }, {-1, -1,  1 } },
        { {-1, -1,  1 }, { 1, -1,  1 }, { 1,  1,  1 }, {-1,  1,  1 } },
        { {-1, -1, -1 }, {-1,  1, -1 }, { 1,  1, -1 }, { 1, -1, -1 } },
    };
    static const float shades[6] = { 0.55f, 0.45f, 0.5f, 0.4f, 0.7f, 0.3f };

    // unlit and untextured, on both sides
    const bool lighting = gfx::Pipeline::lighting();
    const Vector4D color = gfx::Pipeline::color();
    const bool cull = glIsEnabled(GL_CULL_FACE);
    gfx::Pipeline::setLighting(false);
    glDisable(GL_CULL_FACE);

    gfx::Pipeline::pushMatrix();
    gfx::Pipeline::translate(m_position);
    gfx::Pipeline::scale(m_halfSize);
    for (int i = 0; i < 6; i++)
    {
        gfx::Pipeline::Vertex quad[4];
        for (int j = 0; j < 4; j++)
            quad[j] = { Vector3D(faces[i][j][0], faces[i][j][1], faces[i][j][2]), Vector2D(0) };
        gfx::Pipeline::setColor(Vector4D(shades[i], shades[i], 0.8f*shades[i], 1.0f));
        gfx::Pipeline::drawQuads(quad, 4, false);
    }
    gfx::Pipeline::popMatrix();

    gfx::Pipeline::setColor(color);
    gfx::Pipeline::setLighting(lighting);
    if (cull)
        glEnable(GL_CULL_FACE);
}


bool Placeholder::intersect(const Vector3D &start, const Vector3D &dir, float radius, float &distance, Vector3D &normal)
{
    // ray against the box grown by the radius (slab test)
    const Vector3D min = m_box.minPoint() - Vector3D(radius);
    const Vector3D max = m_box.maxPoint() + Vector3D(radius);
    float tNear = -std::numeric_limits<float>::max();
    float tFar = std::numeric_limits<float>::max();
    int axis = 0;
    for (int i = 0; i < 3; i++)
    {
        if (dir[i] == 0.0f)
        {
            if (start[i] < min[i] || start[i] > max[i])
                return false;
            continue;
        }
        float t0 = (min[i] - start[i]) / dir[i];
        float t1 = (max[i] - start[i]) / dir[i];
        if (t0 > t1)
            std::swap(t0, t1);
        if (t0 > tNear)
        {
            tNear = t0;
            axis = i;
        }
        tFar = std::min(tFar, t1);
        if (tNear > tFar || tFar < 0.0f)
            return false;
    }

    distance = std::max(tNear, 0.0f);
    normal = Vector3D(0.0f);
    normal[axis] = dir[axis] > 0.0f ? -1.0f : 1.0f;
    return true;
}


} // namespace fight
