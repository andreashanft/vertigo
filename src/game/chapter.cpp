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

#include "chapter.h"
#include "items.h"
#include "fight/scenario.h"
#include "hud/hud.h"
#include "sfx/soundsystem.h"
#include "txt/desfile.h"
#include "txt/parse.hpp"
#include "util/containers.hpp"
#include "util/path.hpp"
#include "util/settings.hpp"
#include <algorithm>


namespace game {


Chapter *Chapter::m_singleton = NULL;

// of the saved games: a new one's file is named after its slot, slot_<n>.des (the games saved
// before there were slots are named after the game, which can't hold an underscore)
static std::string saveDirectory()
{
    return util::appDataLocation() + "/save";
}

// what the list of saved games shows of one; a game saved before there were slots is its game's
// autosave, named after its file, and has no slot yet
static Chapter::SavedGame readSave(const std::string &path, const String &stem)
{
    txt::DesFile file(path);

    Chapter::SavedGame game;
    game.file = path;
    game.slot = file.value("slot", 0).toInt();
    game.station = file.value("name").toString();
    game.time = util::DateTime::fromIso(file.value("time").toString());
    if (game.slot > 0) {
        game.name = file.value("savename").toString();
        game.game = file.value("game").toString();
        game.autosave = file.value("autosave").toString().toLower() == "yes";
    } else {
        game.name = stem;
        game.game = stem;
        game.autosave = true;
    }
    return game;
}

Chapter::Chapter(const String &name, std::function<void(Renderer*)> funcSetRenderer, std::function<void()> funcEndGame) :
    m_code(-1),
    m_area(NULL),
    m_desktop(NULL),
    m_briefing(NULL),
    m_movie(NULL),
    m_currentStation(-1),
    m_credits(0),
    m_mission(NULL),
    m_scenario(NULL),
    m_HUD(new hud::HUD()),
    m_boat(NULL),
    m_end(false),
    m_name(name),
    m_funcSetRenderer(std::move(funcSetRenderer)),
    m_funcEndGame(std::move(funcEndGame))
{
    assert(m_singleton == NULL);
    m_singleton = this;

    m_movieAutopilot = util::Settings::boolValue("Movie/autopilot", true);
    m_movieApproach = util::Settings::boolValue("Movie/approach", true);
    m_movieHarbour = util::Settings::boolValue("Movie/harbour", true);

    m_tasksFile.load("dat:story/tasks.des");

    m_HUD->eventSuccess() += [this]() { finishMission(); };
    m_HUD->eventAbort() += [this]() { abortMission(); };
    m_HUD->eventRestart() += [this]() { restartMission(); };
    m_HUD->eventLoad() += [this]() { loadLastSave(); };
}


Chapter::~Chapter()
{
    util::Settings::setValue("Movie/autopilot", m_movieAutopilot);
    util::Settings::setValue("Movie/approach", m_movieApproach);
    util::Settings::setValue("Movie/harbour", m_movieHarbour);

    // the desktop's dialog frames still reference pending dialogs
    delete m_desktop;
    util::deleteAll(m_pendingDialogues);
    util::deleteAll(m_missions);
    delete m_scenario;
    delete m_mission;
    delete m_boat;
    delete m_briefing;
    delete m_area;
    m_singleton = NULL;
}



void Chapter::loadChapter(int chapter)
{
    if (m_code != -1) {
        m_tasksFile.setSection(String("localtask_%1").arg(m_code));
        for (const String &key : m_tasksFile.keys().withPrefix("task"))
            util::removeAll(m_runningTasks, m_tasksFile.value(key).toInt());
    }

    load(String("dat:story/ch%1.des").arg(chapter).toUtf8(), false);
}


void Chapter::load(const std::string &filename, bool load)
{
    m_movies = std::queue<Movie, std::list<Movie>>();
    delete m_area;
    // save again once the new station is set up, after a new chapter and in a new game (which so
    // has its autosave at once), but not after loading a saved game
    const bool replaceDesktop = m_desktop != NULL;
    const bool newGame = m_code == -1;
    if (m_desktop) {
        m_desktop->deleteLater();
        m_desktop = NULL;
    }
    m_movie = NULL;
    m_currentStation = -1;
    m_approachMovieReplacement.clear();
    util::deleteAll(m_pendingDialogues);
    m_pendingDialogues.clear();
    util::deleteAll(m_missions);
    m_missions.clear();
    delete m_mission;
    m_mission = NULL;
    m_save = (replaceDesktop || newGame) && !load;

    txt::DesFile file(filename);

    if (file.contains("credit"))
        m_credits = file.value("credit").toInt();

    file.setSection("chapter");
    m_code = file.value("code").toInt();
    m_area = new Area(file.value("area").toString());
    m_stations = m_area->stations();
    const String startStation = file.value("startstation").toString();

    int startStationIndex = 0;
    for (const auto &entry : m_area->stations()) {
        const Station &station = entry.second;
        file.setSection(String("station%1").arg(station.index()));
        if (file.contains("approachmoviereplacement"))
            replaceApproachMovie(station.index(), file.value("approachmoviereplacement").toString());

        if (station.shortName() == startStation)
            startStationIndex = station.index();

        for (const String &key : file.keys().withPrefix("mission"))
            addMission(file.value(key).toString(), station.index());
    }

    file.setSection("pendingdialogues");
    for (const String &key : file.keys().withPrefix("dialogue"))
        addDialog(file.value(key).toInt());

    file.setSection("successmissions");
    if (file.value("clearmissions").toString().toLower() == "yes")
        m_successfulMissions.clear();
    for (const String &key : file.keys().withPrefix("mission"))
        m_successfulMissions.append(file.value(key).toString());

    file.setSection(String("area%1").arg(m_area->code()));
    for (const String &key : file.keys().withPrefix("mission"))
        addMission(file.value(key).toString(), -1);

    file.setSection("runningtasks");
    if (file.value("cleartasks").toString().toLower() == "yes")
        m_runningTasks.clear();
    for (const String &key : file.keys().withPrefix("task"))
        addTask(file.value(key).toInt());

    file.setSection("messageboard");
    if (file.value("clearboard").toString().toLower() == "yes")
        m_messages.clear();
    for (const String &key : file.keys().withPrefix("message"))
        addMessage(file.value(key).toInt());

    file.setSection("disabledstations");
    for (const String &key : file.keys().withPrefix("station"))
        disableStation(file.value(key).toInt());

    file.setSection("statistics");
    m_numSmallTalks = file.value("numofsmalltalkssincelaststory", 0).toInt();

    file.setSection("movies");
    for (const String &key : file.keys().withPrefix("movie"))
        m_playedMovies.insert(file.value(key).toInt());

    file.setSection("Ship");
    if (file.contains("type"))
    {
        delete m_boat;
        m_boat = new Boat(file);
        m_HUD->load(m_boat);
    }

    // the intro has always been played
    m_playedMovies.insert(2);

    setStation(startStationIndex, load);
}


void Chapter::save() const
{
    // the game's autosave, in the next free slot when it has none yet (a game saved before there
    // were slots gets its slot now)
    if (m_autosave.file.empty() || !util::exists(m_autosave.file)) {
        m_autosave = autosaveOf(m_name);
        if (m_autosave.file.empty())
            m_autosave = freeSlot();
    }
    write(m_autosave.file, m_autosave.slot, m_name, true);
}


void Chapter::saveGame(const SavedGame &slot, const String &name) const
{
    // never over an autosave
    assert(!slot.autosave);
    write(slot.file, slot.slot, name, false);
}


void Chapter::write(const std::string &path, int slot, const String &name, bool autosave) const
{
    txt::DesFile file;

    file.setValue("name", m_desktop->name());
    // local, with milliseconds (as QVariant wrote it before)
    file.setValue("time", util::DateTime::now().toIso());
    file.setValue("credit", m_credits);
    // the slot and the save's name; any save belongs to its game, whose autosave the game keeps
    // writing when it is loaded
    file.setValue("slot", slot);
    file.setValue("savename", name);
    file.setValue("game", m_name);
    file.setValue("autosave", autosave ? "yes" : "no");

    file.setSection("chapter");
    file.setValue("code", m_code);
    file.setValue("area", m_area->shortName());
    file.setValue("startstation", m_currentStationName);

    file.setSection("statistics");
    file.setValue("numofsmalltalkssincelaststory", m_numSmallTalks);
    int i = 1;
    file.setSection("pendingdialogues");
    for (const auto &dialog : m_pendingDialogues)
        file.setValue(String("dialogue%1").arg(i++), dialog.first);

    file.setSection("runningtasks");
    file.setValue("cleartasks", "yes");
    i = 1;
    for (int task : m_runningTasks)
        file.setValue(String("task%1").arg(i++), task);

    file.setSection("messageboard");
    file.setValue("clearboard", "yes");
    i = 1;
    // in ascending order (they were in the order of a hash before, which changed from run to run)
    for (int message : m_messages)
        file.setValue(String("message%1").arg(i++), message);

    file.setSection("successmissions");
    file.setValue("clearmissions", "yes");
    i = 1;
    for (const String &mission : m_successfulMissions)
        file.setValue(String("mission%1").arg(i++), mission);

    for (const auto &replacement : m_approachMovieReplacement) {
        file.setSection(String("station%1").arg(replacement.first));
        file.setValue("approachmoviereplacement", replacement.second);
    }

    file.setSection("disabledstations");
    i = 0;
    for (const auto &station : m_stations) {
        if (!station.second.isEnabled())
            file.setValue(String("station%1").arg(i++), station.second.index());
    }

    i = 1;
    for (const Mission *mission : m_missions) {
        if (mission->station() == -1) {
            file.setSection(String("area%1").arg(m_area->code()));
            file.setValue(String("mission%1").arg(i++), mission->shortName());
        } else {
            file.setSection(String("station%1").arg(mission->station()));
            file.setValue(String("mission%1").arg(i++), mission->shortName()); // TODO: Mission index
        }
    }

    file.setSection("movies");
    i = 0;
    for (int movie : m_playedMovies)
        file.setValue(String("movie%1").arg(i++), movie);

    file.setSection("Ship");
    file.setValue("type", m_boat->type());

    for (i = 0; i < m_boat->mountings().size(); i++)
    {
        const Boat::Mounting &mounting = m_boat->mountings()[i];
        file.setSection(String("ShipMounting%1").arg(i));

        std::vector<int> items = m_boat->getItems(mounting.name);
        file.setValue("side", mounting.side);
        file.setValue("type", mounting.type);
        int buzzer = 0;
        int torpedo = 0;
        for (int model : items)
        {
            Items::Item *item = Items::get(model);
            if (item->type == Items::Armor)
                file.setValue("armor", model);
            if (item->type == Items::NRSkin)
                file.setValue("nrs", 0);
            if (item->type == Items::Sensor)
                file.setValue("sensor", model);
            if (item->type == Items::Fixer)
                file.setValue("fixer", 0);
            if (item->type == Items::Buzzer)
                file.setValue(String("buzzer%1").arg(buzzer++), 0);
            if (item->type == Items::Engine)
                file.setValue("engine", model);
            if (item->type == Items::Booster)
                file.setValue("booster", 0);
            if (item->type == Items::Silator)
                file.setValue("silator", 0);
            if (item->type == Items::Gun)
                file.setValue("model", model);
            if (item->type == Items::Magazine)
                file.setValue("model", model);
            if (item->type == Items::Torpedo)
                file.setValue(String("torpedo%1").arg(torpedo++), model);
            if (item->type == Items::Software)
                file.setValue("software", model);
        }
    }

    file.setSection("Defect");
    file.setValue("Gun", m_boat->defects()[Boat::DefectGun]);
    file.setValue("ToMa", m_boat->defects()[Boat::DefectToMa]);
    file.setValue("FArm", m_boat->defects()[Boat::DefectFArm]);
    file.setValue("LArm", m_boat->defects()[Boat::DefectLArm]);
    file.setValue("RArm", m_boat->defects()[Boat::DefectRArm]);
    file.setValue("BArm", m_boat->defects()[Boat::DefectBArm]);
    file.setValue("Tur1", m_boat->defects()[Boat::DefectTur1]);
    file.setValue("Tur2", m_boat->defects()[Boat::DefectTur2]);

    file.save(path);
}


void Chapter::loadGame(const std::string &path)
{
    // the game goes on with its autosave: this one, or for a game saved by hand its game's
    for (const SavedGame &game : savedGames()) {
        if (game.file == path) {
            m_name = game.game;
            m_autosave = game.autosave ? game : autosaveOf(game.game);
        }
    }
    m_playedMovies.clear();
    load(path, true);
}


void Chapter::setStation(int stationIndex, bool load)
{
    if (m_desktop) {
        save();
        m_desktop->deleteLater();
        m_save = true;
    }

    int previousStation = m_currentStation;
    m_currentStation = stationIndex;

    const Station station = m_stations[stationIndex];
    m_currentStationName = station.shortName();
    m_desktop = new Desktop(m_currentStationName);

    const auto here = std::find_if(m_missions.begin(), m_missions.end(), [this](Mission *mission) { return mission->station() == m_currentStation; });
    if (here != m_missions.end()) {
        m_mission = *here;
        m_missions.erase(here);
    }

    if (m_movieHarbour && previousStation >= 0)
        m_movies.emplace(String("gfx:mvi/film/%1hf.mvi").arg(m_boat->moviePrefix()), Movie::MovieTransit, 
                         txt::StringTable::get(txt::Movie_Dock), 
                         String("%1 %2 ...").arg(txt::StringTable::get(txt::Movie_Leaving)).arg(m_stations[previousStation].name()));

    if (!m_mission && !load)
        playApproach(true);

    playMovies();
}


void Chapter::startMission(const String &name)
{
    if (m_desktop) {
        save();
        m_desktop->deleteLater();
        m_desktop = NULL;
    }

    const auto named = std::find_if(m_missions.begin(), m_missions.end(), [&name](Mission *mission) { return mission->shortName() == name; });
    if (named != m_missions.end()) {
        m_mission = *named;
        m_missions.erase(named);
    }

    if (m_movieHarbour)
        m_movies.emplace(String("gfx:mvi/film/%1hf.mvi").arg(m_boat->moviePrefix()), Movie::MovieTransit,
                         txt::StringTable::get(txt::Movie_Dock),
                         String("%1 %2 ...").arg(txt::StringTable::get(txt::Movie_Leaving)).arg(m_stations[m_currentStation].name()));

    m_currentStation = -1;
    m_boatAtStart.reset(new Boat(*m_boat));

    playMovies();
}


void Chapter::startMission()
{
    assert(m_mission);
    assert(m_scenario == NULL);

    if (m_briefing)
        m_briefing->deleteLater();
    m_briefing = new Briefing(*m_mission, m_area->map(), m_HUD.get());
    m_briefing->eventInit() += [this]() { m_scenario = new fight::Scenario(m_mission->scenario()); };
    m_briefing->eventStart() += [this]() { startScenario(); };
    m_funcSetRenderer(m_briefing);
}


void Chapter::startScenario()
{
    assert(m_briefing);
    m_briefing->deleteLater();
    m_briefing = NULL;

    assert(m_mission);
    assert(m_scenario);
    // the autopilot's destinations: the station the boat was on its way to, or any open one after a
    // mission of the area
    m_destinations.clear();
    StringList names;
    for (const auto &station : m_stations) {
        if (m_currentStation >= 0 ? station.first == m_currentStation : station.second.isEnabled()) {
            m_destinations.push_back(station.first);
            names << station.second.name().toUpper();
        }
    }
    m_HUD->setDestinations(names);
    m_HUD->setBriefing(m_mission->textB(), m_mission->textP(), m_mission->textS());
    m_HUD->start(m_scenario);
    m_funcSetRenderer(m_HUD.get());
}


void Chapter::finishMission()
{
    assert(m_scenario);
    m_scenario->deleteLater();
    m_scenario = NULL;

    m_successfulMissions.append(m_mission->shortName());

    assert(m_mission);
    delete m_mission;
    m_mission = NULL;
    m_save = true;

    if (m_currentStation < 0) {
        // the station chosen in the autopilot's menu
        const int destination = m_HUD->destination();
        if (destination >= 0 && destination < int(m_destinations.size()))
            setStation(m_destinations[destination]);
    } else {
        playApproach(true);
        playMovies();
    }
}


void Chapter::abortMission()
{
    assert(m_scenario);
    m_scenario->deleteLater();
    m_scenario = NULL;

    quit();
}


void Chapter::restartMission()
{
    assert(m_scenario);
    m_scenario->deleteLater();
    m_scenario = NULL;
    // its damage and torpedoes undone; in place, as the scenario before still has the boat until
    // it is deleted
    if (m_boatAtStart)
        *m_boat = *m_boatAtStart;

    startMission();
}


void Chapter::loadLastSave()
{
    assert(m_scenario);
    if (m_autosave.file.empty() || !util::exists(m_autosave.file)) {
        restartMission();
        return;
    }

    m_scenario->deleteLater();
    m_scenario = NULL;

    loadGame(m_autosave.file);
}


void Chapter::playApproach(bool autopilot)
{
    if (m_approachMovieReplacement.count(m_currentStation)) {
        const String movie = util::take(m_approachMovieReplacement, m_currentStation);
        String number;
        if (txt::parseMovieNumber(movie, number)) {
            m_playedMovies.insert(number.toInt());
            if (number.toInt() == 36)
                m_end = true;
        }
        m_movies.emplace("gfx:mvi/film/" + movie, Movie::MovieFilm);
    } else {
        if (m_movieAutopilot && autopilot)
            m_movies.emplace(String("gfx:mvi/film/%1fl.mvi").arg(m_boat->moviePrefix()), Movie::MovieTransit,
                             txt::StringTable::get(txt::Movie_Autopilot),
                             String("%1 %2 ...").arg(txt::StringTable::get(txt::Movie_Approaching)).arg(m_desktop->name()));
        if (m_movieApproach)
            m_movies.emplace(m_desktop->approachMovie(), Movie::MovieStation, m_desktop->name(), m_desktop->description());
    }
}


void Chapter::playMovie(int movie)
{
    m_movies.emplace(String("gfx:mvi/film/d%1.mvi").arg(movie, 2, 10, '0'), Movie::MovieFilm);
    playMovies();
}


void Chapter::playMovies()
{
    if (m_movies.empty()) {
        sfx::SoundSystem::get()->resumeAll();
        if (m_mission) {
            startMission();
        } else if (m_end) {
            m_funcEndGame();
        } else {
            if (m_save) {
                save();
                m_save = false;
            }
            m_funcSetRenderer(m_desktop);
        }
    } else {
        sfx::SoundSystem::get()->pauseAll();
        assert(m_movie == NULL);
        m_movie = &m_movies.front();
        m_movie->play([this]() { movieFinished(); });
        m_funcSetRenderer(m_movie);
    }
}


void Chapter::movieFinished()
{
    m_funcSetRenderer(nullptr);
    m_movie = NULL;
    m_movies.pop();
    playMovies();
}


void Chapter::quit()
{
    m_funcEndGame();
}


void Chapter::replaceApproachMovie(int station, const String &movie)
{
    m_approachMovieReplacement[station] = movie;
}


void Chapter::gameOver()
{
    m_funcEndGame();
}


Dialog* Chapter::dialog(int dialogId) const
{
    return util::value(m_pendingDialogues, dialogId);
}


std::vector<Dialog*> Chapter::dialogs(int room)
{
    std::vector<Dialog*> list;
    for (const auto &entry : m_pendingDialogues) {
        Dialog *dialog = entry.second;
        if (dialog->matches(m_area->code(), m_currentStation, room))
            list.push_back(dialog);
    }
    return list;
}


std::vector<Dialog*> Chapter::dialogsEnCom(bool room)
{
    std::vector<Dialog*> list;
    for (const auto &entry : m_pendingDialogues) {
        Dialog *dialog = entry.second;
        if (dialog->matchesEnCom(m_area->code(), m_currentStation, room))
            list.push_back(dialog);
    }
    return list;
}


std::vector<Dialog*> Chapter::dialogsDirect()
{
    std::vector<Dialog*> list;
    for (const auto &entry : m_pendingDialogues) {
        Dialog *dialog = entry.second;
        if (dialog->matchesDirect(m_area->code(), m_currentStation))
            list.push_back(dialog);
    }
    return list;
}


void Chapter::finishDialog(int dialogId)
{
    Dialog *dialog = util::take(m_pendingDialogues, dialogId);
    if (dialog && dialog->isSmallTalk())
        m_numSmallTalks++;

    save();
}


void Chapter::addMessage(int message)
{
    m_messages.insert(message);
}


void Chapter::addTask(int task)
{
    m_runningTasks.push_back(task);
}


void Chapter::removeTask(int task)
{
    util::removeAll(m_runningTasks, task);
}


void Chapter::changeChapter(int chapter)
{
    loadChapter(chapter);
}


void Chapter::addDialog(int dialogId)
{
    assert(!m_pendingDialogues.count(dialogId));

    Dialog *dialog = new Dialog(m_code, dialogId, nullptr,
        [this](int n) { finishDialog(n); },
        [this](int n) { addMessage(n); },
        [this](int n) { addTask(n); },
        [this](int n) { removeTask(n); },
        [this](int n) { changeChapter(n); },
        [this](int n) { addDialog(n); },
        [this](int n) { removeDialog(n); },
        [this](int n) { addCredit(n); },
        [this](int n) { enableStation(n); },
        [this](int n) { disableStation(n); },
        [this](String mission, int station) { addMission(mission, station); },
        [this](int station, String movie) { replaceApproachMovie(station, movie); },
        [this](int type) { upgradeBoat(type); },
        [this]() { gameOver(); }
    );
    m_pendingDialogues[dialogId] = dialog;
}


void Chapter::removeDialog(int dialogId)
{
    delete util::take(m_pendingDialogues, dialogId);
}


void Chapter::addCredit(int credit)
{
    m_credits += credit;
    if (m_credits < 0)
        m_credits = 0;
}


void Chapter::enableStation(int station)
{
    assert(m_stations.count(station));
    m_stations[station].enable();
}


void Chapter::disableStation(int station)
{
    assert(m_stations.count(station));
    m_stations[station].disable();
}


void Chapter::addMission(const String &mission, int station)
{
    m_missions.push_back(new Mission(mission, station));
}


void Chapter::toggleMovieAutopilot()
{
    m_movieAutopilot = !m_movieAutopilot;
}


void Chapter::toggleMovieApproach()
{
    m_movieApproach = !m_movieApproach;
}


void Chapter::toggleMovieHarbour()
{
    m_movieHarbour = !m_movieHarbour;
}


void Chapter::upgradeBoat(int type)
{
    m_boat->upgrade(type);
    m_HUD->load(m_boat);
}


std::vector<Task> Chapter::tasks()
{
    m_tasksFile.setSection("Task");
    std::vector<Task> t;
    for (int id : m_runningTasks)
        t.push_back(Task(m_tasksFile.value(String("Task_%1").arg(id)).toString()));
    return t;
}


std::vector<Chapter::SavedGame> Chapter::savedGames()
{
    std::vector<SavedGame> saves;
    std::vector<SavedGame> withoutSlot;

    // the files ending in .des, but not hidden ones
    for (const std::string &save : util::files(saveDirectory())) {
        const std::string suffix = ".des";
        if (save.size() > suffix.size() && save[0] != '.' && save.compare(save.size() - suffix.size(), suffix.size(), suffix) == 0) {
            SavedGame game = readSave(util::joinPath(saveDirectory(), save), String::fromUtf8(save.substr(0, save.size() - suffix.size())));
            (game.slot > 0 ? saves : withoutSlot).push_back(game);
        }
    }

    // the games saved before there were slots take the free ones, the oldest first, until they
    // are saved again with theirs
    std::sort(withoutSlot.begin(), withoutSlot.end(), [](const SavedGame &a, const SavedGame &b) { return a.time < b.time; });
    std::set<int> used;
    for (const SavedGame &game : saves)
        used.insert(game.slot);
    int slot = 1;
    for (SavedGame &game : withoutSlot) {
        while (used.count(slot))
            slot++;
        game.slot = slot++;
        saves.push_back(game);
    }

    std::sort(saves.begin(), saves.end(), [](const SavedGame &a, const SavedGame &b) { return a.slot < b.slot || (a.slot == b.slot && a.file < b.file); });
    return saves;
}


Chapter::SavedGame Chapter::freeSlot()
{
    // the first slot no saved game has
    std::set<int> used;
    for (const SavedGame &game : savedGames())
        used.insert(game.slot);
    SavedGame slot;
    slot.slot = 1;
    while (used.count(slot.slot))
        slot.slot++;
    slot.file = util::joinPath(saveDirectory(), "slot_" + std::to_string(slot.slot) + ".des");
    return slot;
}


Chapter::SavedGame Chapter::autosaveOf(const String &game)
{
    for (const SavedGame &save : savedGames()) {
        if (save.autosave && save.game.toLower() == game.toLower())
            return save;
    }
    return SavedGame();
}


} // namespace game
