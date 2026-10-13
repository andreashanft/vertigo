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
#include "fight/effects/trash.h"
#include "fight/target.h"
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
// the patch is drawn with the height map's cells at this size (patch widths)
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

    // the terrain: the height map's own grid (so the shading stays with the ground as the boat
    // moves), cut to the patch and drawn from far to near so that nearer cells cover what lies
    // behind; the lowest ground is dark, the highest bright
    auto toPatch = [&](const Vector3D &p) { const Vector2D way(p - boat); return Vector2D(glm::dot(way, right), glm::dot(way, forward)); };
    auto fromPatch = [&](float x, float y, float z) { return Vector3D(Vector2D(boat) + right*x + forward*y, z); };
    const float step = 2*extent/GridSteps;
    const int first[2] = { int(std::floor((boat.x - 1.5f*extent)/step)), int(std::floor((boat.y - 1.5f*extent)/step)) };
    const int count = int(3*extent/step) + 2;
    std::vector<float> heights(count*count);
    for (int j = 0; j < count; j++)
        for (int i = 0; i < count; i++)
            heights[j*count + i] = surface.heightAt((first[0] + i)*step, (first[1] + j)*step);

    struct Cell { std::vector<Vector3D> corners; float height; float depth; };
    std::vector<Cell> cells;
    float lowest = 1e9f, highest = -1e9f;
    for (int j = 0; j + 1 < count; j++)
        for (int i = 0; i + 1 < count; i++)
        {
            // the cell in the patch's frame (x, y) with its heights (z), cut at the patch's edges
            std::vector<Vector3D> polygon;
            for (const auto &c : { std::make_pair(0, 0), std::make_pair(1, 0), std::make_pair(1, 1), std::make_pair(0, 1) })
            {
                const Vector2D local = toPatch(Vector3D((first[0] + i + c.first)*step, (first[1] + j + c.second)*step, 0));
                polygon.push_back(Vector3D(local, heights[(j + c.second)*count + i + c.first]));
            }
            for (int axis = 0; axis < 2 && !polygon.empty(); axis++)
                for (const float sign : { -1.0f, 1.0f })
                {
                    std::vector<Vector3D> kept;
                    for (size_t k = 0; k < polygon.size(); k++)
                    {
                        const Vector3D &a = polygon[k], &b = polygon[(k + 1) % polygon.size()];
                        const float da = extent - sign*a[axis], db = extent - sign*b[axis];
                        if (da >= 0)
                            kept.push_back(a);
                        if ((da >= 0) != (db >= 0))
                            kept.push_back(a + (b - a)*(da/(da - db)));
                    }
                    polygon.swap(kept);
                }
            if (polygon.size() < 3)
                continue;
            Cell cell;
            Vector3D middle(0);
            for (const Vector3D &corner : polygon)
            {
                cell.corners.push_back(fromPatch(corner.x, corner.y, corner.z));
                middle += corner;
                lowest = std::min(lowest, corner.z);
                highest = std::max(highest, corner.z);
            }
            middle /= float(polygon.size());
            cell.height = middle.z;
            cell.depth = glm::length(fromPatch(middle.x, middle.y, middle.z) - eye);
            cells.push_back(cell);
        }
    // the range follows the patch slowly, so the shades don't jump
    const float time = scenario->time();
    const float blend = m_lowest > m_highest || time < m_time ? 1.0f : std::min(1.0f, 2.0f*(time - m_time));
    m_lowest += (lowest - m_lowest)*blend;
    m_highest += (highest - m_highest)*blend;
    m_time = time;
    const float range = std::max(m_highest - m_lowest, 10.0f);
    std::sort(cells.begin(), cells.end(), [](const Cell &a, const Cell &b) { return a.depth > b.depth; });
    const float farthest = cells.empty() ? 1 : cells.front().depth, nearest = cells.empty() ? 0 : cells.back().depth;
    for (const Cell &cell : cells)
    {
        const int level = glm::clamp(int(8 + 54*(cell.height - m_lowest)/range), 0, LightLevels - 1);
        // slightly darker towards the far edge
        const int fog = FogRows - 1 - int(6*(cell.depth - nearest)/std::max(farthest - nearest, 1.0f));
        painter.setBrush(QColor(shade(fog, level)));
        QPointF polygon[8];
        for (size_t k = 0; k < cell.corners.size(); k++)
            polygon[k] = project(cell.corners[k]);
        painter.drawPolygon(polygon, int(cell.corners.size()));
    }

    // a block on the patch (a box in the patch's frame, cut off at its edges), the faces farther
    // away first
    auto drawBlock = [&](const Vector3D &a, const Vector3D &b, const QColor &colour) {
        Vector2D low(1e9f), high(-1e9f);
        for (const Vector3D &corner : { a, Vector3D(a.x, b.y, 0), Vector3D(b.x, a.y, 0), b })
        {
            low = glm::min(low, toPatch(corner));
            high = glm::max(high, toPatch(corner));
        }
        low = glm::max(low, Vector2D(-extent));
        high = glm::min(high, Vector2D(extent));
        if (low.x >= high.x || low.y >= high.y)
            return;
        const float z0 = a.z, z1 = b.z;
        struct Face { Vector3D corners[4]; QColor color; float depth; };
        Face faces[5] = {
            { { fromPatch(low.x, low.y, z1), fromPatch(high.x, low.y, z1), fromPatch(high.x, high.y, z1), fromPatch(low.x, high.y, z1) }, colour.lighter(130), 0 },
            { { fromPatch(low.x, low.y, z0), fromPatch(high.x, low.y, z0), fromPatch(high.x, low.y, z1), fromPatch(low.x, low.y, z1) }, colour, 0 },
            { { fromPatch(low.x, high.y, z0), fromPatch(high.x, high.y, z0), fromPatch(high.x, high.y, z1), fromPatch(low.x, high.y, z1) }, colour, 0 },
            { { fromPatch(low.x, low.y, z0), fromPatch(low.x, high.y, z0), fromPatch(low.x, high.y, z1), fromPatch(low.x, low.y, z1) }, colour.darker(130), 0 },
            { { fromPatch(high.x, low.y, z0), fromPatch(high.x, high.y, z0), fromPatch(high.x, high.y, z1), fromPatch(high.x, low.y, z1) }, colour.darker(130), 0 },
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
    };

    // the water currents as blue boxes over their 2x2 blocks of map cells, one cell high
    const Vector3D &scale = surface.scale();
    for (const fight::Scenario::CurrentCell &current : scenario->currentCells())
    {
        const Vector2D a(current.x*scale.x, current.y*scale.y), b = a + Vector2D(2*scale.x, 2*scale.y);
        const float ground = surface.heightAt(0.5f*(a.x + b.x), 0.5f*(a.y + b.y));
        drawBlock(Vector3D(a, ground), Vector3D(b, ground + scale.x), QColor(40, 72, 232, 160));
    }

    // buildings as orange blocks
    const QColor orange(shade(FogRows - 1, Orange));
    for (const auto &object : *scenario)
    {
        const fight::Building *building = dynamic_cast<const fight::Building *>(object.get());
        if (building && building->isEnabled())
            drawBlock(building->box().minPoint(), building->box().maxPoint(), orange);
    }

    // lines up from the ground: debris (trash) in green
    auto drawLine = [&](const Vector3D &position, const QColor &colour) {
        if (!inPatch(position))
            return;
        painter.setPen(colour);
        painter.drawLine(project(Vector3D(position.x, position.y, surface.heightAt(position.x, position.y))), project(position));
    };
    for (const auto &object : *scenario)
        if (dynamic_cast<const fight::Trash *>(object.get()) && object->isEnabled())
            drawLine(object->position(), QColor(64, 200, 64));

    // the sonar contacts: red hostile, green friendly, blue neutral, the locked target grey
    const fight::ActiveObject *locked = scenario->target().locked();
    for (const auto &entry : scenario->sonar())
    {
        const bool hostile = scenario->isHostile(fight::Scenario::PlayerFaction, entry.object->iff());
        drawLine(entry.object->center(), entry.object == locked ? QColor(160, 160, 160) :
                 entry.isFriend ? QColor(64, 200, 64) : hostile ? QColor(232, 32, 24) : QColor(64, 96, 255));
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
