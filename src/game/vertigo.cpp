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
#include "game/renderer.h"
#include "game/boat.h"
#include "game/items.h"
#include "fight/scenario.h"
#include "fight/objects/activeobject.h"
#include "fight/boats/simpleboat.h"
#include "fight/orders.h"
#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>
#include <cmath>
#include <algorithm>
#include "txt/desfile.h"
#include "util/datapath.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QMap>
#include <QTextStream>
#include <QRegularExpression>
#include <iostream>

namespace game {

Vertigo::Vertigo() :
    m_mainMenu(
       [this](String name) { startGame(name); },
       [this](String name) { loadGame(name); },
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
    bool running = true;
    auto finish = [&](int code) {
        result = code;
        running = false;
        m_window.close();
    };
    const bool headless = options.headless && !options.scenario.isEmpty();

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
                std::cerr << "The boat has no mounting " << item.first << std::endl;
                return ExitUsage;
            }
            boat->buy(item.second, item.first);
        }

        hud.reset(new hud::HUD());
        hud->eventSuccess() += [&]() { finish(ExitSuccess); };
        hud->eventAbort() += [&]() { finish(ExitAborted); };
        hud->eventFailure() += [&]() { finish(ExitFailed); };
        hud->eventDied() += [&]() { finish(ExitDied); };
        // there is no saved game in test mode
        hud->eventLoad() += [&]() { finish(ExitAborted); };
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
                // the test drives the unit instead of its orders
                unit->setOrders(nullptr);
                unit->setVelocityTarget(drive.speed/3.6f);
                unit->setTurnVelocity(Vector3D(0, 0, drive.turn));
            }
            if (options.near) {
                fight::ActiveObject *unit = nullptr;
                for (const auto &record : scenario->objectRecords())
                    if (record.id == options.near)
                        unit = dynamic_cast<fight::ActiveObject *>(record.object);
                if (!unit) {
                    std::cerr << "--near: no unit " << options.near << std::endl;
                } else {
                    // 40 m away on the side of the player's start, 10 m higher, looking at the unit
                    unit->enable();
                    const Vector3D center = unit->center();
                    Vector2D away = Vector2D(scenario->player()->position() - center);
                    away = glm::length(away) > 1 ? glm::normalize(away) : Vector2D(0, -1);
                    Vector3D position = center + Vector3D(away*40.0f, 10.0f);
                    position.z = std::max(position.z, scenario->surface().heightAt(position.x, position.y) + 10.0f);
                    scenario->player()->place(position, center - position);
                }
            }
        };
        hud->eventRestart() += startScenario;
        hud->setDebugOverlay(options.debugOverlay);
        hud->load(boat.get());
        startScenario();
        m_window.setRenderer(hud.get());
    }

    std::vector<int> destroy = options.destroy;
    auto screenshots = options.screenshots;
    if (headless && !screenshots.empty()) {
        std::cerr << "--screenshot needs a window, ignored with --headless" << std::endl;
        screenshots.clear();
    }
    float nextTrace = 0;
    auto keys = options.keys;
    auto clicks = options.clicks;
    QElapsedTimer timer;
    timer.start();
    // test times are mission seconds: simulated without a window, real time with one, unless the
    // mission advances by a fixed step per frame
    float simulatedTime = 0;
    const float fixedStep = options.fixedStep && !headless && hud ? 0.02f : 0;
    hud::HUD::setFixedStep(fixedStep);
    auto elapsed = [&]() { return headless ? simulatedTime : fixedStep > 0 ? hud->frames()*fixedStep : timer.elapsed()/1000.0f; };

    if (headless) {
        sfx::SoundSystem::get()->setVolume(0);
    } else {
        // automated runs (with a time limit) shouldn't take the focus (on macOS see also main())
        if (options.timeout > 0)
            // what Qt::WA_ShowWithoutActivating sets for a widget
            m_window.setProperty("_q_showWithoutActivating", true);
        m_window.show();
    }
    while (running && (headless || m_window.isVisible())) {
        if (headless) {
            qApp->processEvents(QEventLoop::AllEvents);
            if (scenario)
                hud->step(0.02f);
            simulatedTime += 0.02f;
        } else {
            // block until the next event (at least the 20ms frame timer) instead of spinning
            qApp->processEvents(QEventLoop::WaitForMoreEvents);
        }
        util::DeferredDeletable::clear();

        if (scenario && !destroy.empty() && elapsed() >= 1) {
            for (const int id : destroy) {
                bool found = false;
                if (id == scenario->playerId()) {
                    scenario->player()->destroy();
                    found = true;
                }
                for (const auto &record : scenario->objectRecords()) {
                    fight::ActiveObject *object = dynamic_cast<fight::ActiveObject *>(record.object);
                    if (record.id == id && object) {
                        // a hit by the player strong enough to destroy it, so "attacked" fires as well
                        object->damage(object->kineticShield() + 1, 0, object->position(), scenario->player());
                        found = true;
                    }
                }
                // areas and nav points are reached instead
                if (!found)
                    found = scenario->conditionManager().signal(id);
                if (!found)
                    std::cerr << "--destroy: no unit " << id << std::endl;
            }
            destroy.clear();
        }
        // keys and screenshots work in the main menu too, at seconds since the start
        for (auto it = keys.begin(); it != keys.end(); ) {
            // a key's release is due at time + hold; hold < 0 marks a key that is already pressed
            const bool pressed = it->hold < 0;
            if (elapsed() < it->time) {
                ++it;
                continue;
            }
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
            const int key = it->key[0].key();
            const Qt::KeyboardModifiers modifiers = it->key[0].keyboardModifiers();
#else
            const int key = it->key[0] & ~Qt::KeyboardModifierMask;
            const Qt::KeyboardModifiers modifiers = Qt::KeyboardModifiers(it->key[0] & Qt::KeyboardModifierMask);
#endif
            const String text = key < 0x80 ? String(QChar(key)).toLower() : String();
            if (!pressed)
                QCoreApplication::postEvent(&m_window, new QKeyEvent(QEvent::KeyPress, key, modifiers, text));
            if (pressed || it->hold == 0) {
                QCoreApplication::postEvent(&m_window, new QKeyEvent(QEvent::KeyRelease, key, modifiers, text));
                it = keys.erase(it);
            } else {
                it->time += it->hold;
                it->hold = -1;
                ++it;
            }
        }
        if (scenario) {
            for (const auto &record : scenario->objectRecords()) {
                if (std::find(options.fire.begin(), options.fire.end(), record.id) == options.fire.end())
                    continue;
                if (fight::ActiveObject *unit = dynamic_cast<fight::ActiveObject *>(record.object)) {
                    unit->setTrigger(true);
                    unit->aimAt(scenario->player()->position());
                }
            }
        }
        if (scenario && !options.trace.empty() && scenario->time() >= nextTrace) {
            nextTrace = std::floor(scenario->time()) + 1;
            const Vector3D &scale = scenario->surface().scale();
            for (const int id : options.trace) {
                if (id == scenario->playerId()) {
                    const Vector3D pos = scenario->player()->position();
                    const Vector3D current = scenario->current(pos);
                    const fight::Player *player = scenario->player();
                    const fight::HeadMotion &head = scenario->headMotion();
                    std::cout << String("%1 t%2 player: cell %3 %4 h%5 %6 km/h current %7 %8 %9 shields %10 %11 %12 %13 head %15 cm %16 deg%14")
                        .arg(options.scenario).arg(int(scenario->time()))
                        .arg(pos.x/scale.x, 0, 'f', 1).arg(pos.y/scale.y, 0, 'f', 1)
                        .arg(int(scenario->height())).arg(int(scenario->player()->speed()))
                        .arg(current.x, 0, 'f', 1).arg(current.y, 0, 'f', 1).arg(current.z, 0, 'f', 1)
                        .arg(player->shield(0)).arg(player->shield(1)).arg(player->shield(2)).arg(player->shield(3))
                        .arg(scenario->playerDestroyed() ? " destroyed" : "")
                        .arg(glm::length(head.offset())*100, 0, 'f', 1).arg(glm::degrees(glm::length(head.angles())), 0, 'f', 1) << std::endl;
                }
                for (const auto &record : scenario->objectRecords()) {
                    if (record.id != id)
                        continue;
                    const Vector3D pos = record.object->position();
                    String line = String("%1 t%2 %3: cell %4 %5 h%6")
                        .arg(options.scenario).arg(int(scenario->time())).arg(id)
                        .arg(pos.x/scale.x, 0, 'f', 1).arg(pos.y/scale.y, 0, 'f', 1)
                        .arg(int(pos.z - scenario->surface().heightAt(pos.x, pos.y)));
                    if (!record.object->isEnabled())
                        line += " disabled";
                    if (fight::ActiveObject *unit = dynamic_cast<fight::ActiveObject *>(record.object)) {
                        line += String(" %1 km/h shield %2").arg(int(glm::length(unit->velocity())*3.6f)).arg(unit->kineticShield());
                        if (unit->eventAttack()->isCompleted())
                            line += " attacked";
                        if (unit->eventDestroy()->isCompleted())
                            line += " destroyed";
                    }
                    if (const fight::ActiveObject *unit = dynamic_cast<const fight::ActiveObject *>(record.object)) {
                        if (unit->orders() && unit->orders()->currentSlot())
                            line += String(" order %1 %2").arg(unit->orders()->current()).arg(unit->orders()->state());
                        else if (unit->orders())
                            line += " orders done";
                    }
                    std::cout << line << std::endl;
                }
            }
        }
        for (auto it = clicks.begin(); it != clicks.end(); ) {
            if (elapsed() < it->time || !m_window.renderer()) {
                ++it;
                continue;
            }
            // the mouse moves there first, so the button under it is highlighted as for a player
            const QPointF pos = m_window.renderer()->imageToScreen(it->pos);
            const QPointF global = m_window.mapToGlobal(pos.toPoint());
            QCoreApplication::postEvent(&m_window, new QMouseEvent(QEvent::MouseMove, pos, global, Qt::NoButton, Qt::NoButton, Qt::NoModifier));
            QCoreApplication::postEvent(&m_window, new QMouseEvent(QEvent::MouseButtonPress, pos, global, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier));
            QCoreApplication::postEvent(&m_window, new QMouseEvent(QEvent::MouseButtonRelease, pos, global, Qt::LeftButton, Qt::NoButton, Qt::NoModifier));
            it = clicks.erase(it);
        }
        for (auto it = screenshots.begin(); it != screenshots.end(); ) {
            if (elapsed() < it->time) {
                ++it;
                continue;
            }
            m_window.saveScreenshot(it->file);
            it = screenshots.erase(it);
        }
        if (scenario && result == ExitAborted && scenario->conditionManager().failed())
            result = ExitFailed;
        // test mode doesn't wait for the autopilot key
        if (scenario && result == ExitAborted && scenario->conditionManager().autopilot())
            finish(ExitSuccess);
        if (options.timeout > 0 && elapsed() >= options.timeout)
            finish(result == ExitAborted ? ExitTimeout : result);
    }

    if (scenario) {
        int enabled = 0;
        for (const auto &record : scenario->objectRecords())
            if (record.object->isEnabled())
                enabled++;
        static const char *results[] = { "success", "failed", "aborted", "timeout", "died" };
        std::cout << options.scenario << ": " << results[result]
                  << " after " << elapsed() << " s, " << enabled << "/" << scenario->objectRecords().size()
                  << " units enabled, " << scenario->radio().size() << " radio messages, "
                  << scenario->torpedoesLaunched() << " torpedoes" << std::endl;
    }

    return result;
}

int Vertigo::check(const StringList &names)
{
    StringList scenarios = names;
    if (scenarios.isEmpty()) {
        const QDir dir(util::DataPath::resolve("vfx:scenario"));
        foreach (const QString &file, dir.entryList(QStringList() << "*.des", QDir::Files, QDir::Name))
            scenarios << String(QFileInfo(file).completeBaseName()).toLower();
    }

    QTextStream out(stdout);
    QMap<String, int> totals;
    int objects = 0;
    for (const String &name : scenarios) {
        fight::Scenario scenario(name);
        QMap<String, int> counts;
        for (const String &issue : scenario.loadReport())
            counts[issue]++;

        out << name << ": " << scenario.objectRecords().size() << " objects\n";
        for (auto it = counts.cbegin(); it != counts.cend(); ++it) {
            out << "  " << it.value() << "x " << it.key() << "\n";
            // object ids differ between missions, so don't tell them apart in the totals
            String kind = it.key();
            if (kind.startsWith("condition: "))
                kind.replace(QRegularExpression("\\d+"), "#");
            totals[kind] += it.value();
        }
        objects += scenario.objectRecords().size();
        out.flush();
    }

    // totals per kind of issue, most frequent first
    QMultiMap<int, String> byCount;
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

void Vertigo::startGame(const String &name)
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

void Vertigo::loadGame(const String &name)
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
