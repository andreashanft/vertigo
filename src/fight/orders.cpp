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
#include "weapons/torpedo.h"
#include "util/random.h"

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/trigonometric.hpp>
#include <algorithm>
#include <limits>
#include <cmath>


namespace fight {


namespace {

// durations from this on mean "forever"
const int Forever = 100000;
// a unit is "there" within this many map cells of a point (or its top speed in a second, or its hull)
const float ArriveCells = 1.25f;
// and slows down within this many cells (or its hull)
const float SlowCells = 2.0f;
// a unit without a senr key hears this far (metres)
const float DefaultSensorRange = 100.0f;
// the passive sonar: a unit is heard while its noise over the square of its distance beats this
// over the square of the sensor's range, in the game's units of 500 per noise level
const float HearingThreshold = 1500.0f/500.0f;
// the sensor looks for hostiles this often (sen 3 and 4 faster)
const float ScanInterval = 1.5f;
const float QuickScanInterval = 0.7f;
// guns fire while the target is within this cosine of the nose (about 14 degrees)
const float AimCos = 0.97f;
// torpedoes go when the nose points at the target within this, straight-running ones closer
const float TorpedoCosStraight = 0.95f;
const float TorpedoCosHoming = 0.7f;
// the point a unit breaks off to, in the target's frame (metres): far to one side, ahead, up or down
const float FlightSide = 850.0f;
const float FlightAhead = 500.0f;
const float FlightUp = 500.0f;
// a unit in the target's line of fire (within this cosine of its nose) this long moves to the other side
const float LineOfFireCos = 0.99f;
const float LineOfFireTime = 1.5f;
// an escort closes in at full speed from this distance (metres) and slows down inside it
const float EscortSlowDistance = 75.0f;
// an ambushing boat lies this many hull heights over the ground
const float AmbushHeights = 2.0f;

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
    m_pointTime(0),
    m_target(nullptr),
    m_scanned(nullptr),
    m_nextScan(0),
    m_fight(Fight::None),
    m_fightStart(0),
    m_side(Front),
    m_alignEnd(0),
    m_runDistance(0),
    m_attackEnd(0),
    m_flightRadius(0),
    m_attackSpeed(0),
    m_shotsBefore(0),
    m_breakOnHit(false),
    m_flightEnd(0),
    m_lastFlip(0),
    m_targetYaw(0),
    m_torpedo(false),
    m_torpedoRun(false),
    m_torpedoTarget(nullptr),
    m_lying(false),
    m_formation(false),
    m_dockStart(-1),
    m_turretBusy(false),
    m_sweepYaw(0),
    m_nextSweep(0)
{
    if (!m_slots.empty())
        start(0);
}


float Orders::random() const
{
    return util::random()/float(RAND_MAX);
}


float Orders::random(float min, float max) const
{
    return min + random()*(max - min);
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
    dropTarget();
    m_arrived = false;
    m_lying = false;
    m_formation = false;
    m_dockStart = -1;
    m_turretBusy = false;
    m_unit->setTrigger(false);
    m_unit->setSilent(false);
    if (m_current >= 0 && (m_slots[m_current].type == OrderSlot::Patrol || m_slots[m_current].type == OrderSlot::Attack))
    {
        // the area's centre, mih over the ground, is the first point
        const OrderSlot &s = m_slots[m_current];
        m_point = cellCentre(s.refx, s.refy);
        m_point.z = m_scenario->surface().heightAt(m_point.x, m_point.y) + s.mih;
        m_pointTime = m_scenario->time();
    }
    if (m_current >= 0)
    {
        m_sweepYaw = m_slots[m_current].yaw();
        m_nextSweep = m_scenario->time() + random(0.5f, 2.5f);
    }
}


void Orders::next()
{
    start(m_current + 1);
}


txt::StringFight Orders::activity() const
{
    if (m_current < 0)
        return txt::Fight79;
    if (m_target)
        return txt::Fight77;
    switch (m_slots[m_current].type)
    {
    case OrderSlot::Attack:
    case OrderSlot::CardinalAttack:
        return txt::Fight77;
    case OrderSlot::Ambush:
        return txt::Fight79;
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
    if (m_target)
    {
        static const char *phases[] = { "", " align", " turn", " run", " flight" };
        text += phases[int(m_fight)];
    }
    else if (m_lying)
        text += " lying";
    if (m_unit->trigger())
        text += " firing";
    return text;
}


// --- flying

Vector3D Orders::cellCentre(int x, int y) const
{
    const Vector3D &scale = m_scenario->surface().scale();
    return Vector3D((x + 0.5f)*scale.x, (y + 0.5f)*scale.y, 0);
}


float Orders::arriveRadius(float extra) const
{
    const float radius = m_boat ? m_boat->collisionRadius() : 0;
    return std::max(std::max(ArriveCells*m_scenario->surface().scale().x, radius), extra);
}


// a unit slows down proportionally within two cells (or its hull) of a point
float Orders::slowSpeed(const Vector3D &point, float speed) const
{
    const float radius = m_boat ? m_boat->collisionRadius() : 0;
    const float slow = std::max(SlowCells*m_scenario->surface().scale().x, radius);
    return speed*std::min(1.0f, glm::length(point - m_unit->position())/slow);
}


float Orders::groundAhead(const Vector3D &point) const
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
    return ground;
}


Vector3D Orders::inBand(Vector3D point, float minHeight, float maxHeight) const
{
    const float ground = groundAhead(point);
    const float radius = m_boat ? m_boat->collisionRadius() : 0;
    point.z = std::max(point.z, ground + minHeight + 0.6f*radius);
    if (maxHeight > minHeight)
        point.z = std::min(point.z, std::max(ground + maxHeight, ground + minHeight + 0.6f*radius));
    return point;
}


// a random point of the area, at least minFraction of the radius from the current one (a few tries);
// its height wanders by up to half the way, within the order's band
void Orders::pickPatrolPoint(float minFraction)
{
    const OrderSlot &slot = m_slots[m_current];
    const Vector3D centre = cellCentre(slot.refx, slot.refy);
    const Vector3D from = m_point;
    const float radius = std::max(1.0f, float(slot.rad));
    Vector3D point = from;
    float distance = 0;
    for (int tries = 0; tries < 10; tries++)
    {
        const float angle = random()*glm::two_pi<float>();
        const float r = radius*random();
        point = Vector3D(centre.x - std::sin(angle)*r, centre.y + std::cos(angle)*r, from.z);
        distance = glm::length(Vector2D(point) - Vector2D(from));
        if (distance > minFraction*radius)
            break;
    }
    point.z = from.z + (random() - 0.5f)*distance;
    const float ground = m_scenario->surface().heightAt(point.x, point.y);
    point.z = glm::clamp(point.z, ground + slot.mih, ground + std::max(float(slot.mah), float(slot.mih) + 1.0f));
    m_point = point;
    m_pointTime = m_scenario->time();
}


glm::mat3 Orders::frameOf(const ActiveObject *unit)
{
    if (const SimpleBoat *boat = dynamic_cast<const SimpleBoat *>(unit))
        return glm::mat3(boat->right(), boat->dir(), boat->up());
    return glm::mat3(1.0f);
}


Vector3D Orders::toWorld(const ActiveObject *unit, const Vector3D &local)
{
    return unit->position() + frameOf(unit)*local;
}


Vector3D Orders::toLocal(const ActiveObject *unit, const Vector3D &world)
{
    return glm::transpose(frameOf(unit))*(world - unit->position());
}


// --- sensing

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
    return unit->isEnabled() && !unit->isDying();
}


bool Orders::isHostile(const ActiveObject *unit) const
{
    return unit && unit != m_unit && m_scenario->isHostile(m_unit->iff(), unit->iff());
}


// a unit without a sen key has a passive sonar (sen 1)
int Orders::sensorType() const
{
    return m_unit->sensor() < 0 ? 1 : m_unit->sensor();
}


float Orders::sensorRange() const
{
    return m_unit->sensorRange() > 0 ? m_unit->sensorRange() : DefaultSensorRange;
}


// the passive sonar hears a unit while its noise over the square of the distance beats the
// threshold set by the sensor's range; so a unit at noise level 3 is heard right up to the range,
// quieter ones closer (level 1 within 58 percent of it); a doubled range doesn't help
bool Orders::hears(const ActiveObject *unit, float distance2) const
{
    const float range = sensorRange();
    return unit->noise()*range*range > HearingThreshold*distance2;
}


// nothing of the terrain between the two hulls
bool Orders::sees(const ActiveObject *unit) const
{
    const float radius = m_boat ? m_boat->collisionRadius() : 2.0f;
    Vector3D position, normal;
    return !m_scenario->surface().testCollision(m_unit->position() + Vector3D(0, 0, 0.5f*radius), unit->center(), 0.5f, position, normal);
}


bool Orders::isBuzzer(const ActiveObject *unit)
{
    const DecoyContact *decoy = dynamic_cast<const DecoyContact *>(unit);
    return decoy && decoy->isBuzzer();
}


bool Orders::detects(const ActiveObject *unit, float rangeFactor) const
{
    if (!isAlive(unit) || unit == m_unit)
        return false;
    const float range = sensorRange()*rangeFactor;
    const float distance2 = glm::length2(unit->position() - m_unit->position());
    if (distance2 >= range*range)
        return false;
    switch (sensorType())
    {
    case 0: return sees(unit);                                   // sight
    case 1: return hears(unit, distance2);                       // passive sonar
    case 2: return true;                                         // active sonar
    case 3: return isBuzzer(unit);                               // buzzers only
    case 4: return isBuzzer(unit) || hears(unit, distance2);
    default: return false;
    }
}


// looks every 1.5 s (0.7 s with sen 3 and 4) and keeps what it found while that lives: the nearest
// hostile in range (sen 0 in sight, sen 2 regardless), the loudest one heard, a torch too (sen 1),
// the nearest buzzer, whoever dropped it (sen 3), the nearest hostile buzzer, else the loudest one
// heard, a torch too (sen 4); only the passive sonars take decoys (appendices J and S)
ActiveObject* Orders::scan()
{
    if (m_scenario->time() < m_nextScan)
        return isAlive(m_scanned) ? m_scanned : nullptr;
    const int type = sensorType();
    m_nextScan = m_scenario->time() + (type >= 3 ? QuickScanInterval : ScanInterval);
    m_scanned = nullptr;
    const float range = sensorRange();
    float best = 0;
    ActiveObject *buzzer = nullptr;
    float nearestBuzzer = range*range;
    auto consider = [&](ActiveObject *unit) {
        if (!isAlive(unit) || unit == m_unit || !(type == 3 || isHostile(unit)))
            return;
        const float distance2 = glm::length2(unit->position() - m_unit->position());
        if (distance2 >= range*range)
            return;
        const DecoyContact *decoy = dynamic_cast<const DecoyContact *>(unit);
        if (decoy && decoy->isBuzzer())
        {
            if ((type == 3 || type == 4) && distance2 < nearestBuzzer)
            {
                nearestBuzzer = distance2;
                buzzer = unit;
            }
            return;
        }
        if (decoy && type != 1 && type != 4)
            return;
        float score = 0;
        switch (type)
        {
        case 0:
            if (!sees(unit))
                return;
            score = 1/(distance2 + 1);
            break;
        case 1:
        case 4:
            if (!hears(unit, distance2))
                return;
            score = unit->noise()/(distance2 + 1);
            break;
        case 2:
            score = 1/(distance2 + 1);
            break;
        default:
            return;
        }
        if (score > best)
        {
            best = score;
            m_scanned = unit;
        }
    };
    consider(m_scenario->player());
    for (const auto &record : m_scenario->objectRecords())
        if (SimpleBoat *boat = dynamic_cast<SimpleBoat *>(record.object))
            consider(boat);
    for (const auto &decoy : m_scenario->decoyContacts())
        consider(decoy.get());
    if (buzzer)
        m_scanned = buzzer;
    return m_scanned;
}


ActiveObject* Orders::acquire(ActiveObject *enemy)
{
    if (isHostile(enemy) && detects(enemy, 1))
        return engage(enemy);
    // a unit under fire knows the player at double range
    ActiveObject *player = m_scenario->player();
    if (m_unit->hitRecently() && isHostile(player) && detects(player, 2))
        return engage(player);
    return engage(scan());
}


bool Orders::keepTarget(ActiveObject *enemy)
{
    if (!m_target)
        return false;
    ActiveObject *player = m_scenario->player();
    // a player who shoots draws the fire if nearer than the current target
    if (m_unit->hitRecently() && m_target != player && enemy != player && isHostile(player) && detects(player, 2) &&
        glm::length2(player->position() - m_unit->position()) < glm::length2(m_target->position() - m_unit->position()))
    {
        if (ActiveObject *taken = engage(player))
            switchTarget(taken);
    }
    // the ordered enemy comes first once in range
    if (enemy && m_target != enemy && isHostile(enemy) && detects(enemy, 1))
        switchTarget(engage(enemy));
    if (!m_target || !isHostile(m_target) || !detects(m_target, 2))
    {
        dropTarget();
        return false;
    }
    return true;
}


ActiveObject* Orders::engage(ActiveObject *target)
{
    if (!target)
        return nullptr;
    if (target == m_scenario->player())
        return m_scenario->engagePlayer(m_unit) ? target : nullptr;
    m_scenario->disengagePlayer(m_unit);
    return target;
}


// a new target starts a new fight, and the torpedo run against the old one ends
void Orders::switchTarget(ActiveObject *target)
{
    if (target == m_target)
        return;
    endTorpedoRun();
    m_fight = Fight::None;
    m_target = target;
}


// a hit not yet answered; hitRecently() holds for a while, but the dice for it are rolled once
bool Orders::freshHit()
{
    if (!m_unit->hitRecently() || m_unit->hitTime() == m_hitAnswered)
        return false;
    m_hitAnswered = m_unit->hitTime();
    return true;
}


void Orders::dropTarget()
{
    m_target = nullptr;
    m_fight = Fight::None;
    m_unit->setTrigger(false);
    m_scenario->disengagePlayer(m_unit);
    endTorpedoRun();
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


// --- fighting

Vector3D Orders::lead(const ActiveObject *target) const
{
    const float speed = m_unit->gun() ? m_unit->gun()->projectileSpeed() : 0;
    if (speed <= 0)
        return target->center();
    const float time = glm::length(target->center() - m_unit->position())/speed;
    return target->center() + target->velocity()*time;
}


// boats shoot at where the target is, with no lead; at a ground unit a little higher
Vector3D Orders::aimPoint(const ActiveObject *target) const
{
    Vector3D point = target->position();
    if (const SimpleBoat *boat = dynamic_cast<const SimpleBoat *>(target))
        if (boat->isGroundVehicle())
            point.z += 0.4f*boat->collisionRadius();
    return point;
}


bool Orders::aligned(const Vector3D &aim, const Vector3D &target, float range) const
{
    const Vector3D way = target - m_unit->position();
    const float distance = glm::length(way);
    return distance < range && glm::dot(aim, way/distance) > AimCos;
}


// a torpedo goes when the nose points at the target within 18 degrees in the horizontal plane
// (45 for homing types) and the tube has reloaded; the target's list of torpedo attackers must
// have room (appendix J)
bool Orders::tryTorpedo(ActiveObject *target)
{
    if (!m_unit->hasTorpedoes() || !m_boat)
        return false;
    const TorpedoType *type = TorpedoType::get(m_unit->torpedoType());
    const bool homing = type && type->lockType != 0;
    Vector2D way(target->position() - m_unit->position());
    Vector2D nose(m_boat->dir());
    if (glm::length(way) < 0.1f || glm::length(nose) < 0.1f)
        return false;
    const float cosine = glm::dot(glm::normalize(way), glm::normalize(nose));
    if (cosine < (homing ? TorpedoCosHoming : TorpedoCosStraight))
        return false;
    if (!m_torpedoRun)
    {
        if (!target->engageTorpedo(m_unit))
            return false;
        m_torpedoRun = true;
        m_torpedoTarget = target;
    }
    if (!m_unit->fireTorpedo(target, m_unit->position() + m_boat->dir()*6.0f, m_boat->dir()))
        return false;
    endTorpedoRun();
    return true;
}


void Orders::endTorpedoRun()
{
    if (m_torpedoTarget)
        m_torpedoTarget->disengageTorpedo(m_unit);
    m_torpedoTarget = nullptr;
    m_torpedoRun = false;
}


// the fight's phases (docs/ROADMAP.md, appendix J): a side of the target to align on, a run at it
// turning in place or flying, a flight away from it
void Orders::startFight(Fight phase, ActiveObject *target)
{
    const SimpleBoat::AttackSettings &s = m_boat->attackSettings();
    const float now = m_scenario->time();
    m_fight = phase;
    m_fightStart = now;
    m_torpedo = false;
    switch (phase)
    {
    case Fight::Align:
        {
            // the side by the align probabilities: ahead within 30 degrees, left or right between
            // 30 and 150, behind within 30; a distance within the align radii and a little above
            // or below the target's horizon by the align gammas
            const float pick = random();
            float angle;
            if (pick < s.alignFront)
            {
                m_side = Front;
                angle = random(glm::radians(-30.0f), glm::radians(30.0f));
            }
            else if (pick < s.alignFront + s.alignLeft)
            {
                m_side = Left;
                angle = random(glm::radians(30.0f), glm::radians(150.0f));
            }
            else if (pick < s.alignFront + s.alignLeft + s.alignRight)
            {
                m_side = Right;
                angle = -random(glm::radians(30.0f), glm::radians(150.0f));
            }
            else
            {
                m_side = Back;
                angle = random(glm::radians(150.0f), glm::radians(180.0f))*(random() < 0.5f ? -1.0f : 1.0f);
            }
            const float gamma = random(s.minAlignGamma, s.maxAlignGamma);
            const float radius = random(s.minAlignRadius, s.maxAlignRadius);
            m_alignOffset = Vector3D(-std::sin(angle)*radius, std::cos(angle)*radius, std::sin(gamma)*radius);
            m_alignEnd = now + random(s.minAlignTime, s.maxAlignTime);
            m_torpedo = random() < s.alignTorpedo;
        }
        break;

    case Fight::Turn:
    case Fight::Run:
        {
            // the run: for its time, until the target is within the flight radius, or until hit after
            // the first shot if the dice say so; turning in place or flying at 40 to 100 percent speed
            m_runStart = toLocal(target, m_unit->position());
            m_runDistance = glm::length(m_runStart);
            m_attackEnd = now + random(s.minAttackTime, s.maxAttackTime);
            m_flightRadius = random(s.minFlightRadius, s.maxFlightRadius);
            m_attackSpeed = random(0.4f, 1.0f)*m_boat->maxVelocity()*m_slots[m_current].vel/100.0f;
            m_fight = random() < s.turnProbability ? Fight::Turn : Fight::Run;
            m_torpedo = random() < s.attackTorpedo;
            m_shotsBefore = m_unit->gun() ? m_unit->gun()->shots() : 0;
            m_breakOnHit = random() < s.flightHit;
        }
        break;

    case Fight::Flight:
        {
            // away to one of four points far to the target's left or right, ahead and above or below it
            const float pick = random();
            float x = 1, z = 1;
            if (pick < s.flightUL)
                x = -1;
            else if (pick < s.flightUL + s.flightUR)
                ;
            else if (pick < s.flightUL + s.flightUR + s.flightDL)
                x = -1, z = -1;
            else
                z = -1;
            m_flightOffset = Vector3D(x*FlightSide, FlightAhead, z*FlightUp);
            m_flightEnd = now + random(s.minFlightTime, s.maxFlightTime);
            m_lastFlip = now;
            if (random() < s.flightTorpedo)
                tryTorpedo(target);
        }
        break;

    case Fight::None:
        break;
    }
}


void Orders::updateFight(ActiveObject *target, float speed)
{
    const OrderSlot &slot = m_slots[m_current];
    const SimpleBoat::AttackSettings &s = m_boat->attackSettings();
    const float now = m_scenario->time();

    if (m_fight == Fight::None)
        startFight(slot.type == OrderSlot::Ambush ? Fight::Turn : Fight::Align, target);

    switch (m_fight)
    {
    case Fight::Align:
        {
            m_boat->steerTo(inBand(toWorld(target, m_alignOffset), slot.mih, slot.mah), speed);
            m_unit->setTrigger(false);
            // which side of the target the unit is on, and whether it is at the align distance
            const Vector3D way = m_unit->position() - target->position();
            const glm::mat3 frame = frameOf(target);
            const Vector2D forward = glm::normalize(Vector2D(frame[1]) + Vector2D(1e-6f, 0));
            const Vector2D flat(way);
            const float cosine = glm::length(flat) > 0.1f ? glm::dot(glm::normalize(flat), forward) : 1.0f;
            int side = cosine > 0.866f ? Front : cosine < -0.866f ? Back : glm::dot(flat, Vector2D(frame[0])) < 0 ? Left : Right;
            const float distance = glm::length(way);
            if (side == m_side && distance > s.minAlignRadius && distance < s.maxAlignRadius)
                startFight(Fight::Turn, target);
            else if (now > m_alignEnd)
                startFight(Fight::Align, target);
            else if (freshHit())
                startFight(random() < s.hitProbability ? Fight::Turn : Fight::Align, target);
            else if (m_torpedo && tryTorpedo(target))
                m_torpedo = false;
        }
        break;

    case Fight::Turn:
    case Fight::Run:
        {
            // the nose straight at the target, as the guns fire along it; the run flies at the
            // target itself, not at the order's height band, kept clear of the ground ahead, and
            // turns on the spot instead for the rest of its time once the terrain is in the way
            if (m_fight == Fight::Run && !sees(target))
                m_fight = Fight::Turn;
            const Vector3D aim = aimPoint(target);
            if (m_fight == Fight::Turn)
                m_boat->pointAt(aim, 0);
            else
                m_boat->pointAt(inBand(aim, 0, 0), m_attackSpeed);
            // the run ends once the start, carried along with the target, has got farther away than
            // the distance minus the flight radius: the target is within it
            const Vector3D start = toWorld(target, m_runStart);
            const float reach = m_runDistance - m_flightRadius;
            bool over = glm::length2(start - m_unit->position()) > reach*reach || now > m_attackEnd;
            const Vector3D way = aim - m_unit->position();
            m_unit->setTrigger(m_unit->gun() && glm::length(way) > 0.1f && glm::dot(m_boat->dir(), glm::normalize(way)) > AimCos);
            const int shots = m_unit->gun() ? m_unit->gun()->shots() - m_shotsBefore : 0;
            if (shots > 0 && m_breakOnHit && m_unit->hitRecently())
                over = true;
            if (m_torpedo && tryTorpedo(target))
                m_torpedo = false;
            if (over)
                startFight(Fight::Flight, target);
        }
        break;

    case Fight::Flight:
        {
            // in the target's line of fire for a while: over to the other side
            const Vector3D way = m_unit->position() - target->position();
            const Vector3D forward = frameOf(target)[1];
            const float cosine = glm::length(way) > 0.1f ? glm::dot(glm::normalize(way), forward) : 0;
            if (cosine > LineOfFireCos)
            {
                if (now - m_lastFlip > LineOfFireTime)
                {
                    m_flightOffset.x = -m_flightOffset.x;
                    m_lastFlip = now;
                }
            }
            else
                m_lastFlip = now;
            Vector3D point = toWorld(target, m_flightOffset);
            // after a moment the flight levels off at the target's height
            if (now - m_fightStart > LineOfFireTime)
                point.z = target->position().z;
            m_boat->steerTo(inBand(point, slot.mih, slot.mah), speed);
            m_unit->setTrigger(false);
            if (now > m_flightEnd)
                startFight(Fight::Align, target);
        }
        break;

    case Fight::None:
        break;
    }
}


// the tanks' fight (docs/ROADMAP.md, appendix J): the align point lies at the align radius, 20 to
// 30 degrees round the target from the tank's bearing; Hook goes the way the target is turning,
// Cross towards its nose, Retain and Change by which way the two point relative to each other.
// Then the tank drives at the target for the attack time, firing, and aligns again
void Orders::startTankFight(Fight phase, ActiveObject *target, float speed)
{
    const SimpleBoat::AttackSettings &s = m_boat->attackSettings();
    const float now = m_scenario->time();
    m_fight = phase;
    m_fightStart = now;
    const glm::mat3 frame = frameOf(target);
    const float targetYaw = std::atan2(-frame[1].x, frame[1].y);
    switch (phase)
    {
    case Fight::Align:
        {
            const Vector3D local = toLocal(target, m_unit->position());
            float bearing = std::atan2(-local.x, local.y);
            float step = random(glm::radians(20.0f), glm::radians(30.0f));
            const float pick = random();
            const float turned = std::remainder(targetYaw - m_targetYaw, glm::two_pi<float>());
            const Vector2D own(m_boat->dir());
            const float across = frame[1].x*own.y - frame[1].y*own.x;
            if (pick < s.hook)
                step = turned < 0 ? -step : step;
            else if (pick < s.hook + s.cross)
                step = bearing > 0 ? -step : step;
            else if (pick < s.hook + s.cross + s.retain)
                step = across > 0 ? step : -step;
            else
                step = across > 0 ? -step : step;
            bearing += step;
            const float radius = random(s.minAlignRadius, s.maxAlignRadius);
            m_alignOffset = Vector3D(-std::sin(bearing)*radius, std::cos(bearing)*radius, 0);
            m_runDistance = std::max(1.0f, glm::length(Vector2D(toWorld(target, m_alignOffset)) - Vector2D(m_unit->position())));
            m_alignEnd = now + random(s.minAlignTime, s.maxAlignTime);
        }
        break;

    case Fight::Run:
        // (the original draws the speed between two of the unit's speeds; which the lower is isn't read)
        m_attackSpeed = random(0.4f, 1.0f)*speed;
        m_attackEnd = now + random(s.minAttackTime, s.maxAttackTime);
        break;

    default:
        break;
    }
    m_targetYaw = targetYaw;
}


void Orders::updateTankFight(ActiveObject *target, float speed)
{
    const SimpleBoat::AttackSettings &s = m_boat->attackSettings();
    const float now = m_scenario->time();
    if (m_fight == Fight::None || m_fight == Fight::Turn || m_fight == Fight::Flight)
        startTankFight(Fight::Align, target, speed);

    const Vector3D aim = aimPoint(target);
    m_boat->aimAt(aim);
    if (m_fight == Fight::Align)
    {
        // to the point, slow at both ends of the way, without firing
        const Vector3D point = toWorld(target, m_alignOffset);
        const float distance = glm::length(Vector2D(point) - Vector2D(m_unit->position()));
        float f = std::min(1.0f, distance/m_runDistance);
        if (f > 0.5f)
            f = 1.0f - f;
        m_boat->steerTo(point, (f + 0.5f)*speed);
        m_unit->setTrigger(false);
        if (distance < arriveRadius(m_boat->maxVelocity()) || now > m_alignEnd)
            startTankFight(Fight::Run, target, speed);
        else if (freshHit())
            startTankFight(random() < s.alignHit ? Fight::Align : Fight::Run, target, speed);
        return;
    }

    // the drive at the target, the turret firing within 14 degrees of it
    m_boat->steerTo(target->position(), m_attackSpeed);
    const Vector3D way = aim - m_unit->position();
    m_unit->setTrigger(m_unit->gun() && glm::length(way) > 0.1f && glm::dot(m_boat->gunDirection(), glm::normalize(way)) > AimCos);
    const float distance = glm::length(Vector2D(target->position()) - Vector2D(m_unit->position()));
    if (distance < arriveRadius(m_boat->maxVelocity()) || now > m_attackEnd)
        startTankFight(Fight::Align, target, speed);
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


void Orders::update()
{
    // finish slots whose break condition is met or whose time is up; when every slot ends at
    // once (all break conditions met), the unit holds instead of going round forever
    for (int ended = 0; m_current >= 0; ended++)
    {
        if (ended > int(m_slots.size()))
        {
            m_current = -1;
            break;
        }
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
            if (m_boat)
                m_boat->hold();
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
                m_unit->wreck();
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

    // patrols, attackers, escorts and ambushers fight what their sensor finds (appendix J): the
    // ordered enemy once in range, the player when he shoots, any hostile in range; they keep at it
    // until the target is out of double range, and the order's time runs on meanwhile
    const bool fights = slot.type == OrderSlot::Patrol || slot.type == OrderSlot::Attack || slot.type == OrderSlot::Escort || slot.type == OrderSlot::Ambush;
    if (fights && m_boat->engagesHostiles())
    {
        ActiveObject *enemy = slot.type == OrderSlot::Attack || slot.type == OrderSlot::Ambush ? findUnit(slot.ene) : nullptr;
        if (!keepTarget(enemy))
            m_target = acquire(enemy);
        else if (slot.type == OrderSlot::Escort)
        {
            // an escort fights only near its place
            if (SimpleBoat *escorted = dynamic_cast<SimpleBoat *>(findUnit(slot.esc)))
            {
                Vector3D offset = slot.offset;
                if (offset.y < 0)
                    offset.y = -1;
                if (glm::length(toWorld(escorted, offset) - m_unit->position()) > slot.ran)
                    dropTarget();
            }
        }
        if (m_target)
        {
            if (m_lying)
            {
                m_lying = false;
                m_unit->setSilent(false);
            }
            if (m_boat->isGroundVehicle())
                updateTankFight(m_target, speed);
            else
                updateFight(m_target, speed);
            return;
        }
    }

    // a big ship takes Patrol and Attack to the order's cell, at mih over the ground, and stays
    // there once it is within reach of it, until the time is up; Escort, Ambush and
    // GotoWithInfiltrate leave it where it is, Escort and GotoWithInfiltrate ending as a boat's do
    // once the escorted unit or the target is gone (docs/ROADMAP.md, appendix A)
    if (m_boat->isBigShip() && slot.type != OrderSlot::Goto && slot.type != OrderSlot::GotoWithArrive)
    {
        m_unit->setTrigger(false);
        if (slot.type != OrderSlot::Patrol && slot.type != OrderSlot::Attack)
        {
            ActiveObject *escorted = slot.type == OrderSlot::Escort ? findUnit(slot.esc) : nullptr;
            if ((slot.type == OrderSlot::Escort && (!escorted || escorted->isDying())) ||
                (slot.type == OrderSlot::GotoWithInfiltrate && !isAlive(findUnit(slot.ene))))
            {
                next();
                return;
            }
            m_boat->hold();
            return;
        }
        Vector3D target = cellCentre(slot.refx, slot.refy);
        target.z = m_scenario->surface().heightAt(target.x, target.y) + slot.mih;
        if (m_arrived || glm::length(Vector2D(target) - Vector2D(m_unit->position())) < arriveRadius(m_boat->maxVelocity()))
        {
            m_arrived = true;
            m_boat->hold();
            return;
        }
        m_boat->steerTo(target, slowSpeed(target, speed));
        return;
    }

    switch (slot.type)
    {
    case OrderSlot::Goto:
    case OrderSlot::GotoWithArrive:
        {
            Vector3D target = cellCentre(slot.refx, slot.refy);
            target.z = m_scenario->surface().heightAt(target.x, target.y) + slot.hei;
            if (glm::length(target - m_unit->position()) < arriveRadius(m_boat->maxVelocity()))
            {
                if (slot.type == OrderSlot::GotoWithArrive)
                {
                    m_unit->eventFinish()->complete();
                    m_scenario->logEvent(m_unit->name(), m_unit->eventFinish(), txt::Fight9);
                }
                next();
                m_boat->hold();
                return;
            }
            // slowing down over the last cells
            m_boat->steerTo(inBand(target, slot.hei, 0), slowSpeed(target, speed));
        }
        break;

    case OrderSlot::Patrol:
    case OrderSlot::Attack:
        {
            // from point to point of the area: a new one when this one is reached or the time to fly
            // the radius is up; nobody to attack yet (Attack): the same
            m_unit->setTrigger(false);
            const float flyTime = speed > 0.1f ? slot.rad/speed : 10.0f;
            if (glm::length(m_point - m_unit->position()) < arriveRadius(m_boat->maxVelocity()) || m_scenario->time() - m_pointTime > flyTime)
                pickPatrolPoint(slot.type == OrderSlot::Patrol ? 0.5f : 0.3f);
            m_boat->steerTo(inBand(m_point, slot.mih, slot.mah), slowSpeed(m_point, speed));
        }
        break;

    case OrderSlot::Ambush:
        {
            // to the point, then lie there silent, pointing the way car says, a hull's height or two
            // over the ground, until the sensor finds something
            m_unit->setTrigger(false);
            Vector3D point = cellCentre(slot.refx, slot.refy);
            const float ground = m_scenario->surface().heightAt(point.x, point.y);
            if (!m_lying && glm::length(Vector2D(point) - Vector2D(m_unit->position())) < arriveRadius(m_boat->maxVelocity()))
            {
                m_lying = true;
                m_unit->setSilent(true);
            }
            if (m_lying)
            {
                const float yaw = slot.yaw();
                const float height = ground + AmbushHeights*0.5f*m_boat->box().dim().z;
                const Vector3D ahead = m_unit->position() + Vector3D(std::sin(yaw), std::cos(yaw), 0)*50.0f;
                m_boat->steerTo(Vector3D(ahead.x, ahead.y, m_unit->position().z + glm::clamp(height - m_unit->position().z, -5.0f, 5.0f)), 0);
                break;
            }
            point.z = ground + slot.mih;
            m_boat->steerTo(inBand(point, slot.mih, slot.mah), slowSpeed(point, speed));
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
                    m_scenario->logEvent(target->name(), target->eventBoard(), txt::Fight10);
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
            if (!escorted || escorted->isDying())
            {
                next();
                return;
            }
            if (!escorted->isEnabled())
            {
                m_boat->hold();
                return;
            }

            // the place: the offsets in the escorted unit's frame (nothing behind it), measured from
            // hull to hull, within the band
            Vector3D offset = slot.offset;
            if (offset.y < 0)
                offset.y = -1;
            const float gap = glm::length(offset);
            if (gap > 0)
            {
                // along the offset, out of the escorted hull's box, then the escort's own box
                const Vector3D way = offset/gap;
                auto extent = [&way](const BoundingBox &box, float sign) {
                    float reach = std::numeric_limits<float>::max();
                    for (int axis = 0; axis < 3; axis++)
                    {
                        const float along = sign*way[axis];
                        if (along > 1e-4f)
                            reach = std::min(reach, box.maxPoint()[axis]/along);
                        else if (along < -1e-4f)
                            reach = std::min(reach, box.minPoint()[axis]/along);
                    }
                    return std::max(reach, 0.0f);
                };
                offset = way*(extent(escorted->localBox(), 1) + gap + extent(m_boat->localBox(), -1));
            }
            const Vector3D target = inBand(toWorld(escorted, offset), slot.mih, slot.mah);
            const float distance = glm::length(target - m_unit->position());
            const float escortedSpeed = glm::length(escorted->velocity());
            if (m_formation && distance > arriveRadius(1.3f*m_boat->maxVelocity()))
                m_formation = false;
            else if (!m_formation && distance < arriveRadius(m_boat->maxVelocity()))
                m_formation = true;
            if (m_formation && escortedSpeed < 1.0f && distance < 3.0f)
                // the escorted unit waits: so does the escort
                m_boat->hold();
            else if (m_formation)
                // in place: fly along, closing what is left
                m_boat->steerTo(target + escorted->dir()*escortedSpeed*2.0f, std::min(m_boat->maxVelocity(), escortedSpeed + 0.5f*distance));
            else
                // closing in at the order's speed, slower over the last 75 m, never much faster than the escorted unit
                m_boat->steerTo(target, std::min(speed*std::min(1.0f, distance/EscortSlowDistance), 1.1f*escortedSpeed + 2.0f));
        }
        break;

    default:
        // Wait, and the orders that are not implemented yet
        m_unit->setTrigger(false);
        m_boat->hold();
    }
}


} // namespace fight
