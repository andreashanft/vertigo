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

#ifndef HUD_SHIELD_H
#define HUD_SHIELD_H


#include "util/geometry2d.h"
#include "ui/widget.h"
#include "gfx/texture.h"


namespace hud {


class HUD;


// The armor symbol beside the sonar symbol (HUDSHIE1-9): a bar per side of the boat around the
// centre: green, yellow below three quarters of the side's armor, red below half, gone below a
// quarter, and white for a moment when the side is hit.
class Shield : public ui::Widget
{
public:
    Shield(HUD *hud, Rect rect);

protected:
    void draw();

private:
    HUD *m_hud;
    Rect m_rect;
    std::vector<gfx::Texture> m_shield;
    int m_last[4] = { -1, -1, -1, -1 };  // each side's armor when last drawn
    float m_hit[4] = { -1, -1, -1, -1 };  // when each side was last hit
};


} // namespace hud


#endif // HUD_SHIELD_H
