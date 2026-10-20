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

#ifndef FIGHT_PLAYERTURRET_H
#define FIGHT_PLAYERTURRET_H


#include "fight/weapons/gun.h"
#include "util/string.hpp"


namespace fight {


class ActiveObject;
class Player;
class Scenario;


// A turret of the player's boat (docs/ROADMAP.md, appendix T). Its order decides what it does:
// fire with the main gun (sync), attack the player's target, search and destroy, or nothing. The
// software (GLOBWEAP [SoftwareN]) decides what it searches for among the units and torpedoes
// after the player (Claw: torpedoes, Punisher: vessels, Eraser: both), how often it searches, how
// long it locks on before firing, how far its shots scatter, and its volleys. Without software it
// can only fire in sync.
class PlayerTurret
{
public:
    enum class Order { Attack, Sync, Search, Stop };
    // GLOBWEAP [TurretStateN]
    enum class State { Rearming, Scanning, Firing, Jammed, Empty, Locking, Stopped, Sync, Volley };

public:
    // side 0 covers the upper half round the boat, 1 the lower
    PlayerTurret(Scenario *scenario, Player *player, int gunItem, int softwareItem, const Vector3D &mount, int side, float defect);

public:
    Order order() const { return m_order; }
    // false (and nothing changes) for an attack or search order without software
    bool setOrder(Order order);
    bool hasSoftware() const { return m_software.torpedoes || m_software.vessels; }
    State state() const { return m_state; }
    String stateName() const;
    String stateText() const;  // the state written out (GLOBWEAP verbose)
    String softwareName() const { return m_software.shortName; }
    const Gun& gun() const { return m_gun; }
    Gun& gun() { return m_gun; }
    // the target it has locked on, nullptr while it has none (a torpedo is deleted when it explodes)
    ActiveObject* target() const { return isValid(m_target) ? m_target : nullptr; }

    void update(float elapsedTime, bool playerFiring);

private:
    struct Software
    {
        String shortName;
        bool torpedoes = false;
        bool vessels = false;
        float hit = 0.8f;
        float scan = 0.5f;
        float lock = 2.0f;
        int volleyShots = 7;
        float volleyDelay = 1.0f;
    };

    ActiveObject* search() const;
    bool keeps(ActiveObject *target) const;
    bool onSide(const ActiveObject *target) const;
    bool inSight(const ActiveObject *target) const;
    bool isValid(ActiveObject *target) const;
    bool isTorpedo(const ActiveObject *target) const;
    bool torpedoAfterPlayer() const;
    void release();
    void fireAt(ActiveObject *target);
    // a unit or building whose hits would be friendly fire within the distance along the way
    bool friendInLine(const Vector3D &from, const Vector3D &dir, float distance, const ActiveObject *target) const;
    Vector3D mountPosition() const;

private:
    Scenario *m_scenario;
    Player *m_player;
    Gun m_gun;
    Software m_software;
    Vector3D m_mount;      // in the boat's coordinates
    int m_side;
    Order m_order;
    State m_state;
    ActiveObject *m_target;
    ActiveObject *m_attackTarget;  // the player's target when the attack order was given
    bool m_targetIsTorpedo = false;
    float m_lockEnd;       // the lock-on ends then
    float m_nextScan;
    int m_volleyLeft;      // shots left in the current volley after the next
    float m_volleyEnd;     // the pause after a volley ends then
};


} // namespace fight


#endif // FIGHT_PLAYERTURRET_H
