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

#include "player.h"
#include "txt/desfile.h"
#include "fight/effects/effectmanager.h"
#include "fight/weapons/torpedo.h"
#include "fight/scenario.h"
#include "game/boat.h"
#include "util/random.h"

#include <glm/gtx/transform.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/geometric.hpp>
#include <glm/gtx/norm.hpp>
#include <algorithm>
#include <cmath>


namespace fight {


static ActiveObject::ObjectInfo playerInfo()
{
    ActiveObject::ObjectInfo info;
    info.iff = Scenario::PlayerFaction;
    return info;
}


Player::Player(Scenario *scenario, game::Boat *boat, float yaw) :
    SimpleBoat(scenario, boat->boatFile(), playerInfo(), yaw),
    m_engineState(EngineRunning),
    m_speed(0),
    m_debugBoost(0),
    m_roll(0),
    m_currentTime(10.0f),
    m_currentNoise1(0),
    m_currentNoise2(0),
    m_firing(false),
    m_gun(scenario, boat->gun() - 3073, this),
    m_boat(boat),
    m_torpedoItems(boat->torpedoes()),
    m_selectedTorpedo(boat->torpedoes().empty() ? -1 : 0),
    m_torpedoReady(0),
    m_rearmingTime(5),
    m_lockTarget(nullptr),
    m_lockTime(0),
    m_lockAnnounced(false),
    m_threats(0),
    m_threatBlink(false)
{
    if (boat->magazine() >= 1025)
    {
        txt::DesFile weapons("vfx:sobjects/globweap.des");
        weapons.setSection(QString("torpedomagazine%1").arg(boat->magazine() - 1025));
        m_rearmingTime = weapons.value("rearmingtime", 5).toFloat();
    }

    int i;
    txt::DesFile &file = boat->boatFile();

    file.setSection("kinematics");
    m_rollCenterVelocity = file.value("RollCenterVelocity").toFloat();
    m_bottomAlphaSupport = file.value("BottomAlphaSupport").toFloat();
    m_currentAmplitude = Vector3D(file.value("CurrentAmplitudeB").toFloat(), file.value("CurrentAmplitudeC").toFloat(), file.value("CurrentAmplitudeA").toFloat());
    m_current = file.value("Current").toFloat();

    file.setSection("defense");
    for (i = 0; i < 4; i++)
        m_shieldMax[i] = file.value(QString("kineticshield%1").arg(i)).toInt();

    int armor = 0;
    if (boat->armor() == 5121)
        armor = 2000;
    if (boat->armor() == 5122)
        armor = 3500;
    if (boat->armor() == 5123)
        armor = 6500;
    if (boat->armor() == 5124)
        armor = 7500;
    for (i = 0; i < 4; i++)
        m_shieldMax[i] += armor;

    for (i = 0; i < 4; i++)
        m_shield[i] = m_shieldMax[i];

    if (boat->silator() == 4101)
        m_noise -= 1;
    if (boat->nrskin() == 5126)
        m_noise -= 1;

    m_gun.addMuzzles(Vector3D(0, 3.0f, 0), boat->gun());
    m_gun.setDefect(boat->defects()[game::Boat::DefectGun]);
    if (boat->tur1() != 0)
        m_turrets[0].reset(new PlayerTurret(m_scenario, this, boat->tur1(), boat->tur1soft(), Vector3D(0, 3.0f, 0) + boat->getMounting("TUR1")->rel, boat->defects()[game::Boat::DefectTur1]));
    if (boat->tur2() != 0)
        m_turrets[1].reset(new PlayerTurret(m_scenario, this, boat->tur2(), boat->tur2soft(), Vector3D(0, 3.0f, 0) + boat->getMounting("TUR2")->rel, boat->defects()[game::Boat::DefectTur2]));

    sfx::SampleMap::get(sfx::Sample::Engine).playLoop();

    // the player stays in the collision checks, so others' shots can hit it; its own checks ignore it
}


Player::~Player()
{
    // the engine sample is shared and would keep looping after the mission
    sfx::SampleMap::get(sfx::Sample::Engine).stop();
}


float Player::noise() const
{
    return (m_engineState != EngineOff ? ActiveObject::noise() : 0) + (m_scenario->sonar().isActive() || m_scenario->sonar().isActivating() ? 1 : 0);
}


float Player::range() const
{
    return m_gun.range();
}


void Player::damage(int kinetic, int shock, const Vector3D &position, ActiveObject *attacker)
{
    if (attacker && attacker != this)
        m_eventAttack.complete();

    // the side that was hit: 0 front, 1 left, 2 right, 3 back
    int side = 0;
    const Vector3D hit = position - m_position;
    if (glm::length(hit) > 1e-3f)
    {
        const float front = glm::dot(hit, dir());
        const float right = glm::dot(hit, this->right());
        if (glm::abs(front) >= glm::abs(right))
            side = front > 0 ? 0 : 3;
        else
            side = right > 0 ? 2 : 1;
    }
    m_shield[side] -= kinetic;
    // a shot rattles the cockpit a little, a torpedo fully
    if (kinetic > 0)
        m_scenario->headMotion().shake(std::min(1.0f, std::sqrt(kinetic/2000.0f)));
    if (m_shield[side] < 0)
        destroy();
}


bool Player::isCritical() const
{
    for (int side = 0; side < 4; side++)
        if (m_shieldMax[side] > 0 && m_shield[side] < m_shieldMax[side]/10)
            return true;
    return false;
}


bool Player::torpedoReady() const
{
    return m_selectedTorpedo >= 0 && m_scenario->time() >= m_torpedoReady;
}


float Player::lockProgress() const
{
    const TorpedoType *type = m_selectedTorpedo >= 0 ? TorpedoType::get(m_torpedoItems[m_selectedTorpedo] - 1) : nullptr;
    if (!type || type->lockType == 0 || !m_lockTarget)
        return 0;
    return type->lockDelay > 0 ? std::min(1.0f, m_lockTime/type->lockDelay) : 1.0f;
}


float Player::lockRemaining() const
{
    const TorpedoType *type = m_selectedTorpedo >= 0 ? TorpedoType::get(m_torpedoItems[m_selectedTorpedo] - 1) : nullptr;
    return type ? std::max(0.0f, type->lockDelay - m_lockTime) : 0;
}


void Player::fireTorpedo()
{
    if (!torpedoReady())
    {
        sfx::SampleMap::get(sfx::Sample::Error).play();
        return;
    }
    const int type = m_torpedoItems[m_selectedTorpedo] - 1;
    ActiveObject *target = lockProgress() >= 1.0f ? m_lockTarget : nullptr;
    m_scenario->launchTorpedo(type, m_position + dir()*4.0f - up()*1.0f, dir(), glm::length(m_velocity), this, target);

    m_torpedoItems.erase(m_torpedoItems.begin() + m_selectedTorpedo);
    m_boat->useTorpedo(m_selectedTorpedo);
    if (m_selectedTorpedo >= int(m_torpedoItems.size()))
        m_selectedTorpedo = int(m_torpedoItems.size()) - 1;
    m_torpedoReady = m_scenario->time() + m_rearmingTime;
    m_lockTime = 0;
}


void Player::selectNextTorpedo()
{
    if (m_torpedoItems.empty())
        return;
    m_selectedTorpedo = (m_selectedTorpedo + 1) % int(m_torpedoItems.size());
    m_lockTime = 0;
}


void Player::dropBuzzer()
{
    if (!m_scenario->useBuzzer())
    {
        sfx::SampleMap::get(sfx::Sample::Error).play();
        return;
    }
    m_boat->useBuzzer();
    const Vector3D position = m_position - dir()*5.0f;
    m_scenario->addDecoy(position, 12.0f);
    m_scenario->effectManager().addDebris(Effects::Debris_22, position, -dir());
}


void Player::destroy()
{
    m_scenario->destroyPlayer();
}


bool Player::update(float elapsedTime)
{
    m_angleVelocity.x += m_angleVelocity.x*m_angleFriction.x*64*elapsedTime;
    m_angleVelocity.x -= 0.64f*dir().z*m_maxAngleVelocity.x*elapsedTime;
    if (m_turnVelocity.x > 0 && m_angleVelocity.x < m_maxAngleVelocity.x)
        m_angleVelocity.x -= m_maxAngleVelocity.x*m_angleFriction.x*64*elapsedTime;
    else if (m_turnVelocity.x < 0 && m_angleVelocity.x > -m_maxAngleVelocity.x)
        m_angleVelocity.x += m_maxAngleVelocity.x*m_angleFriction.x*64*elapsedTime;

    m_angleVelocity.y += m_angleVelocity.y*m_angleFriction.y*64*elapsedTime;
    m_angleVelocity.y += m_roll*m_rollCenterVelocity*64*m_maxAngleVelocity.y*elapsedTime;
    if (m_turnVelocity.y > 0 && m_angleVelocity.y < m_maxAngleVelocity.y)
        m_angleVelocity.y -= m_maxAngleVelocity.y*m_angleFriction.y*64*elapsedTime;
    if (m_turnVelocity.y < 0 && m_angleVelocity.y > -m_maxAngleVelocity.y)
        m_angleVelocity.y += m_maxAngleVelocity.y*m_angleFriction.y*64*elapsedTime;

    m_angleVelocity.z = m_roll*m_bottomAlphaSupport*m_maxAngleVelocity.z;

    const float currentLen = 5.0f;
    m_currentTime += elapsedTime;
    if (m_currentTime > currentLen*0.75f)
    {
        m_currentTime = 0;
        m_currentNoise1 = m_currentNoise2;
        m_currentNoise2 = Vector3D(0.25f*util::random()/RAND_MAX, 0.25f*util::random()/RAND_MAX, 0.5f*util::random()/RAND_MAX);
        if (m_currentNoise1.z > 0)
            m_currentNoise2 = -m_currentNoise2;
    }
    float t = m_currentTime/currentLen;
    if (t < 0.25f)
        m_angleVelocity += m_currentAmplitude*m_currentNoise1*glm::sin(glm::pi<float>()*2*(t + 0.75f));
    m_angleVelocity += m_currentAmplitude*m_currentNoise2*glm::sin(glm::pi<float>()*2*t);

    m_orientation *= glm::mat3(glm::rotate(m_angleVelocity.z*elapsedTime, Vector3D(0, 0, 1))) * glm::mat3(glm::rotate(m_angleVelocity.y*elapsedTime, dir())) * glm::mat3(glm::rotate(m_angleVelocity.x*elapsedTime, right()));
    m_orientationInverted = glm::transpose(m_orientation);

    m_roll = 0;
    if (glm::abs(dir().z) < 0.99999f)
        m_roll = glm::dot(glm::normalize(Vector3D(dir().y, -dir().x, 0)), up());

    updateVelocity(elapsedTime);

    if (m_engineState == EngineStartup && m_engineTime < m_scenario->time())
    {
        m_engineState = EngineRunning;
        sfx::SampleMap::get(sfx::Sample::Engine).playLoop();
    }
    if (m_engineState == EngineShutdown && m_engineTime < m_scenario->time())
        m_engineState = EngineOff;
    if (m_engineState == EngineRunning)
    {
        float pitch = glm::length(m_velocity)*0.036f;
        if (pitch > 1.0f)
            pitch = 1.0f;
        if (pitch < -0.5f)
            pitch = -0.5f;
        sfx::SampleMap::get(sfx::Sample::Engine).setPitch(1.0f + pitch);
    }

    Vector3D vel = m_velocity + dir()*m_debugBoost + m_scenario->current(m_position)*m_current;
    Vector3D delta = vel*elapsedTime;
    float path = 0;

    int count = 0;
    bool grounded = false;
    while (glm::length2(delta) > 1e-5f && count < 5)
    {
        Vector3D posSurface, normSurface;
        bool colSurface = m_scenario->surface().testCollision(m_position, m_position + delta, 1.5f, posSurface, normSurface);

        Vector3D posObject, normObject;
        Object *colObject = m_scenario->collisionManager().testCollision(m_position, m_position + delta, 1.5f, posObject, normObject, nullptr, this);

        if (colObject && (!colSurface || glm::length2(posObject - m_position) < glm::length2(posSurface - m_position)))
        {
            ActiveObject *activeObject = dynamic_cast<ActiveObject *>(colObject);
            if (activeObject)
                activeObject->damage(m_kineticStrength, m_shockStrength, posObject - normObject*1.5f, this);
            damage(colObject->kineticStrength(), colObject->shockStrength(), posObject - normObject*1.5f);
            sfx::SampleMap::get(sfx::Sample::Boat_Collision).playInstance();
            const Vector3D bounce = -2*glm::dot(m_velocity, normObject)*normObject;
            m_scenario->headMotion().jolt(bounce);
            m_velocity += bounce;
            delta = posObject - m_position;
        }
        else if (colSurface)
        {
            // touching down: the part of the motion into the ground stops at once
            const float into = -glm::dot(vel, normSurface);
            if (!m_grounded && into > 0)
                m_scenario->headMotion().jolt(into*normSurface);
            grounded = true;
            delta -= posSurface - m_position;
            path += glm::length(posSurface - m_position);
            m_position = posSurface;
            delta -= (glm::dot(delta, normSurface) - 1e-3f)*normSurface;
            count++;
            continue;
        }

        path += glm::length(delta);
        m_position += delta;
        break;
    }

    // scraping along the ground rattles the cockpit
    if (grounded)
        m_scenario->headMotion().shake(std::min(0.4f, glm::length(vel)/40.0f));
    m_grounded = grounded;

    updateBox();
    m_speed = path/elapsedTime*3.6f;
    if (glm::dot(dir(), vel) < 0)
        m_speed = -m_speed;


    // torpedo lock: the target must stay locked and in front for the selected torpedo's lock time
    ActiveObject *target = m_scenario->target().locked();
    if (target != m_lockTarget)
    {
        m_lockTarget = target;
        m_lockTime = 0;
    }
    if (m_lockTarget && glm::dot(dir(), glm::normalize(m_lockTarget->center() - m_position)) > std::cos(glm::radians(40.0f)))
        m_lockTime += elapsedTime;
    else
        m_lockTime = 0;
    // a beep when locking starts, beep and voice when a homing torpedo would follow
    const float progress = lockProgress();
    if (progress > 0 && m_lockTime <= elapsedTime)
        sfx::SampleMap::get(sfx::Sample::LockOn).play();
    if (progress >= 1 && !m_lockAnnounced)
    {
        sfx::SampleMap::get(sfx::Sample::TorpedoLockOn).play();
        sfx::SampleMap::get(sfx::Sample::Voice_TorpedoReady).play();
    }
    m_lockAnnounced = progress >= 1;

    // homing torpedoes on their way: the voice for each new one, and a beep each time the THREAT icon blinks on
    const int threats = m_scenario->threatsTo(this);
    if (threats > m_threats)
        sfx::SampleMap::get(sfx::Sample::Voice_TorpedoThreat).play();
    const bool blink = m_scenario->blink();
    if (threats > 0 && blink && !m_threatBlink)
        sfx::SampleMap::get(sfx::Sample::Threat).play();
    m_threatBlink = blink;
    m_threats = threats;

    // critical: the voice once, then the alert beep with each blink of ALERT
    const bool critical = isCritical();
    if (critical && !m_critical)
        sfx::SampleMap::get(sfx::Sample::Voice_SystemCollapse).play();
    else if (critical && blink && !m_alertBlink)
        sfx::SampleMap::get(sfx::Sample::Alert).play();
    m_alertBlink = blink;
    m_critical = critical;

    m_gun.update(elapsedTime);
    if (m_firing && m_gun.state() == Gun::StateReady)
        m_gun.fire(m_position, dir(), up(), right());
    for (auto &turret : m_turrets)
        if (turret)
            turret->update(elapsedTime, m_firing);

    return false;
}


void Player::engineToggle()
{
    if (m_engineState == EngineOff)
    {
        m_engineState = EngineStartup;
        m_engineTime = m_scenario->time() + 1;
        sfx::SampleMap::get(sfx::Sample::Engine_Startup).play();
    }
    if (m_engineState == EngineRunning)
    {
        m_engineState = EngineShutdown;
        m_engineTime = m_scenario->time() + 1;
        sfx::SampleMap::get(sfx::Sample::Engine_Shutdown).play();
        sfx::SampleMap::get(sfx::Sample::Engine).stop();
    }
}


void Player::fire()
{
    m_firing = true;
}


void Player::fireStop()
{
    m_firing = false;
}


} // namespace fight
