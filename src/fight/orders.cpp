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
#include "boats/tank.h"
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
// an ambushing boat hovers twice its hull's depth below its centre over the ground, pushed up or
// down by at least 0.05 and at most 0.5 g
const float AmbushDepths = 2.0f;
const float AmbushMinLift = 0.05f;
const float AmbushMaxLift = 0.5f;
const float Gravity = 9.81f;
// the height band's look-ahead and the way it turns aside (appendix J): half a cell plus the way
// flown in 0.75 s ahead of the hull, and a point 31.25 m off
const float BandAheadTime = 0.75f;
const float BandTurnAway = 31.25f;
// other units ahead bend the way within 3.5 hull radii, fully within 1.5, looking 2 hulls plus
// the way flown in 1.5 s ahead; the bent point lies 93.75 m off
const float AvoidNear = 1.5f;
const float AvoidFar = 3.5f;
const float AvoidAheadTime = 1.5f;
const float AvoidReach = 93.75f;
// a unit takes at most this many homing torpedoes on it, the player 8 (appendix J)
const int TorpedoesOnUnit = 4;
const int TorpedoesOnPlayer = 8;
// an infiltrator docks beside its target, half its own hull clear of it, and sits there while
// within 1.5 times its top speed in a second (or its hull, or 1.25 cells) of that place
const float DockGap = 0.5f;
const float DockStay = 1.5f;
// an escort in formation is pulled into its place
const float FormationPull = 2.0f;

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
    m_lying(false),
    m_formation(false),
    m_dockLeft(0),
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
    m_docking = false;
    m_dockLeft = m_current >= 0 ? float(m_slots[m_current].dur) : 0;
    m_turretBusy = false;
    m_unit->setTrigger(false);
    m_unit->setSilent(false);
    if (m_boat)
        m_boat->setPush(Vector3D(0));
    if (m_current >= 0 && (m_slots[m_current].type == OrderSlot::Patrol || m_slots[m_current].type == OrderSlot::Attack))
    {
        // the area's centre, mih over the ground, is the first point
        const OrderSlot &s = m_slots[m_current];
        m_point = cellCentre(s.refx, s.refy);
        m_point.z = m_scenario->surface().heightAt(m_point.x, m_point.y) + s.mih;
        // a tank gives it four times the time to drive there
        const float speed = m_boat ? m_boat->maxVelocity()*s.vel/100.0f : 0;
        m_pointEnd = m_scenario->time() + (speed > 0.1f ? 4*glm::length(Vector2D(m_point) - Vector2D(m_unit->position()))/speed : 10.0f);
    }
    m_unit->orderStarted(currentSlot());
    if (m_current >= 0)
    {
        m_sweepYaw = m_slots[m_current].yaw();
        m_sweepDirection = 0;
        m_sweepHome = true;
        // CardinalAttack's enemy: the squadron's first living member, chosen now
        m_enemy = m_slots[m_current].type == OrderSlot::CardinalAttack ? findUnit(m_slots[m_current].ene) : nullptr;
        if (m_enemy && !isAlive(m_enemy))
            m_enemy = nullptr;
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


bool Orders::usesBuzzers() const
{
    const OrderSlot *slot = currentSlot();
    return slot && (slot->type == OrderSlot::Patrol || slot->type == OrderSlot::Attack || slot->type == OrderSlot::Ambush || slot->type == OrderSlot::Escort);
}


ActiveObject* Orders::dockingTarget() const
{
    const OrderSlot *slot = currentSlot();
    return slot && slot->type == OrderSlot::GotoWithInfiltrate ? findUnit(slot->ene) : nullptr;
}


// towers sleep while they fight, too (appendix J)
bool Orders::mayPause() const
{
    const OrderSlot *slot = currentSlot();
    return !(slot && slot->type == OrderSlot::Explode);
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


float Orders::hullRadius() const
{
    return m_boat ? m_boat->hullRadius() : 0;
}


// The original's steering (appendix J). The hull's front bottom corners, half a cell plus the way of
// 0.75 s ahead, against the ground under them: below the band's floor the unit turns aside and up
// towards a point 31.25 m off, above its top it goes down to it, both at once it turns aside;
// otherwise it flies straight for the point, height and all.
bool Orders::bandPoint(Vector3D &point, float zmin, float zmax) const
{
    Surface &surface = m_scenario->surface();
    const BoundingBox &box = m_boat->localBox();
    const Vector3D &position = m_unit->position();
    const float ahead = 0.5f*surface.scale().x + BandAheadTime*glm::length(m_unit->velocity());
    const Vector3D front = position + m_boat->dir()*(box.maxPoint().y + ahead) + m_boat->up()*box.minPoint().z;
    const Vector3D corners[2] = { front + m_boat->right()*box.minPoint().x, front + m_boat->right()*box.maxPoint().x };
    float clearance[2];
    bool low = false, high = false;
    for (int i = 0; i < 2; i++)
    {
        const float ground = surface.heightAt(corners[i].x, corners[i].y);
        clearance[i] = corners[i].z - ground;
        low = low || clearance[i] < zmin;
        high = high || (zmax >= 0 && clearance[i] > zmax);
    }
    if (!low && !high)
        return false;
    const Vector3D middle = 0.5f*(corners[0] + corners[1]);
    const float ground = surface.heightAt(middle.x, middle.y);
    if (!low)
    {
        point.z = ground + zmax;
        return true;
    }
    // aside towards the corner with more room under it
    const Vector3D side = clearance[0] > clearance[1] ? -m_boat->right() : m_boat->right();
    if (high)
    {
        point = position + side*BandTurnAway;
        point.z = ground + zmax;
        return true;
    }
    Vector3D way = point - position;
    way = glm::length(way) > 1e-3f ? glm::normalize(way) : m_boat->dir();
    const float k = 0.2f + 0.8f*glm::abs(m_boat->dir().z);
    Vector3D bent = (1 - k)*way + k*side;
    bent = glm::length(bent) > 1e-3f ? glm::normalize(bent) : side;
    point = position + bent*BandTurnAway;
    point.z = ground + zmin;
    return true;
}


// other units ahead push the way aside, the nearer the harder; a unit close enough takes it over
Vector3D Orders::avoidUnits(const Vector3D &point) const
{
    const Vector3D &position = m_unit->position();
    const float radius = hullRadius();
    const Vector3D ahead = position + m_boat->dir()*(2*radius + AvoidAheadTime*glm::length(m_unit->velocity()));
    Vector3D push(0);
    float strongest = 0;
    auto consider = [&](const SimpleBoat *other) {
        if (!other || other == m_unit || !other->isEnabled() || other->isDying() || other->isGroundVehicle())
            return;
        const float otherRadius = other->hullRadius();
        // only what is about the look-ahead point
        if (glm::length(Vector2D(other->position() - ahead)) > radius + otherRadius + AvoidFar*radius)
            return;
        const Vector3D away = other->position() - position;
        const float distance = glm::length(away);
        if (distance < 1e-3f)
            return;
        const float near = AvoidNear*radius + otherRadius, far = AvoidFar*radius + otherRadius;
        const float weight = glm::clamp((far - distance)/(far - near), 0.0f, 1.0f);
        if (weight <= 0)
            return;
        push -= weight*away/distance;
        strongest = std::max(strongest, weight);
    };
    consider(m_scenario->player());
    for (const auto &record : m_scenario->objectRecords())
        consider(dynamic_cast<const SimpleBoat *>(record.object));
    if (strongest <= 0 || glm::length(push) < 1e-4f)
        return point;
    Vector3D way = point - position;
    way = glm::length(way) > 1e-3f ? glm::normalize(way) : m_boat->dir();
    Vector3D bent = glm::normalize(push) + (1 - strongest)*way;
    if (glm::length(bent) < 1e-3f)
        return point;
    return position + glm::normalize(bent)*AvoidReach;
}


bool Orders::fly(const Vector3D &point, float minHeight, float maxHeight, float speed, bool avoid, float hullFactor)
{
    if (m_boat->isGroundVehicle())
    {
        m_boat->steerTo(inBand(point, minHeight, std::max(maxHeight, 0.0f)), speed);
        return false;
    }
    Vector3D aim = avoid ? avoidUnits(point) : point;
    const bool changed = bandPoint(aim, minHeight + hullFactor*hullRadius(), maxHeight);
    m_boat->steerTo(aim, speed);
    return changed;
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
    const float floor = ground + slot.mih + 0.599f*hullRadius();
    point.z = glm::clamp(point.z, floor, std::max(ground + float(slot.mah), floor));
    m_point = point;
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


ActiveObject* Orders::shooter(const ActiveObject *unit, float rangeFactor) const
{
    ActiveObject *attacker = unit->hitRecently() ? unit->lastAttacker() : nullptr;
    return attacker && isHostile(attacker) && detects(attacker, rangeFactor) ? attacker : nullptr;
}


ActiveObject* Orders::acquire(ActiveObject *enemy)
{
    if (isHostile(enemy) && detects(enemy, 1))
        return engage(enemy);
    // a unit under fire knows its attacker at double range
    if (ActiveObject *attacker = shooter(m_unit, 2))
        return engage(attacker);
    return engage(scan());
}


// an escort looks for no one: it takes on whoever hits it, whoever hits its charge, or the first of
// its charge's attackers it detects, all at double range
ActiveObject* Orders::escortTarget(const ActiveObject *escorted)
{
    if (ActiveObject *attacker = shooter(m_unit, 2))
        return engage(attacker);
    if (ActiveObject *attacker = shooter(escorted, 2))
        return engage(attacker);
    for (ActiveObject *attacker : escorted->attackers())
        if (isHostile(attacker) && detects(attacker, 2))
            return engage(attacker);
    return nullptr;
}


bool Orders::keepTarget(ActiveObject *enemy)
{
    if (!m_target)
        return false;
    // whoever shoots draws the fire if nearer than the current target
    ActiveObject *attacker = shooter(m_unit, 2);
    if (attacker && attacker != m_target && attacker != enemy &&
        glm::length2(attacker->position() - m_unit->position()) < glm::length2(m_target->position() - m_unit->position()))
    {
        if (ActiveObject *taken = engage(attacker))
            switchTarget(taken);
    }
    // the ordered enemy comes first once in range
    if (enemy && m_target != enemy && isHostile(enemy) && detects(enemy, 1))
    {
        if (ActiveObject *taken = engage(enemy))
            switchTarget(taken);
    }
    if (!m_target || !isHostile(m_target) || !detects(m_target, 2))
    {
        dropTarget();
        return false;
    }
    return true;
}


ActiveObject* Orders::engage(ActiveObject *target)
{
    if (target != m_engaged && m_engaged)
    {
        m_engaged->disengageAttacker(m_unit);
        m_engaged = nullptr;
    }
    if (!target)
    {
        m_scenario->disengagePlayer(m_unit);
        return nullptr;
    }
    if (target == m_scenario->player())
        return m_scenario->engagePlayer(m_unit) ? target : nullptr;
    m_scenario->disengagePlayer(m_unit);
    if (!target->engageAttacker(m_unit))
        return nullptr;
    m_engaged = target;
    return target;
}


// the escort's place: the offsets in the escorted unit's frame, each axis measured from the face of
// its hull's box to the face of the escort's (appendix J); while closing in, a place ahead of the
// escorted unit is taken 1 m behind it
Vector3D Orders::escortStation(const SimpleBoat *escorted, const OrderSlot &slot, bool approach) const
{
    Vector3D offset = slot.offset;
    if (approach && offset.y > 0)
        offset.y = -1;
    const BoundingBox &theirs = escorted->localBox(), &own = m_boat->localBox();
    for (int axis = 0; axis < 3; axis++)
        offset[axis] += offset[axis] >= 0 ? theirs.maxPoint()[axis] - own.minPoint()[axis] : theirs.minPoint()[axis] - own.maxPoint()[axis];
    return toWorld(escorted, offset);
}


// a new target starts a new fight, and the torpedo run against the old one ends
void Orders::switchTarget(ActiveObject *target)
{
    if (target == m_target)
        return;
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


// the order goes on where it was, a patrol picking its points afresh
void Orders::dropTarget()
{
    m_target = nullptr;
    m_fight = Fight::None;
    m_unit->setTrigger(false);
    engage(nullptr);
    m_pointTime = m_scenario->time();
}


// --- fighting

// boats shoot at where the target is, with no lead; at a ground unit a little higher
Vector3D Orders::aimPoint(const ActiveObject *target) const
{
    Vector3D point = target->position();
    if (const SimpleBoat *boat = dynamic_cast<const SimpleBoat *>(target))
        if (boat->isGroundVehicle())
            point.z += 0.4f*boat->collisionRadius();
    return point;
}


// every try with the tube reloaded reloads it, 2 to 3.5 s; the torpedo goes from the next tube when
// the nose points at the target within 18 degrees in the horizontal plane (tor 0 and 1), or within
// 45 degrees for the others, as long as the target has fewer than 4 (the player 8) homing
// torpedoes on it (appendix J)
bool Orders::tryTorpedo(ActiveObject *target)
{
    if (!m_boat || !m_unit->torpedoReady())
        return false;
    m_unit->startTorpedoReload();
    const Vector3D tube = m_boat->nextTorpedoTube();
    Vector2D way(target->position() - tube);
    Vector2D nose(m_boat->dir());
    if (glm::length(way) < 0.1f || glm::length(nose) < 0.1f)
        return false;
    const float cosine = glm::dot(glm::normalize(way), glm::normalize(nose));
    const bool straight = m_unit->torpedoType() <= 1;
    if (cosine < (straight ? TorpedoCosStraight : TorpedoCosHoming))
        return false;
    if (!straight && m_scenario->threatsTo(target) >= (target == m_scenario->player() ? TorpedoesOnPlayer : TorpedoesOnUnit))
        return false;
    m_unit->launchTorpedo(target, tube, m_boat->dir());
    return true;
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
            fly(toWorld(target, m_alignOffset), slot.mih, slot.mah, speed, true);
            m_unit->setTrigger(false);
            // which side of the target the unit is on, and whether it is at the align distance
            const Vector3D way = m_unit->position() - target->position();
            const glm::mat3 frame = frameOf(target);
            const Vector2D forward = glm::normalize(Vector2D(frame[1]) + Vector2D(1e-6f, 0));
            const Vector2D flat(way);
            const float cosine = glm::length(flat) > 0.1f ? glm::dot(glm::normalize(flat), forward) : 1.0f;
            int side = cosine > 0.866f ? Front : cosine < -0.866f ? Back : glm::dot(flat, Vector2D(frame[0])) < 0 ? Left : Right;
            const float distance = glm::length(way);
            // (the checks one after the other, the last change winning; the torpedo in the same frame)
            Fight next = Fight::None;
            if (side == m_side && distance > s.minAlignRadius && distance < s.maxAlignRadius)
                next = Fight::Turn;
            if (now > m_alignEnd)
                next = Fight::Align;
            if (freshHit())
                next = random() < s.hitProbability ? Fight::Turn : Fight::Align;
            if (m_torpedo && tryTorpedo(target))
                m_torpedo = false;
            if (next != Fight::None)
                startFight(next, target);
        }
        break;

    case Fight::Turn:
    case Fight::Run:
        {
            // the nose straight at the target, as the guns fire along it; the run flies at it,
            // bent away from other units, and turns on the spot instead for the rest of its time
            // once the hull's way ahead leaves the band of mih plus a hull to mah over the ground
            const Vector3D aim = aimPoint(target);
            if (m_fight == Fight::Run)
            {
                Vector3D way = avoidUnits(aim);
                if (m_boat->isGroundVehicle() || !bandPoint(way, slot.mih + hullRadius(), slot.mah))
                    m_boat->pointAt(way, m_attackSpeed);
                else
                    m_fight = Fight::Turn;
            }
            if (m_fight == Fight::Turn)
                m_boat->pointAt(aim, 0);
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
            fly(point, slot.mih, slot.mah, speed, true);
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
// 30 degrees round the target from the tank's bearing; Hook goes against the way the target turned
// in the last frame, Cross round to its tail, Retain and Change by which way the two point relative
// to each other. Then the tank drives at the target for the attack time, at a speed between its
// lowest (backwards for most) and its top speed, and aligns again
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
            const float turned = targetYaw - m_targetYaw;
            const Vector2D own(m_boat->dir());
            const float across = frame[1].x*own.y - frame[1].y*own.x;
            if (pick < s.hook)
                step = turned > 0 ? -step : step;
            else if (pick < s.hook + s.cross)
                step = bearing < 0 ? -step : step;
            else if (pick < s.hook + s.cross + s.retain)
                step = across >= 0 ? -step : step;
            else
                step = across < 0 ? -step : step;
            bearing += step;
            const float radius = random(s.minAlignRadius, s.maxAlignRadius);
            m_alignOffset = Vector3D(-std::sin(bearing)*radius, std::cos(bearing)*radius, 0);
            m_runDistance = std::max(1.0f, glm::length(Vector2D(toWorld(target, m_alignOffset)) - Vector2D(m_unit->position())));
            m_alignEnd = now + random(s.minAlignTime, s.maxAlignTime);
        }
        break;

    case Fight::Run:
        m_attackSpeed = random(m_boat->minVelocity(), m_boat->maxVelocity())*m_slots[m_current].vel/100.0f;
        m_attackEnd = now + random(s.minAttackTime, s.maxAttackTime);
        break;

    default:
        break;
    }
}


void Orders::updateTankFight(ActiveObject *target, float speed)
{
    const SimpleBoat::AttackSettings &s = m_boat->attackSettings();
    const float now = m_scenario->time();
    if (m_fight == Fight::None || m_fight == Fight::Turn || m_fight == Fight::Flight)
        startTankFight(Fight::Align, target, speed);
    const glm::mat3 frame = frameOf(target);
    const float targetYaw = std::atan2(-frame[1].x, frame[1].y);

    // a turret tank's turret leads the target and fires by itself the whole fight, within 90 degrees
    // of the front; a fixed gun tilts at the target and fires only in the run; neither cares about
    // the terrain between
    const Tank *tank = dynamic_cast<const Tank *>(m_boat);
    const bool turret = tank && tank->hasTurret();
    Vector3D aim = aimPoint(target);
    if (turret && m_unit->gun())
    {
        aim = intercept(m_unit->position(), target, m_unit->gun()->projectileSpeed());
        if (const SimpleBoat *boat = dynamic_cast<const SimpleBoat *>(target))
            if (boat->isGroundVehicle())
                aim.z += 0.4f*boat->collisionRadius();
    }
    m_boat->aimAt(aim);
    const Vector3D way = aim - m_unit->position();
    const bool aligned = m_unit->gun() && glm::length(way) > 0.1f && glm::dot(m_boat->gunDirection(), glm::normalize(way)) > AimCos;
    const float arrive = m_boat->maxVelocity();
    if (m_fight == Fight::Align)
    {
        // to the point, slow at both ends of the way
        const Vector3D point = toWorld(target, m_alignOffset);
        const float distance = glm::length(Vector2D(point) - Vector2D(m_unit->position()));
        float f = std::min(1.0f, distance/m_runDistance);
        if (f > 0.5f)
            f = 1.0f - f;
        m_boat->steerTo(point, (f + 0.5f)*speed);
        m_unit->setTrigger(turret && aligned);
        if (distance < arrive || now > m_alignEnd)
            startTankFight(Fight::Run, target, speed);
        else if (freshHit())
            startTankFight(random() < s.alignHit ? Fight::Align : Fight::Run, target, speed);
        m_targetYaw = targetYaw;
        return;
    }

    // the drive at the target
    m_boat->steerTo(target->position(), m_attackSpeed);
    m_unit->setTrigger(aligned);
    const float distance = glm::length(Vector2D(target->position()) - Vector2D(m_unit->position()));
    m_targetYaw = targetYaw;
    if (distance < arrive || now > m_attackEnd)
        startTankFight(Fight::Align, target, speed);
}


// a tank's patrol point: a distance from the tank between half the radius (or how far beyond the
// area's edge the tank is) and its distance to the far side, on the arc of that circle round the
// tank which lies inside the area; given up after four times the time to drive there
void Orders::pickTankPoint(float speed)
{
    const OrderSlot &slot = m_slots[m_current];
    const Vector2D centre(cellCentre(slot.refx, slot.refy));
    const Vector2D own(m_unit->position());
    const float R = std::max(1.0f, float(slot.rad));
    const float d = glm::length(centre - own);
    const float low = d < R ? R/2 : std::max(d - R, R/2);
    const float r = random(low, std::max(low, d + R));
    float angle = random()*glm::two_pi<float>();
    if (d > 1e-3f && r > 1e-3f)
    {
        const float cosine = (r*r + d*d - R*R)/(2*r*d);
        if (cosine > -1 && cosine < 1)
        {
            const float toCentre = std::atan2(centre.x - own.x, centre.y - own.y);
            const float half = std::acos(cosine);
            angle = toCentre + random(-half, half);
        }
    }
    const Vector2D point = own + Vector2D(std::sin(angle), std::cos(angle))*r;
    m_point = Vector3D(point, m_scenario->surface().heightAt(point.x, point.y));
    m_pointEnd = m_scenario->time() + (speed > 0.1f ? 4*r/speed : 10.0f);
}


// the exact intercept of a target moving on at its velocity with a shot at the speed from the
// origin; the target's position if there is none
Vector3D Orders::intercept(const Vector3D &origin, const ActiveObject *target, float speed)
{
    const Vector3D d = target->position() - origin, v = target->velocity();
    if (speed <= 0)
        return target->position();
    const float a = glm::dot(v, v) - speed*speed, b = 2*glm::dot(d, v), c = glm::dot(d, d);
    float t = -1;
    if (glm::abs(a) < 1e-6f)
        t = glm::abs(b) > 1e-6f ? -c/b : -1;
    else if (b*b - 4*a*c >= 0)
    {
        const float root = std::sqrt(b*b - 4*a*c);
        const float t1 = (-b - root)/(2*a), t2 = (-b + root)/(2*a);
        t = std::min(t1, t2) > 0 ? std::min(t1, t2) : std::max(t1, t2);
    }
    return t > 0 ? target->position() + v*t : target->position();
}


// without a target the turret sweeps: to the centre first, then, a pause of 0.5 to 2.5 s once there,
// a step of 15 to 90 degrees either way; with car 0, 2, 4 or 6 within the half circle about the
// centre, back from its edges (appendix J)
void Orders::sweepTurret(const OrderSlot &slot)
{
    TurretBase *turret = static_cast<TurretBase *>(m_unit);
    const float centre = slot.yaw();
    const bool bounded = slot.car == 0 || slot.car == 2 || slot.car == 4 || slot.car == 6;
    const float now = m_scenario->time();
    if (m_sweepHome)
    {
        m_sweepYaw = centre;
        m_sweepWaiting = false;
        m_sweepHome = false;
    }
    if (!m_sweepWaiting)
    {
        if (turret->arrived())
        {
            m_sweepWaiting = true;
            m_nextSweep = now + random(0.5f, 2.5f);
        }
    }
    else if (now >= m_nextSweep)
    {
        const float sign = m_sweepDirection != 0 ? float(m_sweepDirection) : (random() < 0.5f ? -1.0f : 1.0f);
        float offset = std::remainder(m_sweepYaw - centre, glm::two_pi<float>()) + sign*random(glm::radians(15.0f), glm::radians(90.0f));
        m_sweepDirection = 0;
        if (bounded && offset > glm::half_pi<float>())
        {
            offset = glm::half_pi<float>();
            m_sweepDirection = -1;
        }
        else if (bounded && offset < -glm::half_pi<float>())
        {
            offset = -glm::half_pi<float>();
            m_sweepDirection = 1;
        }
        m_sweepYaw = centre + offset;
        m_sweepWaiting = false;
    }
    turret->aimAt(m_sweepYaw);
}


// A tower's CardinalPatrol and CardinalAttack (appendix J): it looks with its own sensor (sen,
// senr), keeps a target while it detects it and takes whoever hits it if nearer; CardinalAttack
// takes its enemy whenever it detects it. The turret tracks the target within the half circle
// about the car's heading (car 0, 2, 4, 6) and fires whenever it points within 14 degrees of the
// target's intercept, whatever the distance and the terrain; a torpedo tower tries a torpedo every
// 2 to 3.5 s. Without a target the turret sweeps.
void Orders::updateTower(const OrderSlot &slot)
{
    TurretBase *turret = dynamic_cast<TurretBase *>(m_unit);
    if (!turret)
        return;
    m_lastCentre = slot.yaw();
    const bool attack = slot.type == OrderSlot::CardinalAttack;
    if (attack)
    {
        ActiveObject *target = isHostile(m_enemy) && detects(m_enemy, 1) ? m_enemy : nullptr;
        if (!target)
            target = shooter(m_unit, 1);
        if (!target)
            target = scan();
        if (target != m_target && !target)
            m_sweepHome = true;
        m_target = engage(target);
    }
    else
    {
        if (m_target && (!isHostile(m_target) || !detects(m_target, 1)))
        {
            dropTarget();
            m_sweepHome = true;
        }
        ActiveObject *attacker = shooter(m_unit, 1);
        if (attacker && attacker != m_target && (!m_target ||
            glm::length2(attacker->position() - m_unit->position()) < glm::length2(m_target->position() - m_unit->position())))
        {
            if (ActiveObject *taken = engage(attacker))
                m_target = taken;
        }
        if (!m_target)
            m_target = engage(scan());
    }
    m_turretBusy = m_target != nullptr;
    if (!m_target)
    {
        m_unit->setTrigger(false);
        sweepTurret(slot);
        return;
    }
    const bool bounded = slot.car == 0 || slot.car == 2 || slot.car == 4 || slot.car == 6;
    if (turret->isTorpedoTurret())
    {
        // no lead and no pitch; a try whenever reloaded, from the next tube, along the turret's
        // level forward: within 18 degrees for tor 0 and 1, else 21.6 degrees with room in the
        // target's homing torpedoes, the terrain clear between the tube and the target
        turret->track(m_target->position(), bounded, slot.yaw());
        if (m_unit->torpedoReady())
        {
            m_unit->startTorpedoReload();
            const Vector3D tube = turret->nextTube();
            const Vector3D forward = glm::normalize(Vector3D(Vector2D(turret->aim()), 0) + Vector3D(1e-6f, 0, 0));
            const Vector2D way(m_target->position() - tube);
            const bool straight = m_unit->torpedoType() <= 1;
            Vector3D hit, normal;
            if (glm::length(way) > 0.1f && glm::dot(glm::normalize(way), Vector2D(forward)) >= (straight ? 0.95f : 0.93f) &&
                (straight || m_scenario->threatsTo(m_target) < (m_target == m_scenario->player() ? 8 : 4)) &&
                !m_scenario->surface().testCollision(tube, m_target->center(), 0.0f, hit, normal))
                m_unit->launchTorpedo(m_target, tube, forward);
        }
        return;
    }
    const float speed = m_unit->gun() ? m_unit->gun()->projectileSpeed() : 0;
    Vector3D point = intercept(turret->muzzles(), m_target, speed);
    if (const SimpleBoat *boat = dynamic_cast<const SimpleBoat *>(m_target))
        if (boat->isGroundVehicle())
            point.z += 0.4f*boat->collisionRadius();
    turret->track(point, bounded, slot.yaw());
    const Vector3D way = point - turret->muzzles();
    m_unit->setTrigger(m_unit->gun() && glm::length(way) > 0.1f && glm::dot(turret->aim(), glm::normalize(way)) > AimCos);
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
            // the unit stops dead and waits
            if (m_boat)
                m_boat->freeze();
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
    case OrderSlot::CardinalAttack:
        updateTower(slot);
        return;
    }

    // a tower waits facing the centre of its last cardinal order, without looking for anyone
    if (!m_boat && slot.type == OrderSlot::Wait)
        if (TurretBase *turret = dynamic_cast<TurretBase *>(m_unit))
            turret->aimAt(m_lastCentre);

    // the rest needs a unit that can move
    if (!m_boat)
        return;

    // patrols, attackers and ambushers fight what their sensor finds (appendix J): the ordered enemy
    // once in range, whoever hits them, any hostile in range; escorts only whoever hits them or
    // their charge or attacks their charge; they keep at it until the target is out of double
    // range, and the order's time runs on meanwhile
    const bool fights = slot.type == OrderSlot::Patrol || slot.type == OrderSlot::Attack || slot.type == OrderSlot::Escort || slot.type == OrderSlot::Ambush;
    if (fights && m_boat->engagesHostiles())
    {
        ActiveObject *enemy = slot.type == OrderSlot::Attack || slot.type == OrderSlot::Ambush ? findUnit(slot.ene) : nullptr;
        SimpleBoat *escorted = slot.type == OrderSlot::Escort ? dynamic_cast<SimpleBoat *>(findUnit(slot.esc)) : nullptr;
        if (keepTarget(enemy))
        {
            // an escort fights only near its place
            if (escorted && glm::length(escortStation(escorted, slot, false) - m_unit->position()) > slot.ran)
                dropTarget();
        }
        else if (slot.type == OrderSlot::Escort)
            m_target = isAlive(escorted) ? escortTarget(escorted) : nullptr;
        else
            m_target = acquire(enemy);
        if (m_target)
        {
            if (m_lying)
            {
                m_lying = false;
                m_unit->setSilent(false);
            }
            m_boat->setPush(Vector3D(0));
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
            // a ground vehicle drives straight at its full speed and is there within its top
            // speed in a second (Goto) or its hull (GotoWithArrive), along the ground
            const bool ground = m_boat->isGroundVehicle();
            const float groundReach = slot.type == OrderSlot::Goto ? m_boat->maxVelocity() : hullRadius();
            if (ground ? glm::length(Vector2D(target) - Vector2D(m_unit->position())) < groundReach
                       : glm::length(target - m_unit->position()) < arriveRadius(m_boat->maxVelocity()))
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
            // slowing down over the last cells, kept over the ground but under no cap
            if (ground)
                m_boat->steerTo(target, speed);
            else
                fly(target, slot.hei, -1, slowSpeed(target, speed), false);
        }
        break;

    case OrderSlot::Patrol:
    case OrderSlot::Attack:
        {
            // from point to point of the area: a new one when this one is reached, and once the time
            // to fly the radius since the patrol began or resumed is up, a new one every frame;
            // nobody to attack yet (Attack): the same
            m_unit->setTrigger(false);
            if (m_boat->isGroundVehicle() && slot.type == OrderSlot::Patrol)
            {
                // a tank drives on at its full speed, a new point within twice its top speed in a
                // second of this one (horizontally) or when its time is up
                if (glm::length(Vector2D(m_point) - Vector2D(m_unit->position())) < 2*m_boat->maxVelocity() || m_scenario->time() > m_pointEnd)
                    pickTankPoint(speed);
                m_boat->steerTo(m_point, speed);
                break;
            }
            const float flyTime = speed > 0.1f ? slot.rad/speed : 10.0f;
            if (glm::length(m_point - m_unit->position()) < arriveRadius(m_boat->maxVelocity()) || m_scenario->time() - m_pointTime > flyTime)
                pickPatrolPoint(slot.type == OrderSlot::Patrol ? 0.5f : 0.3f);
            fly(m_point, slot.mih, slot.mah, slowSpeed(m_point, speed), true);
        }
        break;

    case OrderSlot::Ambush:
        {
            // to the point, then lie there silent, pointing the way car says and level, hovering
            // twice the hull's depth over the ground, until the sensor finds something
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
                const Vector3D &nose = m_boat->dir();
                const float pitch = std::asin(glm::clamp(nose.z, -1.0f, 1.0f));
                m_boat->turnBy(std::remainder(slot.yaw() - std::atan2(nose.x, nose.y), glm::two_pi<float>()), pitch, 0);
                const float depth = AmbushDepths*std::max(0.1f, -m_boat->localBox().minPoint().z);
                const Vector3D &at = m_unit->position();
                const float share = (m_scenario->surface().heightAt(at.x, at.y) + depth - at.z)/depth;
                const float lift = glm::clamp(glm::abs(share), AmbushMinLift, AmbushMaxLift)*(share < 0 ? -1.0f : 1.0f);
                m_boat->setPush(Vector3D(0, 0, lift*Gravity));
                break;
            }
            m_boat->setPush(Vector3D(0));
            point.z = ground + slot.mih;
            fly(point, slot.mih, slot.mah, slowSpeed(point, speed), true);
        }
        break;

    case OrderSlot::GotoWithInfiltrate:
        {
            // fly to beside the target, on the side it comes from, half its own hull clear of it,
            // then sit there with the nose to the target for dur seconds, counted while it is
            // there; then the target has been boarded (appendix J)
            ActiveObject *target = findUnit(slot.ene);
            if (!isAlive(target))
            {
                next();
                return;
            }
            m_unit->setTrigger(false);
            const SimpleBoat *targetBoat = dynamic_cast<const SimpleBoat *>(target);
            const float targetRadius = targetBoat ? targetBoat->hullRadius() : 0.5f*glm::length(target->box().dim());
            Vector3D away = m_unit->position() - target->position();
            away = glm::length(away) > 1e-3f ? glm::normalize(away) : Vector3D(0, -1, 0);
            const Vector3D dock = target->position() + away*(targetRadius + DockGap*hullRadius());
            const float distance = glm::length(dock - m_unit->position());
            if (m_docking && distance > arriveRadius(DockStay*m_boat->maxVelocity()))
                m_docking = false;
            else if (!m_docking && distance < arriveRadius(m_boat->maxVelocity()))
                m_docking = true;
            if (m_docking)
            {
                m_boat->pointAt(target->position(), 0);
                m_dockLeft -= m_scenario->frameTime();
                if (m_dockLeft < 0)
                {
                    target->eventBoard()->complete();
                    m_scenario->logEvent(target->name(), target->eventBoard(), txt::Fight10);
                    next();
                }
                break;
            }
            Vector3D point = dock;
            const float ground = m_scenario->surface().heightAt(point.x, point.y);
            point.z = glm::clamp(point.z, ground + slot.mih, std::max(ground + slot.mah, ground + slot.mih));
            fly(point, slot.mih, slot.mah, slowSpeed(point, speed), false);
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

            // closing in at the order's speed, slower over the last 75 m, never faster than 1.1
            // times the escorted unit (so an escort of a unit standing still doesn't move); in
            // place, it matches the escorted unit's heading, pitch and speed, pulled into its
            // place, and leaves formation beyond 1.3 times its top speed in a second
            const Vector3D station = escortStation(escorted, slot, !m_formation);
            const float distance = glm::length(station - m_unit->position());
            const float escortedSpeed = glm::length(escorted->velocity());
            if (m_formation && distance > arriveRadius(1.3f*m_boat->maxVelocity()))
                m_formation = false;
            else if (!m_formation && distance < arriveRadius(m_boat->maxVelocity()))
                m_formation = true;
            if (m_formation)
            {
                const Vector3D &own = m_boat->dir(), &theirs = escorted->dir();
                const float yaw = std::remainder(std::atan2(theirs.x, theirs.y) - std::atan2(own.x, own.y), glm::two_pi<float>());
                const float pitch = std::asin(glm::clamp(own.z, -1.0f, 1.0f)) - std::asin(glm::clamp(theirs.z, -1.0f, 1.0f));
                m_boat->turnBy(yaw, pitch, escortedSpeed);
                Vector3D pull = FormationPull*(station - m_unit->position()) + FormationPull*(escorted->velocity() - m_unit->velocity());
                const float most = m_boat->maxOrientationAcceleration();
                if (glm::length(pull) > most)
                    pull *= most/glm::length(pull);
                m_boat->setPush(pull);
            }
            else
            {
                m_boat->setPush(Vector3D(0));
                fly(station, slot.mih, slot.mah, std::min(speed*std::min(1.0f, distance/EscortSlowDistance), 1.1f*escortedSpeed), false);
            }
        }
        break;

    default:
        // Wait, and the orders that are not implemented yet
        m_unit->setTrigger(false);
        m_boat->hold();
    }
}


} // namespace fight
