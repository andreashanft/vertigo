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

#ifndef HUD_SLAVEMONITOR_H
#define HUD_SLAVEMONITOR_H


#include "util/geometry2d.h"
#include "ui/widget.h"
#include "gfx/texture.h"


namespace txt { class DesFile; }


namespace hud {


class HUD;


// A cockpit status monitor: the armor page (the boat's silhouette per side, coloured by
// the damage of that side) or the systems page (name, ammunition and state of each weapon).
class SlaveMonitor : public ui::Widget
{
public:
    enum class Page { Armor, Systems };

public:
    SlaveMonitor(HUD *hud, const txt::DesFile &file, Page page);

public:
    void togglePage() { m_page = m_page == Page::Armor ? Page::Systems : Page::Armor; }

protected:
    void draw();

private:
    void drawArmor(const Rect &rect);
    void drawSystems(const Rect &rect);

private:
    HUD *m_hud;
    Rect m_rect;
    Page m_page;
    gfx::Texture m_armor[4][4]; // damage level 1-4, side front/left/right/back
    Point m_tableTopLeft;
    int m_nameX;
    int m_ammoX;
    int m_statusX;
    int m_spacingY;
};


} // namespace hud


#endif // HUD_SLAVEMONITOR_H
