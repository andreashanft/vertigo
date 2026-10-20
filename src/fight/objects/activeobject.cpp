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
#include "fight/effects/billboard.h"
#include "fight/effects/effectmanager.h"
#include "fight/weapons/unitgun.h"
#include "fight/orders.h"
#include "fight/weapons/torpedo.h"
#include "fight/boats/simpleboat.h"
#include "util/random.h"
#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtx/transform.hpp>
#include <cmath>
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
    m_info(info)
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
    updateLaterExplosions();
    if (updateGroundWreck(elapsedTime))
        return false;
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
    const bool wasDying = isDying();
    m_kineticShield -= kinetic;
    m_shockShield -= shock;
    // a building under fire: 5 to 11 small explosions on its side towards the player within
    // 1.5 s, when its armour falls below 40 % and again below 20 % (docs/ROADMAP.md, appendix R)
    if (m_groundWreck == GroundWreck::Building && !isGroundWrecking() && m_kineticShield >= 0)
    {
        bool &flag = m_kineticShield < 0.2f*m_kineticShieldMax ? m_hitHarder : m_hitHard;
        if (m_kineticShield < 0.4f*m_kineticShieldMax && !flag)
        {
            flag = true;
            m_hitHard = true;
            const Vector3D dim = m_box.dim();
            const float r = 0.5f*std::max(dim.x, std::max(dim.y, dim.z));
            Vector3D toward = m_scenario->player() ? m_scenario->player()->position() - center() : Vector3D(0, -1, 0);
            toward.z = 0;
            toward = glm::length(toward) > 0.1f ? glm::normalize(toward) : Vector3D(0, -1, 0);
            for (int i = 5 + util::random()%7; i > 0; i--)
            {
                const int roll = util::random()%4;
                const int type = roll == 0 ? 8 : roll == 3 ? 0 : 5;
                const Vector3D spread(util::random()/float(RAND_MAX) - 0.5f, util::random()/float(RAND_MAX) - 0.5f, util::random()/float(RAND_MAX) - 0.5f);
                const float size = (0.05f + 0.05f*util::random()/float(RAND_MAX))*r;
                addLaterExplosion(1.5f*util::random()/float(RAND_MAX), type, center() + toward*r + spread*r, size);
            }
        }
    }
    if (m_kineticShield < 0)
        destroy();
    else if (m_shockShield < 0 && !m_eventParalyze.isCompleted() && m_groundWreck != GroundWreck::Building)
    {
        m_eventParalyze.complete();
        m_scenario->logEvent(name(), &m_eventParalyze, txt::Fight8);
        explode();
    }

    if (attacker && attacker != this && attacker == m_scenario->player())
        m_scenario->playerHit(this, !wasDying && isDying());
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
    if (m_eventDestroy.isCompleted() || isGroundWrecking())
        return;
    // a ground unit counts as destroyed once its sequence is over
    if (m_groundWreck != GroundWreck::None)
    {
        startGroundWreck();
        return;
    }
    if (!startWreck())
    {
        blowUp(1);
        vanish();
    }
    // a boat counts once its hull is gone; the player's boat at once
    else if (this != m_scenario->player())
        return;
    countDestroyed();
}


void ActiveObject::countDestroyed()
{
    if (m_eventDestroy.isCompleted())
        return;
    m_eventDestroy.complete();
    m_scenario->logEvent(name(), &m_eventDestroy, txt::Fight5);
}


void ActiveObject::explode()
{
    if (!m_enabled || m_eventDestroy.isCompleted() || m_tumbling || isGroundWrecking())
        return;
    if (!startTumbling() && m_groundWreck == GroundWreck::None)
    {
        blowUp(3);
        vanish();
    }
}


void ActiveObject::wreck()
{
    if (!m_enabled || m_eventDestroy.isCompleted() || m_tumbling || isGroundWrecking())
        return;
    // of the ground units only buildings follow an Explode order, with their whole sequence
    if (m_groundWreck == GroundWreck::Building)
        startGroundWreck();
    if (m_groundWreck != GroundWreck::None)
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


// The ground units' destruction (docs/ROADMAP.md, appendix R): the boats' bursts at 0, 0.4, 0.9,
// 1.3, 1.6 and 2.0 s and their kind's debris; tanks and crawlers brake to a stop meanwhile (Tank),
// a building shakes and tilts and goes up in a big blast at 2.6 s. Tanks, crawlers and towers are
// gone at 2.0 s, buildings at 2.7 s, and only then count as destroyed.
void ActiveObject::startGroundWreck()
{
    m_groundStep = 0;
    m_groundStart = m_scenario->time();
    m_groundAnchor = m_position;
    m_trigger = false;
    m_scenario->disengagePlayer(this);
}


void ActiveObject::groundExplosion(int type, float spread)
{
    const Vector3D dim = m_box.dim();
    const float r = 0.5f*std::max(dim.x, std::max(dim.y, dim.z));
    auto u = []() { return util::random()/float(RAND_MAX); };
    const Vector3D at = center() + Vector3D((u() - 0.5f)*r, (u() - 0.5f)*r*spread, u()*r*spread);
    m_scenario->effectManager().addEffect(Effects(int(Effects::Explosion_0) + type), at);
    sfx::SampleMap::playAt(type < 3 ? sfx::Sample::Explosion_Big : sfx::Sample::Explosion_Small, at.x, at.y, at.z);
    m_scenario->explosionAt(at, type < 3 ? 0.6f : 0.3f);
}


void ActiveObject::groundDebris(int index, float minElevation, float maxElevation)
{
    const Vector3D dim = m_box.dim();
    const float r = 0.5f*std::max(dim.x, std::max(dim.y, dim.z));
    auto between = [](float min, float max) { return min + (max - min)*util::random()/float(RAND_MAX); };
    const float azimuth = between(-glm::pi<float>(), glm::pi<float>());
    const float elevation = between(minElevation, maxElevation);
    const Vector3D direction(std::cos(elevation)*std::sin(azimuth), std::cos(elevation)*std::cos(azimuth), std::sin(elevation));
    m_scenario->effectManager().addDebris(Effects(int(Effects::Debris_0) + index), center() + direction*r*between(0.1f, 0.6f), direction);
}


void ActiveObject::addLaterExplosion(float delay, int type, const Vector3D &position, float scale)
{
    m_laterExplosions.push_back({ m_scenario->time() + delay, type, position, scale });
}


// the explosion's size is given as the original's sprite scale, which comes out as metres
void ActiveObject::updateLaterExplosions()
{
    for (size_t i = 0; i < m_laterExplosions.size(); )
    {
        const LaterExplosion explosion = m_laterExplosions[i];
        if (m_scenario->time() < explosion.time)
        {
            i++;
            continue;
        }
        m_laterExplosions.erase(m_laterExplosions.begin() + i);
        const Effects effect = Effects(int(Effects::Explosion_0) + explosion.type);
        const float sizeScale = std::max(1e-3f, m_scenario->effectManager().getBillboard(effect)->sizeScale());
        m_scenario->effectManager().addEffect(effect, explosion.position, 0, explosion.scale/sizeScale);
        const Vector3D &at = explosion.position;
        sfx::SampleMap::playAt(explosion.type < 3 ? sfx::Sample::Explosion_Big : sfx::Sample::Explosion_Small, at.x, at.y, at.z);
        m_scenario->explosionAt(at, explosion.type < 3 ? 1.0f : 0.3f);
    }
}


bool ActiveObject::updateGroundWreck(float elapsedTime)
{
    if (!isGroundWrecking())
        return false;
    auto between = [](float min, float max) { return min + (max - min)*util::random()/float(RAND_MAX); };
    const bool building = m_groundWreck == GroundWreck::Building;
    const bool tower = m_groundWreck == GroundWreck::Tower;
    const float fifth = 0.2f*glm::pi<float>();
    const float t = m_scenario->time() - m_groundStart;
    if (building && m_groundStep > 0)
    {
        // it shakes by up to 5 cm about where it stood and tilts slowly
        m_position = m_groundAnchor + Vector3D(between(-0.05f, 0.05f), between(-0.05f, 0.05f), 0);
        m_tilt = glm::mat3(glm::rotate(m_groundSpin.x*elapsedTime, Vector3D(1, 0, 0))*glm::rotate(m_groundSpin.y*elapsedTime, Vector3D(0, 1, 0)))*m_tilt;
    }

    static const float times[] = { 0, 0.4f, 0.9f, 1.3f, 1.6f, 2.0f, 2.7f };
    while (m_groundStep >= 0 && m_groundStep < 7 && t >= times[m_groundStep])
    {
        switch (m_groundStep++)
        {
        case 0:
            groundExplosion(0, 0.6f);
            groundExplosion(3, 0.6f);
            groundExplosion(3, 0.6f);
            if (building)
            {
                m_groundSpin = Vector2D(between(-0.07f, 0.07f), between(-0.07f, 0.07f));
                if (util::random()%5 < 3)
                    sfx::SampleMap::playAt(sfx::Sample::Sample18, m_position.x, m_position.y, m_position.z);
            }
            break;
        case 1:
            groundExplosion(4, 0.5f);
            break;
        case 2:
            groundExplosion(1, 0.4f);
            groundExplosion(4, 0.4f);
            groundExplosion(4, 0.7f);
            if (building)
                for (int debris : { 2, 2, 1 })
                    groundDebris(debris, fifth, 2*fifth);
            else if (m_groundWreck == GroundWreck::Tank)
                for (int debris : { 2, 1, 3, 3 })
                    groundDebris(debris, fifth, 2*fifth);
            else
                for (int debris : { 2, 2, 1, 3, 3 })
                    groundDebris(debris, fifth, 2*fifth);
            break;
        case 3:
            groundExplosion(5, 0.3f);
            break;
        case 4:
            groundExplosion(5, 0.6f);
            groundExplosion(2, 0.6f);
            break;
        case 5:
            groundExplosion(2, 0.1f);
            groundExplosion(2, 0.4f);
            if (building)
            {
                for (int debris : { 2, 1, 2, 1 })
                    groundDebris(debris, 0, fifth);
                // the big blast 0.6 s later on its side towards the player, half way up
                const Vector3D dim = m_box.dim();
                const float r = 0.5f*std::max(dim.x, std::max(dim.y, dim.z));
                Vector3D toward = m_scenario->player() ? m_scenario->player()->position() - m_position : Vector3D(0, -1, 0);
                toward.z = 0;
                toward = glm::length(toward) > 0.1f ? glm::normalize(toward) : Vector3D(0, -1, 0);
                addLaterExplosion(0.6f, 2, Vector3D(m_position.x, m_position.y, m_box.minPoint().z + 0.5f*dim.z) + toward*r, 0.35f*r);
                break;
            }
            for (int debris : m_groundWreck == GroundWreck::Tank ? std::vector<int>{ 2, 2, 1, 3 } : std::vector<int>{ 2, 2, 2, 1, 3, 3 })
                groundDebris(debris, 0, fifth);
            // the crews' bodies; towers leave nothing
            if (!tower)
                for (int body : { 14, 15, 16 })
                    groundDebris(body, 0, glm::half_pi<float>());
            m_groundStep = 7;
            break;
        default:
            m_groundStep = 7;
            break;
        }
    }
    if (m_groundStep < 7)
        return true;

    // gone: the ground under a building or tower is light again
    m_groundStep = -1;
    vanish();
    if (m_groundX >= 0)
    {
        m_scenario->surface().darken(m_groundX, m_groundY, 1.0f);
        m_scenario->surface().invalidate(m_groundX, m_groundY);
    }
    if (m_kineticShield < 0)
        countDestroyed();
    return true;
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
    // a unit in its destruction sequence runs it to the end, so that it counts as destroyed
    return !m_tumbling && !isDying() && (!m_orders || m_orders->mayPause());
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
