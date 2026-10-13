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

#include "voxelradar.h"
#include "fight/scenario.h"
#include "fight/sonar.h"
#include "fight/boats/player.h"
#include "fight/objects/activeobject.h"
#include "fight/objects/building.h"
#include "txt/desfile.h"

#include <QFile>
#include <QGLContext>
#include <QPainter>
#include <glm/geometric.hpp>
#include <glm/common.hpp>
#include <glm/trigonometric.hpp>
#include <algorithm>
#include <cmath>


namespace hud {


namespace {

// VOX_nnn.S16: 32 rows of 256 RGB565 colours from 0x31a on, from the farthest (the water's
// colour) to the nearest; in each row 0-63 is a ramp from dark to light and 64 is orange.
const int TableOffset = 0x31a;
const int FogRows = 32;
const int Entries = 256;
const int LightLevels = 64;
const int Orange = 64;
// the patch is drawn as a grid of quads
const int GridSteps = 32;
// the patch at the boat's ground level fills at most this part of the view
const float PatchWidth = 0.9f;
const float PatchHeight = 0.9f;

}


VoxelRadar::VoxelRadar(txt::DesFile &file)
{
    file.setSection("voxelradar0");
    if (!file.contains("width"))
        return;
    m_rect = Rect(file.value("StartX").toInt(), file.value("StartY").toInt(), file.value("Width").toInt(), file.value("Height").toInt());
    m_elevation = glm::radians(file.value("Projection", 30).toFloat());
    m_extent = file.value("WorldMaxY", 20).toFloat();

    QFile table(QString("vfx:texture/%1.s16").arg(file.value("ShadingTable", "vox_000").toString()));
    const QByteArray data = table.open(QIODevice::ReadOnly) ? table.readAll() : QByteArray();
    if (m_rect.width <= 0 || m_rect.height <= 0 || data.size() < TableOffset + FogRows*Entries*2)
        return;
    m_table.resize(FogRows*Entries);
    const uchar *colors = reinterpret_cast<const uchar *>(data.constData()) + TableOffset;
    for (int i = 0; i < FogRows*Entries; i++)
    {
        const int c = colors[2*i] | (colors[2*i + 1] << 8);
        m_table[i] = qRgb(((c >> 11) & 31)*255/31, ((c >> 5) & 63)*255/63, (c & 31)*255/31);
    }
    m_image = QImage(m_rect.width, m_rect.height, QImage::Format_ARGB32);
    m_valid = true;
}


void VoxelRadar::limitHeight(int height)
{
    if (m_valid && height > 0 && height < m_rect.height)
    {
        m_rect.height = height;
        m_image = QImage(m_rect.width, m_rect.height, QImage::Format_ARGB32);
    }
}


QRgb VoxelRadar::shade(int fog, int light) const
{
    return m_table[glm::clamp(fog, 0, FogRows - 1)*Entries + light];
}


void VoxelRadar::render(fight::Scenario *scenario)
{
    fight::Surface &surface = scenario->surface();
    const Vector3D boat = scenario->position();
    Vector2D forward(scenario->player()->dir());
    forward = glm::length(forward) > 1e-3f ? glm::normalize(forward) : Vector2D(0, 1);
    const Vector2D right(forward.y, -forward.x);
    const float extent = m_extent*surface.scale().x;

    // the eye looks down at the ground under the boat from behind; in perspective, so the far
    // edge of the patch comes out narrower than the near one
    const float distance = 3.5f*extent;
    const Vector3D target(boat.x, boat.y, surface.heightAt(boat.x, boat.y));
    const Vector3D eye = target - Vector3D(forward, 0)*distance*std::cos(m_elevation) + Vector3D(0, 0, distance*std::sin(m_elevation));
    const Vector3D viewForward = glm::normalize(target - eye);
    const Vector3D viewRight(right, 0);
    const Vector3D viewUp = glm::cross(viewRight, viewForward);
    const float width = m_image.width(), height = m_image.height();

    // fitted so that the patch, flat at the boat's ground level, fills the view and sits on its
    // bottom edge
    auto view = [&](const Vector3D &p) {
        const Vector3D v = p - eye;
        const float z = std::max(glm::dot(v, viewForward), 1.0f);
        return Vector2D(glm::dot(v, viewRight)/z, glm::dot(v, viewUp)/z);
    };
    Vector2D low(1e9f), high(-1e9f);
    for (const float x : { -extent, extent })
        for (const float y : { -extent, extent })
        {
            const Vector2D v = view(target + Vector3D(right*x + forward*y, 0));
            low = glm::min(low, v);
            high = glm::max(high, v);
        }
    const float focal = std::min(PatchWidth*width/(high.x - low.x), PatchHeight*height/(high.y - low.y));
    const Vector2D centre(0.5f*width - focal*0.5f*(low.x + high.x), height - 1 + focal*low.y);
    auto project = [&](const Vector3D &p) {
        const Vector2D v = view(p);
        return QPointF(centre.x + focal*v.x, centre.y - focal*v.y);
    };
    auto inPatch = [&](const Vector3D &p) {
        const Vector2D way(p - boat);
        return std::abs(glm::dot(way, right)) <= extent && std::abs(glm::dot(way, forward)) <= extent;
    };

    m_image.fill(Qt::transparent);
    QPainter painter(&m_image);
    painter.setPen(Qt::NoPen);

    // the terrain, from the far edge to the near one so that nearer quads cover what lies behind;
    // the lowest ground on the patch is dark, the highest bright
    const float step = 2*extent/GridSteps;
    Vector3D points[GridSteps + 1][GridSteps + 1];
    float lowest = 1e9f, highest = -1e9f;
    for (int j = 0; j <= GridSteps; j++)
        for (int i = 0; i <= GridSteps; i++)
        {
            const Vector2D p = Vector2D(boat) + right*(i*step - extent) + forward*(j*step - extent);
            points[j][i] = Vector3D(p, surface.heightAt(p.x, p.y));
            lowest = std::min(lowest, points[j][i].z);
            highest = std::max(highest, points[j][i].z);
        }
    const float range = std::max(highest - lowest, 10.0f);
    for (int j = GridSteps - 1; j >= 0; j--)
        for (int i = 0; i < GridSteps; i++)
        {
            const float z = 0.25f*(points[j][i].z + points[j][i + 1].z + points[j + 1][i].z + points[j + 1][i + 1].z);
            const int level = glm::clamp(int(8 + 54*(z - lowest)/range), 0, LightLevels - 1);
            // slightly darker towards the far edge
            painter.setBrush(QColor(shade(FogRows - 1 - (j*6)/GridSteps, level)));
            const QPointF quad[4] = { project(points[j][i]), project(points[j][i + 1]), project(points[j + 1][i + 1]), project(points[j + 1][i]) };
            painter.drawPolygon(quad, 4);
        }

    // buildings on the patch as orange blocks (their boxes, in the patch's frame and cut off at its
    // edges), the faces farther away first
    const QColor orange(shade(FogRows - 1, Orange));
    auto toPatch = [&](const Vector3D &p) { const Vector2D way(p - boat); return Vector2D(glm::dot(way, right), glm::dot(way, forward)); };
    auto fromPatch = [&](float x, float y, float z) { return Vector3D(Vector2D(boat) + right*x + forward*y, z); };
    for (const auto &object : *scenario)
    {
        const fight::Building *building = dynamic_cast<const fight::Building *>(object.get());
        if (!building || !building->isEnabled())
            continue;
        const Vector3D a = building->box().minPoint(), b = building->box().maxPoint();
        Vector2D low(1e9f), high(-1e9f);
        for (const Vector3D &corner : { a, Vector3D(a.x, b.y, 0), Vector3D(b.x, a.y, 0), b })
        {
            low = glm::min(low, toPatch(corner));
            high = glm::max(high, toPatch(corner));
        }
        low = glm::max(low, Vector2D(-extent));
        high = glm::min(high, Vector2D(extent));
        if (low.x >= high.x || low.y >= high.y)
            continue;
        const float z0 = a.z, z1 = b.z;
        struct Face { Vector3D corners[4]; QColor color; float depth; };
        Face faces[5] = {
            { { fromPatch(low.x, low.y, z1), fromPatch(high.x, low.y, z1), fromPatch(high.x, high.y, z1), fromPatch(low.x, high.y, z1) }, orange.lighter(130), 0 },
            { { fromPatch(low.x, low.y, z0), fromPatch(high.x, low.y, z0), fromPatch(high.x, low.y, z1), fromPatch(low.x, low.y, z1) }, orange, 0 },
            { { fromPatch(low.x, high.y, z0), fromPatch(high.x, high.y, z0), fromPatch(high.x, high.y, z1), fromPatch(low.x, high.y, z1) }, orange, 0 },
            { { fromPatch(low.x, low.y, z0), fromPatch(low.x, high.y, z0), fromPatch(low.x, high.y, z1), fromPatch(low.x, low.y, z1) }, orange.darker(130), 0 },
            { { fromPatch(high.x, low.y, z0), fromPatch(high.x, high.y, z0), fromPatch(high.x, high.y, z1), fromPatch(high.x, low.y, z1) }, orange.darker(130), 0 },
        };
        for (Face &face : faces)
            face.depth = glm::length((face.corners[0] + face.corners[2])*0.5f - eye);
        std::sort(faces, faces + 5, [](const Face &x, const Face &y) { return x.depth > y.depth; });
        for (const Face &face : faces)
        {
            painter.setBrush(face.color);
            const QPointF quad[4] = { project(face.corners[0]), project(face.corners[1]), project(face.corners[2]), project(face.corners[3]) };
            painter.drawPolygon(quad, 4);
        }
    }

    // the sonar contacts on the patch: a line from the ground up to each
    for (const auto &entry : scenario->sonar())
    {
        const Vector3D position = entry.object->center();
        if (!inPatch(position))
            continue;
        const bool hostile = scenario->isHostile(fight::Scenario::PlayerFaction, entry.object->iff());
        painter.setPen(entry.isFriend ? QColor(64, 200, 64) : hostile ? QColor(232, 32, 24) : QColor(64, 96, 255));
        painter.drawLine(project(Vector3D(position.x, position.y, surface.heightAt(position.x, position.y))), project(position));
    }
    painter.setPen(orange);
    painter.drawLine(project(target), project(boat));
}


void VoxelRadar::draw(fight::Scenario *scenario, const RectF &window)
{
    if (!m_valid)
        return;
    render(scenario);
    if (!m_texture.isValid())
        m_texture.createEmpty(m_image.width(), m_image.height(), gfx::Texture::RGBA, false);
    m_texture.update(0, 0, m_image);
    glColor4f(1, 1, 1, 1);
    m_texture.draw(window, RectF(0, 0, m_image.width(), m_image.height()));
}


} // namespace hud
