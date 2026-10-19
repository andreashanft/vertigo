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

#include "activeobject.h"
#include "fight/scenario.h"
#include "sfx/samplemap.h"
#include "txt/stringtable.h"
#include "fight/effects/effectmanager.h"
#include "fight/weapons/unitgun.h"
#include "fight/orders.h"
#include "fight/weapons/torpedo.h"
#include "fight/boats/simpleboat.h"
#include "util/random.h"
#include <glm/geometric.hpp>
#include <algorithm>
#include <limits>


namespace fight {


ActiveObject::ActiveObject(Scenario *scenario) :
    Object(scenario)
{
    m_shockShield = m_shockShieldMax = 10000000;
}


ActiveObject::ActiveObject(Scenario *scenario, txt::DesFile &file, const ObjectInfo &info) :
    Object(scenario, file),
    m_info(info),
    m_identified(false)
{
    file.setSection("noise");
    m_noise = file.value("level").toFloat();

    file.setSection("defense");
    m_kineticShield = m_kineticShieldMax = file.value("kineticshield0").toInt();
    m_shockShield = m_shockShieldMax = file.value("shockshield0", 10000000).toInt();
}


ActiveObject::~ActiveObject()
{
}


void ActiveObject::setDefence(int armour, int energy)
{
    // (10000000, which makes a type indestructible or immune, stays at least a million)
    auto scale = [](int value, int percent) {
        return int(std::min<long long>(std::numeric_limits<int>::max(), (long long)value*percent/100));
    };
    m_kineticShield = m_kineticShieldMax = scale(m_kineticShieldMax, armour);
    m_shockShield = m_shockShieldMax = scale(m_shockShieldMax, energy);
}


void ActiveObject::setOrders(Orders *orders)
{
    m_orders.reset(orders);
}


bool ActiveObject::fireTorpedo(ActiveObject *target, const Vector3D &position, const Vector3D &direction)
{
    const TorpedoType *type = TorpedoType::get(m_torpedoType);
    if (!type || m_torpedoCount <= 0 || m_scenario->time() < m_torpedoReady)
        return false;
    m_scenario->launchTorpedo(m_torpedoType, position, direction, glm::length(m_velocity), this, type->lockType != 0 ? target : nullptr);
    m_torpedoCount--;
    // the next one 2 to 3.5 s later
    m_torpedoReady = m_scenario->time() + 2.0f + 1.5f*util::random()/float(RAND_MAX);
    return true;
}


bool ActiveObject::hitRecently() const
{
    return m_hitTime >= 0 && m_scenario->time() - m_hitTime < 0.1f;
}


bool ActiveObject::update(float elapsedTime)
{
    if (m_orders)
        m_orders->update();
    return false;
}


void ActiveObject::damage(int kinetic, int shock, const Vector3D &position, ActiveObject *attacker)
{
    if (attacker && attacker != this)
    {
        if (!m_eventAttack.isCompleted())
            m_scenario->logEvent(name(), &m_eventAttack, txt::Fight6);
        m_eventAttack.complete();
    }
    // the AI feels a hit when its energy defence changes, so shells without shock go unnoticed
    // (docs/ROADMAP.md, appendix J)
    if (shock != 0)
        m_hitTime = m_scenario->time();
    const bool wasDestroyed = m_eventDestroy.isCompleted();
    m_kineticShield -= kinetic;
    m_shockShield -= shock;
    if (m_kineticShield < 0)
        destroy();
    else if (m_shockShield < 0 && !m_eventParalyze.isCompleted())
    {
        m_eventParalyze.complete();
        m_scenario->logEvent(name(), &m_eventParalyze, txt::Fight8);
        explode();
    }

    if (attacker && attacker != this && attacker == m_scenario->player())
        m_scenario->playerHit(this, !wasDestroyed && m_eventDestroy.isCompleted());
}


bool ActiveObject::engageTorpedo(ActiveObject *attacker)
{
    m_torpedoAttackers.erase(std::remove_if(m_torpedoAttackers.begin(), m_torpedoAttackers.end(), [](ActiveObject *unit) {
        return !unit->isEnabled() || unit->isAsleep() || unit->isTumbling();
    }), m_torpedoAttackers.end());
    if (std::find(m_torpedoAttackers.begin(), m_torpedoAttackers.end(), attacker) != m_torpedoAttackers.end())
        return true;
    if (m_torpedoAttackers.size() >= 8)
        return false;
    m_torpedoAttackers.push_back(attacker);
    return true;
}


void ActiveObject::disengageTorpedo(ActiveObject *attacker)
{
    m_torpedoAttackers.erase(std::remove(m_torpedoAttackers.begin(), m_torpedoAttackers.end(), attacker), m_torpedoAttackers.end());
}


void ActiveObject::destroy()
{
    if (m_eventDestroy.isCompleted())
        return;
    if (!startWreck())
    {
        blowUp(1);
        vanish();
    }
    m_eventDestroy.complete();
    m_scenario->logEvent(name(), &m_eventDestroy, txt::Fight5);
}


void ActiveObject::explode()
{
    if (!m_enabled || m_eventDestroy.isCompleted() || m_tumbling)
        return;
    if (!startTumbling())
    {
        blowUp(3);
        vanish();
    }
}


void ActiveObject::wreck()
{
    if (!m_enabled || m_eventDestroy.isCompleted() || m_tumbling)
        return;
    if (!startWreck())
    {
        blowUp(3);
        vanish();
    }
}


void ActiveObject::blowUp(int explosions)
{
    // explosion sized to the object: small, medium or large flash
    const Vector3D dim = m_box.dim();
    const float size = std::max(dim.x, std::max(dim.y, dim.z));
    const Effects explosion = size < 8.0f ? Effects::Explosion_0 : size < 20.0f ? Effects::Explosion_1 : Effects::Explosion_2;
    const Vector3D at = center();
    m_scenario->effectManager().addEffect(explosion, at);
    // more of them spread over the object
    for (int i = 1; i < explosions; i++)
    {
        const Vector3D spread(util::random()/float(RAND_MAX) - 0.5f, util::random()/float(RAND_MAX) - 0.5f, util::random()/float(RAND_MAX) - 0.5f);
        m_scenario->effectManager().addEffect(explosion, at + spread*size);
    }
    sfx::SampleMap::playAt(size < 20.0f ? sfx::Sample::Explosion_Small : sfx::Sample::Explosion_Big, at.x, at.y, at.z);
    m_scenario->explosionAt(at, size < 8.0f ? 0.3f : size < 20.0f ? 0.6f : 1.0f);

    // debris (DEBRIS.DES): plates 0-2, engine 3, small parts 4-8, large parts 9-13, bodies 14-16
    auto random = [](int min, int max) { return min + util::random()%(max - min + 1); };
    auto direction = []() {
        const Vector3D d(util::random()/float(RAND_MAX) - 0.5f, util::random()/float(RAND_MAX) - 0.5f, 0.5f*util::random()/float(RAND_MAX));
        return glm::length(d) > 0.01f ? glm::normalize(d) : Vector3D(0, 0, 1);
    };
    auto debris = [this, &direction](int index) {
        m_scenario->effectManager().addDebris(Effects(int(Effects::Debris_0) + index), center(), direction());
    };
    const int pieces = size < 8.0f ? 4 : size < 20.0f ? 6 : 10;
    for (int i = 0; i < pieces; i++)
        debris(i % 3 == 0 ? random(0, 2) : size >= 8.0f && i % 3 == 1 ? random(9, 13) : random(4, 8));
    const SimpleBoat *vessel = dynamic_cast<const SimpleBoat *>(this);
    if (vessel)
    {
        debris(3);
        // crewed vessels
        if (!vessel->isGroundVehicle() || size >= 8.0f)
            for (int i = random(1, 3); i > 0; i--)
                debris(random(14, 16));
    }
}


void ActiveObject::vanish()
{
    disable();
    if (m_scenario->target().locked() == this)
    {
        m_scenario->target().lockReset();
        sfx::SampleMap::get(sfx::Sample::TargetDestroyed).play();
    }
}


void ActiveObject::leaveMission()
{
    m_absent = true;
    disable();
}


bool ActiveObject::canSleep() const
{
    return !m_tumbling && (!m_orders || m_orders->mayPause());
}


void ActiveObject::sleep()
{
    m_asleep = true;
    m_trigger = false;
}


void ActiveObject::wake()
{
    if (!m_asleep)
        return;
    m_asleep = false;
    if (m_orders)
        m_orders->restart();
}


void ActiveObject::setEngine(int model)
{
    m_engine = std::clamp(model, 0, 5);
    m_energy = 1.0f;
}


float ActiveObject::rechargeTime(int engine)
{
    static const float seconds[6] = { 5.0f, 3.0f, 1.0f, 1.0f, 1.0f, 0.01f };
    return seconds[std::clamp(engine, 0, 5)];
}


void ActiveObject::rechargeEnergy(float elapsedTime)
{
    if (m_energy < 1.0f)
        m_energy = std::min(1.0f, m_energy + elapsedTime/rechargeTime(m_engine));
}


void ActiveObject::identify()
{
    m_eventIdentify.complete();
    m_scenario->logEvent(name(), &m_eventIdentify, txt::Fight7);
    sfx::SampleMap::get(sfx::Sample::TargetIdentified).play();
}


} // namespace fight
