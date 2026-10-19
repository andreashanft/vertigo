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

#ifndef GAME_VERTIGO_H
#define GAME_VERTIGO_H

#include "chapter.h"
#include "mainmenu.h"
#include "movie.h"
#include "openmission.h"
#include "training.h"
#include "window.h"

#include "gfx/fontmanager.h"
#include "sfx/soundsystem.h"
#include "util/string.hpp"
#include "util/timer.hpp"

#include <memory>

union SDL_Event;
struct SDL_Joystick;
namespace fight { class Scenario; }
namespace hud { class HUD; }

namespace game {

class Vertigo
{
public:
    struct Options
    {
        String scenario;                               // start this mission directly (test mode)
        int boat = 0;                                   // test mode boat type, 0 = chapter 1 boat
        std::vector<std::pair<String, int>> equipment; // test mode items: mounting, model
        float timeout = 0;                              // test mode time limit in seconds
        bool debugOverlay = false;
        bool headless = false;                          // test mode: no window and no sound, simulated time
        bool fixedStep = false;                         // test mode: 20 ms of mission time per frame
        std::vector<int> destroy;                       // test mode: destroy these units after one second
        std::vector<int> collapse;                      // test mode: collapse these units' energy defence after one second
        struct Key { float time; input::Key key; int modifiers; float hold; };
        std::vector<Key> keys;                          // test mode: press these keys at these times (seconds), holding them
        struct Drive { int id; float speed; float turn; };
        std::vector<Drive> drive;                       // test mode: let these boats move straight or in a circle
        std::vector<int> trace;                         // test mode: print these units' state every second
        std::vector<int> fire;                          // test mode: these units fire (towers and tanks aim at the player)
        int near = 0;                                   // test mode: enable this unit and start next to it
        // the player's start moved to this cell of the map, hei height units above the ground
        // (-1: the scenario's), heading card eighths of a turn from north (-1: the scenario's)
        struct Start { int x = -1; int y = -1; int hei = -1; int card = -1; };
        Start start;                                    // test mode: start elsewhere
        struct Screenshot { float time; std::string file; };        // the file is a path in UTF-8
        std::vector<Screenshot> screenshots;            // test mode: save the window at these times
        // buttons pressed together, or a stick's axis (0 left x, 1 left y, 2 right x, 3 right y)
        // moved; held for this long (< 0: buttons 0.1 s, an axis for good)
        struct Pad { float time; std::vector<input::Button> buttons; int axis; float value; float hold; bool down = false; };
        std::vector<Pad> pad;                           // work a virtual game controller at these times
        // a finger from a point to another (fractions of the window), for this long
        struct Touch { float time; PointD from; PointD to; float hold; bool down = false; float start = 0; long long id = 0; };
        std::vector<Touch> touch;                       // touch the screen at these times
        struct Click { float time; PointD pos; PointD to; };
        std::vector<Click> clicks;                      // click at these times, at points of the 640x480 screen, released at to
        bool check = false;                             // report what the mission loader ignores
        StringList checkScenarios;                     // empty = all missions
    };

    enum ExitCode
    {
        ExitSuccess = 0,
        ExitFailed = 1,
        ExitAborted = 2,
        ExitTimeout = 3,
        ExitDied = 4,
        ExitUsage = 64,
    };

public:
    explicit Vertigo(const Options &options);
    ~Vertigo();

public:
    // Run until 0 or more comes back, the exit code: start() once, then iterate() for each frame
    // and event() for what the platform reports in between
    int start();
    int iterate();
    void event(const SDL_Event &event);

private:
    int check(const StringList &names);
    // test mode
    void startScenario();
    void finish(int code);
    // seconds: of the mission in test mode (simulated without a window, by frames with a fixed
    // step), else since the start
    float elapsed() const;
    void testFrame();

    void startGame(const String &name);
    void loadGame(const String &name);
    void endGame();
    void startTraining(int index);
    void endTraining();
    void startOpenMission(int boatType);
    void endOpenMission();
    void introFinished();
    void createMainMenu(bool skipToTitle);
    void createChapter();

private:
    Options m_options;
    Window m_window;
    sfx::SoundSystem m_soundSystem;
    gfx::FontManager m_fontManager;
    MainMenu m_mainMenu;
    std::unique_ptr<Movie> m_intro;
    std::unique_ptr<Chapter> m_chapter;
    std::unique_ptr<Training> m_training;
    std::unique_ptr<OpenMission> m_openMission;
    String m_name;

    // test mode, and the main menu's --keys, --click and --screenshot
    std::unique_ptr<game::Boat> m_boat;
    std::unique_ptr<hud::HUD> m_hud;
    std::unique_ptr<fight::Scenario> m_scenario;
    int m_result = ExitSuccess;
    bool m_running = true;
    bool m_headless = false;
    std::vector<int> m_destroy;
    std::vector<int> m_collapse;
    std::vector<Options::Screenshot> m_screenshots;
    std::vector<Options::Key> m_keys;
    std::vector<Options::Click> m_clicks;
    std::vector<Options::Pad> m_pad;
    std::vector<Options::Touch> m_touch;
    SDL_Joystick *m_virtualPad = nullptr;
    util::ElapsedTimer m_timer;
    float m_simulatedTime = 0;
    float m_fixedStep = 0;
    float m_nextTrace = 0;
};

} // namespace game

#endif // GAME_VERTIGO_H
