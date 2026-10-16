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
#include "fight/boats/simpleboat.h"
#include "fight/orders.h"
#include "gfx/pipeline.h"

#include <glm/geometric.hpp>
#include <set>
#include <vector>


namespace hud {


namespace {

String typeName(int type)
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
    return String::number(type);
}

}


DebugOverlay::DebugOverlay(HUD *hud) :
    ui::Widget(hud->widget()),
    m_hud(hud)
{
}


void DebugOverlay::draw()
{
    drawCurrents();
    drawLabels();
    drawInfo();
}


// an arrow per current cell near the player, a little below it (so it is seen from above), in the direction of the flow
void DebugOverlay::drawCurrents()
{
    fight::Scenario *scenario = m_hud->scenario();
    const Matrix m = m_hud->hudProjectionMatrixInverted() * scenario->projectionMatrix() * scenario->cameraMatrix();
    const Vector3D &scale = scenario->surface().scale();
    auto project = [&m](const Vector3D &point, Vector2D &result) {
        const Vector4D point4 = m * Vector4D(point, 1);
        result = Vector2D(point4)/point4.w;
        return point4.z <= 0;
    };

    std::vector<Vector3D> lines;
    for (const auto &cell : scenario->currentCells()) {
        const Vector3D centre((cell.x + 1)*scale.x, (cell.y + 1)*scale.y, scenario->position().z - 8.0f);
        if (glm::distance(centre, scenario->position()) > 200.0f)
            continue;
        const Vector3D end = centre + glm::normalize(cell.velocity)*8.0f;
        const Vector3D side = glm::length(Vector2D(cell.velocity)) > 0 ? glm::normalize(Vector3D(-cell.velocity.y, cell.velocity.x, 0))*2.0f : Vector3D(2.0f, 0, 0);
        const Vector3D back = end - glm::normalize(cell.velocity)*3.0f;
        Vector2D a, b, c, d;
        if (!project(centre, a) || !project(end, b) || !project(back + side, c) || !project(back - side, d))
            continue;
        for (const Vector2D &point : { a, b, b, c, b, d })
            lines.push_back(Vector3D(point, 0));
    }
    const Vector4D color = gfx::Pipeline::color();
    gfx::Pipeline::setColor(Vector4D(0.2f, 0.6f, 1.0f, 1.0f));
    gfx::Pipeline::drawLines(lines.data(), int(lines.size()));
    gfx::Pipeline::setColor(color);
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

    StringList lines;
    lines << String("%1  T %2").arg(scenario->name()).arg(scenario->time(), 0, 'f', 1);
    lines << String("POS %1 %2 %3  CELL %4 %5  H %6")
             .arg(int(pos.x)).arg(int(pos.y)).arg(int(pos.z))
             .arg(int(pos.x/scale.x)).arg(int(pos.y/scale.y))
             .arg(int(scenario->height()));
    lines << String("SPEED %1  OBJECTS %2/%3")
             .arg(int(glm::length(scenario->player()->velocity())))
             .arg(enabled).arg(scenario->objectRecords().size());
    lines << String("SONAR %1").arg(scenario->sonar().isActive() ? "ACTIVE" : "PASSIVE");
    const Vector3D current = scenario->current(pos);
    if (glm::length(current) > 0.01f)
        lines << String("CURRENT %1 %2 %3 M/S").arg(current.x, 0, 'f', 1).arg(current.y, 0, 'f', 1).arg(current.z, 0, 'f', 1);
    const HUD::FrameRate &rate = m_hud->frameRate();
    if (rate.fps > 0) {
        String line = String("FPS %1  FRAME %2 MS  MAX %3  CPU %4 MS")
                      .arg(rate.fps, 0, 'f', 1).arg(rate.frame, 0, 'f', 1).arg(rate.longest, 0, 'f', 1).arg(rate.work, 0, 'f', 1);
#ifndef NDEBUG
        // unoptimised, the game draws several times slower
        line += "  DEBUG BUILD";
#endif
        lines << line;
    }

    gfx::Font &font = m_hud->fontYellow();
    // right of the touch controls' buttons in the corner
    Point p = m_hud->project(Point(HUD::touchControls() ? 72 : 8, 8));
    for (const String &line : lines) {
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

        String text = String("%1 %2 D%3").arg(record.id).arg(record.des.isEmpty() ? typeName(record.type) : record.des).arg(int(distance));
        gfx::Font *font = &m_hud->fontGreen();
        if (!object->isEnabled()) {
            font = &m_hud->fontRed();
        } else if (const fight::ActiveObject *active = dynamic_cast<const fight::ActiveObject*>(object)) {
            text += String(" IFF%1 S%2/%3").arg(active->iff())
                    .arg(int(scenario->sonar().passiveRange(active)))
                    .arg(int(scenario->sonar().activeRange(active)));
            const float speed = glm::length(active->velocity());
            if (speed > 0.1f)
                text += String(" V%1").arg(int(speed*3.6f));
            if (active->orders() && active->orders()->currentSlot())
                text += String(" O%1 %2").arg(active->orders()->current()).arg(active->orders()->state());
            if (active->isAsleep())
                text += " ASLEEP";
            if (detected.count(object))
                font = &m_hud->fontYellow();
        }
        font->draw(text.toUpper(), point.x + 4, point.y - font->height()/2);
    }
}


} // namespace hud
