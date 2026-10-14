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
#include "util/datapath.h"
#include "gfx/pipeline.h"

#include <QFile>
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
// rendered at this many times the resolution and averaged down, so that the ground moving by
// fractions of a pixel shows as a gradual change rather than pixels stepping at uneven moments
const int SuperSample = 3;
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

    QFile table(util::DataPath::resolve(String("vfx:texture/%1.s16").arg(file.value("ShadingTable", "vox_000").toString())));
    const QByteArray data = table.open(QIODevice::ReadOnly) ? table.readAll() : QByteArray();
    if (m_rect.width <= 0 || m_rect.height <= 0 || data.size() < TableOffset + FogRows*Entries*2)
        return;
    m_table.resize(FogRows*Entries);
    const uchar *colors = reinterpret_cast<const uchar *>(data.constData()) + TableOffset;
    for (int i = 0; i < FogRows*Entries; i++)
    {
        const int c = colors[2*i] | (colors[2*i + 1] << 8);
        m_table[i] = gfx::rgb(((c >> 11) & 31)*255/31, ((c >> 5) & 63)*255/63, (c & 31)*255/31);
    }
    m_image = gfx::Image(m_rect.width, m_rect.height, gfx::Image::ARGB32);
    m_valid = true;
}


void VoxelRadar::limitHeight(int height)
{
    if (m_valid && height > 0 && height < m_rect.height)
    {
        m_rect.height = height;
        m_image = gfx::Image(m_rect.width, m_rect.height, gfx::Image::ARGB32);
    }
}


gfx::Rgb VoxelRadar::shade(int fog, int light) const
{
    return m_table[glm::clamp(fog, 0, FogRows - 1)*Entries + light];
}


// A small software renderer with a depth buffer, so that nearer ground hides what lies behind it
// pixel by pixel: the terrain and the blocks are filled triangles, the lines are tested against
// the depth of what has been filled.
void VoxelRadar::render(fight::Scenario *scenario)
{
    fight::Surface &surface = scenario->surface();
    const Vector3D boat = scenario->position();
    const float extent = m_extent*surface.scale().x;
    const float step = 2*extent/GridSteps;
    Vector2D forward(scenario->player()->dir());
    forward = glm::length(forward) > 1e-3f ? glm::normalize(forward) : Vector2D(0, 1);
    const Vector2D right(forward.y, -forward.x);
    const Vector2D middle(boat);
    auto toPatch = [&](const Vector3D &p) { const Vector2D way = Vector2D(p) - middle; return Vector2D(glm::dot(way, right), glm::dot(way, forward)); };
    auto fromPatch = [&](float x, float y, float z) { return Vector3D(middle + right*x + forward*y, z); };
    auto inPatch = [&](const Vector3D &p) {
        const Vector2D local = toPatch(p);
        return std::abs(local.x) <= extent && std::abs(local.y) <= extent;
    };

    // the height map's own grid (so the shading stays with the ground as the boat moves); lines
    // stand on it, so they meet the terrain as drawn
    const int first[2] = { int(std::floor((middle.x - 1.5f*extent)/step)), int(std::floor((middle.y - 1.5f*extent)/step)) };
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

    // the eye looks down at the ground under the boat from behind; in perspective, so the far
    // edge of the patch comes out narrower than the near one
    const float distance = 3.5f*extent;
    const Vector3D target(middle, ground(middle.x, middle.y));
    const Vector3D eye = target - Vector3D(forward, 0)*distance*std::cos(m_elevation) + Vector3D(0, 0, distance*std::sin(m_elevation));
    const Vector3D viewForward = glm::normalize(target - eye);
    const Vector3D viewRight(right, 0);
    const Vector3D viewUp = glm::cross(viewRight, viewForward);
    if (m_fine.width() != m_image.width()*SuperSample || m_fine.height() != m_image.height()*SuperSample)
        m_fine = gfx::Image(m_image.width()*SuperSample, m_image.height()*SuperSample, gfx::Image::ARGB32);
    const int width = m_fine.width(), height = m_fine.height();

    // fitted so that the patch, flat at the boat's ground level, fills the view and sits on its
    // bottom edge
    auto view = [&](const Vector3D &p) {
        const Vector3D v = p - eye;
        const float z = std::max(glm::dot(v, viewForward), 1.0f);
        return Vector3D(glm::dot(v, viewRight)/z, glm::dot(v, viewUp)/z, z);
    };
    // (worked out in the view's own frame, so the fit is exactly the same from frame to frame:
    // the boat, at the view's centre, then stays in the middle of the same radar pixel and its
    // line doesn't flicker between two columns)
    Vector2D low(1e9f), high(-1e9f);
    for (const float x : { -extent, extent })
        for (const float y : { -extent, extent })
        {
            const float depth = distance + y*std::cos(m_elevation);
            const Vector2D v(x/depth, y*std::sin(m_elevation)/depth);
            low = glm::min(low, v);
            high = glm::max(high, v);
        }
    const float focal = std::min(PatchWidth*width/(high.x - low.x), PatchHeight*height/(high.y - low.y));
    auto pixelMiddle = [](float x) { return (std::floor(x/SuperSample) + 0.5f)*SuperSample; };
    const Vector2D centre(pixelMiddle(0.5f*width - focal*0.5f*(low.x + high.x)), pixelMiddle(height - 1 + focal*low.y));
    // screen x, y and the depth
    auto project = [&](const Vector3D &p) {
        const Vector3D v = view(p);
        return Vector3D(centre.x + focal*v.x, centre.y - focal*v.y, v.z);
    };

    m_fine.fill(0);
    m_depth.assign(width*height, 1e30f);
    auto blend = [](gfx::Rgb under, gfx::Rgb over) {
        const int a = gfx::alpha(over);
        if (a == 255 || gfx::alpha(under) == 0)
            return over;
        auto mix = [a](int u, int o) { return (o*a + u*(255 - a))/255; };
        return gfx::rgba(mix(gfx::red(under), gfx::red(over)), mix(gfx::green(under), gfx::green(over)), mix(gfx::blue(under), gfx::blue(over)), std::max(gfx::alpha(under), a));
    };
    // translucent triangles are blended over what is there and leave the depth as it is
    auto fillTriangle = [&](const Vector3D &a, const Vector3D &b, const Vector3D &c, gfx::Rgb colour) {
        auto edge = [](const Vector3D &p, const Vector3D &q, float x, float y) { return (q.x - p.x)*(y - p.y) - (q.y - p.y)*(x - p.x); };
        const float area = edge(a, b, c.x, c.y);
        if (std::abs(area) < 1e-6f)
            return;
        const int x0 = std::max(0, int(std::floor(std::min({ a.x, b.x, c.x })))), x1 = std::min(width - 1, int(std::ceil(std::max({ a.x, b.x, c.x }))));
        const int y0 = std::max(0, int(std::floor(std::min({ a.y, b.y, c.y })))), y1 = std::min(height - 1, int(std::ceil(std::max({ a.y, b.y, c.y }))));
        const bool opaque = gfx::alpha(colour) == 255;
        for (int y = y0; y <= y1; y++)
        {
            gfx::Rgb *line = reinterpret_cast<gfx::Rgb *>(m_fine.scanLine(y));
            for (int x = x0; x <= x1; x++)
            {
                const float px = x + 0.5f, py = y + 0.5f;
                const float w0 = edge(b, c, px, py)/area, w1 = edge(c, a, px, py)/area, w2 = edge(a, b, px, py)/area;
                if (w0 < 0 || w1 < 0 || w2 < 0)
                    continue;
                const float z = w0*a.z + w1*b.z + w2*c.z;
                float &depth = m_depth[y*width + x];
                if (z >= depth)
                    continue;
                if (opaque)
                {
                    depth = z;
                    line[x] = colour;
                }
                else
                    line[x] = blend(line[x], colour);
            }
        }
    };
    auto fillPolygon = [&](const std::vector<Vector3D> &corners, gfx::Rgb colour) {
        std::vector<Vector3D> screen;
        for (const Vector3D &corner : corners)
            screen.push_back(project(corner));
        for (size_t k = 1; k + 1 < screen.size(); k++)
            fillTriangle(screen[0], screen[k], screen[k + 1], colour);
    };
    // lines from the ground up to positions on the patch; drawn at the end, at the radar's own
    // resolution so they stay crisp, and hidden where something nearer is
    struct Line { Vector3D foot, top; gfx::Rgb colour; };
    std::vector<Line> lines;
    auto drawLine = [&](const Vector3D &position, gfx::Rgb colour) {
        if (inPatch(position))
            lines.push_back({ project(Vector3D(position.x, position.y, ground(position.x, position.y))), project(position), colour });
    };

    // the terrain cells, cut at the patch's edges
    struct Cell { std::vector<Vector3D> corners; float height; float depth; };
    std::vector<Cell> cells;
    float lowest = 1e9f, highest = -1e9f, nearest = 1e9f, farthest = -1e9f;
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
            cell.depth = view(fromPatch(middle.x, middle.y, middle.z)).z;
            nearest = std::min(nearest, cell.depth);
            farthest = std::max(farthest, cell.depth);
            cells.push_back(cell);
        }
    // the range of shades follows the patch quickly but not in jumps
    const float time = scenario->time();
    const bool reset = m_lowest > m_highest || time < m_time || time - m_time > 1.0f;
    const float follow = reset ? 1.0f : std::min(1.0f, 10.0f*(time - m_time));
    m_time = time;
    m_lowest += (lowest - m_lowest)*follow;
    m_highest += (highest - m_highest)*follow;
    const float range = std::max(m_highest - m_lowest, 10.0f);
    for (const Cell &cell : cells)
    {
        // the lowest ground dark, the highest bright, slightly darker towards the far edge
        const int level = glm::clamp(int(8 + 54*(cell.height - m_lowest)/range), 0, LightLevels - 1);
        const int fog = FogRows - 1 - int(6*(cell.depth - nearest)/std::max(farthest - nearest, 1.0f));
        fillPolygon(cell.corners, shade(fog, level));
    }

    // a block on the patch (a box in the patch's frame, cut off at its edges)
    auto fillBlock = [&](const Vector3D &a, const Vector3D &b, gfx::Rgb colour) {
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
        fillPolygon({ fromPatch(lo.x, lo.y, z1), fromPatch(hi.x, lo.y, z1), fromPatch(hi.x, hi.y, z1), fromPatch(lo.x, hi.y, z1) }, gfx::lighter(colour, 130));
        fillPolygon({ fromPatch(lo.x, lo.y, z0), fromPatch(hi.x, lo.y, z0), fromPatch(hi.x, lo.y, z1), fromPatch(lo.x, lo.y, z1) }, colour);
        fillPolygon({ fromPatch(lo.x, hi.y, z0), fromPatch(hi.x, hi.y, z0), fromPatch(hi.x, hi.y, z1), fromPatch(lo.x, hi.y, z1) }, colour);
        fillPolygon({ fromPatch(lo.x, lo.y, z0), fromPatch(lo.x, hi.y, z0), fromPatch(lo.x, hi.y, z1), fromPatch(lo.x, lo.y, z1) }, gfx::darker(colour, 130));
        fillPolygon({ fromPatch(hi.x, lo.y, z0), fromPatch(hi.x, hi.y, z0), fromPatch(hi.x, hi.y, z1), fromPatch(hi.x, lo.y, z1) }, gfx::darker(colour, 130));
    };

    // buildings as orange blocks (opaque, so before anything translucent)
    const gfx::Rgb orange = shade(FogRows - 1, Orange);
    for (const auto &object : *scenario)
        if (object->isEnabled() && dynamic_cast<const fight::Building *>(object.get()))
            fillBlock(object->box().minPoint(), object->box().maxPoint(), orange);

    // the water currents as translucent blue boxes over their 2x2 blocks of map cells, one cell high
    const Vector3D &scale = surface.scale();
    for (const fight::Scenario::CurrentCell &current : scenario->currentCells())
    {
        const Vector2D a(current.x*scale.x, current.y*scale.y), b = a + Vector2D(2*scale.x, 2*scale.y);
        const float bottom = ground(0.5f*(a.x + b.x), 0.5f*(a.y + b.y));
        fillBlock(Vector3D(a, bottom), Vector3D(b, bottom + scale.x), gfx::rgba(40, 72, 232, 160));
    }

    // lines: debris (trash) green; sonar contacts red hostile, green friendly, blue neutral, the
    // locked target grey; the boat orange
    for (const auto &object : *scenario)
        if (object->isEnabled() && dynamic_cast<const fight::Trash *>(object.get()))
            drawLine(object->position(), gfx::rgb(64, 200, 64));
    const fight::ActiveObject *locked = scenario->target().locked();
    for (const auto &entry : scenario->sonar())
    {
        const bool hostile = scenario->isHostile(fight::Scenario::PlayerFaction, entry.object->iff());
        drawLine(entry.object->center(), entry.object == locked ? gfx::rgb(160, 160, 160) :
                 entry.isFriend ? gfx::rgb(64, 200, 64) : hostile ? gfx::rgb(232, 32, 24) : gfx::rgb(64, 96, 255));
    }
    drawLine(boat, orange);

    // averaged down to the radar's pixels (weighted by coverage, so edges fade out)
    for (int y = 0; y < m_image.height(); y++)
    {
        gfx::Rgb *out = reinterpret_cast<gfx::Rgb *>(m_image.scanLine(y));
        for (int x = 0; x < m_image.width(); x++)
        {
            int r = 0, g = 0, b = 0, a = 0;
            for (int dy = 0; dy < SuperSample; dy++)
            {
                const gfx::Rgb *in = reinterpret_cast<const gfx::Rgb *>(m_fine.constScanLine(y*SuperSample + dy)) + x*SuperSample;
                for (int dx = 0; dx < SuperSample; dx++)
                {
                    const int alpha = gfx::alpha(in[dx]);
                    r += gfx::red(in[dx])*alpha;
                    g += gfx::green(in[dx])*alpha;
                    b += gfx::blue(in[dx])*alpha;
                    a += alpha;
                }
            }
            out[x] = a > 0 ? gfx::rgba(r/a, g/a, b/a, a/(SuperSample*SuperSample)) : 0;
        }
    }

    for (const Line &line : lines)
    {
        const Vector3D a = line.foot/float(SuperSample), b = line.top/float(SuperSample);
        const int steps = int(std::max(std::abs(b.x - a.x), std::abs(b.y - a.y))) + 1;
        for (int k = 0; k <= steps; k++)
        {
            const Vector3D p = line.foot + (line.top - line.foot)*(float(k)/steps);
            const int x = int(std::floor(p.x/SuperSample)), y = int(std::floor(p.y/SuperSample));
            if (x < 0 || x >= m_image.width() || y < 0 || y >= m_image.height())
                continue;
            // the depth of what is drawn right where the line passes
            const int fx = glm::clamp(int(p.x), 0, width - 1), fy = glm::clamp(int(p.y), 0, height - 1);
            if (p.z > m_depth[fy*width + fx] + 0.5f*step)
                continue;
            reinterpret_cast<gfx::Rgb *>(m_image.scanLine(y))[x] = line.colour;
        }
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
    gfx::Pipeline::setColor(Vector4D(1));
    m_texture.draw(window, RectF(0, 0, m_image.width(), m_image.height()));
}


} // namespace hud
