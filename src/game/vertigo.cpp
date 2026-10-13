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

#include "hud/hud.h"
#include "game/boat.h"
#include "game/items.h"
#include "fight/scenario.h"
#include "fight/objects/activeobject.h"
#include "fight/boats/simpleboat.h"
#include "txt/desfile.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QKeyEvent>
#include <QMap>
#include <QTextStream>
#include <iostream>

namespace game {

Vertigo::Vertigo() :
    m_mainMenu(
       [this](QString name) { startGame(name); },
       [this](QString name) { loadGame(name); },
       [this]() { m_window.close(); }
    )
{
}

int Vertigo::start(const Options &options)
{
    if (options.check)
        return check(options.checkScenarios);

    std::unique_ptr<hud::HUD> hud;
    std::unique_ptr<game::Boat> boat;
    std::unique_ptr<fight::Scenario> scenario;

    // test mode: closing the window counts as aborting the mission
    int result = options.scenario.isEmpty() ? ExitSuccess : ExitAborted;

    if (options.scenario.isEmpty()) {
        m_window.setRenderer(&m_mainMenu);
    } else {
        txt::DesFile desfile("dat:story/ch1.des");
        boat.reset(new game::Boat(desfile));
        if (options.boat != 0) {
            if (options.boat < 2049 || options.boat > 2052) {
                std::cerr << "Unknown boat type " << options.boat << " (2049-2052)" << std::endl;
                return ExitUsage;
            }
            boat->upgrade(options.boat);
        }
        for (const auto &item : options.equipment) {
            if (Items::all().count(item.second) == 0) {
                std::cerr << "Unknown item " << item.second << " (see --list-items)" << std::endl;
                return ExitUsage;
            }
            if (!boat->getMounting(item.first)) {
                std::cerr << "The boat has no mounting " << item.first.toUtf8().constData() << std::endl;
                return ExitUsage;
            }
            boat->buy(item.second, item.first);
        }

        hud.reset(new hud::HUD());
        hud->eventSuccess() += [this, &result]() { result = ExitSuccess; m_window.close(); };
        hud->eventAbort() += [this, &result]() { result = ExitAborted; m_window.close(); };
        hud->eventFailure() += [this, &result]() { result = ExitFailed; m_window.close(); };
        hud->eventDied() += [this, &result]() { result = ExitDied; m_window.close(); };
        // there is no saved game in test mode
        hud->eventLoad() += [this, &result]() { result = ExitAborted; m_window.close(); };
        auto startScenario = [&]() {
            scenario.reset(new fight::Scenario(options.scenario));
            hud->start(scenario.get());
            for (const Options::Drive &drive : options.drive) {
                fight::SimpleBoat *unit = nullptr;
                for (const auto &record : scenario->objectRecords())
                    if (record.id == drive.id)
                        unit = dynamic_cast<fight::SimpleBoat *>(record.object);
                if (!unit) {
                    std::cerr << "--drive: no boat " << drive.id << std::endl;
                    continue;
                }
                unit->setVelocityTarget(drive.speed/3.6f);
                unit->setTurnVelocity(Vector3D(0, 0, drive.turn));
            }
        };
        hud->eventRestart() += startScenario;
        hud->setDebugOverlay(options.debugOverlay);
        hud->load(boat.get());
        startScenario();
        m_window.setRenderer(hud.get());
    }

    std::vector<int> destroy = options.destroy;
    auto keys = options.keys;
    QElapsedTimer timer;
    timer.start();

    m_window.show();
    while (m_window.isVisible()) {
        // block until the next event (at least the 20ms frame timer) instead of spinning
        qApp->processEvents(QEventLoop::WaitForMoreEvents);
        util::DeferredDeletable::clear();

        if (scenario && !destroy.empty() && timer.elapsed() >= 1000) {
            for (const int id : destroy) {
                bool found = false;
                if (id == scenario->playerId()) {
                    scenario->player()->destroy();
                    found = true;
                }
                for (const auto &record : scenario->objectRecords()) {
                    fight::ActiveObject *object = dynamic_cast<fight::ActiveObject *>(record.object);
                    if (record.id == id && object) {
                        // a hit strong enough to destroy it, so "attacked" fires as well
                        object->damage(object->kineticShield() + 1, 0, object->position());
                        found = true;
                    }
                }
                if (!found)
                    std::cerr << "--destroy: no unit " << id << std::endl;
            }
            destroy.clear();
        }
        for (auto it = keys.begin(); scenario && it != keys.end(); ) {
            if (timer.elapsed() < it->first*1000) {
                ++it;
                continue;
            }
            const int key = it->second[0] & ~Qt::KeyboardModifierMask;
            const Qt::KeyboardModifiers modifiers = Qt::KeyboardModifiers(it->second[0] & Qt::KeyboardModifierMask);
            const QString text = key < 0x80 ? QString(QChar(key)).toLower() : QString();
            QCoreApplication::postEvent(&m_window, new QKeyEvent(QEvent::KeyPress, key, modifiers, text));
            QCoreApplication::postEvent(&m_window, new QKeyEvent(QEvent::KeyRelease, key, modifiers, text));
            it = keys.erase(it);
        }
        if (scenario && result == ExitAborted && scenario->conditionManager().failed())
            result = ExitFailed;
        // test mode doesn't wait for the autopilot key
        if (scenario && result == ExitAborted && scenario->conditionManager().autopilot()) {
            result = ExitSuccess;
            m_window.close();
        }
        if (scenario && options.timeout > 0 && timer.elapsed() >= options.timeout*1000) {
            if (result == ExitAborted)
                result = ExitTimeout;
            m_window.close();
        }
    }

    if (scenario) {
        int enabled = 0;
        for (const auto &record : scenario->objectRecords())
            if (record.object->isEnabled())
                enabled++;
        static const char *results[] = { "success", "failed", "aborted", "timeout", "died" };
        std::cout << options.scenario.toUtf8().constData() << ": " << results[result]
                  << " after " << timer.elapsed()/1000.0 << " s, " << enabled << "/" << scenario->objectRecords().size()
                  << " units enabled, " << scenario->radio().size() << " radio messages" << std::endl;
    }

    return result;
}

int Vertigo::check(const QStringList &names)
{
    QStringList scenarios = names;
    if (scenarios.isEmpty()) {
        const QDir dir(QDir::searchPaths("data").value(0) + "/vfx/scenario");
        foreach (const QString &file, dir.entryList(QStringList() << "*.des", QDir::Files, QDir::Name))
            scenarios << QFileInfo(file).completeBaseName().toLower();
    }

    QTextStream out(stdout);
    QMap<QString, int> totals;
    int objects = 0;
    foreach (const QString &name, scenarios) {
        fight::Scenario scenario(name);
        QMap<QString, int> counts;
        foreach (const QString &issue, scenario.loadReport())
            counts[issue]++;

        out << name << ": " << scenario.objectRecords().size() << " objects\n";
        for (auto it = counts.cbegin(); it != counts.cend(); ++it) {
            out << "  " << it.value() << "x " << it.key() << "\n";
            // object ids differ between missions, so don't tell them apart in the totals
            QString kind = it.key();
            if (kind.startsWith("condition: "))
                kind.replace(QRegExp("\\d+"), "#");
            totals[kind] += it.value();
        }
        objects += scenario.objectRecords().size();
        out.flush();
    }

    // totals per kind of issue, most frequent first
    QMultiMap<int, QString> byCount;
    for (auto it = totals.cbegin(); it != totals.cend(); ++it)
        byCount.insert(it.value(), it.key());
    out << "\nTotal: " << scenarios.size() << " missions, " << objects << " objects\n";
    for (auto it = byCount.cend(); it != byCount.cbegin();) {
        --it;
        out << "  " << it.key() << "x " << it.value() << "\n";
    }

    return ExitSuccess;
}

void Vertigo::createChapter()
{
    m_chapter.reset(new Chapter(m_name,
        [this](Renderer *renderer) { m_window.setRenderer(renderer); },
        [this]() { endGame(); }
    ));
}

void Vertigo::startGame(const QString &name)
{
    if (name.isEmpty())
        return;

    foreach (const Chapter::SavedGame &game, Chapter::savedGames()) {
        if (game.name.toLower() == name)
            return;
    }

    m_name = name;
    m_intro.reset(new Movie("gfx:mvi/film/d02.mvi", Movie::MovieFilm));
    m_window.setRenderer(m_intro.get());
    m_intro->play([this]() { introFinished(); });
}

void Vertigo::loadGame(const QString &name)
{
    createChapter();
    m_chapter->load(name);
}

void Vertigo::endGame()
{
    m_chapter.release()->deleteLater();
    m_window.setRenderer(&m_mainMenu);
}

void Vertigo::introFinished()
{
    m_window.setRenderer(NULL);
    m_intro.release()->deleteLater();

    createChapter();
    m_chapter->loadChapter(1);
}

} // namespace game
