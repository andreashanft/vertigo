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

#include "debugoverlay.h"
#include "hud.h"
#include "fight/scenario.h"
#include "fight/objects/activeobject.h"

#include <glm/geometric.hpp>
#include <set>


namespace hud {


namespace {

QString typeName(int type)
{
    switch (type) {
    case 2049: return "boat";
    case 2050: return "bomber";
    case 2051: return "tank";
    case 2052: return "tower";
    case 2053: return "crawler";
    case 2054: return "trash";
    case 2056: return "building";
    case 2058: return "mine";
    case 2059: return "torpedotower";
    case 2060: return "space";
    case 2061: return "navpoint";
    case 2062: return "activebuilding";
    }
    return QString::number(type);
}

}


DebugOverlay::DebugOverlay(HUD *hud) :
    ui::Widget(hud->widget()),
    m_hud(hud)
{
}


void DebugOverlay::draw()
{
    drawLabels();
    drawInfo();
}


void DebugOverlay::drawInfo()
{
    fight::Scenario *scenario = m_hud->scenario();
    const Vector3D &pos = scenario->position();
    const Vector3D &scale = scenario->surface().scale();

    int enabled = 0;
    for (const auto &record : scenario->objectRecords())
        if (record.object->isEnabled())
            enabled++;

    QStringList lines;
    lines << QString("%1  T %2").arg(scenario->name()).arg(scenario->time(), 0, 'f', 1);
    lines << QString("POS %1 %2 %3  CELL %4 %5  H %6")
             .arg(int(pos.x)).arg(int(pos.y)).arg(int(pos.z))
             .arg(int(pos.x/scale.x)).arg(int(pos.y/scale.y))
             .arg(int(scenario->height()));
    lines << QString("SPEED %1  OBJECTS %2/%3")
             .arg(int(glm::length(scenario->player()->velocity())))
             .arg(enabled).arg(scenario->objectRecords().size());
    lines << QString("SONAR %1").arg(scenario->sonar().isActive() ? "ACTIVE" : "PASSIVE");

    gfx::Font &font = m_hud->fontYellow();
    Point p = m_hud->project(Point(8, 8));
    for (const QString &line : lines) {
        font.draw(line, p.x, p.y);
        p.y += font.height() + 2;
    }
}


void DebugOverlay::drawLabels()
{
    fight::Scenario *scenario = m_hud->scenario();
    const Matrix m = m_hud->hudProjectionMatrixInverted() * scenario->projectionMatrix() * scenario->cameraMatrix();

    std::set<const fight::Object*> detected;
    for (const auto &entry : scenario->sonar())
        detected.insert(entry.object);

    // yellow: detected by the sonar, green: enabled, red: not enabled (yet)
    for (const auto &record : scenario->objectRecords()) {
        const fight::Object *object = record.object;
        const float distance = glm::distance(object->center(), scenario->position());
        if (distance > 500.0f)
            continue;

        const Vector4D point4 = m * Vector4D(object->center(), 1);
        if (point4.z > 0)
            continue;
        const Point point = glm::round(Vector2D(point4)/point4.w);

        QString text = QString("%1 %2 D%3").arg(record.id).arg(record.des.isEmpty() ? typeName(record.type) : record.des).arg(int(distance));
        gfx::Font *font = &m_hud->fontGreen();
        if (!object->isEnabled()) {
            font = &m_hud->fontRed();
        } else if (const fight::ActiveObject *active = dynamic_cast<const fight::ActiveObject*>(object)) {
            text += QString(" IFF%1 S%2/%3").arg(active->iff())
                    .arg(int(scenario->sonar().passiveRange(active)))
                    .arg(int(scenario->sonar().activeRange(active)));
            if (detected.count(object))
                font = &m_hud->fontYellow();
        }
        font->draw(text.toUpper(), point.x + 4, point.y - font->height()/2);
    }
}


} // namespace hud
