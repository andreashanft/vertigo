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

#ifndef FIGHT_ORDERS_H
#define FIGHT_ORDERS_H


#include "util/geometry3d.h"
#include "util/string.hpp"
#include "txt/stringtable.h"
#include <glm/mat3x3.hpp>
#include <vector>


namespace fight {


class Condition;
class Object;
class Scenario;
class SimpleBoat;
class ActiveObject;


// One of the up to 8 order slots of a movable (o<N>... keys), see docs/ROADMAP.md, appendix B.
struct OrderSlot
{
    enum Type { None, Wait, Goto, Patrol, Attack, Escort, Ambush, CardinalPatrol, CardinalAttack, GotoWithArrive, GotoWithInfiltrate, Explode };

    int type = None;
    int dur = 0;       // seconds, 100000 and more means forever
    int refx = 0;      // map cell
    int refy = 0;
    int vel = 100;     // percent of the unit's maximum velocity
    int hei = 0;       // height above ground in metres
    int rad = 0;       // metres
    int mih = 0;       // height band above ground in metres
    int mah = 0;
    int esc = -1;      // escorted unit
    Vector3D offset;   // escort offset in metres (xof, yof, zof)
    int ran = 150;     // Escort: how far from its place the escort may stray to fight (metres)
    int ene = -1;      // enemy, used by the combat orders
    int car = 0;       // a quarter turn (2, 4, 6; anything else is straight ahead), see yaw()
    int con = 0;       // Explode: 0 timer unless boarded, 1 boarded then timer, 2 timer

    // the heading car stands for, in radians (0 is +y, increasing to the right)
    float yaw() const;

    // the order ends early once this is met ("stop when", o<N>bccond...)
    int cond1 = 0, dep1 = 0, ref1 = -1, op = 0, cond2 = 0, dep2 = 0, ref2 = -1, del = 0;
    Condition *breakCondition = nullptr;

    static String name(int type);
    static bool isImplemented(int type);
};


// Runs a unit's orders slot by slot. Movement orders need a unit that can move (a SimpleBoat or Tank).
// How the orders and the fighting go is in docs/ROADMAP.md, appendix J.
class Orders
{
public:
    Orders(Scenario *scenario, ActiveObject *unit, const std::vector<OrderSlot> &orderSlots);

public:
    // the current slot, or -1 after the last one
    int current() const { return m_current; }
    const OrderSlot* currentSlot() const { return m_current >= 0 ? &m_slots[m_current] : nullptr; }
    // a short description of what the unit does, for the debug overlay
    String state() const;
    // what the unit does, as the master monitor shows it (FIGHTEXT 72-84: attacking, waiting,
    // guarding, on its way, escorting, infiltrating, exploding)
    txt::StringFight activity() const;
    // the unit it is docking on (GotoWithInfiltrate), which it mustn't bump into
    ActiveObject* dockingTarget() const;
    // the unit may be put to sleep: boats always but in an Explode order, towers while they have no target
    bool mayPause() const;
    // begins the current slot again (after the unit has slept)
    void restart() { if (m_current >= 0) start(m_current); }
    // the buzzer magazine works only in patrols, attacks, ambushes and escorts (appendix J)
    bool usesBuzzers() const;

    // sensing, also for a bomber's turrets, which look with their own sensor through an Orders
    // without slots
    ActiveObject* findUnit(int id) const;
    bool isAlive(const ActiveObject *unit) const;
    bool isHostile(const ActiveObject *unit) const;
    // whether the unit's sensor picks the other up within rangeFactor times its range (appendix J)
    bool detects(const ActiveObject *unit, float rangeFactor) const;
    // what the sensor finds, looked for every 1.5 s (0.7 s with sen 3 and 4)
    ActiveObject* scan();
    // whoever hit the unit just now, if hostile and detected within rangeFactor times the range
    ActiveObject* shooter(const ActiveObject *unit, float rangeFactor) const;
    // takes the target, or nullptr if its list of attackers is full (the player's by difficulty,
    // anyone else's at 4); lets go of the previous target's list
    ActiveObject* engage(ActiveObject *target);
    void update();

private:
    // the phases of a boat's fight (appendix J): it aligns on a side of the target, runs at it
    // turning in place or flying, breaks off, and aligns again
    enum class Fight { None, Align, Turn, Run, Flight };
    enum Side { Front, Left, Back, Right };

    void start(int slot);
    void next();

    // --- flying
    // the game's cells and the unit's size set how near is "there" and where it starts slowing down
    float arriveRadius(float extra) const;
    float slowSpeed(const Vector3D &point, float speed) const;
    // a point over the ground: at least minHeight metres plus half a hull above the highest ground
    // on the way, at most maxHeight above it (0: no cap); for ground vehicles
    Vector3D inBand(Vector3D point, float minHeight, float maxHeight) const;
    float groundAhead(const Vector3D &point) const;
    // flies for the point at the speed (appendix J): bent away from other units ahead if avoid,
    // then kept in the height band, minHeight plus hullFactor hulls to maxHeight over the ground
    // (maxHeight < 0: no cap); true if the band changed the way
    bool fly(const Vector3D &point, float minHeight, float maxHeight, float speed, bool avoid, float hullFactor = 0.599f);
    // the band's check: the hull's front bottom corners a little ahead against the ground; true
    // and the point moved if they leave it
    bool bandPoint(Vector3D &point, float zmin, float zmax) const;
    Vector3D avoidUnits(const Vector3D &point) const;
    // a ground vehicle's way (appendix J): bent around units ahead, and sideways off ground steeper
    // than its climbing angle
    Vector3D groundWay(const Vector3D &point) const;
    float hullRadius() const;
    // a new point of the patrol area, at least minFraction of the radius from the current one
    void pickPatrolPoint(float minFraction);
    // a tank's: on its arc of a circle round the tank inside the area (appendix J)
    void pickTankPoint(float speed);
    Vector3D cellCentre(int x, int y) const;
    // the target's frame: right, forward and up
    static glm::mat3 frameOf(const ActiveObject *unit);
    static Vector3D toWorld(const ActiveObject *unit, const Vector3D &local);
    static Vector3D toLocal(const ActiveObject *unit, const Vector3D &world);

    // --- sensing
    int sensorType() const;
    float sensorRange() const;
    bool hears(const ActiveObject *unit, float distance2) const;
    bool sees(const ActiveObject *unit) const;
    static bool isBuzzer(const ActiveObject *unit);
    // the unit to fight: the ordered enemy in sensor range, whoever hit it at double range, the scan
    ActiveObject* acquire(ActiveObject *enemy);
    // an escort's target (appendix J): whoever hits it or its charge, or attacks its charge
    ActiveObject* escortTarget(const ActiveObject *escorted);
    // whether the fight goes on: the enemy and a player who shoots take precedence, a target out of
    // double sensor range is lost
    bool keepTarget(ActiveObject *enemy);
    void dropTarget();
    bool freshHit();
    void switchTarget(ActiveObject *target);
    // the escort's place; approach: a place ahead of the escorted unit moves to just behind it
    Vector3D escortStation(const SimpleBoat *escorted, const OrderSlot &slot, bool approach) const;

    // --- fighting
    // where the shots go: the target, a little above a ground unit
    Vector3D aimPoint(const ActiveObject *target) const;
    // fires a torpedo when the nose points at the target closely enough (appendix J)
    bool tryTorpedo(ActiveObject *target);
    void startFight(Fight phase, ActiveObject *target);
    void updateFight(ActiveObject *target, float speed);
    // tanks: an align point at the align radius, 20-30 degrees round the target from where the tank
    // is, the way round by the Hook/Cross/Retain/Change probabilities; then a drive at the target
    void startTankFight(Fight phase, ActiveObject *target, float speed);
    void updateTankFight(ActiveObject *target, float speed);
    // towers (appendix J): CardinalPatrol and CardinalAttack sweep and fight with the tower's sensor
    void updateTower(const OrderSlot &slot);
    void sweepTurret(const OrderSlot &slot);
    static Vector3D intercept(const Vector3D &origin, const ActiveObject *target, float speed);
    float random() const;
    float random(float min, float max) const;

private:
    Scenario *m_scenario;
    ActiveObject *m_unit;
    SimpleBoat *m_boat;  // nullptr if the unit can't move
    std::vector<OrderSlot> m_slots;
    int m_current;
    float m_startTime;
    Vector3D m_point;    // current patrol point
    float m_pointTime;   // when the patrol began or resumed: past the time to fly the radius, a new point every frame
    bool m_arrived = false; // a big ship at its patrol cell

    // sensing
    ActiveObject *m_target;      // the unit fought
    ActiveObject *m_engaged = nullptr;   // whose list of attackers the unit is on (not the player's)
    ActiveObject *m_scanned;     // what the last scan found
    float m_nextScan;

    // the fight
    Fight m_fight;
    float m_fightStart;          // when the phase began
    int m_side;                  // Align: the side of the target to reach ...
    Vector3D m_alignOffset;      // ... and the point there, in the target's frame
    float m_alignEnd;
    Vector3D m_runStart;         // Turn and Run: where the run began, in the target's frame
    float m_runDistance;         // and how far from the target that was
    float m_attackEnd;
    float m_flightRadius;        // the run ends this close to the target
    float m_attackSpeed;
    int m_shotsBefore;           // the gun's shots when the run began
    bool m_breakOnHit;           // this run ends with the first hit taken after a shot
    Vector3D m_flightOffset;     // Flight: where to, in the target's frame
    float m_flightEnd;
    float m_lastFlip;            // when the flight last changed sides
    float m_targetYaw;           // tanks: the target's heading on the frame before
    float m_pointEnd = 0;        // tanks: when the patrol's point is given up
    bool m_torpedo;              // this phase may fire a torpedo
    float m_hitAnswered = -1;        // the hit the fight last answered

    bool m_lying;        // Ambush: in position, lying silent
    bool m_formation;    // Escort: in place beside the escorted unit
    float m_dockLeft;    // GotoWithInfiltrate: the docking time still to sit out
    bool m_docking = false;  // and whether it sits beside its target
    bool m_turretBusy;   // towers: the turret tracks a target
    float m_sweepYaw;    // the cardinal orders: where the sweeping turret turns to
    float m_nextSweep;   // when it turns next, once there
    int m_sweepDirection = 0;    // the next step's way after an edge of the sweep's half circle
    bool m_sweepWaiting = false; // there, pausing
    bool m_sweepHome = true;     // back to the centre first (at the start, after a lost target)
    float m_lastCentre = 0;      // the centre of the last cardinal order (a Wait faces it)
    ActiveObject *m_enemy = nullptr; // CardinalAttack: the enemy, chosen at the order's start
};


} // namespace fight


#endif // FIGHT_ORDERS_H
