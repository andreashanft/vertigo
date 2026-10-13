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
#include <QString>
#include <vector>


namespace fight {


class Condition;
class Object;
class Scenario;
class SimpleBoat;


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

    static QString name(int type);
    static bool isImplemented(int type);
};


// Runs a unit's orders slot by slot.
class Orders
{
public:
    Orders(Scenario *scenario, SimpleBoat *boat, const std::vector<OrderSlot> &orderSlots);

public:
    // the current slot, or -1 after the last one
    int current() const { return m_current; }
    const OrderSlot* currentSlot() const { return m_current >= 0 ? &m_slots[m_current] : nullptr; }
    void update();

private:
    void start(int slot);
    void next();
    // a point at the given height (in map height units) above the ground, looking ahead along the way
    float altitude(const Vector3D &point, int height) const;
    void pickPatrolPoint();

private:
    Scenario *m_scenario;
    SimpleBoat *m_boat;
    std::vector<OrderSlot> m_slots;
    int m_current;
    float m_startTime;
    Vector3D m_point;  // current patrol point
    int m_pointHeight;
    float m_pointTime; // when the patrol point was picked
};


} // namespace fight


#endif // FIGHT_ORDERS_H
