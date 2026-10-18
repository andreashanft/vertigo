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

#include "sonarmonitor.h"
#include "txt/desfile.h"
#include "hud.h"
#include "fight/scenario.h"
#include "fight/objects/activeobject.h"
#include "fight/objects/navpoint.h"
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>


namespace hud {


// a 3x3 dot in a 4x4 image
gfx::Image dot(gfx::Rgb color)
{
    gfx::Image image(4, 4, gfx::Image::ARGB32);
    for (int i = 0; i < 16; i++)
        image.setPixel(i % 4, i / 4, i % 4 < 3 && i < 12 ? color : 0);
    return image;
}

SonarMonitor::SonarMonitor(HUD *hud, const txt::DesFile &file, int scale, fight::Scenario::Monitor monitor) :
    ui::Widget(hud->widget()),
    m_hud(hud),
    m_point(file.value("X1").toInt(), file.value("Y1").toInt()),
    m_radius(file.value("Radius").toInt()),
    m_scale(scale),
    m_monitor(monitor),
    m_lamp(hud, file)
{
    m_center.fromImage(dot(0xffa88400), true);
    m_nav.fromImage(dot(0xff205c98), true);
    m_friend.fromImage(dot(0xff408038), true);
    m_enemy.fromImage(dot(0xffa00000), true);
    m_target.fromImage(dot(0xff888c88), true);
}


void SonarMonitor::draw()
{
    if (m_hud->scenario()->monitorBlanked(m_monitor))
        return;
    m_lamp.draw(m_hud->scenario()->sonar().isActive());
    Point point = m_hud->project(m_point);
    int radius = (m_hud->project(m_point + Point(0, m_radius)) - point).y;
    float scale = static_cast<float>(radius)/m_scale;
    PointF center = PointF(point) - PointF(1.5f);
    RectF rect = RectF(center, SizeF(4, 4));
    Matrix m(1);
    m = glm::scale(m, Vector3D(1, -1, 0));
    m = glm::rotate(m, m_hud->scenario()->yaw(), Vector3D(0, 0, 1));
    m = glm::translate(m, -m_hud->scenario()->position());

    m_hud->fontGreen().draw("T", point + Point(-2, -radius));
    m_hud->fontGreen().draw(String("%1M").arg(m_scale), Rect(point + Point(-100, radius - 8), SizeF(200, -1)), true, false);
    m_center.draw(rect);

    for (fight::NavPoint *navPoint : m_hud->scenario()->navPoints())
        if (navPoint->isEnabled())
        {
            Vector2D dir = Vector2D(m * Vector4D(navPoint->position(), 1));
            float distance = glm::length(dir);
            if (distance < m_scale)
            {
                rect.setPos(center - dir*scale);
                m_nav.draw(rect);
            }
        }

    for (const auto &entry : m_hud->scenario()->sonar())
    {
        // the original draws no neutral contact here once the sonar tells friend from foe
        if (entry.isNeutral)
            continue;
        Vector2D dir = Vector2D(m * Vector4D(entry.object->position(), 1));
        float distance = glm::length(dir);
        if (distance < m_scale)
        {
            rect.setPos(center - dir*scale);
            (entry.isFriend ? m_friend : m_enemy).draw(rect);
        }
    }

    fight::Target &target = m_hud->scenario()->target();
    if (target.isLocked())
    {
        Vector2D dir = Vector2D(m * Vector4D(target.position(), 1));
        float distance = glm::length(dir);
        if (distance < m_scale)
        {
            rect.setPos(center - dir*scale);
            m_target.draw(rect);
        }
    }
}


} // namespace hud
