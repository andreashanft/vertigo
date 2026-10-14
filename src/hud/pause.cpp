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

#include "pause.h"
#include "hud.h"
#include "util/ticks.h"


namespace hud {


Pause::Pause(HUD *hud, Rect rect) :
    ui::Widget(hud->widget()),
    m_hud(hud),
    m_rect(rect),
    m_pause(hud->getImage("hudpause"), true)
{
}


void Pause::draw()
{
    // mission time stands still, so blink with the wall clock
    if (m_hud->isPaused() && (util::ticks()/500) % 2 == 0)
    {
        const Rect rect = m_hud->projectCenter(m_rect);
        m_pause.draw(rect.x, rect.y);
    }
}


} // namespace hud
