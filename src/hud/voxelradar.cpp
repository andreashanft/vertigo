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
#include <array>
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
    const float extent = m_extent*surface.scale().x;
    const float step = 2*extent/GridSteps;

    // the heading and the height the view is centred on follow the boat smoothly, so the view
    // doesn't shake with every small turn or bump of the ground
    const float time = scenario->time();
    const bool reset = m_time <= 0 || time < m_time || time - m_time > 1.0f;
    const float blend = reset ? 1.0f : std::min(1.0f, 4.0f*(time - m_time));
    m_time = time;
    Vector2D heading(scenario->player()->dir());
    if (glm::length(heading) > 1e-3f)
        m_heading = reset || glm::length(m_heading) < 1e-3f ? glm::normalize(heading) : glm::normalize(m_heading + (glm::normalize(heading) - m_heading)*blend);
    const Vector2D forward = glm::length(m_heading) > 1e-3f ? m_heading : Vector2D(0, 1);
    const Vector2D right(forward.y, -forward.x);
    auto toPatch = [&](const Vector3D &p) { const Vector2D way(p - boat); return Vector2D(glm::dot(way, right), glm::dot(way, forward)); };
    auto fromPatch = [&](float x, float y, float z) { return Vector3D(Vector2D(boat) + right*x + forward*y, z); };
    auto inPatch = [&](const Vector3D &p) {
        const Vector2D local = toPatch(p);
        return std::abs(local.x) <= extent && std::abs(local.y) <= extent;
    };

    // the height map's own grid (so the shading stays with the ground as the boat moves); the
    // ground under lines is taken from it too, so they stand on the terrain as drawn
    const int first[2] = { int(std::floor((boat.x - 1.5f*extent)/step)), int(std::floor((boat.y - 1.5f*extent)/step)) };
    const int count = int(3*extent/step) + 2;
    std::vector<float> heights(count*count);
    for (int j = 0; j < count; j++)
        for (int i = 0; i < count; i++)
            heights[j*count + i] = surface.heightAt((first[0] + i)*step, (first[1] + j)*step);
    auto ground = [&](float x, float y) {
        const float u = glm::clamp(x/step - first[0], 0.0f, count - 1.001f), v = glm::clamp(y/step - first[1], 0.0f, count - 1.001f);
        const int i = int(u), j = int(v);
        const float fu = u - i, fv = v - j;
        return (heights[j*count + i]*(1 - fu) + heights[j*count + i + 1]*fu)*(1 - fv) + (heights[(j + 1)*count + i]*(1 - fu) + heights[(j + 1)*count + i + 1]*fu)*fv;
    };
    m_ground = reset ? ground(boat.x, boat.y) : m_ground + (ground(boat.x, boat.y) - m_ground)*blend;

    // the eye looks down at the ground under the boat from behind; in perspective, so the far
    // edge of the patch comes out narrower than the near one
    const float distance = 3.5f*extent;
    const Vector3D target(boat.x, boat.y, m_ground);
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
    auto depth = [&](const Vector3D &p) { return glm::length(p - eye); };

    // everything is drawn from far to near, so nearer ground covers what lies behind it
    struct Item
    {
        float depth;
        QPolygonF polygon;  // a filled shape, or a line with two points
        QColor colour;
        float height;       // terrain: for the shade, set once the height range is known
        bool terrain;
    };
    std::vector<Item> items;

    // the terrain cells, cut at the patch's edges
    float lowest = 1e9f, highest = -1e9f;
    for (int j = 0; j + 1 < count; j++)
        for (int i = 0; i + 1 < count; i++)
        {
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
            Item item;
            item.terrain = true;
            Vector3D middle(0);
            for (const Vector3D &corner : polygon)
            {
                item.polygon << project(fromPatch(corner.x, corner.y, corner.z));
                middle += corner;
                lowest = std::min(lowest, corner.z);
                highest = std::max(highest, corner.z);
            }
            middle /= float(polygon.size());
            item.height = middle.z;
            item.depth = depth(fromPatch(middle.x, middle.y, middle.z));
            items.push_back(item);
        }
    const float rangeBlend = reset || m_lowest > m_highest ? 1.0f : blend;
    m_lowest += (lowest - m_lowest)*rangeBlend;
    m_highest += (highest - m_highest)*rangeBlend;

    // a block on the patch (a box in the patch's frame, cut off at its edges), face by face
    auto addBlock = [&](const Vector3D &a, const Vector3D &b, const QColor &colour) {
        Vector2D lo(1e9f), hi(-1e9f);
        for (const Vector3D &corner : { a, Vector3D(a.x, b.y, 0), Vector3D(b.x, a.y, 0), b })
        {
            lo = glm::min(lo, toPatch(corner));
            hi = glm::max(hi, toPatch(corner));
        }
        lo = glm::max(lo, Vector2D(-extent));
        hi = glm::min(hi, Vector2D(extent));
        if (lo.x >= hi.x || lo.y >= hi.y)
            return;
        const float z0 = a.z, z1 = b.z;
        const std::pair<std::array<Vector3D, 4>, QColor> faces[5] = {
            { {{ fromPatch(lo.x, lo.y, z1), fromPatch(hi.x, lo.y, z1), fromPatch(hi.x, hi.y, z1), fromPatch(lo.x, hi.y, z1) }}, colour.lighter(130) },
            { {{ fromPatch(lo.x, lo.y, z0), fromPatch(hi.x, lo.y, z0), fromPatch(hi.x, lo.y, z1), fromPatch(lo.x, lo.y, z1) }}, colour },
            { {{ fromPatch(lo.x, hi.y, z0), fromPatch(hi.x, hi.y, z0), fromPatch(hi.x, hi.y, z1), fromPatch(lo.x, hi.y, z1) }}, colour },
            { {{ fromPatch(lo.x, lo.y, z0), fromPatch(lo.x, hi.y, z0), fromPatch(lo.x, hi.y, z1), fromPatch(lo.x, lo.y, z1) }}, colour.darker(130) },
            { {{ fromPatch(hi.x, lo.y, z0), fromPatch(hi.x, hi.y, z0), fromPatch(hi.x, hi.y, z1), fromPatch(hi.x, lo.y, z1) }}, colour.darker(130) },
        };
        for (const auto &face : faces)
        {
            Item item;
            item.terrain = false;
            item.colour = face.second;
            for (const Vector3D &corner : face.first)
                item.polygon << project(corner);
            item.depth = depth((face.first[0] + face.first[2])*0.5f);
            items.push_back(item);
        }
    };
    // a line from the ground up to a position on the patch, as far away as its foot
    auto addLine = [&](const Vector3D &position, const QColor &colour) {
        if (!inPatch(position))
            return;
        const Vector3D foot(position.x, position.y, ground(position.x, position.y));
        Item item;
        item.terrain = false;
        item.colour = colour;
        item.polygon << project(foot) << project(position);
        item.depth = depth(foot) - 0.5f*step;
        items.push_back(item);
    };

    // the water currents as blue boxes over their 2x2 blocks of map cells, one cell high
    const Vector3D &scale = surface.scale();
    for (const fight::Scenario::CurrentCell &current : scenario->currentCells())
    {
        const Vector2D a(current.x*scale.x, current.y*scale.y), b = a + Vector2D(2*scale.x, 2*scale.y);
        const float bottom = ground(0.5f*(a.x + b.x), 0.5f*(a.y + b.y));
        addBlock(Vector3D(a, bottom), Vector3D(b, bottom + scale.x), QColor(40, 72, 232, 160));
    }
    // buildings as orange blocks, debris (trash) as green lines
    const QColor orange(shade(FogRows - 1, Orange));
    for (const auto &object : *scenario)
    {
        if (!object->isEnabled())
            continue;
        if (dynamic_cast<const fight::Building *>(object.get()))
            addBlock(object->box().minPoint(), object->box().maxPoint(), orange);
        else if (dynamic_cast<const fight::Trash *>(object.get()))
            addLine(object->position(), QColor(64, 200, 64));
    }
    // the sonar contacts: red hostile, green friendly, blue neutral, the locked target grey
    const fight::ActiveObject *locked = scenario->target().locked();
    for (const auto &entry : scenario->sonar())
    {
        const bool hostile = scenario->isHostile(fight::Scenario::PlayerFaction, entry.object->iff());
        addLine(entry.object->center(), entry.object == locked ? QColor(160, 160, 160) :
                entry.isFriend ? QColor(64, 200, 64) : hostile ? QColor(232, 32, 24) : QColor(64, 96, 255));
    }
    // the boat in orange
    addLine(boat, orange);

    std::stable_sort(items.begin(), items.end(), [](const Item &a, const Item &b) { return a.depth > b.depth; });
    const float range = std::max(m_highest - m_lowest, 10.0f);
    const float farthest = items.empty() ? 1 : items.front().depth, nearest = items.empty() ? 0 : items.back().depth;
    m_image.fill(Qt::transparent);
    QPainter painter(&m_image);
    for (const Item &item : items)
    {
        if (item.polygon.size() == 2)
        {
            painter.setPen(item.colour);
            painter.drawLine(item.polygon[0], item.polygon[1]);
            continue;
        }
        QColor colour = item.colour;
        if (item.terrain)
        {
            // the lowest ground dark, the highest bright, slightly darker towards the far edge
            const int level = glm::clamp(int(8 + 54*(item.height - m_lowest)/range), 0, LightLevels - 1);
            const int fog = FogRows - 1 - int(6*(item.depth - nearest)/std::max(farthest - nearest, 1.0f));
            colour = QColor(shade(fog, level));
        }
        painter.setPen(Qt::NoPen);
        painter.setBrush(colour);
        painter.drawPolygon(item.polygon);
    }
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
