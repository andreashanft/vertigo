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


// A cockpit status monitor. As in the original, with two monitors the first shows a weapon page
// and the second the armor page, which I switches to the systems table and back; a single
// monitor shows the weapon page and I steps through all three. F1-F4 pick the weapon page's
// system: the gun, the magazine (the default) or a turret. The lamp beside the monitor lights
// its upper slot for the armor page.
class SlaveMonitor : public ui::Widget
{
public:
    enum class Page { Weapon, Armor, Systems };
    enum class Weapon { Gun, Magazine, Turret1, Turret2 };

public:
    SlaveMonitor(HUD *hud, const txt::DesFile &file, Page page);

public:
    Page page() const { return m_page; }
    void setPage(Page page) { m_page = page; }
    void showWeapon(Weapon weapon) { m_weapon = weapon; }

protected:
    void draw();

private:
    void drawArmor(const Rect &rect);
    void drawSystems(const Rect &rect);
    void drawWeapon(const Rect &rect);
    // a picture from GFX/IMG/COCKPIT, loaded once
    gfx::Texture* picture(const QString &name);

private:
    HUD *m_hud;
    Rect m_rect;
    Page m_page;
    Weapon m_weapon = Weapon::Magazine;
    Lamp m_lamp;
    std::map<QString, gfx::Texture> m_pictures;
    gfx::Texture m_armor[4][4]; // damage level 1-4, side front/left/right/back
    Point m_tableTopLeft;
    int m_nameX;
    int m_ammoX;
    int m_statusX;
    int m_spacingY;
};


} // namespace hud


#endif // HUD_SLAVEMONITOR_H
