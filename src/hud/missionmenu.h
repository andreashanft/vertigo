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

#ifndef HUD_MISSIONMENU_H
#define HUD_MISSIONMENU_H


#include "util/geometry2d.h"
#include "ui/widget.h"
#include "gfx/texture.h"
#include "util/string.hpp"


namespace gfx { class Font; }


namespace hud {


class HUD;


// The in-mission menus (Esc menu, confirmations, game over, turret orders, autopilot), which
// the master monitor shows in place of the target, as the original did; and short notices such
// as "autopilot not available", drawn in a box in the middle of the view (the menus too, in a
// cockpit without a master monitor).
class MissionMenu : public ui::Widget
{
public:
    enum class Page { None, Main, ConfirmAbort, ConfirmRestart, ConfirmLoad, GameOver, Died, Turrets, Turret1, Turret2, Autopilot, ConfirmAutopilot };
    enum class Action { None, Continue, Abort, Restart, Load, TurretOrder, Autopilot };

public:
    MissionMenu(HUD *hud, const Point &center);

public:
    Page page() const { return m_page; }
    // the turret and autopilot menus don't stop the mission
    bool pauses() const { return m_page != Page::None && m_page != Page::Turrets && m_page != Page::Turret1 && m_page != Page::Turret2 &&
                                 m_page != Page::Autopilot && m_page != Page::ConfirmAutopilot; }
    // after Action::TurretOrder: which turret (0, 1, or -1 for both) and which order (0 attack, 1 sync, 2 search, 3 stop)
    int turret() const { return m_turret; }
    int turretOrder() const { return m_turretOrder; }
    void setTurrets(int count) { m_turretCount = count; }
    // the master monitor's rect (in the 640x480 art)
    void setScreen(const Rect &rect) { m_screen = rect; }
    // whether the master monitor shows a menu now, and drawing it there
    bool onScreen() const { return m_page != Page::None && m_screen.width > 0 && !m_overView; }
    void drawOnScreen();
    // the autopilot's destinations (up to 9), and after Action::Autopilot the one chosen (a
    // single one is chosen from the start: the autopilot goes there without a menu)
    void setDestinations(const StringList &names) { m_destinations = names; m_destination = names.size() == 1 ? 0 : -1; }
    const StringList& destinations() const { return m_destinations; }
    int destination() const { return m_destination; }
    void open(Page page);
    Action keyPressEvent(int key);
    // a game controller: the d-pad highlights an option (the first press shows the highlight),
    // choose() takes it as its digit, back() is Esc
    void select(int step);
    Action choose();
    Action back();
    // a tap: the option at this point (in the coordinates the menu is drawn in), or -1, and taking it
    int optionAt(const Point &point) const;
    Action choose(int option);
    // the death view: the menu and the notices over the world, without the box (docs/ROADMAP.md,
    // appendix R)
    void setOverView(bool overView) { m_overView = overView; }
    // shows the lines for a few seconds while no menu is open
    void notice(const StringList &lines, float seconds);

protected:
    void draw();

private:
    struct Line
    {
        String text;
        gfx::Font *font;
    };
    std::vector<Line> lines() const;
    // the box, or the master monitor's rect, and each line's place in it
    Rect layout(const std::vector<Line> &lines, std::vector<Rect> &places) const;
    void drawLines(const std::vector<Line> &lines, const std::vector<Rect> &places);
    int key(int option) const;
    Action press(int key);
    Action pressOnPage(int key);

private:
    HUD *m_hud;
    Point m_center;
    Rect m_screen = Rect(0, 0, 0, 0);
    Page m_page;
    int m_turret = 0;
    int m_turretOrder = 0;
    int m_turretCount = 0;
    StringList m_destinations;
    int m_destination = -1;
    // the option highlighted for a game controller, while it is used
    bool m_highlight = false;
    int m_selected = 0;
    bool m_overView = false;
    StringList m_notice;
    unsigned int m_noticeEnd;

    gfx::Texture m_black;
    gfx::Texture m_edgeBL;
    gfx::Texture m_edgeBR;
    gfx::Texture m_edgeTL;
    gfx::Texture m_edgeTR;
};


} // namespace hud


#endif // HUD_MISSIONMENU_H
