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
#include "fight/scenario.h"
#include <map>
#include "util/string.hpp"


namespace txt { class DesFile; }


namespace hud {


class HUD;


// A cockpit status monitor. As in the original, with two monitors the first shows a weapon page
// and the second the armor page; I switches between the armor page and the system state page
// (on a single monitor too, which starts on the weapon page). F1-F4 pick the weapon page's
// system: the gun, the magazine (the default) or a turret; the same key again shows the overview
// of all weapons, and once more the system again. The lamp beside the monitor is lit for the
// armor page.
class SlaveMonitor : public ui::Widget
{
public:
    enum class Page { Weapon, Armor, Systems };
    enum class Weapon { Gun, Magazine, Turret1, Turret2 };

public:
    SlaveMonitor(HUD *hud, const txt::DesFile &file, Page page, fight::Scenario::Monitor monitor);

public:
    Page page() const { return m_page; }
    void setPage(Page page) { m_page = page; }
    void showWeapon(Weapon weapon) { m_weapon = weapon; }
    // F1-F4: the weapon's page, or the overview if that page is showing, or back
    void pressWeaponKey(Weapon weapon);

protected:
    void draw();

private:
    void drawArmor(const Rect &rect);
    void drawSystems(const Rect &rect);
    void drawBar(int x, int y, float condition, int band);
    void drawWeapon(const Rect &rect);
    void drawOverview(const Rect &rect);
    // a picture from GFX/IMG/COCKPIT, loaded once
    gfx::Texture* picture(const String &name);

private:
    HUD *m_hud;
    Rect m_rect;
    fight::Scenario::Monitor m_monitor;  // which one, for the radiation's flicker
    Page m_page;
    Weapon m_weapon = Weapon::Magazine;
    bool m_overview = false;
    Lamp m_lamp;
    std::map<String, gfx::Texture> m_pictures;
    gfx::Texture m_armor[4][4]; // damage level 1-4, side front/left/right/back
    Point m_tableTopLeft;
    int m_spacingY;
    int m_nameX, m_ammoX, m_statusX;  // the overview's columns
    gfx::Texture m_bar[3][2];   // the system state page's bar colours: band, bright/dim
};


} // namespace hud


#endif // HUD_SLAVEMONITOR_H
