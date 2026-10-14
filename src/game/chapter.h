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

#ifndef GAME_CHAPTER_H
#define GAME_CHAPTER_H


#include <cassert>
#include <list>
#include <map>
#include <memory>
#include <queue>
#include <set>
#include <vector>
#include "area.h"
#include "briefing.h"
#include "desktop.h"
#include "dialog.h"
#include "mission.h"
#include "movie.h"
#include "task.h"
#include "boat.h"

#include <QDateTime>
#include "util/string.hpp"


namespace fight { class Scenario; }
namespace hud { class HUD; }


namespace game {


class Chapter : public util::DeferredDeletable
{
public:
    struct SavedGame
    {
        bool operator < (const SavedGame& rho) const
        {
            return time > rho.time;
        }

        String name;
        String station;
        QDateTime time;
    };

public:
    Chapter(const String &name, std::function<void(Renderer*)> funcSetRenderer, std::function<void()> funcEndGame);
    ~Chapter();

public:
    void quit();
    void toggleMovieAutopilot();
    void toggleMovieApproach();
    void toggleMovieHarbour();
    void addCredit(int credit);

    void save() const;
    void load(const String &name);
    void loadChapter(int chapter);
    void setStation(int station, bool load = false);
    Area* area() const { return m_area; }
    const std::map<int, Station>& stations() const { return m_stations; }
    int currentStation() const { return m_currentStation; }
    Dialog* dialog(int dialogId) const;
    std::vector<Dialog*> dialogs(int room);
    std::vector<Dialog*> dialogsEnCom(bool room);
    std::vector<Dialog*> dialogsDirect();
    const std::set<int>& messages() const { return m_messages; }
    int numSmallTalks() const { return m_numSmallTalks; }
    bool movieAutopilot() const { return m_movieAutopilot; }
    bool movieApproach() const { return m_movieApproach; }
    bool movieHarbour() const { return m_movieHarbour; }
    const std::set<int>& playedMovies() const { return m_playedMovies; }
    void playMovie(int movie);
    const std::vector<Mission*>& missions() const { return m_missions; }
    const StringList& successfulMissions() const { return m_successfulMissions; }
    void startMission(const String &name);
    const Mission* mission() const { return m_mission; }
    std::vector<Task> tasks();
    Boat* boat() const { return m_boat; }
    int credits() const { return m_credits; }
    hud::HUD* hud() const { return m_HUD.get(); }

private:
    void load(const std::string &filename, bool load);
    void playApproach(bool autopilot);
    void playMovies();
    void startMission();

private:
    void movieFinished();
    void finishDialog(int dialogId);
    void addMessage(int message);
    void addTask(int task);
    void removeTask(int task);
    void changeChapter(int chapter);
    void addDialog(int dialogId);
    void removeDialog(int dialogId);
    void enableStation(int station);
    void disableStation(int station);
    void addMission(const String &mission, int station);
    void replaceApproachMovie(int station, const String &movie);
    void upgradeBoat(int type);
    void gameOver();
    void startScenario();
    void finishMission();
    void abortMission();
    void restartMission();
    void loadLastSave();

public:
    static Chapter* get() { assert(m_singleton); return m_singleton; }
    static std::vector<SavedGame> savedGames();

private:
    int m_code;
    Area *m_area;
    Desktop *m_desktop;
    Briefing *m_briefing;
    Movie *m_movie;
    std::queue<Movie, std::list<Movie>> m_movies;
    std::map<int, String> m_approachMovieReplacement;
    std::map<int, Station> m_stations;
    int m_currentStation;
    String m_currentStationName;
    std::set<int> m_playedMovies;
    std::map<int, Dialog*> m_pendingDialogues;
    static Chapter *m_singleton;
    std::set<int> m_messages;
    std::vector<int> m_runningTasks;
    std::vector<Mission*> m_missions;
    StringList m_successfulMissions;
    int m_credits;
    int m_numSmallTalks;
    bool m_movieAutopilot;
    bool m_movieApproach;
    bool m_movieHarbour;
    Mission *m_mission;
    fight::Scenario *m_scenario;
    std::unique_ptr<hud::HUD> m_HUD;
    txt::DesFile m_tasksFile;
    bool m_save;
    Boat *m_boat;
    bool m_end;
    String m_name;
    std::function<void(Renderer*)> m_funcSetRenderer;
    std::function<void()> m_funcEndGame;
};


} // namespace game


#endif // GAME_CHAPTER_H
