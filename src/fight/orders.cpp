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

#include "orders.h"
#include "scenario.h"
#include "boats/simpleboat.h"
#include "objects/activeobject.h"
#include "objects/turretbase.h"
#include "weapons/unitgun.h"
#include "util/random.h"

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/trigonometric.hpp>
#include <algorithm>
#include <cmath>


namespace fight {


namespace {

// a unit counts as having arrived within this horizontal distance (metres)
const float ArriveDistance = 20.0f;
const float EngageLeash = 2.5f;     // patrols give up a target this many sensor ranges away
// durations from this on mean "forever"
const int Forever = 100000;
// guns fire when they point at the target within this angle (radians)
const float AimTolerance = 0.06f;
// an ambush is sprung when its target comes this close (metres)
const float AmbushDistance = 300.0f;

float random()
{
    return util::random()/float(RAND_MAX);
}

float random(float min, float max)
{
    return min + random()*(max - min);
}

}


String OrderSlot::name(int type)
{
    static const char *names[] = { "none", "wait", "goto", "patrol", "attack", "escort", "ambush",
                                   "cardinalpatrol", "cardinalattack", "gotowitharrive", "gotowithinfiltrate", "explode" };
    return type >= 0 && type <= Explode ? names[type] : String::number(type);
}


bool OrderSlot::isImplemented(int type)
{
    return type != None;
}


// docs/ROADMAP.md, appendix J: 2 and 6 are the two quarter turns, 4 is about; 0, odd values and
// the frequent 10 are straight ahead. Which of 2 and 6 is left is still to be checked.
float OrderSlot::yaw() const
{
    switch (car)
    {
    case 2: return -glm::half_pi<float>();
    case 4: return glm::pi<float>();
    case 6: return glm::half_pi<float>();
    default: return 0;
    }
}


Orders::Orders(Scenario *scenario, ActiveObject *unit, const std::vector<OrderSlot> &orderSlots) :
    m_scenario(scenario),
    m_unit(unit),
    m_boat(dynamic_cast<SimpleBoat *>(unit)),
    m_slots(orderSlots),
    m_current(-1),
    m_startTime(0),
    m_pointHeight(0),
    m_pointTime(0),
    m_attack(Attack::Approach),
    m_attackEnd(0),
    m_ambushed(false),
    m_torpedoRun(false),
    m_torpedoTarget(nullptr),
    m_engaged(nullptr),
    m_nextScan(0),
    m_dockStart(-1),
    m_turretBusy(false),
    m_sweepYaw(0),
    m_nextSweep(0)
{
    if (!m_slots.empty())
        start(0);
}


void Orders::start(int slot)
{
    // the 8 slots are taken in turn, skipping empty ones, round and round; boats skip
    // CardinalPatrol and CardinalAttack too (docs/ROADMAP.md, appendix J)
    m_current = -1;
    for (int tried = 0; tried < int(m_slots.size()) && m_current < 0; tried++, slot++)
    {
        const int candidate = slot % int(m_slots.size());
        const int type = m_slots[candidate].type;
        if (type == OrderSlot::None || (m_boat && (type == OrderSlot::CardinalPatrol || type == OrderSlot::CardinalAttack)))
            continue;
        m_current = candidate;
    }
    m_startTime = m_scenario->time();
    m_attack = Attack::Approach;
    m_attackEnd = 0;
    m_ambushed = false;
    m_engaged = nullptr;
    endTorpedoRun();
    m_dockStart = -1;
    m_unit->setTrigger(false);
    m_scenario->disengagePlayer(m_unit);
    if (m_current >= 0 && (m_slots[m_current].type == OrderSlot::Patrol || m_slots[m_current].type == OrderSlot::Attack))
        pickPatrolPoint();
    if (m_current >= 0)
    {
        m_sweepYaw = m_slots[m_current].yaw();
        m_nextSweep = m_scenario->time() + random(0.5f, 2.5f);
    }
}


ActiveObject* Orders::engage(ActiveObject *target)
{
    if (target == m_scenario->player())
        return m_scenario->engagePlayer(m_unit) ? target : nullptr;
    m_scenario->disengagePlayer(m_unit);
    return target;
}


void Orders::next()
{
    start(m_current + 1);
}


txt::StringFight Orders::activity() const
{
    if (m_current < 0)
        return txt::Fight79;
    if (m_engaged)
        return txt::Fight77;
    switch (m_slots[m_current].type)
    {
    case OrderSlot::Attack:
    case OrderSlot::CardinalAttack:
        return txt::Fight77;
    case OrderSlot::Ambush:
        return m_ambushed ? txt::Fight77 : txt::Fight79;
    case OrderSlot::Patrol:
    case OrderSlot::CardinalPatrol:
        return txt::Fight80;
    case OrderSlot::Goto:
    case OrderSlot::GotoWithArrive:
        return txt::Fight82;
    case OrderSlot::Escort:
        return txt::Fight83;
    case OrderSlot::GotoWithInfiltrate:
        return txt::Fight84;
    case OrderSlot::Explode:
        return txt::Fight72;
    default:
        return txt::Fight79;
    }
}


ActiveObject* Orders::dockingTarget() const
{
    const OrderSlot *slot = currentSlot();
    return slot && slot->type == OrderSlot::GotoWithInfiltrate ? findUnit(slot->ene) : nullptr;
}


bool Orders::mayPause() const
{
    const OrderSlot *slot = currentSlot();
    if (slot && slot->type == OrderSlot::Explode)
        return false;
    return !m_turretBusy;
}


String Orders::state() const
{
    if (m_current < 0)
        return String();
    String text = OrderSlot::name(m_slots[m_current].type);
    const int type = m_slots[m_current].type;
    if (m_engaged)
        text += " engaging";
    if ((type == OrderSlot::Attack || (type == OrderSlot::Ambush && m_ambushed) || m_engaged) && m_attackEnd > 0)
        text += m_attack == Attack::Approach ? " approach" : m_attack == Attack::Attack ? " attack" : " break";
    if (m_unit->trigger())
        text += " firing";
    return text;
}


Vector3D Orders::cellCentre(int x, int y) const
{
    const Vector3D &scale = m_scenario->surface().scale();
    return Vector3D((x + 0.5f)*scale.x, (y + 0.5f)*scale.y, 0);
}


float Orders::altitude(const Vector3D &point, int height) const
{
    Surface &surface = m_scenario->surface();
    const Vector3D &position = m_unit->position();

    // the highest ground under the unit, a little ahead of it, and at the point once it is close
    Vector2D way = Vector2D(point) - Vector2D(position);
    const float distance = glm::length(way);
    if (distance > 0.1f)
        way /= distance;
    float ground = surface.heightAt(position.x, position.y);
    for (float ahead : { 30.0f, 60.0f })
    {
        const Vector2D p = Vector2D(position) + way*std::min(ahead, distance);
        ground = std::max(ground, surface.heightAt(p.x, p.y));
    }
    if (distance < 60.0f)
        ground = std::max(ground, surface.heightAt(point.x, point.y));

    return ground + std::max(height*surface.scale().z, 3.0f);
}


void Orders::pickPatrolPoint()
{
    const OrderSlot &slot = m_slots[m_current];
    const float angle = random()*glm::two_pi<float>();
    const float radius = slot.rad*std::sqrt(random());
    m_point = cellCentre(slot.refx, slot.refy) + Vector3D(radius*std::cos(angle), radius*std::sin(angle), 0);
    m_pointHeight = slot.mih + int(random()*(slot.mah - slot.mih));
    m_pointTime = m_scenario->time();
}


ActiveObject* Orders::findUnit(int id) const
{
    if (id == m_scenario->playerId())
        return m_scenario->player();
    for (const auto &record : m_scenario->objectRecords())
        if (record.id == id)
            return dynamic_cast<ActiveObject *>(record.object);
    return nullptr;
}


bool Orders::isAlive(const ActiveObject *unit) const
{
    if (!unit)
        return false;
    if (unit == m_scenario->player())
        return !m_scenario->playerDestroyed();
    return unit->isEnabled() && !const_cast<ActiveObject *>(unit)->eventDestroy()->isCompleted();
}


// the nearest hostile vessel in range; sector 0-7 limits it to that side (compass direction, +-90 degrees)
ActiveObject* Orders::nearestHostile(float range, int sector) const
{
    ActiveObject *nearest = nullptr;
    float nearestDistance = range;
    auto consider = [&](ActiveObject *unit) {
        if (unit == m_unit || !isAlive(unit) || !m_scenario->isHostile(m_unit->iff(), unit->iff()))
            return;
        const Vector3D way = unit->position() - m_unit->position();
        const float distance = glm::length(way);
        if (distance >= nearestDistance)
            return;
        if (sector >= 0 && sector < 8)
        {
            const Vector2D side(std::sin(sector*glm::quarter_pi<float>()), std::cos(sector*glm::quarter_pi<float>()));
            if (glm::dot(Vector2D(way), side) < 0)
                return;
        }
        nearest = unit;
        nearestDistance = distance;
    };
    consider(m_scenario->player());
    for (const auto &record : m_scenario->objectRecords())
        if (SimpleBoat *boat = dynamic_cast<SimpleBoat *>(record.object))
            consider(boat);
    return nearest;
}


Vector3D Orders::lead(const ActiveObject *target) const
{
    const float speed = m_unit->gun() ? m_unit->gun()->projectileSpeed() : 0;
    if (speed <= 0)
        return target->center();
    const float time = glm::length(target->center() - m_unit->position())/speed;
    return target->center() + target->velocity()*time;
}


bool Orders::aligned(const Vector3D &aim, const Vector3D &target, float range) const
{
    const Vector3D way = target - m_unit->position();
    const float distance = glm::length(way);
    return distance < range && glm::dot(aim, way/distance) > std::cos(AimTolerance);
}


// towers: turn to the target and fire once the turret points at it; without one the turret rests
// turned by car, and on patrol it sweeps: every 0.5-2.5 s it turns by 15-90 degrees either way,
// wherever that takes it (docs/ROADMAP.md, appendix J)
void Orders::updateTurret(ActiveObject *target, float range, bool sweep)
{
    TurretBase *turret = dynamic_cast<TurretBase *>(m_unit);
    if (!turret)
        return;
    m_turretBusy = target != nullptr;
    if (!target)
    {
        m_unit->setTrigger(false);
        const float rest = m_slots[m_current].yaw();
        if (sweep && m_scenario->time() >= m_nextSweep)
        {
            const float step = random(glm::radians(15.0f), glm::radians(90.0f))*(random() < 0.5f ? -1.0f : 1.0f);
            m_sweepYaw = rest + std::remainder(m_sweepYaw - rest, glm::two_pi<float>()) + step;
            m_nextSweep = m_scenario->time() + random(0.5f, 2.5f);
        }
        turret->aimAt(sweep ? m_sweepYaw : rest);
        return;
    }
    const Vector3D point = lead(target);
    turret->aimAt(point);
    const float gunRange = m_unit->gun() ? std::min(range, m_unit->gun()->range()) : 0;
    m_unit->setTrigger(m_unit->gun() && aligned(turret->aim(), point, gunRange));
    // torpedo towers: a torpedo whenever reloaded and pointing at the target
    if (m_unit->hasTorpedoes() && aligned(turret->aim(), target->center(), range))
        m_unit->fireTorpedo(target, turret->aim()*4.0f + m_unit->position() + Vector3D(0, 0, 4), turret->aim());
}


void Orders::endTorpedoRun()
{
    if (m_torpedoTarget)
        m_torpedoTarget->disengageTorpedo(m_unit);
    m_torpedoTarget = nullptr;
    m_torpedoRun = false;
}


void Orders::startAttackState(Attack state, ActiveObject *target)
{
    const SimpleBoat::AttackSettings &settings = m_boat->attackSettings();
    m_attack = state;
    if (state != Attack::Attack)
        endTorpedoRun();
    switch (state)
    {
    case Attack::Approach:
        {
            // a side of the target, picked by the align probabilities, at the align radius
            const float total = settings.alignFront + settings.alignBack + settings.alignLeft + settings.alignRight;
            float pick = random()*(total > 0 ? total : 1);
            Vector3D side(0, 1, 0);
            if ((pick -= settings.alignFront) < 0)
                side = Vector3D(0, 1, 0);
            else if ((pick -= settings.alignBack) < 0)
                side = Vector3D(0, -1, 0);
            else if ((pick -= settings.alignLeft) < 0)
                side = Vector3D(-1, 0, 0);
            else
                side = Vector3D(1, 0, 0);
            m_offset = side*random(settings.minAlignRadius, settings.maxAlignRadius);
            m_attackEnd = m_scenario->time() + random(settings.minAlignTime, settings.maxAlignTime);
        }
        break;
    case Attack::Attack:
        {
            // a torpedo run, if the dice say so and the target's list of torpedo attackers has room
            m_torpedoRun = random() < settings.torpedoProbability && target->engageTorpedo(m_unit);
            m_torpedoTarget = m_torpedoRun ? target : nullptr;
            // AttackProbabilityHit is the chance that the run is on target; otherwise it aims past the hull,
            // to the side of the line of fire
            m_offset = Vector3D(0);
            if (random() >= settings.hitProbability)
            {
                const Vector3D dim = target->box().dim();
                const float miss = 0.5f*std::max(dim.x, std::max(dim.y, dim.z)) + random(3.0f, 8.0f);
                Vector3D line = target->center() - m_unit->position();
                line = glm::length(line) > 0.1f ? glm::normalize(line) : Vector3D(0, 1, 0);
                const Vector3D side = glm::normalize(std::abs(line.z) < 0.9f ? glm::cross(line, Vector3D(0, 0, 1)) : glm::cross(line, Vector3D(1, 0, 0)));
                const Vector3D up = glm::cross(side, line);
                const float angle = random()*glm::two_pi<float>();
                m_offset = (side*std::cos(angle) + up*0.5f*std::sin(angle))*miss;
            }
            m_attackEnd = m_scenario->time() + random(settings.minAttackTime, settings.maxAttackTime);
        }
        break;
    case Attack::Break:
        {
            const float angle = random()*glm::two_pi<float>();
            // at least 60 m out, so the way there doesn't cross the target
            m_point = target->position() + Vector3D(std::cos(angle), std::sin(angle), 0)*std::max(60.0f, random(settings.minFlightRadius, settings.maxFlightRadius));
            m_attackEnd = m_scenario->time() + random(settings.minFlightTime, settings.maxFlightTime);
        }
        break;
    }
}


// boats: approach a side of the target, attack, break off, and again; tanks: drive at the target and fire
void Orders::updateAttack(ActiveObject *target, float speed)
{
    const OrderSlot &slot = m_slots[m_current];
    const Vector3D &scale = m_scenario->surface().scale();
    const float range = m_unit->gun() ? m_unit->gun()->range() : 0;

    auto inBand = [&](Vector3D point) {
        if (slot.mah > 0)
        {
            const float ground = m_scenario->surface().heightAt(point.x, point.y);
            point.z = glm::clamp(point.z, ground + slot.mih*scale.z, ground + slot.mah*scale.z);
        }
        return point;
    };

    if (m_boat->isGroundVehicle())
    {
        const Vector3D point = lead(target);
        const float distance = glm::length(point - m_unit->position());
        m_boat->steerTo(point, distance > 0.6f*range ? speed : 0.3f*speed);
        m_boat->aimAt(point);
        m_unit->setTrigger(aligned(m_boat->gunDirection(), point, range));
        return;
    }

    if (m_attackEnd == 0)
        startAttackState(Attack::Approach, target);

    // too close: veer away instead of ramming the target
    const Vector3D away = m_unit->position() - target->center();
    if (glm::length(away) < 35.0f)
    {
        m_unit->setTrigger(false);
        m_boat->steerTo(m_unit->position() + glm::normalize(away)*60.0f, speed);
        if (m_attack == Attack::Attack)
            startAttackState(Attack::Break, target);
        return;
    }

    // a point to fly to without crossing the target: if the way passes it within 40 m, go round it first
    auto around = [&](const Vector3D &point) {
        const Vector3D start = m_unit->position();
        const Vector3D way = point - start;
        const float length = glm::length(way);
        if (length < 1.0f)
            return point;
        const float t = glm::clamp(glm::dot(target->center() - start, way)/(length*length), 0.0f, 1.0f);
        const Vector3D closest = start + way*t;
        Vector3D side = closest - target->center();
        if (glm::length(side) >= 40.0f || t <= 0.0f || t >= 1.0f)
            return point;
        if (glm::length(side) < 0.1f)
            side = Vector3D(way.y, -way.x, 0);
        return target->center() + glm::normalize(side)*55.0f;
    };

    switch (m_attack)
    {
    case Attack::Approach:
        {
            // the side is relative to the target's heading, so units come in from its front, back or flanks
            Vector3D forward = target->velocity();
            if (const SimpleBoat *boat = dynamic_cast<const SimpleBoat *>(target))
                forward = boat->dir();
            forward.z = 0;
            forward = glm::length(forward) > 0.01f ? glm::normalize(forward) : Vector3D(0, 1, 0);
            const Vector3D right(forward.y, -forward.x, 0);
            const Vector3D point = inBand(target->position() + right*m_offset.x + forward*m_offset.y);
            m_boat->steerTo(around(point), speed);
            m_unit->setTrigger(false);
            if (glm::length(point - m_unit->position()) < ArriveDistance || m_scenario->time() > m_attackEnd)
                startAttackState(Attack::Attack, target);
        }
        break;

    case Attack::Attack:
        {
            const Vector3D point = lead(target) + m_offset;
            const float distance = glm::length(point - m_unit->position());
            m_boat->steerTo(point, speed);
            m_unit->setTrigger(aligned(m_boat->dir(), point, range));
            // one torpedo per run, if the dice said so, when pointing at the target from up to 250 m
            if (m_torpedoRun && m_unit->hasTorpedoes() && aligned(m_boat->dir(), target->center(), 250.0f) &&
                m_unit->fireTorpedo(target, m_unit->position() + m_boat->dir()*6.0f, m_boat->dir()))
                endTorpedoRun();
            // break off early enough not to ram the target
            if (distance < 50.0f || m_scenario->time() > m_attackEnd)
                startAttackState(Attack::Break, target);
        }
        break;

    case Attack::Break:
        m_boat->steerTo(around(inBand(m_point)), speed);
        m_unit->setTrigger(false);
        if (glm::length(Vector2D(m_point) - Vector2D(m_unit->position())) < ArriveDistance || m_scenario->time() > m_attackEnd)
            startAttackState(Attack::Approach, target);
        break;
    }
}


void Orders::update()
{
    // finish slots whose break condition is met or whose time is up
    while (m_current >= 0)
    {
        const OrderSlot &slot = m_slots[m_current];
        if (slot.breakCondition && slot.breakCondition->isCompleted())
        {
            next();
            continue;
        }
        // dur is the docking time for GotoWithInfiltrate
        const bool timed = slot.type != OrderSlot::Goto && slot.type != OrderSlot::GotoWithArrive && slot.type != OrderSlot::Explode &&
                           slot.type != OrderSlot::GotoWithInfiltrate;
        if (timed && slot.dur < Forever && m_scenario->time() - m_startTime >= slot.dur)
        {
            next();
            continue;
        }
        break;
    }

    if (m_current < 0)
    {
        m_unit->setTrigger(false);
        if (m_boat)
            m_boat->hold();
        return;
    }

    const OrderSlot &slot = m_slots[m_current];
    const float speed = m_boat ? m_boat->maxVelocity()*slot.vel/100.0f : 0;

    // orders for every unit
    switch (slot.type)
    {
    case OrderSlot::Explode:
        {
            // con says what sets it off (docs/ROADMAP.md, appendix J): 0 when dur is over unless the unit has
            // been boarded by then, 1 dur seconds after it has been boarded (S00202's control unit), 2 when dur
            // is over no matter what
            const bool boarded = m_unit->eventBoard()->isCompleted();
            if (slot.con == 1 && !boarded)
            {
                m_startTime = m_scenario->time();
                return;
            }
            if (m_scenario->time() - m_startTime >= slot.dur && !(slot.con == 0 && boarded))
            {
                m_unit->explode();
                m_current = -1;
            }
        }
        return;

    case OrderSlot::CardinalPatrol:
        updateTurret(engage(nearestHostile(slot.rad, -1)), slot.rad, true);
        return;

    case OrderSlot::CardinalAttack:
        {
            ActiveObject *target = findUnit(slot.ene);
            const bool inRange = isAlive(target) && glm::length(target->position() - m_unit->position()) < slot.rad;
            updateTurret(engage(inRange ? target : nullptr), slot.rad, false);
        }
        return;
    }

    // the rest needs a unit that can move
    if (!m_boat)
        return;

    // patrols and escorts attack hostile units within their sensor range (senr, 100 m if unset),
    // and go back to their order when the target is gone or has got away
    if ((slot.type == OrderSlot::Patrol || slot.type == OrderSlot::Escort) && m_boat->engagesHostiles())
    {
        const float sensorRange = m_unit->sensorRange() > 0 ? m_unit->sensorRange() : 100.0f;
        if (m_engaged && (!isAlive(m_engaged) || glm::length(m_engaged->position() - m_unit->position()) > EngageLeash*sensorRange))
        {
            m_engaged = nullptr;
            m_scenario->disengagePlayer(m_unit);
            m_attackEnd = 0;
            m_unit->setTrigger(false);
            if (slot.type == OrderSlot::Patrol)
                pickPatrolPoint();
        }
        if (!m_engaged && m_scenario->time() >= m_nextScan)
        {
            m_nextScan = m_scenario->time() + 1.0f;
            m_engaged = engage(nearestHostile(sensorRange, -1));
        }
        if (m_engaged)
        {
            updateAttack(m_engaged, m_boat->maxVelocity());
            return;
        }
    }

    switch (slot.type)
    {
    case OrderSlot::Goto:
    case OrderSlot::GotoWithArrive:
        {
            Vector3D target = cellCentre(slot.refx, slot.refy);
            const float distance = glm::length(Vector2D(target) - Vector2D(m_unit->position()));
            if (distance < ArriveDistance)
            {
                if (slot.type == OrderSlot::GotoWithArrive)
                    m_unit->eventFinish()->complete();
                next();
                m_boat->hold();
                return;
            }
            target.z = altitude(target, slot.hei);
            // GotoWithArrive stops at the point, Goto passes through it
            const float arriveSpeed = slot.type == OrderSlot::GotoWithArrive ? std::min(speed, 2.0f + 0.3f*distance) : speed;
            m_boat->steerTo(target, arriveSpeed);
        }
        break;

    case OrderSlot::Patrol:
        // a new point once this one is reached, or after a while when the way is blocked (steep ground for tanks)
        if (glm::length(Vector2D(m_point) - Vector2D(m_unit->position())) < ArriveDistance || m_scenario->time() - m_pointTime > 45.0f)
            pickPatrolPoint();
        m_point.z = altitude(m_point, m_pointHeight);
        m_boat->steerTo(m_point, speed);
        break;

    case OrderSlot::Attack:
        {
            ActiveObject *target = findUnit(slot.ene);
            if (isAlive(target) && (target = engage(target)))
            {
                updateAttack(target, speed);
                break;
            }
            // nobody to attack (yet): patrol the area
            m_unit->setTrigger(false);
            if (glm::length(Vector2D(m_point) - Vector2D(m_unit->position())) < ArriveDistance || m_scenario->time() - m_pointTime > 45.0f)
                pickPatrolPoint();
            m_point.z = altitude(m_point, m_pointHeight);
            m_boat->steerTo(m_point, speed);
        }
        break;

    case OrderSlot::Ambush:
        {
            // wait at the point until the target comes near or the unit is attacked
            ActiveObject *target = findUnit(slot.ene);
            if (!m_ambushed && isAlive(target) &&
                (glm::length(target->position() - m_unit->position()) < AmbushDistance || m_unit->eventAttack()->isCompleted()))
                m_ambushed = true;
            if (m_ambushed && isAlive(target) && (target = engage(target)))
            {
                updateAttack(target, speed);
                break;
            }
            m_unit->setTrigger(false);
            Vector3D point = cellCentre(slot.refx, slot.refy);
            if (glm::length(Vector2D(point) - Vector2D(m_unit->position())) < ArriveDistance)
            {
                // lie in wait pointing the way car says
                const float yaw = slot.yaw();
                m_boat->steerTo(m_unit->position() + Vector3D(std::sin(yaw), std::cos(yaw), 0)*50.0f, 0);
                break;
            }
            point.z = altitude(point, slot.mih);
            m_boat->steerTo(point, speed);
        }
        break;

    case OrderSlot::GotoWithInfiltrate:
        {
            // fly to the target, dock on top of it for dur seconds, then it has been boarded
            ActiveObject *target = findUnit(slot.ene);
            if (!isAlive(target))
            {
                next();
                return;
            }
            m_unit->setTrigger(false);
            const Vector3D dock(target->center().x, target->center().y, target->box().maxPoint().z + 3.0f);
            const float distance = glm::length(dock - m_unit->position());
            if (distance < 12.0f)
            {
                if (m_dockStart < 0)
                    m_dockStart = m_scenario->time();
                m_boat->hold();
                if (m_scenario->time() - m_dockStart >= slot.dur)
                {
                    target->eventBoard()->complete();
                    next();
                }
                break;
            }
            m_dockStart = -1;
            m_boat->steerTo(dock, std::min(speed, 2.0f + 0.3f*distance));
        }
        break;

    case OrderSlot::Escort:
        {
            SimpleBoat *escorted = dynamic_cast<SimpleBoat *>(findUnit(slot.esc));
            if (!escorted || escorted->eventDestroy()->isCompleted())
            {
                next();
                return;
            }
            if (!escorted->isEnabled())
            {
                m_boat->hold();
                return;
            }

            const Vector3D &scale = m_scenario->surface().scale();
            const Vector3D &offset = slot.offset;
            Vector3D target = escorted->position() + escorted->right()*offset.x + escorted->dir()*offset.y + escorted->up()*offset.z;
            if (slot.mah > 0)
            {
                const float ground = m_scenario->surface().heightAt(target.x, target.y);
                target.z = glm::clamp(target.z, ground + slot.mih*scale.z, ground + slot.mah*scale.z);
            }

            const float distance = glm::length(target - m_unit->position());
            const float escortedSpeed = glm::length(escorted->velocity());
            if (distance < 5.0f)
                // in place: fly along with the escorted unit
                m_boat->steerTo(m_unit->position() + escorted->dir()*50.0f + Vector3D(0, 0, target.z - m_unit->position().z), escortedSpeed);
            else
                m_boat->steerTo(target, std::min(std::max(speed, escortedSpeed), escortedSpeed + 0.5f*distance));
        }
        break;

    default:
        // Wait, and the orders that are not implemented yet
        m_unit->setTrigger(false);
        m_boat->hold();
    }
}


} // namespace fight
