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
#include "util/file.h"

#include "items.h"
#include "hud/hud.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QTextStream>
#include <QDir>
#include <QFileDialog>
#include <QSettings>
#include <QTime>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    qsrand(QTime::currentTime().second() * 1000 + QTime::currentTime().msec());

    app.setOrganizationName("Vertigo");
    app.setApplicationName("Vertigo");

    QCommandLineParser parser;
    parser.setApplicationDescription(
        "Open-source clone of Archimedean Dynasty / Schleichfahrt.\n\n"
        "Starting a mission directly (test mode) ends with exit code 0 on success,\n"
        "1 if the mission failed, 2 if it was aborted, 3 on --timeout and 4 if the\n"
        "player was destroyed.");
    parser.addHelpOption();
    parser.addPositionalArgument("mission", "Start this mission directly (test mode), e.g. s00101.", "[mission]");
    const QCommandLineOption boatOption("boat", "Boat in test mode: 2049 Hiob, 2050 Gator, 2051 Zorn, 2052 Succubus.", "type");
    const QCommandLineOption equipOption("equip", "Install items in test mode, e.g. GUN=3082,TUR1=3074 (repeatable, see --list-items).", "mounting=model");
    const QCommandLineOption timeoutOption("timeout", "End test mode after this many seconds.", "seconds");
    const QCommandLineOption overlayOption("overlay", "Show the debug overlay (toggle with Ctrl+D).");
    const QCommandLineOption destroyOption("destroy", "Destroy these units one second into the mission (test mode), e.g. 2064,2068; the player's id destroys the player.", "ids");
    const QCommandLineOption keysOption("keys", "Press keys at the given seconds (test mode), optionally holding them, e.g. 2=Esc,3=2,4=Alt+A,5=Down/0.5.", "time=key[/seconds]");
    const QCommandLineOption driveOption("drive", "Let boats move at a speed in km/h, optionally turning (-1 to 1), in test mode, e.g. 2064=40,2068=60:0.5.", "id=speed[:turn]");
    const QCommandLineOption traceOption("trace", "Print the position, speed and order of these units every second (test mode), e.g. 2064,2068.", "ids");
    const QCommandLineOption headlessOption("headless", "Run the mission without a window and sound, as fast as possible (test mode; use with --timeout).");
    const QCommandLineOption fireOption("fire", "Let these units fire their guns (test mode); towers aim at the player.", "ids");
    const QCommandLineOption checkOption("check", "Load all missions, or the given ones, and report what the loader skips or ignores.");
    const QCommandLineOption itemsOption("list-items", "List the item models for --equip.");
    const QCommandLineOption cockpitOption("cockpit", "Show the original 2D cockpit or the 3D cockpit for this run (Alt+V switches and saves the choice).", "2d|3d");
    const QCommandLineOption shakeOption("shake", "How strongly the pilot's head moves in the 3D cockpit, 0 to turn it off (default 1).", "factor");
    const QCommandLineOption screenshotOption("screenshot", "Save the window at the given mission seconds (test mode), e.g. 3=front.png.", "time=file");
    parser.addOptions({ boatOption, equipOption, timeoutOption, overlayOption, destroyOption, keysOption, driveOption, traceOption, fireOption, headlessOption, checkOption, itemsOption, cockpitOption, shakeOption, screenshotOption });
    parser.process(app);

    game::Vertigo::Options options;
    if (parser.isSet(checkOption)) {
        options.check = true;
        options.checkScenarios = parser.positionalArguments();
    } else {
        options.scenario = parser.positionalArguments().value(0);
    }
    options.boat = parser.value(boatOption).toInt();
    options.timeout = parser.value(timeoutOption).toInt();
    options.debugOverlay = parser.isSet(overlayOption);
    options.headless = parser.isSet(headlessOption);
    if (parser.isSet(cockpitOption)) {
        const QString cockpit = parser.value(cockpitOption).toLower();
        if (cockpit != "2d" && cockpit != "3d") {
            QTextStream(stderr) << "Invalid --cockpit value " << cockpit << ", expected 2d or 3d\n";
            return game::Vertigo::ExitUsage;
        }
        hud::HUD::setCockpitMode(cockpit == "3d" ? hud::HUD::CockpitMode::Model : hud::HUD::CockpitMode::Art, false);
    }
    if (parser.isSet(shakeOption)) {
        bool ok = false;
        const float shake = parser.value(shakeOption).toFloat(&ok);
        if (!ok || shake < 0) {
            QTextStream(stderr) << "Invalid --shake value " << parser.value(shakeOption) << ", expected a factor of 0 or more\n";
            return game::Vertigo::ExitUsage;
        }
        hud::HUD::setHeadMotion(shake, false);
    }
    foreach (const QString &value, parser.values(screenshotOption)) {
        const QRegExp format("([\\d.]+)=(.+)");
        if (!format.exactMatch(value)) {
            QTextStream(stderr) << "Invalid --screenshot value " << value << ", expected time=file\n";
            return game::Vertigo::ExitUsage;
        }
        options.screenshots.push_back({ format.cap(1).toFloat(), format.cap(2) });
    }
    foreach (const QString &value, parser.values(destroyOption)) {
        foreach (const QString &id, value.split(',', QString::SkipEmptyParts)) {
            bool ok = false;
            options.destroy.push_back(id.toInt(&ok));
            if (!ok) {
                QTextStream(stderr) << "Invalid --destroy value " << id << ", expected unit ids\n";
                return game::Vertigo::ExitUsage;
            }
        }
    }
    foreach (const QString &value, parser.values(keysOption)) {
        foreach (const QString &item, value.split(',', QString::SkipEmptyParts)) {
            // time=key or time=key/hold
            const QRegExp format("([\\d.]+)=([^/]+)(/([\\d.]+))?");
            const QKeySequence key(format.exactMatch(item) ? format.cap(2) : QString());
            if (key.isEmpty()) {
                QTextStream(stderr) << "Invalid --keys value " << item << ", expected time=key[/seconds]\n";
                return game::Vertigo::ExitUsage;
            }
            options.keys.push_back({ format.cap(1).toFloat(), key, format.cap(4).toFloat() });
        }
    }
    foreach (const QString &value, parser.values(fireOption)) {
        foreach (const QString &id, value.split(',', QString::SkipEmptyParts)) {
            bool ok = false;
            options.fire.push_back(id.toInt(&ok));
            if (!ok) {
                QTextStream(stderr) << "Invalid --fire value " << id << ", expected unit ids\n";
                return game::Vertigo::ExitUsage;
            }
        }
    }
    foreach (const QString &value, parser.values(traceOption)) {
        foreach (const QString &id, value.split(',', QString::SkipEmptyParts)) {
            bool ok = false;
            options.trace.push_back(id.toInt(&ok));
            if (!ok) {
                QTextStream(stderr) << "Invalid --trace value " << id << ", expected unit ids\n";
                return game::Vertigo::ExitUsage;
            }
        }
    }
    foreach (const QString &value, parser.values(driveOption)) {
        foreach (const QString &item, value.split(',', QString::SkipEmptyParts)) {
            const QRegExp format("(\\d+)=(-?[\\d.]+)(:(-?[\\d.]+))?");
            if (!format.exactMatch(item)) {
                QTextStream(stderr) << "Invalid --drive value " << item << ", expected id=speed[:turn]\n";
                return game::Vertigo::ExitUsage;
            }
            options.drive.push_back({ format.cap(1).toInt(), format.cap(2).toFloat(), format.cap(4).toFloat() });
        }
    }
    foreach (const QString &value, parser.values(equipOption)) {
        foreach (const QString &item, value.split(',', QString::SkipEmptyParts)) {
            const QStringList parts = item.split('=');
            bool ok = false;
            const int model = parts.value(1).toInt(&ok);
            if (parts.size() != 2 || !ok) {
                QTextStream(stderr) << "Invalid --equip value " << item << ", expected mounting=model\n";
                return game::Vertigo::ExitUsage;
            }
            options.equipment.emplace_back(parts[0].toUpper(), model);
        }
    }

    QDir::addSearchPath("dat", "data:dat");
    QDir::addSearchPath("gfx", "data:gfx");
    QDir::addSearchPath("sfx", "data:sfx");
    QDir::addSearchPath("txt", "data:txt");
    QDir::addSearchPath("vfx", "data:vfx");

    QSettings s;
    QDir::setSearchPaths("data", QStringList() << s.value("datadir").toString());
    util::File::setDataPath(s.value("datadir").toString().toUtf8().constData());
    while (!txt::StringTable::load()) {
        const QString dir = QFileDialog::getExistingDirectory(NULL, "Select Schleichfahrt / Archimedean Dynasty directory", s.value("datadir").toString());
        if (dir.isNull())
            return false;
        s.setValue("datadir", dir);
        QDir::setSearchPaths("data", QStringList() << s.value("datadir").toString());
        util::File::setDataPath(s.value("datadir").toString().toUtf8().constData());
    }

    if (parser.isSet(itemsOption)) {
        static const char *types[] = { "torpedo", "magazine", "gun", "engine", "booster", "silator", "armor", "nrskin", "sensor", "software", "buzzer", "fixer" };
        QTextStream out(stdout);
        for (const auto &pair : game::Items::all())
            out << pair.first << "\t" << types[pair.second.type] << "\t" << pair.second.longname << "\n";
        return 0;
    }

    game::Vertigo vertigo;
    return vertigo.start(options);
}
