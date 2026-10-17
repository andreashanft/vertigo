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


// A turret of the player's boat. Its order decides what it does: fire with the main gun (sync),
// attack the player's target, search and destroy, or nothing. The software (GLOBWEAP [SoftwareN])
// decides what it searches for (Claw: attacking torpedoes, Punisher: hostile vessels, Eraser: both),
// how often it scans, how long it locks on before firing, how accurately, and its volleys.
class PlayerTurret
{
public:
    enum class Order { Attack, Sync, Search, Stop };
    // GLOBWEAP [TurretStateN]
    enum class State { Rearming, Scanning, Firing, Jammed, Empty, Locking, Stopped, Sync, Volley };

public:
    PlayerTurret(Scenario *scenario, Player *player, int gunItem, int softwareItem, const Vector3D &mount, float defect);

public:
    Order order() const { return m_order; }
    void setOrder(Order order);
    State state() const { return m_state; }
    String stateName() const;
    String stateText() const;  // the state written out (GLOBWEAP verbose)
    String softwareName() const { return m_software.shortName; }
    const Gun& gun() const { return m_gun; }
    Gun& gun() { return m_gun; }
    // nullptr once the target is gone (a torpedo is deleted when it explodes)
    ActiveObject* target() const { return isValid(m_target) ? m_target : nullptr; }

    void update(float elapsedTime, bool playerFiring);

private:
    struct Software
    {
        String shortName;
        bool torpedoes = false;
        bool vessels = true;
        float hit = 0.8f;
        float scan = 0.5f;
        float lock = 2.0f;
        int volleyShots = 7;
        float volleyDelay = 1.0f;
    };

    ActiveObject* search() const;
    bool isValid(ActiveObject *target) const;
    bool isTorpedo(const ActiveObject *target) const;
    Vector3D mountPosition() const;
    Vector3D toWorld(const Vector3D &local) const;
    Vector3D toLocal(const Vector3D &world) const;
    // turns towards the world direction, returns whether the turret points there
    bool turnTo(const Vector3D &direction, float elapsedTime);

private:
    Scenario *m_scenario;
    Player *m_player;
    Gun m_gun;
    Software m_software;
    Vector3D m_mount;      // in the boat's coordinates
    Order m_order;
    State m_state;
    ActiveObject *m_target;
    bool m_targetIsTorpedo = false;
    float m_lockTime;
    float m_nextScan;
    int m_shots;           // fired in the current volley
    float m_volleyEnd;     // the pause after a volley ends then
    Vector3D m_aimError;
    Vector3D m_aim;        // where the turret points, in the boat's coordinates
};


} // namespace fight


#endif // FIGHT_PLAYERTURRET_H
