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

#ifndef HUD_SONARMONITOR_H
#define HUD_SONARMONITOR_H


#include "util/geometry2d.h"
#include "ui/widget.h"
#include "gfx/texture.h"
#include "lamp.h"
#include "fight/scenario.h"


namespace txt { class DesFile; }


namespace hud {


class HUD;


class SonarMonitor : public ui::Widget
{
public:
    // the lamp beside it (from the monitor's section) is lit while the active sonar is on
    SonarMonitor(HUD *hud, const txt::DesFile &file, int scale, fight::Scenario::Monitor monitor);

public:
    int scale() const { return m_scale; }
    void setScale(int scale) { m_scale = scale; }
    // the contacts round another point of the 640x480 view, without the lamp (the chase view's)
    void drawAt(const Point &center, int radius);

protected:
    void draw();

private:
    HUD *m_hud;
    Point m_point;
    int m_radius;
    int m_scale;
    fight::Scenario::Monitor m_monitor;  // which one, for the radiation's flicker

    gfx::Texture m_center;
    gfx::Texture m_nav;
    gfx::Texture m_friend;
    gfx::Texture m_enemy;
    gfx::Texture m_target;
    Lamp m_lamp;
};


} // namespace hud


#endif // HUD_SONARMONITOR_H
