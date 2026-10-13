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
#include "txt/desfile.h"

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <cmath>


namespace fight {


namespace {

float random()
{
    return qrand()/float(RAND_MAX);
}

// The data has no turn rates for the player's turrets; the turret models (TURRETn.DES) mostly turn
// at 1.5 rad/s, so they do too. A shot leaves once the turret is within this angle of its aim.
const float TurnRate = 1.5f;
const float AimTolerance = 0.035f;

}


PlayerTurret::PlayerTurret(Scenario *scenario, Player *player, int gunItem, int softwareItem, const Vector3D &mount, float defect) :
    m_scenario(scenario),
    m_player(player),
    m_gun(scenario, gunItem - 3073, player),
    m_mount(mount),
    m_order(Order::Sync),
    m_state(State::Sync),
    m_target(nullptr),
    m_lockTime(0),
    m_nextScan(0),
    m_shots(0),
    m_volleyEnd(0),
    m_aim(0, 1, 0)
{
    m_gun.addMuzzles(Vector3D(0), gunItem);
    m_gun.setDefect(defect);

    // items 7169-7177 are Software0-8: Claw (torpedoes), Punisher (vessels), Eraser (both); 9 is "No Software"
    const int index = softwareItem >= 7169 && softwareItem <= 7177 ? softwareItem - 7169 : 9;
    txt::DesFile file("vfx:sobjects/globweap.des");
    file.setSection(QString("software%1").arg(index));
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
const QStringList& turretStateNames(bool brief)
{
    static QStringList shortNames, verboseNames;
    if (shortNames.isEmpty())
    {
        txt::DesFile file("vfx:sobjects/globweap.des");
        for (int i = 0; i < 9; i++)
        {
            file.setSection(QString("turretstate%1").arg(i));
            shortNames << file.value("short").toString();
            verboseNames << file.value("verbose").toString();
        }
    }
    return brief ? shortNames : verboseNames;
}

}


QString PlayerTurret::stateName() const
{
    return turretStateNames(true).value(int(m_state));
}


QString PlayerTurret::stateText() const
{
    return turretStateNames(false).value(int(m_state));
}


void PlayerTurret::setOrder(Order order)
{
    m_order = order;
    m_target = nullptr;
    m_targetIsTorpedo = false;
    m_lockTime = 0;
    m_shots = 0;
}


Vector3D PlayerTurret::mountPosition() const
{
    return m_player->position() + m_player->right()*m_mount.x + m_player->dir()*m_mount.y + m_player->up()*m_mount.z;
}


Vector3D PlayerTurret::toWorld(const Vector3D &local) const
{
    return m_player->right()*local.x + m_player->dir()*local.y + m_player->up()*local.z;
}


Vector3D PlayerTurret::toLocal(const Vector3D &world) const
{
    return Vector3D(glm::dot(world, m_player->right()), glm::dot(world, m_player->dir()), glm::dot(world, m_player->up()));
}


bool PlayerTurret::turnTo(const Vector3D &direction, float elapsedTime)
{
    const Vector3D wanted = glm::normalize(toLocal(direction));
    const float angle = std::acos(glm::clamp(glm::dot(m_aim, wanted), -1.0f, 1.0f));
    const float step = TurnRate*elapsedTime;
    if (angle <= step)
        m_aim = wanted;
    else
    {
        // rotate within the plane of both directions (any plane when they are opposite)
        Vector3D axis = glm::cross(m_aim, wanted);
        if (glm::length(axis) < 1e-4f)
            axis = glm::cross(m_aim, std::abs(m_aim.z) < 0.9f ? Vector3D(0, 0, 1) : Vector3D(1, 0, 0));
        axis = glm::normalize(axis);
        m_aim = glm::normalize(m_aim*std::cos(step) + glm::cross(axis, m_aim)*std::sin(step));
    }
    return angle - step < AimTolerance;
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
    return target->isEnabled() && !target->eventDestroy()->isCompleted();
}


// the nearest target the software looks for, within the gun's range
ActiveObject* PlayerTurret::search() const
{
    ActiveObject *nearest = nullptr;
    float nearestDistance = m_gun.range();
    auto consider = [&](ActiveObject *unit) {
        const float distance = glm::length(unit->center() - m_player->position());
        if (distance < nearestDistance)
        {
            nearest = unit;
            nearestDistance = distance;
        }
    };
    if (m_software.torpedoes)
        for (const auto &torpedo : m_scenario->torpedoes())
            if (!torpedo->isDone() && torpedo->owner() != m_player &&
                (torpedo->target() == m_player || (torpedo->owner() && m_scenario->isHostile(Scenario::PlayerFaction, torpedo->owner()->iff()))))
                consider(torpedo.get());
    if (m_software.vessels)
        for (const auto &record : m_scenario->objectRecords())
        {
            ActiveObject *unit = dynamic_cast<ActiveObject *>(record.object);
            const bool vessel = dynamic_cast<SimpleBoat *>(unit) || dynamic_cast<TurretBase *>(unit);
            if (vessel && isValid(unit) && m_scenario->isHostile(Scenario::PlayerFaction, unit->iff()))
                consider(unit);
        }
    return nearest;
}


void PlayerTurret::update(float elapsedTime, bool playerFiring)
{
    m_gun.update(elapsedTime);
    const float time = m_scenario->time();

    switch (m_order)
    {
    case Order::Stop:
        m_state = State::Stopped;
        turnTo(m_player->dir(), elapsedTime);
        return;

    case Order::Sync:
        m_state = State::Sync;
        if (turnTo(m_player->dir(), elapsedTime) && playerFiring && m_gun.state() == Gun::StateReady)
            m_gun.fire(mountPosition(), m_player->dir(), m_player->up(), m_player->right());
        return;

    case Order::Attack:
        {
            ActiveObject *locked = m_scenario->target().locked();
            if (locked != m_target)
            {
                m_target = locked;
                m_targetIsTorpedo = false;
                m_lockTime = 0;
            }
        }
        break;

    case Order::Search:
        if (time >= m_nextScan)
        {
            m_nextScan = time + m_software.scan;
            if (!isValid(m_target))
            {
                m_target = m_software.torpedoes || m_software.vessels ? search() : nullptr;
                m_targetIsTorpedo = isTorpedo(m_target);
                m_lockTime = 0;
            }
        }
        break;
    }

    if (!isValid(m_target))
    {
        m_target = nullptr;
        m_state = State::Scanning;
        turnTo(m_player->dir(), elapsedTime);
        return;
    }

    // lead the target; the software's accuracy decides how far off a shot may go
    const Vector3D mount = mountPosition();
    const float distance = glm::length(m_target->center() - mount);
    const float flight = m_gun.projectileSpeed() > 0 ? distance/m_gun.projectileSpeed() : 0;
    const Vector3D aim = glm::normalize(m_target->center() + m_target->velocity()*flight + m_aimError - mount);
    const bool aligned = turnTo(aim, elapsedTime);
    if (m_lockTime < m_software.lock)
    {
        m_lockTime += elapsedTime;
        m_state = State::Locking;
        return;
    }
    if (time < m_volleyEnd)
    {
        m_state = State::Volley;
        return;
    }
    if (distance > m_gun.range() || !aligned)
    {
        m_state = State::Locking;
        return;
    }
    const Vector3D barrel = toWorld(m_aim);
    Vector3D right = glm::cross(barrel, m_player->up());
    right = glm::length(right) > 1e-3f ? glm::normalize(right) : m_player->right();
    if (m_gun.state() == Gun::StateReady)
    {
        m_gun.fire(mount, barrel, glm::cross(right, barrel), right);
        const float error = (1 - m_software.hit)*6.0f*random();
        const float angle = random()*glm::two_pi<float>();
        m_aimError = Vector3D(error*std::cos(angle), error*std::sin(angle), 0);
        if (++m_shots >= m_software.volleyShots)
        {
            m_shots = 0;
            m_volleyEnd = time + m_software.volleyDelay;
        }
    }
    m_state = m_gun.state() == Gun::StateJammed ? State::Jammed : m_gun.state() == Gun::StateEmpty ? State::Empty :
              m_gun.state() == Gun::StateRearming ? State::Rearming : State::Firing;
}


} // namespace fight
