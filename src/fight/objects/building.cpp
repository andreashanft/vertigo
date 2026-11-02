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

#include "building.h"
#include "fight/scenario.h"
#include "fight/module.h"
#include "util/debug.h"
#include "gfx/pipeline.h"

#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

namespace fight {


namespace {

// the light part of the shading under a ground plan's lifted clusters, towers and active buildings
// (Surface::darken)
const float BuildingShade = 0.35f;

}


// The clusters lie on the building's square of size + 1 cells, offset 0, 0 at its corner with the
// lowest cell numbers (x, y - size); card turns the square clockwise about its middle, a quarter turn per two steps,
// and every cluster's model with it (docs/ROADMAP.md, appendix N).
Building::Building(Scenario *scenario, txt::DesFile &file, int size, int card, int x, int y, int refx, int refy) :
    Object(scenario, file),
    m_size(size),
    m_angle(card % 2 == 0 ? (card/2 % 4)*90.0f : 0.0f)
{
    file.setSection("display");
    m_gouraud = file.value("ShadingMode", "f").toString().trimmed().toLower() == "g";
    int i = 0;
    Vector3D scale = scenario->surface().scale();
    const float middle = 0.5f*size;
    m_position = Vector3D(x + 0.5f + middle, y + 0.5f - size + middle, 0)*scale;
    const int turns = int(m_angle)/90;

    while (file.sections().contains(String("cluster%1").arg(i))) {
        file.setSection(String("cluster%1").arg(i));

        m_clusters.emplace_back();
        Cluster &cluster = m_clusters.back();
        cluster.module = scenario->moduleManager().get(file.value("base").toString());

        file.setSection(String("size%1").arg(i));
        cluster.scale = file.value("scale").toFloat() * 16;

        file.setSection(String("position%1").arg(i));
        float offsetX = file.value("OffsetX").toInt() - middle;
        float offsetY = file.value("OffsetY").toInt() - middle;
        for (int turn = 0; turn < turns; turn++)
        {
            const float east = offsetY;
            offsetY = -offsetX;
            offsetX = east;
        }
        const int clusterX = x + int(std::lround(offsetX + middle));
        const int clusterY = y - size + int(std::lround(offsetY + middle));
        cluster.offset = Vector3D((clusterX + 0.5f)*scale.x, (clusterY + 0.5f)*scale.y, 0);

        int heightRef = file.value("HeightReference").toInt();
        if (heightRef >= 1000)
            cluster.offset.z = m_clusters[heightRef - 1000].offset.z + 256*cluster.scale; // TODO: test moar
        else
        {
            // the cluster stands at the height map's value of the building's reference cell, with
            // HeightReference -1 of its own cell; with SurfaceLifting p the ground under it is
            // flattened to that value, without it keeps the height over lower or higher ground, so
            // the pipes run level into the elbows; p and l darken the cell's ground
            // (docs/ROADMAP.md, appendix N)
            const String lifting = file.value("SurfaceLifting").toString();
            const int heightX = heightRef < 0 ? clusterX : refx, heightY = heightRef < 0 ? clusterY : refy;
            if (lifting == "p" || heightRef < 0)
                scenario->surface().lift(clusterX, clusterY, heightX, heightY);
            cluster.offset.z = scenario->surface().height(heightX, heightY)*scale.z;
            if (lifting == "p" || lifting == "l")
                scenario->surface().darken(clusterX, clusterY, BuildingShade);
        }

        switch (file.value("cardinal").toString().toLatin1()[0]) {
        case 'n':
            cluster.angle = 0;
            break;
        case 'w':
            cluster.angle = 90;
            break;
        case 's':
            cluster.angle = 180;
            break;
        case 'e':
            cluster.angle = 270;
            break;
        }

        i++;
    }

    for (Cluster &cluster : m_clusters)
    {
        Matrix m(1);
        m = glm::translate(m, cluster.offset);
        m = glm::rotate(m, glm::radians(-m_angle), Vector3D(0, 0, 1));
        m = glm::scale(m, Vector3D(cluster.scale));
        m = glm::rotate(m, glm::radians(cluster.angle), Vector3D(0, 0, 1));
        m_box.add(cluster.module->box().transform(m));

        cluster.transform = m;
        cluster.invTransform = glm::inverse(m);
    }

    scenario->collisionManager().addObject(this);
}


void Building::draw()
{
    if (m_gouraud)
        m_scenario->shadeModels(true);
    for (Cluster &cluster : m_clusters)
    {
        gfx::Pipeline::pushMatrix();
        gfx::Pipeline::multiply(cluster.transform);
        cluster.module->draw();
        gfx::Pipeline::popMatrix();
    }
    if (m_gouraud)
        m_scenario->shadeModels(false);
}


const std::vector<Vector3D>& Building::triangles() const
{
    if (m_triangles.empty())
        for (const Cluster &cluster : m_clusters)
            cluster.module->addTriangles(cluster.transform, m_triangles);
    return m_triangles;
}


bool Building::intersect(const Vector3D &start, const Vector3D &dir, float radius, float &distance, Vector3D &normal)
{
    bool collision = false;
    for (Cluster &cluster : m_clusters)
    {
        float d;
        Vector3D norm;
        if (cluster.module->intersect(Vector3D(cluster.invTransform * Vector4D(start, 1)), Vector3D(cluster.invTransform * Vector4D(dir, 0))*cluster.scale, radius/cluster.scale, d, norm))
        {
            d *= cluster.scale;
            if (!collision || distance > d)
            {
                distance = d;
                normal = Vector3D(cluster.transform * Vector4D(norm, 0))/cluster.scale;
                collision = true;
            }
        }
    }
    return collision;
}


} // namespace fight
