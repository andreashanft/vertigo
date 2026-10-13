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

#include "shield.h"
#include "hud.h"
#include "fight/scenario.h"
#include "fight/boats/player.h"


namespace hud {


Shield::Shield(HUD *hud, Rect rect) :
    ui::Widget(hud->widget()),
    m_hud(hud),
    m_rect(rect)
{
    for (int i = 1; i <= 9; i++)
        m_shield.emplace_back(m_hud->getImage(QString("hudshie%1").arg(i)), true);
}


void Shield::draw()
{
    const fight::Scenario *scenario = m_hud->scenario();
    const fight::Player *player = scenario->player();
    const float time = scenario->time();
    Rect rect = m_hud->projectCenter(m_rect);
    m_shield[0].draw(rect.x + 3, rect.y + 3);

    // images 2-9: green, white, yellow and red, each upright (left, right) and lying (front, back)
    for (int side = 0; side < 4; side++)
    {
        const int armor = player->shield(side);
        if (m_last[side] >= 0 && armor < m_last[side])
            m_hit[side] = time;
        m_last[side] = armor;
        const float left = player->shieldMax(side) > 0 ? float(armor)/player->shieldMax(side) : 0;
        const bool hit = m_hit[side] >= 0 && time - m_hit[side] < 0.25f && time >= m_hit[side];
        const int colour = hit ? 1 : left > 0.5f ? 0 : left > 0.25f ? 2 : 3;
        const bool lying = side == 0 || side == 3;
        gfx::Texture &bar = m_shield[1 + 2*colour + (lying ? 1 : 0)];
        switch (side)
        {
        case 0: bar.draw(rect.x + 3, rect.y); break;        // front
        case 1: bar.draw(rect.x, rect.y + 3); break;        // left
        case 2: bar.draw(rect.x + 14, rect.y + 3); break;   // right
        case 3: bar.draw(rect.x + 3, rect.y + 14); break;   // back
        }
    }
}


} // namespace hud
