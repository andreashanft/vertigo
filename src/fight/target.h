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

#ifndef FIGHT_TARGET_H
#define FIGHT_TARGET_H


#include "util/geometry3d.h"

#include <vector>


namespace fight {


class Scenario;
class Object;
class ActiveObject;
class NavPoint;


class Target
{
public:
    Target(Scenario *scenario);

public:
    bool isLocked() const { return m_locked != nullptr || m_lockedNavPoint != nullptr; }
    Vector3D position() const;
    Object* object() const;

    float distance() const { return m_distance; }
    bool isPassive() const { return m_isPassive; }
    void update(float distance, bool isPassive) { m_distance = distance; m_isPassive = isPassive; }

    ActiveObject* locked() const { return m_locked; }
    NavPoint* lockedNavPoint() const { return m_lockedNavPoint; }

    void lockReset();
    // Each lock returns whether it locked anything.
    // the contact or nav point in the reticle (L)
    bool lockReticle();
    // the nearest of the units attacking the player that the sonar has (U)
    bool lockNearestEnemy();
    // the next of the same units, in the sonar's order, the first one after the last (J)
    bool lockNextAttacker();
    // the next hostile torpedo on its way (G)
    bool lockNextTorpedo();
    // the next contact, friend or foe, that is no torpedo, then the enabled nav points (T)
    bool lockNextObject();
    void lockNavPoint(NavPoint *navPoint);

private:
    // locks the one after the locked one among these, the first if none of them is locked
    bool lockNext(const std::vector<Object *> &objects);

private:
    Scenario *m_scenario;
    ActiveObject *m_locked;
    NavPoint *m_lockedNavPoint;
    float m_distance;
    bool m_isPassive;
};


}


#endif // FIGHT_TARGET_H
