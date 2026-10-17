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

#include "energybar.h"
#include "hud.h"
#include "fight/scenario.h"
#include "fight/boats/player.h"
#include <algorithm>


namespace hud {


EnergyBar::EnergyBar(HUD *hud, Rect rect) :
    ui::Widget(hud->widget()),
    m_hud(hud),
    m_rect(rect),
    m_energy(hud->getImage("hudenerg"), false)
{
}


// the bar fills from the bottom with the boat's energy, never less than 4 pixels (appendix K)
void EnergyBar::draw()
{
    const fight::Player *player = m_hud->scenario() ? m_hud->scenario()->player() : nullptr;
    const float energy = player ? player->energy() : 1.0f;
    const int filled = std::max(4, int(energy*m_energy.height()));
    const int offset = m_energy.height() - filled;
    Rect rect = m_hud->projectCenter(m_rect);
    m_energy.draw(rect.x, rect.y + offset, RectF(0, offset, m_energy.width(), m_energy.height() - offset));
}


} // namespace hud
