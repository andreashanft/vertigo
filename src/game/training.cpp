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

#include "training.h"
#include "boat.h"
#include "briefing.h"
#include "items.h"
#include "mission.h"

#include "fight/scenario.h"
#include "hud/hud.h"
#include "txt/desfile.h"
#include "util/datapath.h"
#include "util/path.hpp"

#include <iostream>
#include <map>

namespace game {

namespace {

const int Count = 4;


String setupFile(int index)
{
    return String("dat:save/ftrai%1.des").arg(index);
}


// The boat of a training setup as a saved game has it. The setups have it as the fight engine
// takes it: the boat by name, and its items numbered within their kind (armor 3 is the third
// armor, 5123), where saved games have the boat's type and the items' models.
txt::DesFile savedBoat(txt::DesFile &setup)
{
    static const std::map<String, int> types = { { "hiob", 2049 }, { "gator", 2050 }, { "zorn", 2051 }, { "succubus", 2052 } };
    // the first model of a kind, by key; "model" depends on the mounting: a magazine, a gun or
    // a turret's gun
    static const std::map<String, int> bases = { { "armor", 5120 }, { "sensor", 6144 }, { "engine", 4096 }, { "software", 7168 } };
    static const std::map<int, int> modelBases = { { 0, 1024 }, { 1, 3072 }, { 2, 3072 } };

    txt::DesFile saved;
    setup.setSection("Ship");
    const String name = setup.value("name").toString().toLower();
    saved.setSection("Ship");
    saved.setValue("type", types.count(name) ? types.at(name) : setup.value("type").toInt() + 3);

    for (int i = 0; ; i++) {
        setup.setSection(String("ShipMounting%1").arg(i));
        if (setup.keys().isEmpty())
            break;
        saved.setSection(String("ShipMounting%1").arg(i));
        const int mountingType = setup.value("type").toInt();
        for (const String &key : setup.keys()) {
            const int number = setup.value(key).toInt();
            int model = -1;
            if (bases.count(key))
                model = bases.at(key) + number;
            else if (key == "model" && modelBases.count(mountingType))
                model = modelBases.at(mountingType) + number;
            else if (key.startsWith("torpedo"))
                model = number;
            if (model >= 0 && Items::all().count(model) == 0) {
                std::cerr << "Training: no item " << key << " = " << number << std::endl;
                continue;
            }
            // side, type and the items that are just there (buzzers, a booster, ...) as they are
            saved.setValue(key, model >= 0 ? txt::Value(model) : setup.value(key));
        }
    }
    return saved;
}

} // namespace


StringList Training::names()
{
    StringList names;
    for (int i = 0; i < Count; i++) {
        const String mission = String("dat:mission/mtrai%1.des").arg(i);
        if (!util::exists(util::DataPath::resolve(setupFile(i))) || !util::exists(util::DataPath::resolve(mission)))
            break;
        names.append(Mission(String("mtrai%1").arg(i), -1).name());
    }
    return names;
}


Training::Training(int index, std::function<void(Renderer*)> funcSetRenderer, std::function<void()> funcEnd) :
    m_index(index),
    m_hud(new hud::HUD()),
    m_funcSetRenderer(std::move(funcSetRenderer)),
    m_funcEnd(std::move(funcEnd))
{
    txt::DesFile setup(setupFile(index));
    setup.setSection("Training");
    m_map = setup.value("Map").toString();
    m_mission.reset(new Mission(setup.value("Mission").toString(), -1));

    // without saved games, loading the last one starts the training anew
    m_hud->eventSuccess() += [this]() { end(); };
    m_hud->eventAbort() += [this]() { end(); };
    m_hud->eventRestart() += [this]() { startBriefing(); };
    m_hud->eventLoad() += [this]() { startBriefing(); };
    startBriefing();
}


Training::~Training()
{
    delete m_scenario;
    delete m_briefing;
}


void Training::startBriefing()
{
    if (m_scenario)
        m_scenario->deleteLater();
    m_scenario = nullptr;
    // the boat as it was set up, with all its torpedoes again; in place, as the scenario before
    // still has it until it is deleted
    txt::DesFile setup(setupFile(m_index));
    txt::DesFile saved = savedBoat(setup);
    if (m_boat)
        *m_boat = Boat(saved);
    else
        m_boat.reset(new Boat(saved));
    m_hud->load(m_boat.get());

    if (m_briefing)
        m_briefing->deleteLater();
    m_briefing = new Briefing(*m_mission, m_map, m_hud.get());
    m_briefing->eventInit() += [this]() { m_scenario = new fight::Scenario(m_mission->scenario()); };
    m_briefing->eventStart() += [this]() { startScenario(); };
    m_funcSetRenderer(m_briefing);
}


void Training::startScenario()
{
    m_briefing->deleteLater();
    m_briefing = nullptr;
    m_hud->start(m_scenario);
    m_funcSetRenderer(m_hud.get());
}


void Training::end()
{
    if (m_scenario)
        m_scenario->deleteLater();
    m_scenario = nullptr;
    m_funcEnd();
}

} // namespace game
