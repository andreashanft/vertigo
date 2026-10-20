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

    // torpedoes: Tab fires the loaded one, Enter selects the next; homing ones need the target
    // locked (and in front) for their lock time; Backspace/D drops a buzzer (a decoy)
    void fireTorpedo();
    void selectNextTorpedo();
    void dropBuzzer();
    const std::vector<int>& torpedoes() const { return m_torpedoItems; } // item numbers 1-22
    int selectedTorpedo() const { return m_selectedTorpedo; }            // index, -1 if none
    // the magazine (docs/ROADMAP.md, appendix S), its states numbered as GLOBWEAP's
    // [TorpedoMagazineStateN]: ready, rearming (loading the selected torpedo), jammed, empty, and
    // waiting the 2 s before a different torpedo is loaded, during which the loaded one still fires
    enum class MagazineState { Ready = 0, Rearming = 1, Jammed = 3, Empty = 4, Waiting = 5 };
    MagazineState magazineState() const { return m_magazineState; }
    bool torpedoReady() const { return m_magazineState == MagazineState::Ready; }
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

    // the Fixer, the repair robot (docs/ROADMAP.md, appendix O): the system it mends now and the
    // one the pilot forced with Alt+I (0 none/automatic, 1 armour, 2 gun, 3 torpedo magazine,
    // 4 and 5 the turrets)
    bool hasFixer() const { return m_fixer; }
    int fixerSystem() const { return m_fixerSystem; }
    int fixerForced() const { return m_fixerForced; }
    void cycleFixer();
    // the torpedo magazine's condition, 0 to 1 (the Fixer mends it; nothing else uses it yet)
    float magazineCondition() const { return m_magazineCondition; }
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
    int m_loadedTorpedo;        // index, -1 while none is in the tube
    MagazineState m_magazineState;
    float m_magazineTime = 0;   // when the waiting, rearming or jam ends
    float m_rearmingTime;
    float m_magazineDamping = 0;
    float m_magazineDelay = 0;
    static constexpr float MagazineWait = 2.0f;   // s before a different torpedo is loaded
    void updateMagazine();
    ActiveObject *m_lockTarget;
    float m_lockTime;
    bool m_lockAnnounced;
    int m_threats;
    bool m_threatBlink;
    int m_attackers = 0;     // how many units fought the player last frame
    bool m_critical = false;
    bool m_alertBlink = false;
    bool m_radiation = false;
    // near the surface (docs/ROADMAP.md, appendix D): held below it from 40 m up until 70 m down,
    // never above 10 m
    static constexpr float SurfaceHoldDepth = 40;
    static constexpr float SurfaceReleaseDepth = 70;
    static constexpr float SurfaceLimitDepth = 10;
    bool m_surfaceHold = false;
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

    // the Fixer
    void updateFixer();
    int fixerChoice(float threshold) const;
    // the armour sides' and the systems' conditions, kept on the boat for after the mission
    void storeCondition();
    bool m_fixer = false;
    int m_fixerSystem = 1;      // the Fixer starts on the armour
    int m_fixerForced = 0;
    int m_fixerSide = -1;        // the armour side under repair
    float m_fixerNext = 0;       // when the next step is due
    float m_magazineCondition = 1;
};


} // namespace fight


#endif // FIGHT_PLAYER_H
