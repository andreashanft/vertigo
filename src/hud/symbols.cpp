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

#include "symbols.h"
#include "hud.h"
#include "fight/scenario.h"

#include <glm/geometric.hpp>


namespace hud {


Threat::Threat(HUD *hud, Rect rect) :
    ui::Widget(hud->widget()),
    m_hud(hud),
    m_rect(rect),
    m_threat(hud->getImage("hudthrea"), true)
{
}


void Threat::draw()
{
    fight::Scenario *scenario = m_hud->scenario();
    if (scenario->threatsTo(scenario->player()) > 0 && scenario->blink())
    {
        const Rect rect = m_hud->projectCenter(m_rect);
        m_threat.draw(rect.x, rect.y);
    }
}


Symbols::Symbols(HUD *hud, Rect rect) :
    ui::Widget(hud->widget()),
    m_hud(hud),
    m_rect(rect),
    m_lock(hud->getImage("hudsym1"), true),
    m_radiation(hud->getImage("hudsym2"), true),
    m_current(hud->getImage("hudsym3"), true)
{
}


void Symbols::draw()
{
    fight::Scenario *scenario = m_hud->scenario();
    const Rect rect = m_hud->projectCenter(m_rect);
    // side by side, in the order of the images
    const int width = m_lock.width();
    if (scenario->player()->lockProgress() >= 1)
        m_lock.draw(rect.x, rect.y);
    if (glm::length(scenario->current(scenario->position())) > 0.5f)
        m_current.draw(rect.x + 2*width, rect.y);
}


} // namespace hud
