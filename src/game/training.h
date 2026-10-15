/***************************************************************************
 *  Copyright (C) 2010  Philipp Nordhus                                    *
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

#ifndef GAME_TRAINING_H
#define GAME_TRAINING_H

#include "util/deferreddeletable.h"
#include "util/string.hpp"

#include <functional>
#include <memory>

namespace fight { class Scenario; }
namespace hud { class HUD; }

namespace game {

class Boat;
class Briefing;
class Mission;
class Renderer;

// A training mission of the main menu, one of the four the game data has: dat/save/ftrai0-3.des
// set them up as the original handed them to its fight engine (the scenario, the area map, the
// briefing and the boat), dat/mission/mtrai0-3.des has their briefings. Each starts with the
// briefing and goes back to the menu when it is over; restarting starts it anew.
class Training : public util::DeferredDeletable
{
public:
    // the names of the training missions there are ("Moghs", ...), in their order
    static StringList names();

public:
    Training(int index, std::function<void(Renderer*)> funcSetRenderer, std::function<void()> funcEnd);
    ~Training();

private:
    void startBriefing();
    void startScenario();
    void end();

private:
    int m_index;
    String m_map;
    std::unique_ptr<Mission> m_mission;
    std::unique_ptr<Boat> m_boat;
    std::unique_ptr<hud::HUD> m_hud;
    Briefing *m_briefing = nullptr;
    fight::Scenario *m_scenario = nullptr;
    std::function<void(Renderer*)> m_funcSetRenderer;
    std::function<void()> m_funcEnd;
};

} // namespace game

#endif // GAME_TRAINING_H
