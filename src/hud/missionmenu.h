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
#include <QStringList>


namespace gfx { class Font; }


namespace hud {


class HUD;


// The in-mission menus (Esc menu, confirmations, game over) and short notices
// such as "autopilot not available", drawn in a box in the middle of the view.
class MissionMenu : public ui::Widget
{
public:
    enum class Page { None, Main, ConfirmAbort, ConfirmRestart, ConfirmLoad, GameOver, Died, Turrets, Turret1, Turret2 };
    enum class Action { None, Continue, Abort, Restart, Load, TurretOrder };

public:
    MissionMenu(HUD *hud, const Point &center);

public:
    Page page() const { return m_page; }
    // the turret menu doesn't stop the mission
    bool pauses() const { return m_page != Page::None && m_page != Page::Turrets && m_page != Page::Turret1 && m_page != Page::Turret2; }
    // after Action::TurretOrder: which turret (0, 1, or -1 for both) and which order (0 attack, 1 sync, 2 search, 3 stop)
    int turret() const { return m_turret; }
    int turretOrder() const { return m_turretOrder; }
    void setTurrets(int count) { m_turretCount = count; }
    void open(Page page);
    Action keyPressEvent(int key);
    // shows the lines for a few seconds while no menu is open
    void notice(const QStringList &lines, float seconds);

protected:
    void draw();

private:
    struct Line
    {
        QString text;
        gfx::Font *font;
    };
    std::vector<Line> lines() const;

private:
    HUD *m_hud;
    Point m_center;
    Page m_page;
    int m_turret = 0;
    int m_turretOrder = 0;
    int m_turretCount = 0;
    QStringList m_notice;
    unsigned int m_noticeEnd;

    gfx::Texture m_black;
    gfx::Texture m_edgeBL;
    gfx::Texture m_edgeBR;
    gfx::Texture m_edgeTL;
    gfx::Texture m_edgeTR;
};


} // namespace hud


#endif // HUD_MISSIONMENU_H
