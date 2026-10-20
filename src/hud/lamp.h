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

#ifndef HUD_LAMP_H
#define HUD_LAMP_H


#include "util/geometry2d.h"
#include "gfx/texture.h"


namespace txt { class DesFile; }


namespace hud {


class HUD;


// The indicator lamp beside a cockpit monitor (Lamp* keys of its section): its file (*.D16)
// holds a picture of RGB565 pixels twice the lamp's width, the lamp dark on the left and lit on
// the right, so it shows which of two modes the monitor is in.
class Lamp
{
public:
    Lamp(HUD *hud, const txt::DesFile &file);

public:
    bool isValid() const { return m_valid; }
    void draw(bool lit);

private:
    HUD *m_hud;
    Rect m_rect;
    gfx::Texture m_frames[2];
    bool m_valid = false;
};


} // namespace hud


#endif // HUD_LAMP_H
