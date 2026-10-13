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
#include "fight/boats/player.h"
#include "fight/objects/activeobject.h"
#include "txt/desfile.h"

#include <QFile>
#include <QGLContext>
#include <glm/geometric.hpp>
#include <glm/common.hpp>
#include <glm/trigonometric.hpp>
#include <algorithm>
#include <cmath>


namespace hud {


namespace {

// VOX_nnn.S16: 32 rows of 256 RGB565 colours from 0x31a on, from the farthest (the water's
// colour) to the nearest; in each row 0-63 is a ramp from dark to light and 64-66 are marker
// colours.
const int TableOffset = 0x31a;
const int FogRows = 32;
const int Entries = 256;
const int LightLevels = 64;
const int MarkerBoat = 64;
const int MarkerContact = 65;

}


VoxelRadar::VoxelRadar(txt::DesFile &file)
{
    file.setSection("voxelradar0");
    if (!file.contains("width"))
        return;
    m_rect = Rect(file.value("StartX").toInt(), file.value("StartY").toInt(), file.value("Width").toInt(), file.value("Height").toInt());
    m_behind = -file.value("WorldMinY", -5).toFloat();
    m_ahead = file.value("WorldMaxY", 20).toFloat();
    m_focal = 0.5f*m_rect.width/std::tan(glm::radians(file.value("Projection", 30).toFloat()));
    m_horizon = file.value("FlightY", 16).toInt();

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


QRgb VoxelRadar::shade(int fog, int light) const
{
    return m_table[glm::clamp(fog, 0, FogRows - 1)*Entries + light];
}


void VoxelRadar::mark(int x, int y, QRgb color)
{
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++)
            if (x + dx >= 0 && x + dx < m_image.width() && y + dy >= 0 && y + dy < m_image.height())
                m_image.setPixel(x + dx, y + dy, color);
}


// A ray per column, marching from near to far over the height map; each sample fills the
// column from the lowest row not drawn yet up to its height, so nearer terrain hides what
// lies behind it.
void VoxelRadar::render(fight::Scenario *scenario)
{
    fight::Surface &surface = scenario->surface();
    const float cell = surface.scale().x;
    const Vector3D boat = scenario->position();
    Vector2D forward(scenario->player()->dir());
    forward = glm::length(forward) > 1e-3f ? glm::normalize(forward) : Vector2D(0, 1);
    const Vector2D right(forward.y, -forward.x);

    // the eye: behind the boat and above it, so the boat is near the bottom of the view
    const Vector2D eye = Vector2D(boat) - forward*m_behind*cell;
    const float eyeHeight = std::max(boat.z, surface.heightAt(eye.x, eye.y)) + 2*cell;
    const float nearest = 0.5f*m_behind*cell;
    const float farthest = (m_behind + m_ahead)*cell;
    const int width = m_image.width();
    const int height = m_image.height();
    const Vector3D light = glm::normalize(Vector3D(-0.6f, 0.5f, 0.5f));

    m_image.fill(shade(0, 0));
    for (int x = 0; x < width; x++)
    {
        const Vector2D ray = forward + right*((x + 0.5f - 0.5f*width)/m_focal);
        int lowest = height;    // rows from here down are drawn
        for (float z = nearest; z < farthest && lowest > 0; z += 1.0f + z*0.012f)
        {
            const Vector2D point = eye + ray*z;
            Vector3D normal;
            const float ground = surface.heightAt(point.x, point.y, normal);
            const int row = int(m_horizon + m_focal*(eyeHeight - ground)/z);
            if (row >= lowest)
                continue;
            const int fog = FogRows - 1 - int((FogRows - 1)*(z - nearest)/(farthest - nearest));
            const int level = glm::clamp(int(4 + 44*std::max(0.0f, glm::dot(normal, light))), 0, LightLevels - 1);
            const QRgb color = shade(fog, level);
            for (int y = std::max(row, 0); y < lowest; y++)
                m_image.setPixel(x, y, color);
            lowest = std::max(row, 0);
        }
    }

    // the boat and the sonar contacts, where they are in the view
    auto project = [&](const Vector3D &position, int marker) {
        const Vector2D way = Vector2D(position) - eye;
        const float z = glm::dot(way, forward);
        if (z < nearest || z > farthest)
            return;
        const int x = int(0.5f*width + m_focal*glm::dot(way, right)/z);
        const int y = int(m_horizon + m_focal*(eyeHeight - position.z)/z);
        mark(x, y, shade(FogRows - 1, marker));
    };
    for (const auto &entry : scenario->sonar())
        project(entry.object->center(), MarkerContact);
    project(boat, MarkerBoat);
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
