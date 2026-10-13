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

#ifndef HUD_SYMBOLS_H
#define HUD_SYMBOLS_H


#include "util/geometry2d.h"
#include "ui/widget.h"
#include "gfx/texture.h"


namespace hud {


class HUD;


// THREAT blinks while homing torpedoes are on their way to the player
class Threat : public ui::Widget
{
public:
    Threat(HUD *hud, Rect rect);

protected:
    void draw();

private:
    HUD *m_hud;
    Rect m_rect;
    gfx::Texture m_threat;
};


// The symbol block: torpedo lock complete (HUDSYM1), radiation (HUDSYM2, not yet), current (HUDSYM3)
class Symbols : public ui::Widget
{
public:
    Symbols(HUD *hud, Rect rect);

protected:
    void draw();

private:
    HUD *m_hud;
    Rect m_rect;
    gfx::Texture m_lock;
    gfx::Texture m_radiation;
    gfx::Texture m_current;
};


} // namespace hud


#endif // HUD_SYMBOLS_H
