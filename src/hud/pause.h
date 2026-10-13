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

#ifndef HUD_PAUSE_H
#define HUD_PAUSE_H


#include "util/geometry2d.h"
#include "ui/widget.h"
#include "gfx/texture.h"


namespace hud {


class HUD;


class Pause : public ui::Widget
{
public:
    Pause(HUD *hud, Rect rect);

protected:
    void draw();

private:
    HUD *m_hud;
    Rect m_rect;
    gfx::Texture m_pause;
};


} // namespace hud


#endif // HUD_PAUSE_H
