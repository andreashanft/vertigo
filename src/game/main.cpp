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

#include "vertigo.h"

#include "txt/stringtable.h"
#include "util/commandline.hpp"
#include "util/datapath.h"
#include "util/random.h"
#include "util/settings.hpp"

#include "items.h"
#include "fight/scenario.h"
#include "hud/hud.h"

// Qt, until SDL takes the platform over (step Q5 of the iPad port)
#include <QApplication>
#include <QDataStream>
#include <QFileDialog>
#include <QKeySequence>
#include <QRect>
#include <QSettings>

#include <chrono>
#include <iostream>


namespace {

// the settings of the builds before, kept by QSettings; taken over once (only the game's own:
// on macOS, QSettings also lists the system's)
std::map<std::string, std::string> qtSettings()
{
    QSettings settings;
    std::map<std::string, std::string> values;
    auto geometry = [](const QRect &rect) {
        return QString("%1 %2 %3 %4").arg(rect.x()).arg(rect.y()).arg(rect.width()).arg(rect.height()).toStdString();
    };
    for (const char *name : { "datadir", "Window/geometry", "Window/fullScreen", "MainWindow/geometry", "Movie/autopilot", "Movie/approach",
                              "Movie/harbour", "Game/difficulty", "cockpit/mode", "cockpit/shake", "cockpit/directory" }) {
        const QString key(name);
        if (!settings.contains(key))
            continue;
        const QVariant value = settings.value(key);
        if (key == "Window/geometry") {
            if (value.toRect().isValid())
                values[key.toStdString()] = geometry(value.toRect());
        } else if (key == "MainWindow/geometry") {
            // what QWidget::saveGeometry() stored before (version 3): frame and normal geometry,
            // screen, maximized, full screen, screen width, geometry
            QDataStream stream(value.toByteArray());
            stream.setVersion(QDataStream::Qt_4_0);
            quint32 magic;
            quint16 major, minor;
            QRect frameGeometry, normalGeometry, lastGeometry;
            qint32 screen, screenWidth;
            quint8 maximized, wasFullScreen;
            stream >> magic >> major >> minor >> frameGeometry >> normalGeometry >> screen >> maximized >> wasFullScreen >> screenWidth >> lastGeometry;
            if (stream.status() == QDataStream::Ok && magic == 0x1D9D0CB && major >= 3 && !settings.contains("Window/geometry")) {
                values["Window/geometry"] = geometry(wasFullScreen ? normalGeometry : lastGeometry);
                values["Window/fullScreen"] = wasFullScreen ? "true" : "false";
            }
        } else if (value.canConvert<QString>()) {
            values[key.toStdString()] = value.toString().toStdString();
        }
    }
    return values;
}


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

} // namespace


int main(int argc, char *argv[])
{
    util::CommandLine parser(
        "Open-source clone of Archimedean Dynasty / Schleichfahrt.\n\n"
        "Starting a mission directly (test mode) ends with exit code 0 on success,\n"
        "1 if the mission failed, 2 if it was aborted, 3 on --timeout and 4 if the\n"
        "player was destroyed.");
    parser.addArgument("mission", "Start this mission directly (test mode), e.g. s00101.", "[mission]");
    parser.addOption("boat", "Boat in test mode: 2049 Hiob, 2050 Gator, 2051 Zorn, 2052 Succubus.", "type");
    parser.addOption("equip", "Install items in test mode, e.g. GUN=3082,TUR1=3074 (repeatable, see --list-items).", "mounting=model");
    parser.addOption("timeout", "End test mode, or the game, after this many seconds.", "seconds");
    parser.addOption("overlay", "Show the debug overlay (toggle with Ctrl+D).");
    parser.addOption("destroy", "Destroy these units one second into the mission (test mode), e.g. 2064,2068; the player's id destroys the player.", "ids");
    parser.addOption("keys", "Press keys at the given seconds (test mode, or since the start), optionally holding them, e.g. 2=Esc,3=2,4=Alt+A,5=Down/0.5.", "time=key[/seconds]");
    parser.addOption("drive", "Let boats move at a speed in km/h, optionally turning (-1 to 1), in test mode, e.g. 2064=40,2068=60:0.5.", "id=speed[:turn]");
    parser.addOption("trace", "Print the position, speed and order of these units every second (test mode), e.g. 2064,2068.", "ids");
    parser.addOption("fire", "Let these units fire their guns (test mode); towers and tanks aim at the player.", "ids");
    parser.addOption("near", "Enable this unit and start 40 m from it, looking at it (test mode).", "id");
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
        return game::Vertigo::ExitUsage;
    }
    if (parser.helpRequested()) {
        std::cout << parser.help();
        return game::Vertigo::ExitSuccess;
    }

    // Automated runs with a window (with a time limit) shouldn't take the focus. On macOS, Qt
    // and AppKit (which takes a mission argument for a file to open) activate the app
    // when it has finished launching. Unless this is set, Qt makes the process a foreground app
    // when the QApplication is created, so the arguments are checked before: a background app
    // can't be activated, but it has no Dock icon and gets no keyboard input other than --keys.
    if (!parser.isSet("check") && !parser.isSet("headless") && text(parser.value("timeout")).toInt() > 0)
        qputenv("QT_MAC_DISABLE_FOREGROUND_APPLICATION_TRANSFORM", "1");

    QApplication app(argc, argv);
    const long long milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    util::seedRandom(unsigned(milliseconds % 60000));

    // where QSettings found the settings of the builds before
    app.setOrganizationName("Vertigo");
    app.setApplicationName("Vertigo");
    if (!util::Settings::exists())
        util::Settings::import(qtSettings());

    game::Vertigo::Options options;
    if (parser.isSet("check")) {
        options.check = true;
        for (const std::string &scenario : parser.arguments())
            options.checkScenarios.append(text(scenario));
    } else if (!parser.arguments().empty()) {
        options.scenario = text(parser.arguments()[0]);
    }
    options.boat = text(parser.value("boat")).toInt();
    options.timeout = text(parser.value("timeout")).toInt();
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
            return game::Vertigo::ExitUsage;
        }
        hud::HUD::setCockpitMode(cockpit == "3d" ? hud::HUD::CockpitMode::Model : hud::HUD::CockpitMode::Art, false);
    }
    if (parser.isSet("shake")) {
        bool ok = false;
        const float shake = text(parser.value("shake")).toFloat(&ok);
        if (!ok || shake < 0) {
            std::cerr << "Invalid --shake value " << parser.value("shake") << ", expected a factor of 0 or more" << std::endl;
            return game::Vertigo::ExitUsage;
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
            return game::Vertigo::ExitUsage;
        }
        hud::HUD::setLook(yaw, pitch);
    }
    for (const std::string &value : parser.values("screenshot")) {
        float time;
        std::string file;
        if (!timed(value, time, file)) {
            std::cerr << "Invalid --screenshot value " << value << ", expected time=file" << std::endl;
            return game::Vertigo::ExitUsage;
        }
        options.screenshots.push_back({ time, file });
    }
    for (const std::string &value : parser.values("click")) {
        float time;
        std::string point;
        const StringList xy = timed(value, time, point) ? text(point).split(',') : StringList();
        if (xy.size() != 2 || !isNumber(xy[0]) || !isNumber(xy[1])) {
            std::cerr << "Invalid --click value " << value << ", expected time=x,y" << std::endl;
            return game::Vertigo::ExitUsage;
        }
        options.clicks.push_back({ time, PointD(xy[0].toFloat(), xy[1].toFloat()) });
    }
    if (!ids(parser, "destroy", "unit ids", options.destroy))
        return game::Vertigo::ExitUsage;
    for (const std::string &item : items(parser.values("keys"))) {
        // time=key or time=key/hold
        float time = 0;
        std::string rest;
        const bool format = timed(item, time, rest);
        const String keyHold = text(rest);
        const int slash = keyHold.indexOf('/');
        const String hold = slash < 0 ? String() : keyHold.mid(slash + 1);
        const QKeySequence key(format && (slash < 0 || isNumber(hold)) ? QString(keyHold.left(slash)) : QString());
        if (key.isEmpty()) {
            std::cerr << "Invalid --keys value " << item << ", expected time=key[/seconds]" << std::endl;
            return game::Vertigo::ExitUsage;
        }
        options.keys.push_back({ time, key, hold.toFloat() });
    }
    if (parser.isSet("near")) {
        bool ok = false;
        options.near = text(parser.value("near")).toInt(&ok);
        if (!ok) {
            std::cerr << "Invalid --near value " << parser.value("near") << ", expected a unit id" << std::endl;
            return game::Vertigo::ExitUsage;
        }
    }
    if (!ids(parser, "fire", "unit ids", options.fire) || !ids(parser, "trace", "unit ids", options.trace))
        return game::Vertigo::ExitUsage;
    for (const std::string &item : items(parser.values("drive"))) {
        // id=speed or id=speed:turn
        const StringList parts = text(item).split('=');
        const StringList speedTurn = parts.value(1).split(':');
        bool ok = false;
        const int id = parts.value(0).toInt(&ok);
        if (parts.size() != 2 || !ok || speedTurn.size() > 2 || !isNumber(speedTurn.value(0), true) || (speedTurn.size() == 2 && !isNumber(speedTurn[1], true))) {
            std::cerr << "Invalid --drive value " << item << ", expected id=speed[:turn]" << std::endl;
            return game::Vertigo::ExitUsage;
        }
        options.drive.push_back({ id, speedTurn[0].toFloat(), speedTurn.value(1).toFloat() });
    }
    for (const std::string &item : items(parser.values("equip"))) {
        const StringList parts = text(item).split('=');
        bool ok = false;
        const int model = parts.value(1).toInt(&ok);
        if (parts.size() != 2 || !ok) {
            std::cerr << "Invalid --equip value " << item << ", expected mounting=model" << std::endl;
            return game::Vertigo::ExitUsage;
        }
        options.equipment.emplace_back(parts[0].toUpper(), model);
    }

    util::DataPath::set(parser.isSet("data") ? parser.value("data") : util::Settings::value("datadir"));
    while (!txt::StringTable::load()) {
        const QString dir = QFileDialog::getExistingDirectory(NULL, "Select Schleichfahrt / Archimedean Dynasty directory", QString::fromStdString(util::DataPath::directory()));
        if (dir.isNull())
            return game::Vertigo::ExitSuccess;
        util::Settings::setValue("datadir", dir.toStdString());
        util::DataPath::set(dir.toStdString());
    }

    if (parser.isSet("list-items")) {
        static const char *types[] = { "torpedo", "magazine", "gun", "engine", "booster", "silator", "armor", "nrskin", "sensor", "software", "buzzer", "fixer" };
        for (const auto &pair : game::Items::all())
            std::cout << pair.first << "\t" << types[pair.second.type] << "\t" << pair.second.longname << "\n";
        return game::Vertigo::ExitSuccess;
    }

    game::Vertigo vertigo;
    return vertigo.start(options);
}
