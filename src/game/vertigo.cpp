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
#include "game/mission.h"
#include "fight/scenario.h"
#include "fight/objects/activeobject.h"
#include "fight/boats/simpleboat.h"
#include "fight/boats/player.h"
#include "fight/surface/surface.h"
#include "fight/orders.h"
#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>
#include <glm/gtc/constants.hpp>
#include <cmath>
#include <algorithm>
#include "txt/desfile.h"
#include "txt/parse.hpp"
#include "util/datapath.h"
#include "util/path.hpp"

#include "util/timer.hpp"
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_init.h>
#include <iostream>

namespace game {

namespace {

// --pad: a button of the virtual game controller (the triggers are axes, at their minimum when
// let go)
void setPadButton(SDL_Joystick *pad, input::Button button, bool down)
{
    if (button == input::Button_LeftTrigger || button == input::Button_RightTrigger) {
        SDL_SetJoystickVirtualAxis(pad, button == input::Button_LeftTrigger ? SDL_GAMEPAD_AXIS_LEFT_TRIGGER : SDL_GAMEPAD_AXIS_RIGHT_TRIGGER,
                                   down ? SDL_JOYSTICK_AXIS_MAX : SDL_JOYSTICK_AXIS_MIN);
        SDL_UpdateJoysticks();
        return;
    }
    static const SDL_GamepadButton buttons[] = {
        SDL_GAMEPAD_BUTTON_SOUTH, SDL_GAMEPAD_BUTTON_EAST, SDL_GAMEPAD_BUTTON_WEST, SDL_GAMEPAD_BUTTON_NORTH,
        SDL_GAMEPAD_BUTTON_BACK, SDL_GAMEPAD_BUTTON_START, SDL_GAMEPAD_BUTTON_LEFT_STICK, SDL_GAMEPAD_BUTTON_RIGHT_STICK,
        SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,
        SDL_GAMEPAD_BUTTON_DPAD_UP, SDL_GAMEPAD_BUTTON_DPAD_DOWN, SDL_GAMEPAD_BUTTON_DPAD_LEFT, SDL_GAMEPAD_BUTTON_DPAD_RIGHT,
    };
    SDL_SetJoystickVirtualButton(pad, buttons[button], down);
    // its event now, so the buttons of LB+A come in this order and not in SDL's
    SDL_UpdateJoysticks();
}

// a mission of test mode by its scenario file's name (s00101, in any case); an error message if
// there is none
bool knownScenario(const String &name)
{
    if (util::exists(util::DataPath::resolve(String("vfx:scenario/%1.des").arg(name))))
        return true;
    std::cerr << "Unknown mission " << name << " (a scenario file of vfx/scenario, e.g. s00101)" << std::endl;
    return false;
}

} // namespace

Vertigo::Vertigo(const Options &options) :
    m_options(options),
    // automated runs (with a time limit) shouldn't take the focus (on macOS see also main())
    m_window(options.timeout <= 0),
    // automated runs (headless, with a time limit, or checking missions) are silent
    m_soundSystem(options.headless || options.timeout > 0 || options.check),
    m_mainMenu(
       [this](String name) { startGame(name); },
       [this](String name) { loadGame(name); },
       [this](int index) { startTraining(index); },
       [this](int boatType) { startOpenMission(boatType); },
       [this]() { m_window.close(); }
    )
{
}

Vertigo::~Vertigo()
{
    // before the window and its context
    m_window.setRenderer(nullptr);
    m_scenario.reset();
    m_hud.reset();
    if (m_virtualPad) {
        const SDL_JoystickID id = SDL_GetJoystickID(m_virtualPad);
        SDL_CloseJoystick(m_virtualPad);
        SDL_DetachVirtualJoystick(id);
    }
}

int Vertigo::start()
{
    if (m_options.check)
        return check(m_options.checkScenarios);

    // test mode: closing the window counts as aborting the mission
    m_result = m_options.scenario.isEmpty() ? ExitSuccess : ExitAborted;
    m_headless = m_options.headless && !m_options.scenario.isEmpty();

    if (m_options.scenario.isEmpty()) {
        m_window.setRenderer(&m_mainMenu);
    } else {
        if (!knownScenario(m_options.scenario))
            return ExitUsage;
        txt::DesFile desfile("dat:story/ch1.des");
        m_boat.reset(new game::Boat(desfile));
        if (m_options.boat != 0) {
            if (m_options.boat < 2049 || m_options.boat > 2052) {
                std::cerr << "Unknown boat type " << m_options.boat << " (2049-2052)" << std::endl;
                return ExitUsage;
            }
            m_boat->upgrade(m_options.boat);
        }
        for (const auto &item : m_options.equipment) {
            if (Items::all().count(item.second) == 0) {
                std::cerr << "Unknown item " << item.second << " (see --list-items)" << std::endl;
                return ExitUsage;
            }
            // DEFE takes the items without a mounting: armour, skin, sensor, buzzers, the Fixer
            if (item.first != "DEFE" && !m_boat->getMounting(item.first)) {
                std::cerr << "The boat has no mounting " << item.first << std::endl;
                return ExitUsage;
            }
            m_boat->buy(item.second, item.first);
        }

        m_hud.reset(new hud::HUD());
        m_hud->eventSuccess() += [this]() { finish(ExitSuccess); };
        m_hud->eventAbort() += [this]() { finish(ExitAborted); };
        m_hud->eventFailure() += [this]() { finish(ExitFailed); };
        m_hud->eventDied() += [this]() { finish(ExitDied); };
        // there is no saved game in test mode
        m_hud->eventLoad() += [this]() { finish(ExitAborted); };
        m_hud->eventRestart() += [this]() { startScenario(); };
        m_hud->setDebugOverlay(m_options.debugOverlay);
        m_hud->load(m_boat.get());
        startScenario();
        m_window.setRenderer(m_hud.get());
    }

    m_destroy = m_options.destroy;
    m_collapse = m_options.collapse;
    m_screenshots = m_options.screenshots;
    if (m_headless && !m_screenshots.empty()) {
        std::cerr << "--screenshot needs a window, ignored with --headless" << std::endl;
        m_screenshots.clear();
    }
    m_keys = m_options.keys;
    m_clicks = m_options.clicks;
    m_pad = m_options.pad;
    m_touch = m_options.touch;
    for (size_t i = 0; i < m_touch.size(); i++)
        m_touch[i].id = (long long)i + 1;
    if (!m_pad.empty()) {
        // a virtual game controller, the only one that counts, whose input also comes without the
        // focus (as automated runs don't take it)
        SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
        SDL_VirtualJoystickDesc desc;
        SDL_INIT_INTERFACE(&desc);
        desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
        desc.naxes = SDL_GAMEPAD_AXIS_COUNT;
        desc.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
        desc.name = "Vertigo --pad";
        const SDL_JoystickID id = SDL_InitSubSystem(SDL_INIT_GAMEPAD) ? SDL_AttachVirtualJoystick(&desc) : 0;
        m_virtualPad = id ? SDL_OpenJoystick(id) : nullptr;
        if (m_virtualPad) {
            m_window.setOnlyGamepad(id);
        } else {
            std::cerr << "--pad: no virtual game controller: " << SDL_GetError() << std::endl;
            m_pad.clear();
        }
    }
    m_timer.start();
    // test times are mission seconds: simulated without a window, real time with one, unless the
    // mission advances by a fixed step per frame
    m_fixedStep = m_options.fixedStep && !m_headless && m_hud ? 0.02f : 0;
    hud::HUD::setFixedStep(m_fixedStep);

    if (!m_headless)
        m_window.show();
    return -1;
}

void Vertigo::startScenario()
{
    m_scenario.reset(new fight::Scenario(m_options.scenario));
    // the briefing of the mission that plays the scenario (s00101 of m00101, strai3 of mtrai3)
    const String mission = "m" + m_options.scenario.toLower().mid(1);
    if (util::exists(util::DataPath::resolve(String("dat:mission/%1.des").arg(mission)))) {
        const Mission briefing(mission, -1);
        m_hud->setBriefing(briefing.textB(), briefing.textP(), briefing.textS());
    }
    m_hud->start(m_scenario.get());
    for (const Options::Drive &drive : m_options.drive) {
        fight::SimpleBoat *unit = nullptr;
        for (const auto &record : m_scenario->objectRecords())
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
    if (m_options.start.x >= 0) {
        // at the cell's centre, as the scenario places the player; the scenario's height
        // above the ground and heading unless given
        const Options::Start &start = m_options.start;
        fight::Player *player = m_scenario->player();
        fight::Surface &surface = m_scenario->surface();
        const Vector3D scale = surface.scale();
        const float above = start.hei >= 0 ? start.hei*scale.z : player->position().z - surface.heightAt(player->position().x, player->position().y);
        Vector3D position((start.x + 0.5f)*scale.x, (start.y + 0.5f)*scale.y, 0);
        position.z = surface.heightAt(position.x, position.y) + above;
        const float yaw = start.card*glm::quarter_pi<float>();
        player->place(position, start.card >= 0 ? Vector3D(std::sin(yaw), std::cos(yaw), 0) : player->dir());
    }
    if (m_options.near) {
        fight::ActiveObject *unit = nullptr;
        for (const auto &record : m_scenario->objectRecords())
            if (record.id == m_options.near)
                unit = dynamic_cast<fight::ActiveObject *>(record.object);
        if (!unit) {
            std::cerr << "--near: no unit " << m_options.near << std::endl;
        } else {
            // 40 m away on the side of the player's start, 10 m higher, looking at the unit
            unit->enable();
            const Vector3D center = unit->center();
            Vector2D away = Vector2D(m_scenario->player()->position() - center);
            away = glm::length(away) > 1 ? glm::normalize(away) : Vector2D(0, -1);
            Vector3D position = center + Vector3D(away*40.0f, 10.0f);
            position.z = std::max(position.z, m_scenario->surface().heightAt(position.x, position.y) + 10.0f);
            m_scenario->player()->place(position, center - position);
        }
    }
}

void Vertigo::finish(int code)
{
    m_result = code;
    m_running = false;
    m_window.close();
}

float Vertigo::elapsed() const
{
    return m_headless ? m_simulatedTime : m_fixedStep > 0 ? m_hud->frames()*m_fixedStep : m_timer.elapsed()/1000.0f;
}

void Vertigo::event(const SDL_Event &event)
{
    m_window.handle(event);
    // the sound rests in the background (on the iPad)
    if (event.type == SDL_EVENT_DID_ENTER_BACKGROUND || event.type == SDL_EVENT_WILL_ENTER_FOREGROUND)
        m_soundSystem.interrupt(sfx::SoundSystem::Background, event.type == SDL_EVENT_DID_ENTER_BACKGROUND);
}

int Vertigo::iterate()
{
    // what test runs make up comes first, as the platform's input would
    m_window.deliverPosted();
    if (m_headless) {
        if (m_scenario)
            m_hud->step(0.02f);
        m_simulatedTime += 0.02f;
    } else {
        m_window.render();
    }
    util::DeferredDeletable::clear();
    testFrame();

    if (m_running && (m_headless || m_window.isVisible()))
        return -1;
    if (m_scenario) {
        int enabled = 0;
        for (const auto &record : m_scenario->objectRecords())
            if (record.object->isEnabled())
                enabled++;
        static const char *results[] = { "success", "failed", "aborted", "timeout", "died" };
        std::cout << m_options.scenario << ": " << results[m_result]
                  << " after " << elapsed() << " s, " << enabled << "/" << m_scenario->objectRecords().size()
                  << " units enabled, " << m_scenario->radio().size() << " radio messages, "
                  << m_scenario->torpedoesLaunched() << " torpedoes" << std::endl;
    }

    return m_result;
}

// the test options, once a frame
void Vertigo::testFrame()
{
    if (m_scenario && !m_destroy.empty() && elapsed() >= 1) {
        for (const int id : m_destroy) {
            bool found = false;
            if (id == m_scenario->playerId()) {
                m_scenario->player()->destroy();
                found = true;
            }
            for (const auto &record : m_scenario->objectRecords()) {
                fight::ActiveObject *object = dynamic_cast<fight::ActiveObject *>(record.object);
                if (record.id == id && object) {
                    // a hit by the player strong enough to destroy it, so "attacked" fires as well
                    object->damage(object->kineticShield() + 1, 0, object->position(), m_scenario->player());
                    found = true;
                }
            }
            // areas and nav points are reached instead
            if (!found)
                found = m_scenario->conditionManager().signal(id);
            if (!found)
                std::cerr << "--destroy: no unit " << id << std::endl;
        }
        m_destroy.clear();
    }
    if (m_scenario && !m_collapse.empty() && elapsed() >= 1) {
        for (const int id : m_collapse) {
            bool found = false;
            for (const auto &record : m_scenario->objectRecords()) {
                fight::ActiveObject *object = dynamic_cast<fight::ActiveObject *>(record.object);
                if (record.id == id && object) {
                    object->damage(0, object->shockShield() + 1, object->position(), m_scenario->player());
                    found = true;
                }
            }
            if (!found)
                std::cerr << "--collapse: no unit " << id << std::endl;
        }
        m_collapse.clear();
    }
    // keys and screenshots work in the main menu too, at seconds since the start
    for (auto it = m_keys.begin(); it != m_keys.end(); ) {
        // a key's release is due at time + hold; hold < 0 marks a key that is already pressed
        const bool pressed = it->hold < 0;
        if (elapsed() < it->time) {
            ++it;
            continue;
        }
        const input::KeyEvent key(it->key, it->modifiers, it->key < 0x80 ? String(1, char(it->key)).toLower() : String());
        if (!pressed)
            m_window.post(Window::EventType::KeyPress, key);
        if (pressed || it->hold == 0) {
            m_window.post(Window::EventType::KeyRelease, key);
            it = m_keys.erase(it);
        } else {
            it->time += it->hold;
            it->hold = -1;
            ++it;
        }
    }
    // the fingers, each with its own id, as the platform's would come
    for (size_t i = 0; i < m_touch.size(); ) {
        Options::Touch &touch = m_touch[i];
        if (elapsed() < touch.time) {
            i++;
            continue;
        }
        const long long id = touch.id;
        const PointD size(m_window.width(), m_window.height());
        if (!touch.down) {
            touch.down = true;
            touch.start = touch.time;
            m_window.post(Window::EventType::TouchPress, input::TouchEvent(id, touch.from*size));
        }
        const float moved = std::min((elapsed() - touch.start)/0.1f, 1.0f);
        if (touch.to != touch.from)
            m_window.post(Window::EventType::TouchMove, input::TouchEvent(id, (touch.from + (touch.to - touch.from)*double(moved))*size));
        if (elapsed() >= touch.start + touch.hold) {
            m_window.post(Window::EventType::TouchRelease, input::TouchEvent(id, touch.to*size));
            m_touch.erase(m_touch.begin() + i);
            continue;
        }
        i++;
    }
    // the virtual controller's input reaches the game with SDL's next events, before the next frame
    for (auto it = m_pad.begin(); it != m_pad.end(); ) {
        if (elapsed() < it->time) {
            ++it;
            continue;
        }
        const bool down = !it->down;
        if (it->axis >= 0)
            SDL_SetJoystickVirtualAxis(m_virtualPad, it->axis, Sint16(down ? it->value*SDL_JOYSTICK_AXIS_MAX : 0));
        else if (down)
            for (const input::Button button : it->buttons)
                setPadButton(m_virtualPad, button, true);
        else
            for (auto button = it->buttons.rbegin(); button != it->buttons.rend(); ++button)
                setPadButton(m_virtualPad, *button, false);
        const float hold = it->hold >= 0 ? it->hold : it->axis >= 0 ? -1 : 0.1f;
        if (!down || hold < 0) {
            it = m_pad.erase(it);
        } else {
            it->time += hold;
            it->down = true;
            ++it;
        }
    }
    if (m_scenario) {
        for (const auto &record : m_scenario->objectRecords()) {
            if (std::find(m_options.fire.begin(), m_options.fire.end(), record.id) == m_options.fire.end())
                continue;
            if (fight::ActiveObject *unit = dynamic_cast<fight::ActiveObject *>(record.object)) {
                unit->setTrigger(true);
                unit->aimAt(m_scenario->player()->position());
            }
        }
    }
if (m_scenario && !m_options.trace.empty() && m_scenario->time() >= m_nextTrace) {
        m_nextTrace = std::floor(m_scenario->time()) + 1;
        const Vector3D &scale = m_scenario->surface().scale();
        for (const int id : m_options.trace) {
            if (id == m_scenario->playerId()) {
                const Vector3D pos = m_scenario->player()->position();
                const Vector3D current = m_scenario->current(pos);
                const fight::Player *player = m_scenario->player();
                const fight::HeadMotion &head = m_scenario->headMotion();
                std::cout << String("%1 t%2 player: cell %3 %4 h%5 pitch %20 %6 km/h current %7 %8 %9 shields %10 %11 %12 %13 head %15 cm %16 deg attackers %17 energy %18 shots %19%14")
                    .arg(m_options.scenario).arg(int(m_scenario->time()))
                    .arg(pos.x/scale.x, 0, 'f', 1).arg(pos.y/scale.y, 0, 'f', 1)
                    .arg(int(m_scenario->height())).arg(int(m_scenario->player()->speed()))
                    .arg(current.x, 0, 'f', 1).arg(current.y, 0, 'f', 1).arg(current.z, 0, 'f', 1)
                    .arg(player->shield(0)).arg(player->shield(1)).arg(player->shield(2)).arg(player->shield(3))
                    .arg(m_scenario->playerDestroyed() ? " destroyed" : "")
                    .arg(glm::length(head.offset())*100, 0, 'f', 1).arg(glm::degrees(glm::length(head.angles())), 0, 'f', 1)
                    .arg(m_scenario->playerAttackers()).arg(int(player->energy()*100)).arg(player->playerGun().shots()).arg(int(player->pitch())) << std::endl;
            }
            for (const auto &record : m_scenario->objectRecords()) {
                if (record.id != id)
                    continue;
                const Vector3D pos = record.object->position();
                String line = String("%1 t%2 %3: cell %4 %5 h%6")
                    .arg(m_options.scenario).arg(int(m_scenario->time())).arg(id)
                    .arg(pos.x/scale.x, 0, 'f', 1).arg(pos.y/scale.y, 0, 'f', 1)
                    .arg(int(pos.z - m_scenario->surface().heightAt(pos.x, pos.y)));
                if (!record.object->isEnabled())
                    line += " disabled";
                if (fight::ActiveObject *unit = dynamic_cast<fight::ActiveObject *>(record.object)) {
                    line += String(" %1 km/h shield %2").arg(int(glm::length(unit->velocity())*3.6f)).arg(unit->kineticShield());
                    if (unit->eventAttack()->isCompleted())
                        line += " attacked";
                    if (unit->eventDestroy()->isCompleted())
                        line += " destroyed";
                    else if (unit->eventParalyze()->isCompleted())
                        line += " paralysed";
                    if (unit->isAsleep())
                        line += " asleep";
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
    for (auto it = m_clicks.begin(); it != m_clicks.end(); ) {
        if (elapsed() < it->time || !m_window.renderer()) {
            ++it;
            continue;
        }
        // the mouse moves there first, so the button under it is highlighted as for a player
        const PointD pos = m_window.renderer()->imageToScreen(it->pos);
        m_window.post(Window::EventType::MouseMove, input::MouseEvent(pos, input::NoButton, input::NoButton));
        m_window.post(Window::EventType::MousePress, input::MouseEvent(pos, input::LeftButton, input::LeftButton));
        m_window.post(Window::EventType::MouseRelease, input::MouseEvent(pos, input::LeftButton, input::NoButton));
        it = m_clicks.erase(it);
    }
    for (auto it = m_screenshots.begin(); it != m_screenshots.end(); ) {
        if (elapsed() < it->time) {
            ++it;
            continue;
        }
        m_window.saveScreenshot(it->file);
        it = m_screenshots.erase(it);
    }
    if (m_scenario && m_result == ExitAborted && m_scenario->conditionManager().failed())
        m_result = ExitFailed;
    // test mode doesn't wait for the autopilot key
    if (m_scenario && m_result == ExitAborted && m_scenario->conditionManager().autopilot())
        finish(ExitSuccess);
    if (m_options.timeout > 0 && elapsed() >= m_options.timeout)
        finish(m_result == ExitAborted ? ExitTimeout : m_result);
}

int Vertigo::check(const StringList &names)
{
    StringList scenarios = names;
    if (scenarios.isEmpty()) {
        // *.des, in any case
        for (const std::string &file : util::files(util::DataPath::resolve("vfx:scenario"))) {
            const String name = String::fromUtf8(file);
            if (name.toLower().endsWith(".des"))
                scenarios << name.left(name.lastIndexOf('.')).toLower();
        }
    }
    for (const String &name : scenarios)
        if (!knownScenario(name))
            return ExitUsage;

    std::ostream &out = std::cout;
    std::map<String, int> totals;
    int objects = 0;
    for (const String &name : scenarios) {
        fight::Scenario scenario(name);
        std::map<String, int> counts;
        for (const String &issue : scenario.loadReport())
            counts[issue]++;

        out << name << ": " << scenario.objectRecords().size() << " objects\n";
        for (const auto &count : counts) {
            out << "  " << count.second << "x " << count.first << "\n";
            // object ids differ between missions, so don't tell them apart in the totals
            String kind = count.first;
            if (kind.startsWith("condition: "))
                kind = txt::replaceNumbers(kind, "#");
            totals[kind] += count.second;
        }
        objects += scenario.objectRecords().size();
        out.flush();
    }

    // totals per kind of issue, most frequent first
    // (equal counts in the order of the issues, as before)
    std::multimap<int, String> byCount;
    for (const auto &total : totals)
        byCount.emplace_hint(byCount.lower_bound(total.second), total.second, total.first);
    out << "\nTotal: " << scenarios.size() << " missions, " << objects << " objects\n";
    for (auto it = byCount.crbegin(); it != byCount.crend(); ++it)
        out << "  " << it->first << "x " << it->second << "\n";

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

    for (const Chapter::SavedGame &game : Chapter::savedGames()) {
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

void Vertigo::startTraining(int index)
{
    m_training.reset(new Training(index,
        [this](Renderer *renderer) { m_window.setRenderer(renderer); },
        [this]() { endTraining(); }
    ));
}

void Vertigo::endTraining()
{
    m_window.setRenderer(&m_mainMenu);
    m_training.release()->deleteLater();
}

void Vertigo::startOpenMission(int boatType)
{
    m_openMission.reset(new OpenMission(boatType,
        [this](Renderer *renderer) { m_window.setRenderer(renderer); },
        [this]() { endOpenMission(); }
    ));
    m_window.setRenderer(m_openMission.get());
}

void Vertigo::endOpenMission()
{
    m_window.setRenderer(&m_mainMenu);
    m_openMission.release()->deleteLater();
}

void Vertigo::introFinished()
{
    m_window.setRenderer(NULL);
    m_intro.release()->deleteLater();

    createChapter();
    m_chapter->loadChapter(1);
}

} // namespace game
