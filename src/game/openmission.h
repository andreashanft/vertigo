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

#ifndef GAME_OPENMISSION_H
#define GAME_OPENMISSION_H

#include "menu.h"
#include "ui/label.h"
#include "util/string.hpp"

#include <functional>
#include <memory>
#include <utility>
#include <vector>

namespace fight { class Scenario; }
namespace hud { class HUD; }
namespace ui { class Frame; }

namespace game {

class Boat;
class Depot;

// The main menu's open mission: any mission of the game with one of the four boats, fitted out
// first in a depot that sells every item, with a budget that buys them all. The mission plays as
// test mode plays it (vertigo s00101): without the story, the briefing screen or movies. When it
// is over the list of missions comes back, the boat as it was fitted out.
class OpenMission : public Menu
{
public:
    // the boat types (2049 Hiob to 2052 Succubus) with their names, in this order
    static std::vector<std::pair<int, String>> boats();

public:
    OpenMission(int boatType, std::function<void(Renderer*)> funcSetRenderer, std::function<void()> funcEnd);
    ~OpenMission();

private:
    void showDepot();
    void hideDepot();
    void showMissions();
    void hideMissions();
    void startScenario();
    void stopScenario();
    void end();
    void mousePressEvent(input::MouseEvent *event);
    void keyPressEvent(input::KeyEvent *event);

private:
    ui::Label m_background;
    Depot *m_depot = nullptr;
    ui::Frame *m_missions = nullptr;
    std::unique_ptr<Boat> m_loadout;  // as fitted out in the depot
    std::unique_ptr<Boat> m_boat;     // the mission's copy, which it uses up
    int m_credits;
    String m_scenarioName;
    std::unique_ptr<hud::HUD> m_hud;
    fight::Scenario *m_scenario = nullptr;
    std::function<void(Renderer*)> m_funcSetRenderer;
    std::function<void()> m_funcEnd;
};

} // namespace game

#endif // GAME_OPENMISSION_H
