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

#ifndef FIGHT_PLAYER_H
#define FIGHT_PLAYER_H


#include "simpleboat.h"
#include "fight/weapons/gun.h"
#include "playerturret.h"
#include <memory>


namespace game { class Boat; }


namespace fight {


class Player : public SimpleBoat
{
public:
    enum EngineState { EngineOff, EngineStartup, EngineRunning, EngineShutdown };

public:
    Player(Scenario *scenario, game::Boat *boat, float yaw);
    ~Player();

public:
    int shield(int index) const { return m_shield[index]; };
    const Gun& playerGun() const { return m_gun; }

    // torpedoes: Tab fires the selected one, Enter selects the next; homing ones need the target
    // locked (and in front) for their lock time; Backspace/D drops a buzzer (a decoy)
    void fireTorpedo();
    void selectNextTorpedo();
    void dropBuzzer();
    const std::vector<int>& torpedoes() const { return m_torpedoItems; } // item numbers 1-22
    int selectedTorpedo() const { return m_selectedTorpedo; }            // index, -1 if none
    bool torpedoReady() const;
    ActiveObject* lockTarget() const { return m_lockTarget; }
    float lockProgress() const; // 0-1, 1 when a homing torpedo would follow the target
    float lockRemaining() const; // seconds until the lock is complete
    PlayerTurret* turret(int index) const { return index >= 0 && index < 2 ? m_turrets[index].get() : nullptr; }
    int shieldMax(int index) const { return m_shieldMax[index]; };
    bool engineRunning() const { return m_engineState == EngineRunning || m_engineState == EngineStartup; }
    float roll() const { return m_roll; };
    float pitch() const; // the nose's angle above the horizon in degrees
    // the ticks per second at which the turn rates relax towards the stick (appendix L)
    static float TurnTickRate;
    // mission time of the last hit, -1 if never hit
    // a side's armor is below 10 %: ALERT, with the "system collapse" voice when it starts
    bool isCritical() const;
    float speed() const { return m_speed; };
    float noise() const;
    float range() const;

    // the booster (appendix K): while Q is held it runs as long as the energy lasts
    void setBoostKey(bool held) { m_boostKey = held; }
    bool boosting() const { return m_boosting; }

public:
    void damage(int kinetic, int shock, const Vector3D &position, ActiveObject *attacker = nullptr);
    void destroy();
    bool update(float elapsedTime);

    void engineToggle();
    void fire();
    void fireStop();

private:
    game::Boat *m_boat;
    std::vector<int> m_torpedoItems;
    int m_selectedTorpedo;
    float m_torpedoReady;
    float m_rearmingTime;
    ActiveObject *m_lockTarget;
    float m_lockTime;
    bool m_lockAnnounced;
    int m_threats;
    bool m_threatBlink;
    bool m_critical = false;
    bool m_alertBlink = false;
    bool m_radiation = false;
    float m_nextRadiationVoice = 0;
    float m_nextRadiationBeep = 0;

    int m_shield[4];
    bool m_grounded = false;    // touching the ground in the last step
    int m_shieldMax[4];

    EngineState m_engineState;
    float m_engineTime;

    float m_speed;
    bool m_hasBooster;
    bool m_boostKey = false;
    bool m_boosting = false;

    float m_roll;
    float m_turnTicks = 0;  // ticks due since the last relaxation
    float m_rollCenterVelocity;
    float m_bottomAlphaSupport;

    float m_currentTime;
    Vector3D m_currentNoise1;
    Vector3D m_currentNoise2;
    Vector3D m_currentAmplitude;

    bool m_firing;
    Gun m_gun;
    std::unique_ptr<PlayerTurret> m_turrets[2];
};


} // namespace fight


#endif // FIGHT_PLAYER_H
