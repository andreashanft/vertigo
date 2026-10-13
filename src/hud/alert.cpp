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

#include "alert.h"
#include "hud.h"
#include "fight/scenario.h"


namespace hud {


namespace {

const float AlertDuration = 3.0f;

}


Alert::Alert(HUD *hud, Rect rect) :
    ui::Widget(hud->widget()),
    m_hud(hud),
    m_rect(rect),
    m_alert(hud->getImage("hudalert"), true)
{
}


void Alert::draw()
{
    fight::Scenario *scenario = m_hud->scenario();
    const float lastHit = scenario->player()->lastHit();
    if (lastHit >= 0 && scenario->time() - lastHit < AlertDuration && scenario->blink())
    {
        const Rect rect = m_hud->projectCenter(m_rect);
        m_alert.draw(rect.x, rect.y);
    }
}


} // namespace hud
