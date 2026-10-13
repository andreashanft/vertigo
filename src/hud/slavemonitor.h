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
#include "lamp.h"
#include <map>


namespace txt { class DesFile; }


namespace hud {


class HUD;


// A cockpit status monitor: the armor page (the boat's silhouette per side, coloured by
// the damage of that side) or the systems page (name, ammunition and state of each weapon).
// F1-F4 show one system in detail instead: its picture, name, ammunition, state and condition.
// The lamp beside it lights its upper slot for the armor page.
class SlaveMonitor : public ui::Widget
{
public:
    enum class Page { Armor, Systems };
    enum class Detail { None, Gun, Magazine, Turret1, Turret2 };

public:
    SlaveMonitor(HUD *hud, const txt::DesFile &file, Page page);

public:
    Page page() const { return m_page; }
    // back from a detail page, or to the other page
    void togglePage();
    // shows the detail page, or goes back if it is already shown
    void showDetail(Detail detail) { m_detail = m_detail == detail ? Detail::None : detail; }

protected:
    void draw();

private:
    void drawArmor(const Rect &rect);
    void drawSystems(const Rect &rect);
    void drawDetail(const Rect &rect);
    // the item's picture (GFX/IMG/COCKPIT), loaded once
    gfx::Texture* picture(int item);

private:
    HUD *m_hud;
    Rect m_rect;
    Page m_page;
    Detail m_detail = Detail::None;
    Lamp m_lamp;
    std::map<int, gfx::Texture> m_pictures;
    gfx::Texture m_armor[4][4]; // damage level 1-4, side front/left/right/back
    Point m_tableTopLeft;
    int m_nameX;
    int m_ammoX;
    int m_statusX;
    int m_spacingY;
};


} // namespace hud


#endif // HUD_SLAVEMONITOR_H
