/***************************************************************************
 *  Copyright (C) 2026  Andreas Hanft                                      *
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

#include "playerturret.h"
#include "player.h"
#include "fight/scenario.h"
#include "fight/objects/turretbase.h"
#include "fight/weapons/torpedo.h"
#include "sfx/samplemap.h"
#include "txt/desfile.h"
#include "util/random.h"

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <algorithm>
#include <cmath>


namespace fight {


namespace {

// uniform in [-1, 1]
float spread()
{
    return 2.0f*util::random()/float(RAND_MAX) - 1.0f;
}

}


PlayerTurret::PlayerTurret(Scenario *scenario, Player *player, int gunItem, int softwareItem, const Vector3D &mount, int side, float defect) :
    m_scenario(scenario),
    m_player(player),
    m_gun(scenario, gunItem - 3073, player),
    m_mount(mount),
    m_side(side),
    m_order(Order::Search),
    m_state(State::Scanning),
    m_target(nullptr),
    m_attackTarget(nullptr),
    m_lockEnd(0),
    m_nextScan(0),
    m_volleyLeft(0),
    m_volleyEnd(0)
{
    m_gun.addMuzzles(Vector3D(0), gunItem);
    m_gun.setDefect(defect);

    // items 7169-7177 are Software0-8: Claw V1.6-V3.6 (torpedoes), Punisher V1.0-V2.2 (vessels),
    // Eraser V0.8-V1.4 (both); 9 is "No Software"
    const int index = softwareItem >= 7169 && softwareItem <= 7177 ? softwareItem - 7169 : 9;
    txt::DesFile file("vfx:sobjects/globweap.des");
    file.setSection(String("software%1").arg(index));
    m_software.shortName = file.value("short").toString();
    m_software.hit = file.value("hit", 0.8).toFloat();
    m_software.scan = file.value("scan", 0.5).toFloat();
    m_software.lock = file.value("lock", 2).toFloat();
    m_software.volleyShots = file.value("vollsho", 7).toInt();
    m_software.volleyDelay = file.value("volldel", 1).toFloat();
    m_software.torpedoes = index <= 2 || (index >= 6 && index <= 8);
    m_software.vessels = index >= 3 && index <= 8;
}


namespace {

// GLOBWEAP [TurretStateN]: short (true) or verbose names
const StringList& turretStateNames(bool brief)
{
    static StringList shortNames, verboseNames;
    if (shortNames.isEmpty())
    {
        txt::DesFile file("vfx:sobjects/globweap.des");
        for (int i = 0; i < 9; i++)
        {
            file.setSection(String("turretstate%1").arg(i));
            shortNames << file.value("short").toString();
            verboseNames << file.value("verbose").toString();
        }
    }
    return brief ? shortNames : verboseNames;
}

}


// a jammed gun shows as such, whatever the turret is doing
String PlayerTurret::stateName() const
{
    return turretStateNames(true).value(int(m_gun.state() == Gun::StateJammed ? State::Jammed : m_state));
}


String PlayerTurret::stateText() const
{
    return turretStateNames(false).value(int(m_gun.state() == Gun::StateJammed ? State::Jammed : m_state));
}


bool PlayerTurret::setOrder(Order order)
{
    if ((order == Order::Attack || order == Order::Search) && !hasSoftware())
        return false;
    m_order = order;
    release();
    switch (order)
    {
    case Order::Stop: m_state = State::Stopped; break;
    case Order::Sync: m_state = State::Sync; break;
    case Order::Attack:
    case Order::Search:
        // it searches at once
        m_attackTarget = order == Order::Attack ? m_scenario->target().locked() : nullptr;
        m_state = State::Scanning;
        m_nextScan = m_scenario->time();
        break;
    }
    return true;
}


void PlayerTurret::release()
{
    m_target = nullptr;
    m_targetIsTorpedo = false;
}


Vector3D PlayerTurret::mountPosition() const
{
    return m_player->position() + m_player->right()*m_mount.x + m_player->dir()*m_mount.y + m_player->up()*m_mount.z;
}


bool PlayerTurret::isTorpedo(const ActiveObject *target) const
{
    // compared by address only: a torpedo that has been removed must not be touched
    for (const auto &torpedo : m_scenario->torpedoes())
        if (torpedo.get() == target)
            return true;
    return false;
}


bool PlayerTurret::isValid(ActiveObject *target) const
{
    if (!target || target == m_player)
        return false;
    for (const auto &torpedo : m_scenario->torpedoes())
        if (torpedo.get() == target)
            return !torpedo->isDone() && torpedo->owner() != m_player;
    // not a torpedo in the water: a unit (units are never deleted during a mission), or a torpedo that is gone
    if (m_targetIsTorpedo && target == m_target)
        return false;
    return target->isEnabled() && !target->isDying();
}


// the turret covers the half above the boat (side 0) or below it (side 1)
bool PlayerTurret::onSide(const ActiveObject *target) const
{
    const float above = glm::dot(target->center() - m_player->position(), m_player->up());
    return m_side == 0 ? above >= 0 : above <= 0;
}


bool PlayerTurret::inSight(const ActiveObject *target) const
{
    Vector3D position, normal;
    return !m_scenario->surface().testCollision(m_player->center(), target->center(), 0.0f, position, normal);
}


bool PlayerTurret::torpedoAfterPlayer() const
{
    for (const auto &torpedo : m_scenario->torpedoes())
        if (!torpedo->isDone() && torpedo->target() == m_player)
            return true;
    return false;
}


// the nearest torpedo after the player (Claw, Eraser first), else the nearest unit attacking the
// player (Punisher, Eraser), else the target of an attack order; each on the turret's side, in
// sight and a contact of the sonar
ActiveObject* PlayerTurret::search() const
{
    auto contact = [this](const ActiveObject *object) {
        return std::any_of(m_scenario->sonar().begin(), m_scenario->sonar().end(), [object](const Sonar::SonarEntry &entry) { return entry.object == object; });
    };
    auto usable = [&](ActiveObject *object) {
        return isValid(object) && onSide(object) && inSight(object) && contact(object);
    };
    auto nearer = [this](ActiveObject *a, ActiveObject *b) {
        return !b || glm::length(a->center() - m_player->position()) < glm::length(b->center() - m_player->position());
    };

    ActiveObject *nearest = nullptr;
    if (m_software.torpedoes)
        for (const auto &torpedo : m_scenario->torpedoes())
            if (torpedo->target() == m_player && usable(torpedo.get()) && nearer(torpedo.get(), nearest))
                nearest = torpedo.get();
    if (!nearest && m_software.vessels)
        for (ActiveObject *unit : m_scenario->playerAttackerList())
            if (!isTorpedo(unit) && usable(unit) && nearer(unit, nearest))
                nearest = unit;
    if (!nearest && m_attackTarget && usable(m_attackTarget))
        nearest = m_attackTarget;
    return nearest;
}


// whether the turret stays on its target: on its side and in sight, and a vessel only as long as
// the software allows (Claw and Eraser leave it for incoming torpedoes, Punisher keeps only units
// attacking the player, Eraser such units once any attack)
bool PlayerTurret::keeps(ActiveObject *target) const
{
    if (!onSide(target) || !inSight(target))
        return false;
    if (m_targetIsTorpedo)
        return true;
    const std::vector<ActiveObject*> &attackers = m_scenario->playerAttackerList();
    const bool attacking = std::find(attackers.begin(), attackers.end(), target) != attackers.end();
    if (!m_software.vessels)
        return !torpedoAfterPlayer();
    if (!m_software.torpedoes)
        return attacking;
    return !torpedoAfterPlayer() && (attackers.empty() || attacking);
}


// a shot at a point scattered round the target by the software's accuracy, led for the target's
// motion; nothing leaves when the shot can't catch up
void PlayerTurret::fireAt(ActiveObject *target)
{
    const Vector3D dim = target->box().dim();
    const float size = std::max(dim.x, std::max(dim.y, dim.z));
    const bool vessel = m_targetIsTorpedo || (dynamic_cast<SimpleBoat *>(target) && !static_cast<SimpleBoat *>(target)->isGroundVehicle());
    const float scatter = (1 - m_software.hit)*size*(vessel ? 1.0f : 0.5f);
    const Vector3D point = target->center() + scatter*Vector3D(spread(), spread(), spread());

    const Vector3D mount = mountPosition();
    const Vector3D offset = point - mount;
    const Vector3D velocity = target->velocity();
    const float speed = m_gun.projectileSpeed();
    // |offset + velocity*t| = speed*t
    const float a = glm::dot(velocity, velocity) - speed*speed;
    const float b = 2*glm::dot(offset, velocity);
    const float c = glm::dot(offset, offset);
    float t;
    if (std::abs(a) < 1e-6f)
        t = b < 0 ? -c/b : -1;
    else
    {
        const float discriminant = b*b - 4*a*c;
        if (discriminant < 0)
            return;
        const float root = std::sqrt(discriminant);
        const float t1 = (-b - root)/(2*a), t2 = (-b + root)/(2*a);
        t = t1 > 0 && (t1 < t2 || t2 <= 0) ? t1 : t2;
    }
    if (t <= 0)
        return;
    const Vector3D dir = glm::normalize(offset + velocity*t);
    Vector3D right = glm::cross(dir, m_player->up());
    right = glm::length(right) > 1e-3f ? glm::normalize(right) : m_player->right();
    m_gun.fire(mount, dir, glm::cross(right, dir), right);
}


void PlayerTurret::update(float elapsedTime, bool playerFiring)
{
    m_gun.update(elapsedTime);
    const float time = m_scenario->time();
    const bool ready = m_gun.state() == Gun::StateReady;

    switch (m_order)
    {
    case Order::Stop:
        return;

    case Order::Sync:
        // each pull of the main gun's trigger fires the turret's own gun too, straight ahead and
        // from all its barrels at once
        if (m_state == State::Rearming && ready)
            m_state = State::Sync;
        if (playerFiring && ready)
        {
            m_gun.fire(mountPosition(), m_player->dir(), m_player->up(), m_player->right(), true);
            m_state = State::Rearming;
        }
        return;

    case Order::Attack:
    case Order::Search:
        break;
    }

    if (m_gun.usesAmmo() && m_gun.ammo() <= 0)
    {
        release();
        m_state = State::Empty;
        return;
    }

    if (m_target && (!isValid(m_target) || !keeps(m_target)))
    {
        release();
        m_state = State::Scanning;
        m_nextScan = time + m_software.scan;
    }

    if (!m_target)
    {
        m_state = State::Scanning;
        if (!hasSoftware() || time < m_nextScan)
            return;
        m_target = search();
        if (!m_target)
        {
            m_nextScan = time + m_software.scan;
            return;
        }
        // the lock-on starts with the turret's whirr
        m_targetIsTorpedo = isTorpedo(m_target);
        m_lockEnd = time + m_software.lock;
        m_volleyLeft = m_software.volleyShots;
        m_state = State::Locking;
        sfx::SampleMap::get(sfx::Sample::Engine_Turret).play();
        return;
    }

    if (m_state == State::Locking && time < m_lockEnd)
        return;
    if (m_state == State::Volley)
    {
        if (time < m_volleyEnd)
            return;
        m_volleyLeft = m_software.volleyShots;
    }
    m_state = ready ? State::Firing : State::Rearming;
    if (!ready)
        return;

    // a volley is volleyShots + 1 shots, one barrel each, then a pause
    fireAt(m_target);
    m_state = State::Rearming;
    if (m_volleyLeft-- <= 0)
    {
        m_volleyEnd = time + m_software.volleyDelay;
        m_state = State::Volley;
    }
}


} // namespace fight
