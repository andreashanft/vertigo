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
    gfx::Texture m_red;
    gfx::Texture m_yellow;
};


// The symbol block (docs/ROADMAP.md, appendix E): radiation (HUDSYM2) in the first slot, a water
// current (HUDSYM3) in the second, the marker under fire (HUDSYM1) in the third. Each symbol
// blinks on its own clock from the moment it is switched on: half a second on, half a second off.
class Symbols : public ui::Widget
{
public:
    Symbols(HUD *hud, Rect rect);

protected:
    void draw();

private:
    struct Symbol
    {
        Symbol(HUD *hud, const char *image, int x);
        // switches the symbol on or off and draws it in its blink phase
        void draw(bool on, float time, const Rect &block);

        gfx::Texture texture;
        int x;              // the slot's offset in the block
        float shownAt = -1; // when it was switched on, -1 while off
    };

    HUD *m_hud;
    Rect m_rect;
    Symbol m_radiation;
    Symbol m_current;
    Symbol m_attacked;
};


} // namespace hud


#endif // HUD_SYMBOLS_H
