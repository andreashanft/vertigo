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

#include "openmission.h"
#include "boat.h"
#include "depot.h"
#include "items.h"
#include "mission.h"

#include "fight/scenario.h"
#include "gfx/font.h"
#include "gfx/image.h"
#include "hud/hud.h"
#include "txt/desfile.h"
#include "txt/stringtable.h"
#include "ui/button.h"
#include "ui/frame.h"
#include "util/datapath.h"
#include "util/path.hpp"

#include <algorithm>

namespace game {

namespace {

// enough to buy the best of everything for any boat, several times over
const int Budget = 10000000;

// Spy II: friend or foe
const int DefaultSonar = 6146;

// the boats' files, from type 2049 on
const char *const BoatFiles[] = { "hiob", "gator", "zorn", "succubus" };

// the briefing of the mission that plays a scenario (s00101 of m00101, strai3 of mtrai3)
String missionOf(const String &scenario)
{
    return "m" + scenario.mid(1);
}

bool hasMission(const String &scenario)
{
    return util::exists(util::DataPath::resolve(String("dat:mission/%1.des").arg(missionOf(scenario))));
}

// the scenarios of the data (s00101, ..., strai3), each with its mission's name if it has one
std::vector<std::pair<String, String>> scenarios()
{
    std::vector<std::pair<String, String>> list;
    for (const std::string &file : util::files(util::DataPath::resolve("vfx:scenario"))) {
        const String name = String::fromUtf8(file);
        if (!name.toLower().endsWith(".des"))
            continue;
        const String scenario = name.left(name.lastIndexOf('.')).toLower();
        list.emplace_back(scenario, hasMission(scenario) ? Mission(missionOf(scenario), -1).name() : String());
    }
    std::sort(list.begin(), list.end());
    return list;
}

} // namespace


std::vector<std::pair<int, String>> OpenMission::boats()
{
    std::vector<std::pair<int, String>> list;
    for (int i = 0; i < 4; i++) {
        txt::DesFile file(String("vfx:sobjects/%1.des").arg(BoatFiles[i]));
        file.setSection("Ship");
        list.emplace_back(2049 + i, file.value("Name").toString());
    }
    return list;
}


OpenMission::OpenMission(int boatType, std::function<void(Renderer*)> funcSetRenderer, std::function<void()> funcEnd) :
    m_credits(Budget),
    m_hud(new hud::HUD()),
    m_funcSetRenderer(std::move(funcSetRenderer)),
    m_funcEnd(std::move(funcEnd))
{
    m_background.setTexture(gfx::Image::loadPCX("gfx:pic/title/title.pcx"));
    setRootWidget(&m_background);

    // the boat test mode starts with (chapter 1's), as the chosen type
    txt::DesFile story("dat:story/ch1.des");
    m_loadout.reset(new Boat(story));
    m_loadout->upgrade(boatType);
    // upgrade the sonar by default: chapter 1's Spy I has no IFF, so every contact would show as a friend
    m_loadout->buy(DefaultSonar, "DEFE");

    // without saved games, loading the last one starts the mission anew
    m_hud->eventSuccess() += [this]() { stopScenario(); };
    m_hud->eventAbort() += [this]() { stopScenario(); };
    m_hud->eventRestart() += [this]() { startScenario(); };
    m_hud->eventLoad() += [this]() { startScenario(); };

    Items::setOpenOffer(true);
    showDepot();
}


OpenMission::~OpenMission()
{
    Items::setOpenOffer(false);
    delete m_scenario;
}


void OpenMission::showDepot()
{
    // closing the depot goes on to the missions
    Depot::Account account{ [this]() { return m_credits; }, [this](int credit) { m_credits = std::max(0, m_credits + credit); } };
    m_depot = new Depot(m_loadout.get(), std::move(account), [this]() { hideDepot(); showMissions(); });
    m_depot->setParentWidget(&m_background);
    m_depot->setPosition((640 - m_depot->width())/2, (480 - m_depot->height())/2);
    // the depot clicks by itself
    setClick(false);
}


void OpenMission::hideDepot()
{
    m_depot->setParentWidget(nullptr);
    m_depot->deleteLater();
    m_depot = nullptr;
    setClick(true);
}


// All the scenarios in columns, as "S00101  Trümmer beseitigen"; below them the way back to the
// depot and to the main menu, which closing the window also goes to
void OpenMission::showMissions()
{
    const gfx::Font font(gfx::Font::Small);
    const gfx::Font fontButtons(gfx::Font::Medium);
    const int lineHeight = font.height() + 2;
    const auto list = scenarios();
    const int count = int(list.size());

    StringList texts;
    int columnWidth = 0;
    for (const auto &entry : list) {
        texts << (entry.second.isEmpty() ? entry.first.toUpper() : String("%1  %2").arg(entry.first.toUpper(), entry.second));
        columnWidth = std::max(columnWidth, font.width(texts.back()));
    }
    columnWidth += 16;
    // two columns, or more if they get too long for the screen and there is room for them
    int columns = 2;
    while ((count + columns - 1)/columns*lineHeight > 380 && (columns + 1)*columnWidth + 24 <= 640)
        columns++;
    const int rows = std::max(1, (count + columns - 1)/columns);

    const int top = 22;
    const int bottom = top + rows*lineHeight + 8;
    const Size size(std::max(columns*columnWidth + 24, 320), bottom + fontButtons.height() + 20);
    m_missions = new ui::Frame([this]() { end(); }, &m_background);
    m_missions->setupFrame(size, txt::StringTable::get(txt::OpenMission_Missions), true);
    m_missions->setPosition((640 - size.width)/2, (480 - size.height)/2);

    for (int i = 0; i < count; i++) {
        const String scenario = list[i].first;
        ui::Button *button = new ui::Button([this, scenario]() { m_scenarioName = scenario; hideMissions(); startScenario(); }, m_missions);
        button->setFont(font);
        button->setText(texts[i]);
        button->setPosition(12 + i/rows*columnWidth, top + i%rows*lineHeight);
        button->setWidth(columnWidth);
    }

    ui::Button *button = new ui::Button([this]() { hideMissions(); showDepot(); }, m_missions);
    button->setFont(fontButtons);
    button->setText("DEPOT");
    button->setPosition(0, bottom);
    button->setWidth(size.width/2);
    button->setAlignment(ui::Label::AlignHCenter);

    button = new ui::Button([this]() { end(); }, m_missions);
    button->setFont(fontButtons);
    button->setText(txt::StringTable::get(txt::MainMenu));
    button->setPosition(size.width/2, bottom);
    button->setWidth(size.width/2);
    button->setAlignment(ui::Label::AlignHCenter);
}


void OpenMission::hideMissions()
{
    m_missions->setParentWidget(nullptr);
    m_missions->deleteLater();
    m_missions = nullptr;
}


// The mission as test mode starts it: the scenario with the boat as fitted out (all its torpedoes
// again) and the mission's briefing in the HUD
void OpenMission::startScenario()
{
    if (m_scenario)
        m_scenario->deleteLater();
    // in place, as the scenario before still has the boat until it is deleted
    if (m_boat)
        *m_boat = *m_loadout;
    else
        m_boat.reset(new Boat(*m_loadout));
    m_hud->load(m_boat.get());

    m_scenario = new fight::Scenario(m_scenarioName);
    if (hasMission(m_scenarioName)) {
        const Mission briefing(missionOf(m_scenarioName), -1);
        m_hud->setBriefing(briefing.textB(), briefing.textP(), briefing.textS());
    }
    m_hud->start(m_scenario);
    m_funcSetRenderer(m_hud.get());
}


void OpenMission::stopScenario()
{
    if (m_scenario)
        m_scenario->deleteLater();
    m_scenario = nullptr;
    showMissions();
    m_funcSetRenderer(this);
}


void OpenMission::end()
{
    if (m_scenario)
        m_scenario->deleteLater();
    m_scenario = nullptr;
    m_funcEnd();
}


// the right button and Esc close the depot (on to the missions) or the missions (back to the menu)
void OpenMission::mousePressEvent(input::MouseEvent *event)
{
    const bool missions = m_missions != nullptr;
    Menu::mousePressEvent(event);
    if (missions && m_missions && event->button() == input::RightButton)
        end();
}


void OpenMission::keyPressEvent(input::KeyEvent *event)
{
    if (event->key() != input::Key_Escape)
        return;
    if (m_depot) {
        hideDepot();
        showMissions();
    } else if (m_missions) {
        end();
    }
}

} // namespace game
