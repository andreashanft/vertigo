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
        "1 if the mission failed, 2 if it was aborted and 3 on --timeout.");
    parser.addHelpOption();
    parser.addPositionalArgument("mission", "Start this mission directly (test mode), e.g. s00101.", "[mission]");
    const QCommandLineOption boatOption("boat", "Boat in test mode: 2049 Hiob, 2050 Gator, 2051 Zorn, 2052 Succubus.", "type");
    const QCommandLineOption equipOption("equip", "Install items in test mode, e.g. GUN=3082,TUR1=3074 (repeatable, see --list-items).", "mounting=model");
    const QCommandLineOption timeoutOption("timeout", "End test mode after this many seconds.", "seconds");
    const QCommandLineOption overlayOption("overlay", "Show the debug overlay (toggle with Ctrl+D).");
    const QCommandLineOption checkOption("check", "Load all missions, or the given ones, and report what the loader skips or ignores.");
    const QCommandLineOption itemsOption("list-items", "List the item models for --equip.");
    parser.addOptions({ boatOption, equipOption, timeoutOption, overlayOption, checkOption, itemsOption });
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
