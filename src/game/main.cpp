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

#include "gamedata.h"
#include "vertigo.h"

#include "util/commandline.hpp"
#include "util/datapath.h"
#include "util/path.hpp"
#include "util/random.h"
#include "util/settings.hpp"

#include "items.h"
#include "fight/scenario.h"
#include "hud/hud.h"

// SDL calls SDL_AppInit(), then SDL_AppIterate() for each frame and SDL_AppEvent() for each
// event, as iOS wants it
#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>


namespace {

String text(const std::string &utf8)
{
    return String::fromUtf8(utf8);
}


bool isNumber(const String &text, bool sign = false)
{
    if (text.isEmpty())
        return false;
    for (int i = 0; i < text.size(); i++)
        if (!((text[i] >= '0' && text[i] <= '9') || text[i] == '.' || (sign && i == 0 && text[i] == '-' && text.size() > 1)))
            return false;
    return true;
}


// "time=rest", the time in seconds
bool timed(const std::string &value, float &time, std::string &rest)
{
    const std::size_t equals = value.find('=');
    if (equals == std::string::npos || !isNumber(text(value.substr(0, equals))) || equals + 1 == value.size())
        return false;
    time = text(value.substr(0, equals)).toFloat();
    rest = value.substr(equals + 1);
    return true;
}


// the comma-separated items of the values, without empty ones
std::vector<std::string> items(const std::vector<std::string> &values)
{
    std::vector<std::string> result;
    for (const std::string &value : values)
        for (const String &item : text(value).split(',', String::SkipEmptyParts))
            result.push_back(item.toUtf8());
    return result;
}


// a list of ids; false after telling which isn't one
bool ids(const util::CommandLine &parser, const char *name, const char *expected, std::vector<int> &out)
{
    for (const std::string &id : items(parser.values(name))) {
        bool ok = false;
        out.push_back(text(id).toInt(&ok));
        if (!ok) {
            std::cerr << "Invalid --" << name << " value " << id << ", expected " << expected << std::endl;
            return false;
        }
    }
    return true;
}

std::unique_ptr<game::Vertigo> g_vertigo;
int g_exitCode = game::Vertigo::ExitSuccess;


// a folder for game data built in the directory: "Game Data", or "Game Data 2" and so on if that
// is taken
std::string importTarget(const std::string &directory)
{
    std::string target = util::joinPath(directory, "Game Data");
    for (int i = 2; util::exists(target); i++)
        target = util::joinPath(directory, "Game Data " + std::to_string(i));
    return target;
}


#if !defined(SDL_PLATFORM_IOS)
// SDL's folder dialog, waited for; empty if the player cancels it
std::string chooseDirectory(const std::string &start)
{
    struct Choice { bool done = false; std::string directory; } choice;
    const SDL_PropertiesID properties = SDL_CreateProperties();
    SDL_SetStringProperty(properties, SDL_PROP_FILE_DIALOG_TITLE_STRING, "Choose the folder of the game data");
    if (!start.empty())
        SDL_SetStringProperty(properties, SDL_PROP_FILE_DIALOG_LOCATION_STRING, start.c_str());
    SDL_ShowFileDialogWithProperties(SDL_FILEDIALOG_OPENFOLDER, [](void *userdata, const char * const *files, int) {
        Choice *choice = static_cast<Choice *>(userdata);
        if (files && files[0])
            choice->directory = files[0];
        choice->done = true;
    }, &choice, properties);
    SDL_DestroyProperties(properties);
    while (!choice.done) {
        SDL_Event event;
        SDL_WaitEventTimeout(&event, 100);
    }
    return choice.directory;
}


// tells what the directory lacks; false if the player would rather quit than choose another
bool chooseAgain(const std::string &directory, const std::string &missing)
{
    const std::string message = directory + " doesn't have the complete game data, " + missing + " is missing.\n\n"
        "Vertigo needs DAT, GFX, SFX, TXT and VFX from the CD's SF directory, with the installed files "
        "on top. A GOG installation only has part of them, the rest is in its CD image (game.gog).";
    const SDL_MessageBoxButtonData buttons[] = {
        { SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "Quit" },
        { SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "Choose Another Folder" },
    };
    const SDL_MessageBoxData data = { SDL_MESSAGEBOX_WARNING, nullptr, "Game data incomplete", message.c_str(), 2, buttons, nullptr };
    int button = 0;
    return SDL_ShowMessageBox(&data, &button) && button == 1;
}


// offers to build the game data from the directory's CD image: 0 to quit, 1 to choose another
// folder, 2 to import
int offerImport(const std::string &directory, const std::string &image, const std::string &target)
{
    const std::string message = directory + " has a CD image of the game (" + util::fileName(image) + "), but not the "
        "complete game data.\n\nVertigo can copy the game data from the CD image, with the installed files next to it "
        "on top, into " + target + ".";
    const SDL_MessageBoxButtonData buttons[] = {
        { SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "Quit" },
        { 0, 1, "Choose Another Folder" },
        { SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 2, "Import" },
    };
    const SDL_MessageBoxData data = { SDL_MESSAGEBOX_INFORMATION, nullptr, "Import the game data?", message.c_str(), 3, buttons, nullptr };
    int button = 0;
    return SDL_ShowMessageBox(&data, &button) ? button : 0;
}
#else
// a message box with one button, waited for
void tell(SDL_MessageBoxFlags flags, const char *title, const std::string &message, const char *button)
{
    const SDL_MessageBoxButtonData buttons[] = { { SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 0, button } };
    const SDL_MessageBoxData data = { flags, nullptr, title, message.c_str(), 1, buttons, nullptr };
    int choice = 0;
    SDL_ShowMessageBox(&data, &choice);
}


// The game data in the app's Documents folder, which the Files app shows as On My iPad › Vertigo:
// right there or in a folder in it, or else built from a GOG installation or CD image in one of
// them (into "Game Data"). Asked for until it is there, as iPad apps don't quit.
void findGameData()
{
    const char *folder = SDL_GetUserFolder(SDL_FOLDER_DOCUMENTS);
    const std::string documents = util::DataPath::cleanPath(folder ? folder : ".");
    for (;;) {
        std::vector<std::string> directories = { documents };
        for (const std::string &name : util::directories(documents))
            if (name[0] != '.')
                directories.push_back(util::joinPath(documents, name));
        const auto where = [&](const std::string &directory) {
            return directory == documents ? std::string("The Vertigo folder") : "\u201c" + util::fileName(directory) + "\u201d";
        };

        std::string incomplete;
        for (const std::string &directory : directories) {
            util::DataPath::set(directory);
            const std::string missing = game::GameData::missing();
            if (missing.empty())
                return;
            if (incomplete.empty() && util::exists(util::DataPath::resolve("gfx:")))
                incomplete = where(directory) + " lacks " + missing + ".\n\n";
        }

        std::string image;
        for (const std::string &directory : directories) {
            image = game::GameData::findImage(directory);
            if (image.empty())
                continue;
            const std::string target = importTarget(documents);
            tell(SDL_MESSAGEBOX_INFORMATION, "Import the game data", where(directory) + " has a CD image of the game (" +
                 util::fileName(image) + "). Vertigo copies the game data from it, with the installed files next to it on "
                 "top, into the folder \u201c" + util::fileName(target) + "\u201d, which takes a moment. Afterwards Vertigo "
                 "no longer needs the CD image and the installation.", "Import");
            std::string error;
            if (!game::GameData::import(image, target, error))
                tell(SDL_MESSAGEBOX_ERROR, "Import failed", error, "OK");
            break;
        }
        if (image.empty()) {
            // the Files app only shows the folder with something in it
            if (directories.size() == 1 && util::files(documents).empty()) {
                util::writeFile(util::joinPath(documents, "Copy the game data here.txt"),
                    "Vertigo needs the game data of Archimedean Dynasty or Schleichfahrt. Copy into this folder either\n\n"
                    "- a GOG installation of the game, with its CD image game.gog: Vertigo builds the game data from it, "
                    "in the folder \"Game Data\"; or\n"
                    "- DAT, GFX, SFX, TXT and VFX from the CD's SF directory, with the installed files on top.\n");
            }
            tell(SDL_MESSAGEBOX_WARNING, "Game data missing", incomplete + "Vertigo needs the game data of Archimedean Dynasty "
                 "or Schleichfahrt. In the Files app, copy into On My iPad \u203a Vertigo a GOG installation of the game (with "
                 "game.gog), or DAT, GFX, SFX, TXT and VFX from the CD's SF directory with the installed files on top.",
                 "Check Again");
        }
    }
}
#endif


// ends before the first frame with this exit code
SDL_AppResult end(int code)
{
    g_exitCode = code;
    return SDL_APP_SUCCESS;
}

} // namespace


SDL_AppResult SDL_AppInit(void **, int argc, char *argv[])
{
    util::CommandLine parser(
        "Open-source clone of Archimedean Dynasty / Schleichfahrt.\n\n"
        "Starting a mission directly (test mode) ends with exit code 0 on success,\n"
        "1 if the mission failed, 2 if it was aborted, 3 on --timeout and 4 if the\n"
        "player was destroyed.");
    parser.addArgument("mission", "Start this mission directly (test mode), e.g. s00101.", "[mission]");
    parser.addOption("boat", "Boat in test mode: 2049 Hiob, 2050 Gator, 2051 Zorn, 2052 Succubus.", "type");
    parser.addOption("equip", "Install items in test mode, e.g. GUN=3082,TUR1=3074 (repeatable, see --list-items).", "mounting=model");
    parser.addOption("timeout", "End test mode, or the game, after this many seconds; such automated runs are silent and don't take the focus.", "seconds");
    parser.addOption("overlay", "Show the debug overlay (toggle with Ctrl+D).");
    parser.addOption("destroy", "Destroy these units one second into the mission (test mode), e.g. 2064,2068; the player's id destroys the player.", "ids");
    parser.addOption("collapse", "Collapse these units' energy defence one second into the mission (test mode): they explode and tumble away, as after an Explode order.", "ids");
    parser.addOption("keys", "Press keys at the given seconds (test mode, or since the start), optionally holding them, e.g. 2=Esc,3=2,4=Alt+A,5=Down/0.5.", "time=key[/seconds]");
    parser.addOption("pad", "Work a virtual game controller at the given seconds (test mode, or since the start): buttons by an Xbox controller's names (A, B, X, Y, LB, RB, LT, RT, L3, R3, Start, Back, Up, Down, Left, Right), held for 0.1 s or the given seconds, together with +; sticks as LeftX, LeftY, RightX or RightY with a value from -1 to 1 (y down), held for good or the given seconds, e.g. 2=Start,3=A,4=RT/1,5=LB+A,6=LeftY:-0.5/2.", "time=control[/seconds]");
    parser.addOption("touch", "Touch the screen at the given seconds (test mode, or since the start) at a point given as fractions of the window's width and height, for 0.1 s or the given seconds; with a second point the finger moves there in 0.1 s. Repeatable, for several fingers, e.g. --touch 2=0.9,0.8/1 --touch 3=0.2,0.7>0.2,0.5/2.", "time=x,y[>x,y][/seconds]");
    parser.addOption("drive", "Let boats move at a speed in km/h, optionally turning (-1 to 1), in test mode, e.g. 2064=40,2068=60:0.5.", "id=speed[:turn]");
    parser.addOption("trace", "Print the position, speed and order of these units every second (test mode), e.g. 2064,2068.", "ids");
    parser.addOption("fire", "Let these units fire their guns (test mode); towers and tanks aim at the player.", "ids");
    parser.addOption("near", "Enable this unit and start 40 m from it, looking at it (test mode).", "id");
    parser.addOption("start", "Start at this cell of the map (test mode), optionally hei height units above the ground and heading card (0 north, 2 east, 4 south, 6 west) as in the scenario's player record, e.g. 76,76 or 76,76,10,4.", "x,y[,hei[,card]]");
    parser.addOption("headless", "Run the mission without a window and sound, as fast as possible (test mode; use with --timeout).");
    parser.addOption("check", "Load all missions, or the given ones, and report what the loader skips or ignores.");
    parser.addOption("list-items", "List the item models for --equip.");
    parser.addOption("difficulty", "Difficulty in test mode: 0 easy, 1 normal (default), 2 hard.", "level");
    parser.addOption("seed", "Random seed in test mode (default 1), so runs repeat exactly.", "number");
    parser.addOption("cockpit", "Show the original 2D cockpit or the 3D cockpit for this run (Alt+V switches and saves the choice).", "2d|3d");
    parser.addOption("shake", "How strongly the pilot's head moves in the 3D cockpit, 0 to turn it off (default 1).", "factor");
    parser.addOption("screenshot", "Save the window as PNG at the given mission seconds (test mode), or seconds since the start, e.g. 3=front.png.", "time=file");
    parser.addOption("fixed-step", "Advance the mission by 20 ms per frame instead of by the real time (test mode, with a window), so runs repeat exactly, e.g. for --screenshot.");
    parser.addOption("click", "Click at the given seconds (test mode, or since the start) at a point of the 640x480 screen, e.g. 2=320,300.", "time=x,y");
    parser.addOption("look", "Turn the pilot's head in the 3D cockpit for the whole run, in degrees left and up, e.g. --look=60,-10 (otherwise drag with the mouse).", "yaw,pitch");
    parser.addOption("data", "Use the game data in this directory for this run, instead of the remembered one.", "directory");

    if (!parser.parse(util::CommandLine::arguments(argc, argv))) {
        std::cerr << parser.error() << " See --help." << std::endl;
        return end(game::Vertigo::ExitUsage);
    }
    if (parser.helpRequested()) {
        std::cout << parser.help();
        return end(game::Vertigo::ExitSuccess);
    }

    // the time limit, decimals too; a value that isn't one would leave an automated run without
    // it, as a normal game that takes the focus and plays its sound
    float timeout = 0;
    if (parser.isSet("timeout")) {
        const String value = text(parser.value("timeout")).trimmed();
        timeout = isNumber(value) ? value.toFloat() : 0;
        if (timeout <= 0) {
            std::cerr << "Invalid --timeout value " << parser.value("timeout") << ", expected seconds above 0" << std::endl;
            return end(game::Vertigo::ExitUsage);
        }
    }

    // Automated runs with a window (with a time limit) shouldn't take the focus: on macOS, as a
    // background app (without a Dock icon, and no keyboard input other than --keys), and the
    // window isn't focusable either
    if (!parser.isSet("check") && !parser.isSet("headless") && timeout > 0)
        SDL_SetHint(SDL_HINT_MAC_BACKGROUND_APP, "1");
    SDL_SetAppMetadata("Vertigo", nullptr, "com.vertigo.Vertigo");
    // game controllers, except in automated runs (where --pad brings a virtual one)
    const bool automated = parser.isSet("check") || parser.isSet("headless") || timeout > 0;
    if (!SDL_Init(SDL_INIT_VIDEO | (automated ? 0 : SDL_INIT_GAMEPAD))) {
        std::cerr << "Cannot start SDL: " << SDL_GetError() << std::endl;
        return SDL_APP_FAILURE;
    }
    const long long milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    util::seedRandom(unsigned(milliseconds % 60000));

    game::Vertigo::Options options;
    if (parser.isSet("check")) {
        options.check = true;
        for (const std::string &scenario : parser.arguments())
            options.checkScenarios.append(text(scenario));
    } else if (!parser.arguments().empty()) {
        options.scenario = text(parser.arguments()[0]);
    }
    options.boat = text(parser.value("boat")).toInt();
    options.timeout = timeout;
    options.debugOverlay = parser.isSet("overlay");
    options.headless = parser.isSet("headless");
    options.fixedStep = parser.isSet("fixed-step");
    // test runs don't depend on the difficulty chosen in the game
    if (options.check || !options.scenario.isEmpty())
    {
        fight::Scenario::overrideDifficulty(parser.isSet("difficulty") ? text(parser.value("difficulty")).toInt() : 1);
        fight::Scenario::setRandomSeed(parser.isSet("seed") ? text(parser.value("seed")).toInt() : 1);
    }
    if (parser.isSet("cockpit")) {
        const String cockpit = text(parser.value("cockpit")).toLower();
        if (cockpit != "2d" && cockpit != "3d") {
            std::cerr << "Invalid --cockpit value " << cockpit << ", expected 2d or 3d" << std::endl;
            return end(game::Vertigo::ExitUsage);
        }
        hud::HUD::setCockpitMode(cockpit == "3d" ? hud::HUD::CockpitMode::Model : hud::HUD::CockpitMode::Art, false);
    }
    if (parser.isSet("shake")) {
        bool ok = false;
        const float shake = text(parser.value("shake")).toFloat(&ok);
        if (!ok || shake < 0) {
            std::cerr << "Invalid --shake value " << parser.value("shake") << ", expected a factor of 0 or more" << std::endl;
            return end(game::Vertigo::ExitUsage);
        }
        hud::HUD::setHeadMotion(shake, false);
    }
    if (parser.isSet("look")) {
        const StringList look = text(parser.value("look")).split(',');
        bool okYaw = false, okPitch = false;
        const float yaw = look.value(0).toFloat(&okYaw);
        const float pitch = look.value(1).toFloat(&okPitch);
        if (look.size() != 2 || !okYaw || !okPitch) {
            std::cerr << "Invalid --look value " << parser.value("look") << ", expected yaw,pitch in degrees" << std::endl;
            return end(game::Vertigo::ExitUsage);
        }
        hud::HUD::setLook(yaw, pitch);
    }
    for (const std::string &value : parser.values("screenshot")) {
        float time;
        std::string file;
        if (!timed(value, time, file)) {
            std::cerr << "Invalid --screenshot value " << value << ", expected time=file" << std::endl;
            return end(game::Vertigo::ExitUsage);
        }
        options.screenshots.push_back({ time, file });
    }
    for (const std::string &value : parser.values("click")) {
        float time;
        std::string point;
        const StringList xy = timed(value, time, point) ? text(point).split(',') : StringList();
        if (xy.size() != 2 || !isNumber(xy[0]) || !isNumber(xy[1])) {
            std::cerr << "Invalid --click value " << value << ", expected time=x,y" << std::endl;
            return end(game::Vertigo::ExitUsage);
        }
        options.clicks.push_back({ time, PointD(xy[0].toFloat(), xy[1].toFloat()) });
    }
    if (!ids(parser, "destroy", "unit ids", options.destroy))
        return end(game::Vertigo::ExitUsage);
    if (!ids(parser, "collapse", "unit ids", options.collapse))
        return end(game::Vertigo::ExitUsage);
    for (const std::string &item : items(parser.values("keys"))) {
        // time=key or time=key/hold
        float time = 0;
        std::string rest;
        const bool format = timed(item, time, rest);
        const String keyHold = text(rest);
        const int slash = keyHold.indexOf('/');
        const String hold = slash < 0 ? String() : keyHold.mid(slash + 1);
        int modifiers = input::NoModifier;
        const input::Key key = format && (slash < 0 || isNumber(hold)) ? input::parseKey(keyHold.left(slash), modifiers) : input::Key_None;
        if (key == input::Key_None) {
            std::cerr << "Invalid --keys value " << item << ", expected time=key[/seconds]" << std::endl;
            return end(game::Vertigo::ExitUsage);
        }
        options.keys.push_back({ time, key, modifiers, hold.toFloat() });
    }
    for (const std::string &item : items(parser.values("pad"))) {
        // time=buttons or time=axis:value, then optionally /hold
        float time = 0;
        std::string rest;
        const bool format = timed(item, time, rest);
        const String controlHold = text(rest);
        const int slash = controlHold.indexOf('/');
        const String control = controlHold.left(slash);
        const String hold = slash < 0 ? String() : controlHold.mid(slash + 1);
        game::Vertigo::Options::Pad pad = { time, {}, -1, 0, slash < 0 ? -1 : hold.toFloat() };
        bool ok = format && (slash < 0 || isNumber(hold));
        if (control.contains(':')) {
            static const char *axes[] = { "leftx", "lefty", "rightx", "righty" };
            const String name = control.section(':', 0, 0).trimmed().toLower();
            const String value = control.section(':', 1).trimmed();
            for (int axis = 0; axis < 4; axis++)
                if (name == axes[axis])
                    pad.axis = axis;
            ok = ok && pad.axis >= 0 && isNumber(value, true) && std::abs(value.toFloat()) <= 1;
            pad.value = value.toFloat();
        } else {
            for (const String &name : control.split('+')) {
                pad.buttons.push_back(input::parseButton(name));
                ok = ok && pad.buttons.back() != input::Button_None;
            }
        }
        if (!ok) {
            std::cerr << "Invalid --pad value " << item << ", expected time=buttons[/seconds] or time=stick axis:value[/seconds]" << std::endl;
            return end(game::Vertigo::ExitUsage);
        }
        options.pad.push_back(pad);
    }
    for (const std::string &item : parser.values("touch")) {
        // time=x,y or time=x,y>x,y, then optionally /hold
        float time = 0;
        std::string rest;
        const bool format = timed(item, time, rest);
        const String pointsHold = text(rest);
        const int slash = pointsHold.indexOf('/');
        const String hold = slash < 0 ? String() : pointsHold.mid(slash + 1);
        const StringList points = pointsHold.left(slash).split('>');
        game::Vertigo::Options::Touch touch = { time, PointD(0), PointD(0), slash < 0 ? 0.1f : hold.toFloat() };
        bool ok = format && (slash < 0 || isNumber(hold)) && points.size() >= 1 && points.size() <= 2;
        for (int i = 0; ok && i < points.size(); i++) {
            const StringList xy = points[i].split(',');
            ok = xy.size() == 2 && isNumber(xy[0].trimmed()) && isNumber(xy[1].trimmed());
            if (ok)
                (i == 0 ? touch.from : touch.to) = PointD(xy[0].toFloat(), xy[1].toFloat());
        }
        if (points.size() == 1)
            touch.to = touch.from;
        if (!ok) {
            std::cerr << "Invalid --touch value " << item << ", expected time=x,y[>x,y][/seconds]" << std::endl;
            return end(game::Vertigo::ExitUsage);
        }
        options.touch.push_back(touch);
    }
    if (parser.isSet("start")) {
        const StringList parts = text(parser.value("start")).split(',');
        bool ok = parts.size() >= 2 && parts.size() <= 4;
        for (const String &part : parts)
            ok = ok && isNumber(part);
        if (!ok) {
            std::cerr << "Invalid --start value " << parser.value("start") << ", expected x,y[,hei[,card]]" << std::endl;
            return end(game::Vertigo::ExitUsage);
        }
        options.start = { parts[0].toInt(), parts[1].toInt(), parts.value(2, "-1").toInt(), parts.value(3, "-1").toInt() };
    }
    if (parser.isSet("near")) {
        bool ok = false;
        options.near = text(parser.value("near")).toInt(&ok);
        if (!ok) {
            std::cerr << "Invalid --near value " << parser.value("near") << ", expected a unit id" << std::endl;
            return end(game::Vertigo::ExitUsage);
        }
    }
    if (!ids(parser, "fire", "unit ids", options.fire) || !ids(parser, "trace", "unit ids", options.trace))
        return end(game::Vertigo::ExitUsage);
    for (const std::string &item : items(parser.values("drive"))) {
        // id=speed or id=speed:turn
        const StringList parts = text(item).split('=');
        const StringList speedTurn = parts.value(1).split(':');
        bool ok = false;
        const int id = parts.value(0).toInt(&ok);
        if (parts.size() != 2 || !ok || speedTurn.size() > 2 || !isNumber(speedTurn.value(0), true) || (speedTurn.size() == 2 && !isNumber(speedTurn[1], true))) {
            std::cerr << "Invalid --drive value " << item << ", expected id=speed[:turn]" << std::endl;
            return end(game::Vertigo::ExitUsage);
        }
        options.drive.push_back({ id, speedTurn[0].toFloat(), speedTurn.value(1).toFloat() });
    }
    for (const std::string &item : items(parser.values("equip"))) {
        const StringList parts = text(item).split('=');
        bool ok = false;
        const int model = parts.value(1).toInt(&ok);
        if (parts.size() != 2 || !ok) {
            std::cerr << "Invalid --equip value " << item << ", expected mounting=model" << std::endl;
            return end(game::Vertigo::ExitUsage);
        }
        options.equipment.emplace_back(parts[0].toUpper(), model);
    }

#if defined(SDL_PLATFORM_IOS)
    if (!parser.isSet("data")) {
        findGameData();
    } else {
        util::DataPath::set(parser.value("data"));
        const std::string missing = game::GameData::missing();
        if (!missing.empty()) {
            std::cerr << parser.value("data") << " lacks " << missing << std::endl;
            return end(game::Vertigo::ExitUsage);
        }
    }
#else
    util::DataPath::set(parser.isSet("data") ? parser.value("data") : util::Settings::value("datadir"));
    // the Schleichfahrt / Archimedean Dynasty directory, asked for until one has the game data,
    // which can also be built from a GOG installation's CD image; remembered once it has
    bool chosen = false;
    for (std::string missing = game::GameData::missing(); !missing.empty(); missing = game::GameData::missing()) {
        const std::string directory = util::DataPath::directory();
        const std::string image = directory.empty() ? std::string() : game::GameData::findImage(directory);
        if (!image.empty()) {
            const std::string target = importTarget(util::appDataLocation());
            const int choice = offerImport(directory, image, target);
            if (choice == 0)
                return end(game::Vertigo::ExitSuccess);
            std::string error;
            if (choice == 2 && game::GameData::import(image, target, error)) {
                util::DataPath::set(target);
                chosen = true;
                continue;
            }
            if (choice == 2)
                SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Import failed", error.c_str(), nullptr);
        } else if (!directory.empty() && !chooseAgain(directory, missing)) {
            return end(game::Vertigo::ExitSuccess);
        }
        const std::string chosenDirectory = chooseDirectory(directory);
        if (chosenDirectory.empty())
            return end(game::Vertigo::ExitSuccess);
        util::DataPath::set(chosenDirectory);
        chosen = true;
    }
    if (chosen)
        util::Settings::setValue("datadir", util::DataPath::directory());
#endif

    if (parser.isSet("list-items")) {
        static const char *types[] = { "torpedo", "magazine", "gun", "engine", "booster", "silator", "armor", "nrskin", "sensor", "software", "buzzer", "fixer" };
        for (const auto &pair : game::Items::all())
            std::cout << pair.first << "\t" << types[pair.second.type] << "\t" << pair.second.longname << "\n";
        return end(game::Vertigo::ExitSuccess);
    }

    g_vertigo.reset(new game::Vertigo(options));
    const int code = g_vertigo->start();
    if (code >= 0)
        return end(code);
    // a frame every 20 ms with a window; without one as fast as it goes
    SDL_SetHint(SDL_HINT_MAIN_CALLBACK_RATE, options.headless && !options.scenario.isEmpty() ? "0" : "50");
    return SDL_APP_CONTINUE;
}


SDL_AppResult SDL_AppIterate(void *)
{
    const int code = g_vertigo->iterate();
    return code >= 0 ? end(code) : SDL_APP_CONTINUE;
}


SDL_AppResult SDL_AppEvent(void *, SDL_Event *event)
{
    if (g_vertigo)
        g_vertigo->event(*event);
    return SDL_APP_CONTINUE;
}


void SDL_AppQuit(void *, SDL_AppResult result)
{
    g_vertigo.reset();
    SDL_Quit();
    // the exit code of test mode (SDL's own would only tell success from failure)
    std::exit(result == SDL_APP_FAILURE ? EXIT_FAILURE : g_exitCode);
}
