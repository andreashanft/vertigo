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
    int hei = 0;       // height above ground in map height units
    int rad = 0;       // metres
    int mih = 0;       // height band above ground in map height units
    int mah = 0;
    int esc = -1;      // escorted unit
    Vector3D offset;   // escort offset in metres (xof, yof, zof)
    int ene = -1;      // enemy, used by the combat orders
    int car = 0;
    int con = 0;

    // the order ends early once this is met ("stop when", o<N>bccond...)
    int cond1 = 0, dep1 = 0, ref1 = -1, op = 0, cond2 = 0, dep2 = 0, ref2 = -1, del = 0;
    Condition *breakCondition = nullptr;

    static String name(int type);
    static bool isImplemented(int type);
};


// Runs a unit's orders slot by slot. Movement orders need a unit that can move (a SimpleBoat or Tank).
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
    void update();

private:
    enum class Attack { Approach, Attack, Break };

    void start(int slot);
    void next();
    // a point at the given height (in map height units) above the ground, looking ahead along the way
    float altitude(const Vector3D &point, int height) const;
    void pickPatrolPoint();
    Vector3D cellCentre(int x, int y) const;

    ActiveObject* findUnit(int id) const;
    bool isAlive(const ActiveObject *unit) const;
    ActiveObject* nearestHostile(float range, int sector) const;
    // where to aim at a moving target so that the shot meets it
    Vector3D lead(const ActiveObject *target) const;
    // the unit's gun points at the target (within a few degrees) and the target is in range
    bool aligned(const Vector3D &aim, const Vector3D &target, float range) const;

    void updateTurret(ActiveObject *target, float range, int sector);
    void updateAttack(ActiveObject *target, float speed);
    void startAttackState(Attack state, ActiveObject *target);

private:
    Scenario *m_scenario;
    ActiveObject *m_unit;
    SimpleBoat *m_boat;  // nullptr if the unit can't move
    std::vector<OrderSlot> m_slots;
    int m_current;
    float m_startTime;
    Vector3D m_point;    // current patrol or attack point
    int m_pointHeight;
    float m_pointTime;   // when the patrol point was picked

    Attack m_attack;
    float m_attackEnd;   // when the current attack state ends
    Vector3D m_offset;   // approach side, or the aim error for this attack run
    bool m_ambushed;     // an ambush has been triggered
    bool m_torpedoRun;   // this attack run may fire a torpedo
    ActiveObject *m_engaged; // Patrol and Escort: the hostile unit being attacked
    float m_nextScan;        // when the sensor looks for hostiles next
    float m_dockStart;   // GotoWithInfiltrate: when the unit docked at its target, -1 while on its way
    bool m_turretBusy;   // towers: the turret tracks a target
};


} // namespace fight


#endif // FIGHT_ORDERS_H
