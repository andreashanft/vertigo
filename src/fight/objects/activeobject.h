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

#ifndef FIGHT_ACTIVEOBJECT_H
#define FIGHT_ACTIVEOBJECT_H


#include "object.h"
#include <memory>
#include <algorithm>
#include "util/string.hpp"


namespace txt { class DesFile; }


namespace fight {


class Scenario;
class UnitGun;
class Orders;


class ActiveObject : public Object
{
public:
    struct ObjectInfo
    {
        int iff;
        String name;
        String cargo;
        int sensor;
        int sensorRange;
        bool sleeps;     // may sleep out of the player's sonar range (sle)

        ObjectInfo() : iff(0), name(""), cargo(""), sensor(0), sensorRange(0), sleeps(true) { }
    };

public:
    ActiveObject(Scenario *scenario);
    ActiveObject(Scenario *scenario, txt::DesFile &file, const ObjectInfo &info);
    ~ActiveObject();

public:
    ConditionEvent* eventDestroy() { return &m_eventDestroy; }
    ConditionEvent* eventAttack() { return &m_eventAttack; }
    ConditionEvent* eventIdentify() { return &m_eventIdentify; }
    ConditionEvent* eventParalyze() { return &m_eventParalyze; }
    ConditionEvent* eventFinish() { return &m_eventFinish; }
    ConditionEvent* eventBoard() { return &m_eventBoard; }

    int iff() const { return m_info.iff; }
    const String& name() const { return m_info.name; }
    const String& cargo() const { return m_info.cargo; }
    int sensor() const { return m_info.sensor; }
    int sensorRange() const { return m_info.sensorRange; }
    virtual float noise() const { return m_noise + (m_info.sensor > 1 ? 1 : 0); }
    bool isIdentified() const { return m_eventIdentify.isCompleted(); }

    int kineticShield() const { return m_kineticShield; };
    int shockShield() const { return m_shockShield; };
    int kineticShieldMax() const { return m_kineticShieldMax; };
    int shockShieldMax() const { return m_shockShieldMax; };
    // the scenario's armour (inte) and energy defence (eint) of the unit, in percent of its type's
    void setDefence(int armour, int energy);

    const Vector3D& velocity() const { return m_velocity; }

public:
    Orders* orders() const { return m_orders.get(); }
    void setOrders(Orders *orders);
    bool update(float elapsedTime);

    // the unit's gun (none by default): model from the mission's "gun" key, -1 for the file's default
    virtual void arm(int model, txt::DesFile &file) {}
    UnitGun* gun() const { return m_unitGun.get(); }
    void setTrigger(bool trigger) { m_trigger = trigger; }
    bool trigger() const { return m_trigger; }
    // units with a turret turn it towards the point
    virtual void aimAt(const Vector3D &point) {}

    // torpedoes (the mission's tor and ntor keys); fireTorpedo() launches one when reloaded,
    // following the target if the type homes, otherwise straight along the direction
    void setTorpedoes(int type, int count) { m_torpedoType = type; m_torpedoCount = count; }
    bool hasTorpedoes() const { return m_torpedoCount > 0 && m_torpedoType >= 0; }
    int torpedoType() const { return m_torpedoType; }
    bool fireTorpedo(ActiveObject *target, const Vector3D &position, const Vector3D &direction);
    // the units running a torpedo attack on this one: at most 8 at a time, for every target
    // (docs/ROADMAP.md, appendix J); false if the list is full
    bool engageTorpedo(ActiveObject *attacker);
    void disengageTorpedo(ActiveObject *attacker);

    // attacker: who fired or rammed, nullptr for damage nobody caused (e.g. crashing into something)
    virtual void damage(int kinetic, int shock, const Vector3D &position, ActiveObject *attacker = nullptr);
    virtual void destroy();
    // an Explode order, and an energy defence that has collapsed: three explosions, then the unit
    // tumbles away with a random spin (a boat; anything else is simply gone) and, as its armour
    // was never taken down, doesn't count as destroyed for the mission's conditions
    // (docs/ROADMAP.md, appendix J)
    void explode();
    bool isTumbling() const { return m_tumbling; }
    virtual void identify();
    // whether the sonar shows it (a cargo ship shows its blocks instead)
    virtual bool isContact() const { return true; }
    // a picture (GFX/IMG/COCKPIT) the master monitor shows instead of the model, e.g. shpcontf
    virtual String observationImage() const { return String(); }
    // the unit isn't in the mission at this difficulty: it never appears (see ConditionEntry::absent)
    void leaveMission();

    // Sleep (docs/ROADMAP.md, appendix J): a unit with sle 1 (or no sle) stops altogether while its
    // horizontal distance from the player exceeds the player's sonar range plus 3 map cells, and
    // starts its current order slot again when the player comes back within that radius.
    bool sleeps() const { return m_info.sleeps; }
    bool isAsleep() const { return m_asleep; }
    // whether the unit may go to sleep right now (boats not while exploding, towers not while attacking)
    virtual bool canSleep() const;
    void sleep();
    void wake();

    // Energy (docs/ROADMAP.md, appendix K): one pool per unit, 0 to 1, full at the start. The engine
    // model (a unit's eng key, the boat's engine) sets the seconds it takes to refill from empty:
    // 5, 3, 1, 1, 1 and 0.01 for engines 0 to 5. Energy guns draw from it, so does the booster.
    void setEngine(int model);
    int engine() const { return m_engine; }
    float energy() const { return m_energy; }
    void drawEnergy(float amount) { m_energy = std::max(0.0f, m_energy - amount); }
    void rechargeEnergy(float elapsedTime);
    static float rechargeTime(int engine);

protected:
    // the explosions and debris shared by destroy() and explode(), and the going
    void blowUp(int explosions);
    void vanish();
    // starts the tumble; false if the unit can't (then it vanishes)
    virtual bool startTumbling() { return false; }

protected:
    ConditionEvent m_eventDestroy;
    ConditionEvent m_eventAttack;
    ConditionEvent m_eventIdentify;
    ConditionEvent m_eventParalyze;
    ConditionEvent m_eventFinish;
    ConditionEvent m_eventBoard;

    ObjectInfo m_info;
    float m_noise;
    bool m_identified;
    bool m_asleep = false;
    bool m_tumbling = false;
    int m_engine = 0;
    float m_energy = 1.0f;

    int m_kineticShield;
    int m_shockShield;
    int m_kineticShieldMax;
    int m_shockShieldMax;

    Vector3D m_velocity;

    std::unique_ptr<Orders> m_orders;
    std::unique_ptr<UnitGun> m_unitGun;
    bool m_trigger = false;
    int m_torpedoType = -1;
    int m_torpedoCount = 0;
    std::vector<ActiveObject*> m_torpedoAttackers;
    float m_torpedoReady = 0;
};


} // namespace fight


#endif // FIGHT_ACTIVEOBJECT_H
