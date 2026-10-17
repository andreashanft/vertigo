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

#include <cmath>
#include <glm/geometric.hpp>


namespace hud {


namespace {

// the slots' offsets in the block (docs/ROADMAP.md, appendix E)
const int RadiationSlot = 0;
const int CurrentSlot = 28;
const int AttackedSlot = 56;
// a symbol is shown for this long and hidden for as long, from the moment it is switched on
const float BlinkPhase = 0.5f;

} // namespace


Threat::Threat(HUD *hud, Rect rect) :
    ui::Widget(hud->widget()),
    m_hud(hud),
    m_rect(rect),
    m_threat(hud->getImage("hudthrey"), true)
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


Symbols::Symbol::Symbol(HUD *hud, const char *image, int x) :
    texture(hud->getImage(image), true),
    x(x)
{
}


void Symbols::Symbol::draw(bool on, float time, const Rect &block)
{
    if (!on)
    {
        shownAt = -1;
        return;
    }
    if (shownAt < 0)
        shownAt = time;
    if (std::fmod(time - shownAt, 2*BlinkPhase) < BlinkPhase)
        texture.draw(block.x + x, block.y);
}


Symbols::Symbols(HUD *hud, Rect rect) :
    ui::Widget(hud->widget()),
    m_hud(hud),
    m_rect(rect),
    m_radiation(hud, "hudsym2", RadiationSlot),
    m_current(hud, "hudsym3", CurrentSlot),
    m_attacked(hud, "hudsym1", AttackedSlot)
{
}


void Symbols::draw()
{
    fight::Scenario *scenario = m_hud->scenario();
    const Rect rect = m_hud->projectCenter(m_rect);
    const float time = scenario->time();
    // the radiation less than 100 m below the surface (appendix D), a current in the player's
    // cell, and the marker while units fight the player (appendix J, the attacker list)
    m_radiation.draw(scenario->depth() < fight::Scenario::RadiationDepth, time, rect);
    m_current.draw(glm::length(scenario->current(scenario->position())) > 0.5f, time, rect);
    m_attacked.draw(scenario->playerAttackers() > 0, time, rect);
}


} // namespace hud
