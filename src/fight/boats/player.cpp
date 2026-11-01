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
#include "fight/effects/trash.h"
#include "fight/effects/effectmanager.h"
#include "txt/desfile.h"
#include "fight/effects/effectmanager.h"
#include "fight/weapons/torpedo.h"
#include "fight/scenario.h"
#include "game/boat.h"
#include "util/random.h"
#include "util/settings.hpp"

#include <glm/gtx/transform.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/geometric.hpp>
#include <glm/gtx/norm.hpp>
#include <algorithm>
#include <cmath>


namespace fight {


// the player whose engine loops in the shared engine sample: a restarted mission's new player
// starts it before the old one is gone
static const Player *enginePlayer = nullptr;


static ActiveObject::ObjectInfo playerInfo()
{
    ActiveObject::ObjectInfo info;
    info.iff = Scenario::PlayerFaction;
    info.faction = true;
    return info;
}


Player::Player(Scenario *scenario, game::Boat *boat, float yaw) :
    SimpleBoat(scenario, boat->boatFile(), playerInfo(), yaw),
    m_engineState(EngineRunning),
    m_speed(0),
    m_hasBooster(boat->booster() != 0),
    m_roll(0),
    m_currentTime(10.0f),
    m_currentNoise1(0),
    m_currentNoise2(0),
    m_firing(false),
    m_gun(scenario, boat->gun() - 3073, this),
    m_boat(boat),
    m_torpedoItems(boat->torpedoes()),
    m_selectedTorpedo(boat->torpedoes().empty() ? -1 : 0),
    m_loadedTorpedo(m_selectedTorpedo),
    m_magazineState(boat->torpedoes().empty() ? MagazineState::Empty : MagazineState::Ready),
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
        weapons.setSection(String("torpedomagazine%1").arg(boat->magazine() - 1025));
        m_rearmingTime = weapons.value("rearmingtime", 5).toFloat();
        m_magazineDamping = weapons.value("defectdamping", 0).toFloat();
        m_magazineDelay = weapons.value("defectdelay", 0).toFloat();
    }

    int i;
    txt::DesFile &file = boat->boatFile();

    file.setSection("kinematics");
    m_rollCenterVelocity = file.value("RollCenterVelocity").toFloat();
    m_bottomAlphaSupport = file.value("BottomAlphaSupport").toFloat();
    m_currentAmplitude = Vector3D(file.value("CurrentAmplitudeB").toFloat(), file.value("CurrentAmplitudeC").toFloat(), file.value("CurrentAmplitudeA").toFloat());
    m_currentFactor = file.value("Current").toFloat();

    file.setSection("defense");
    for (i = 0; i < 4; i++)
        m_shieldMax[i] = file.value(String("kineticshield%1").arg(i)).toInt();

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

    // each side as the boat left its last mission: front, left, right and back
    for (i = 0; i < 4; i++)
        m_shield[i] = int(std::lround(boat->defects()[game::Boat::DefectFArm + i]*m_shieldMax[i]));

    if (boat->silator() == 4101)
        m_noise -= 1;
    if (boat->nrskin() == 5126)
        m_noise -= 1;

    // the boat's engine: Small, Medium or Big Block as engine 0, 1 or 2 (appendix K)
    setEngine(boat->engine() - 4097);
    // settings.ini [Fight] turnticks: the relaxation rate of the turn rates, to be tuned against
    // the original (appendix L)
    TurnTickRate = util::Settings::floatValue("Fight/turnticks", 30.0f);
    m_gun.addMuzzle(Vector3D(0, 3.0f, 0));
    m_gun.setDefect(boat->defects()[game::Boat::DefectGun]);
    if (boat->tur1() != 0)
        m_turrets[0].reset(new PlayerTurret(m_scenario, this, boat->tur1(), boat->tur1soft(), Vector3D(0, 3.0f, 0) + boat->getMounting("TUR1")->rel, boat->getMounting("TUR1")->side, boat->defects()[game::Boat::DefectTur1]));
    if (boat->tur2() != 0)
        m_turrets[1].reset(new PlayerTurret(m_scenario, this, boat->tur2(), boat->tur2soft(), Vector3D(0, 3.0f, 0) + boat->getMounting("TUR2")->rel, boat->getMounting("TUR2")->side, boat->defects()[game::Boat::DefectTur2]));

    m_fixer = boat->fixer() != 0;
    m_magazineCondition = boat->defects()[game::Boat::DefectToMa];

    sfx::SampleMap::get(sfx::Sample::Engine).playLoop();
    enginePlayer = this;

    // the player stays in the collision checks, so others' shots can hit it; its own checks ignore it
}


Player::~Player()
{
    // the engine sample is shared and would keep looping after the mission, unless a new
    // mission's player has taken it over
    if (enginePlayer == this)
    {
        sfx::SampleMap::get(sfx::Sample::Engine).stop();
        enginePlayer = nullptr;
    }
}


float Player::noise() const
{
    return (m_engineState != EngineOff ? ActiveObject::noise() : 0) + (m_scenario->sonar().isActive() || m_scenario->sonar().isActivating() ? 1 : 0);
}


float Player::range() const
{
    return m_gun.range();
}


namespace {

// below this share of its armour a side passes what it loses on to the systems behind it (the
// front's to the gun and the torpedo magazine, the others' to the turrets), this many times over
// (appendix O)
const float SystemDamageBelow = 0.45f;
const float SystemDamageFactor = 2.2f;

// slower into an object (m/s) the boat only touches it
const float ImpactSpeed = 1.0f;

} // namespace


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
    const int before = m_shield[side];
    m_shield[side] -= kinetic;
    if (kinetic > 0 && m_shieldMax[side] > 0)
        damageSystems(side, float(before)/m_shieldMax[side], float(m_shield[side])/m_shieldMax[side]);
    // a shot rattles the cockpit a little, a torpedo fully
    if (kinetic > 0)
        m_scenario->headMotion().shake(std::min(1.0f, std::sqrt(kinetic/2000.0f)));
    storeCondition();
    if (m_shield[side] < 0)
        destroy();
}


void Player::damageSystems(int side, float before, float after)
{
    if (after >= SystemDamageBelow)
        return;
    const float loss = (std::min(before, SystemDamageBelow) - after)*SystemDamageFactor;
    if (side == 0)
    {
        m_gun.setDefect(std::max(0.0f, m_gun.defect() - loss));
        m_magazineCondition = std::max(0.0f, m_magazineCondition - loss);
        return;
    }
    for (const std::unique_ptr<PlayerTurret> &turret : m_turrets)
        if (turret)
            turret->gun().setDefect(std::max(0.0f, turret->gun().defect() - loss));
}


bool Player::isCritical() const
{
    for (int side = 0; side < 4; side++)
        if (m_shieldMax[side] > 0 && m_shield[side] < m_shieldMax[side]/10)
            return true;
    return false;
}


float Player::lockProgress() const
{
    const TorpedoType *type = m_loadedTorpedo >= 0 ? TorpedoType::get(m_torpedoItems[m_loadedTorpedo] - 1) : nullptr;
    if (!type || type->lockType == 0 || !m_lockTarget)
        return 0;
    return type->lockDelay > 0 ? std::min(1.0f, m_lockTime/type->lockDelay) : 1.0f;
}


float Player::lockRemaining() const
{
    const TorpedoType *type = m_loadedTorpedo >= 0 ? TorpedoType::get(m_torpedoItems[m_loadedTorpedo] - 1) : nullptr;
    return type ? std::max(0.0f, type->lockDelay - m_lockTime) : 0;
}


// Tab clicks whether or not a torpedo leaves: the loaded one, while the magazine is ready or
// waiting, unless it jams as a gun does (its condition, at least 0.2, against a random share of
// its defectdamping; then jammed for defectdelay times 0.5 to 1.5). The selection then moves on to
// the next torpedo, which is loaded after the wait
void Player::fireTorpedo()
{
    sfx::SampleMap::get(sfx::Sample::Click).play();
    if (m_loadedTorpedo < 0 || (m_magazineState != MagazineState::Ready && m_magazineState != MagazineState::Waiting))
        return;
    const float now = m_scenario->time();
    if (util::random()/float(RAND_MAX)*m_magazineDamping >= std::max(m_magazineCondition, 0.2f))
    {
        m_magazineState = MagazineState::Jammed;
        m_magazineTime = now + m_magazineDelay*(0.5f + util::random()/float(RAND_MAX));
        return;
    }
    const int type = m_torpedoItems[m_loadedTorpedo] - 1;
    ActiveObject *target = lockProgress() >= 1.0f ? m_lockTarget : nullptr;
    m_scenario->launchTorpedo(type, m_position + dir()*4.0f - up()*1.0f, dir(), glm::length(m_velocity), this, target);

    m_torpedoItems.erase(m_torpedoItems.begin() + m_loadedTorpedo);
    m_boat->useTorpedo(m_loadedTorpedo);
    // the selection moves on past itself; the fired one's place is taken by the one after it
    if (m_selectedTorpedo != m_loadedTorpedo)
        m_selectedTorpedo += m_selectedTorpedo > m_loadedTorpedo ? 0 : 1;
    m_loadedTorpedo = -1;
    m_lockTime = 0;
    if (m_torpedoItems.empty())
    {
        m_selectedTorpedo = -1;
        m_magazineState = MagazineState::Empty;
        return;
    }
    m_selectedTorpedo %= int(m_torpedoItems.size());
    m_magazineState = MagazineState::Waiting;
    m_magazineTime = now + MagazineWait;
}


// Enter clicks and selects the next torpedo; one of the loaded type needs no loading, unless the
// magazine is busy loading already, any other is loaded after the wait
void Player::selectNextTorpedo()
{
    sfx::SampleMap::get(sfx::Sample::Click).play();
    if (m_torpedoItems.empty())
        return;
    m_selectedTorpedo = (m_selectedTorpedo + 1) % int(m_torpedoItems.size());
    m_lockTime = 0;
    if (m_loadedTorpedo >= 0 && m_torpedoItems[m_selectedTorpedo] == m_torpedoItems[m_loadedTorpedo] &&
        m_magazineState != MagazineState::Rearming && m_magazineState != MagazineState::Waiting)
    {
        m_loadedTorpedo = m_selectedTorpedo;
        m_magazineState = MagazineState::Ready;
        return;
    }
    m_magazineState = MagazineState::Waiting;
    m_magazineTime = m_scenario->time() + MagazineWait;
}


// after the wait the selected torpedo is loaded, with the revolver's sound, for the magazine's
// rearming time, and then the voice says it is ready; a jam just ends
void Player::updateMagazine()
{
    const float now = m_scenario->time();
    if (now < m_magazineTime)
        return;
    switch (m_magazineState)
    {
    case MagazineState::Waiting:
        m_loadedTorpedo = m_selectedTorpedo;
        m_magazineState = MagazineState::Rearming;
        m_magazineTime = now + m_rearmingTime;
        sfx::SampleMap::get(sfx::Sample::Torpedo_Reload).play();
        break;
    case MagazineState::Rearming:
        m_magazineState = MagazineState::Ready;
        sfx::SampleMap::get(sfx::Sample::Voice_TorpedoReady).play();
        break;
    case MagazineState::Jammed:
        m_magazineState = MagazineState::Ready;
        break;
    default:
        break;
    }
}


void Player::dropBuzzer()
{
    if (!m_scenario->useBuzzer())
    {
        sfx::SampleMap::get(sfx::Sample::Error).play();
        return;
    }
    m_boat->useBuzzer();
    // (docs/ROADMAP.md, appendix S) from the tail, upwards and backwards at 12.5 m/s on top of the
    // boat's own velocity: a buzzer (DEBRIS.DES 18), one in ten a dummy (20) that draws nothing
    const Vector3D position = m_position + dir()*m_localBox.minPoint().y;
    const Vector3D velocity = glm::normalize(2.0f*up() - dir())*12.5f + m_velocity;
    if (util::random()%10 == 0)
        m_scenario->effectManager().addDecoy(Effects::Debris_20, Particles::Decoy::None, iff(), position, velocity);
    else
        m_scenario->effectManager().addDecoy(Effects::Debris_18, Particles::Decoy::Buzzer, iff(), position, velocity);
}


// the destruction sequence, watched from outside (docs/ROADMAP.md, appendix R)
void Player::destroy()
{
    if (m_scenario->playerDestroyed())
        return;
    m_firing = false;
    sfx::SampleMap::get(sfx::Sample::Engine).stop();
    m_scenario->destroyPlayer();
    startWreck();
}


float Player::TurnTickRate = 30.0f;


float Player::pitch() const
{
    return glm::degrees(std::asin(glm::clamp(dir().z, -1.0f, 1.0f)));
}


bool Player::update(float elapsedTime)
{
    if (isWrecking())
    {
        updateWreck(elapsedTime);
        return false;
    }
    // The pitch and yaw rates relax towards the stick's share of the maximum by the boat's
    // AngleFriction fraction per tick (docs/ROADMAP.md, appendix L), so they carry on for a
    // while after the key is released; the nose also drops by its own angle and the roll turns
    // the boat (RollCenterVelocity), both per tick. The rotation itself uses the frame time.
    const Vector3D stick = m_turnVelocity*m_maxAngleVelocity;
    for (m_turnTicks += TurnTickRate*elapsedTime; m_turnTicks >= 1.0f; m_turnTicks -= 1.0f)
    {
        m_angleVelocity.x += (stick.x - m_angleVelocity.x)*-m_angleFriction.x;
        m_angleVelocity.x -= 0.01f*dir().z*m_maxAngleVelocity.x;
        m_angleVelocity.y += (stick.y - m_angleVelocity.y)*-m_angleFriction.y;
        m_angleVelocity.y += m_roll*m_rollCenterVelocity*m_maxAngleVelocity.y;
    }

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

    // Near the surface (docs/ROADMAP.md, appendix D): less than 40 m below it the throttle is held
    // at 1 m/s and the boat pushed down at 1 g until it is more than 70 m below again; it never
    // comes closer than 10 m
    const float depth = m_scenario->surfaceLevel() - m_position.z;
    if (!m_surfaceHold && depth < SurfaceHoldDepth)
        m_surfaceHold = true;
    else if (m_surfaceHold && depth > SurfaceReleaseDepth)
        m_surfaceHold = false;
    if (m_surfaceHold)
        m_scenario->holdThrottle(1.0f);

    // the booster (docs/ROADMAP.md, appendix K) draws from the same pool: held, it starts (again)
    // once the pool is at 0.8, costing 0.15 at once and 1.2 per second while the engine keeps
    // recharging, doubles the top speed and raises the acceleration 2.5-fold, and stops when the
    // pool is below 0.05; in the surface hold it does nothing
    if (m_boosting && (!m_boostKey || m_energy < 0.05f || m_surfaceHold))
        m_boosting = false;
    if (!m_boosting && m_boostKey && m_hasBooster && m_energy >= 0.8f && !m_surfaceHold)
    {
        m_boosting = true;
        drawEnergy(0.15f);
        sfx::SampleMap::get(sfx::Sample::Booster).play();
    }
    if (m_boosting)
        drawEnergy(1.2f*elapsedTime);
    m_speedBoost = m_boosting ? 2.0f : 1.0f;
    m_accelerationBoost = m_boosting ? 2.5f : 1.0f;

    updateVelocity(elapsedTime);
    if (m_surfaceHold)
        m_velocity.z -= 9.81f*elapsedTime;
    rechargeEnergy(elapsedTime);
    updateMagazine();
    if (m_fixer)
        updateFixer();

    if (m_engineState == EngineStartup && m_engineTime < m_scenario->time())
    {
        m_engineState = EngineRunning;
        sfx::SampleMap::get(sfx::Sample::Engine).playLoop();
        enginePlayer = this;
    }
    if (m_engineState == EngineShutdown && m_engineTime < m_scenario->time())
        m_engineState = EngineOff;
    if (m_engineState == EngineRunning)
    {
        // the engine's pitch follows the speed alone, up to 150 km/h (a boost reaches about 140)
        float pitch = glm::length(m_velocity)*0.036f;
        if (pitch > 1.5f)
            pitch = 1.5f;
        if (pitch < -0.5f)
            pitch = -0.5f;
        sfx::SampleMap::get(sfx::Sample::Engine).setPitch(1.0f + pitch);
    }

    Vector3D vel = m_velocity;
    Vector3D delta = vel*elapsedTime;
    float path = 0;

    // running into debris slower than the boat (docs/ROADMAP.md, appendix Q)
    Particles::DebrisHit debris;
    if (m_scenario->effectManager().hitDebris(m_position, m_position + delta, 1.5f, glm::length(vel), m_kineticStrength, debris))
        damage(debris.kinetic, debris.shock, debris.position);

    int count = 0;
    bool grounded = false;
    while (glm::length2(delta) > 1e-5f && count < 5)
    {
        Vector3D posSurface, normSurface;
        bool colSurface = m_scenario->surface().testCollision(m_position, m_position + delta, 1.5f, posSurface, normSurface);

        Vector3D posObject, normObject;
        // trash and the player don't touch (docs/ROADMAP.md, appendix Q); torpedoes see to their
        // own hits, the boat never bounces off them and its own never touch it (appendix S)
        Object *colObject = m_scenario->collisionManager().testCollision(m_position, m_position + delta, 1.5f, posObject, normObject, nullptr, this,
                                                                         [](const Object *object) { return !dynamic_cast<const Trash *>(object) && !dynamic_cast<const Torpedo *>(object); });

        const bool objectFirst = colObject && (!colSurface || glm::length2(posObject - m_position) < glm::length2(posSurface - m_position));
        const float intoObject = objectFirst ? -glm::dot(m_velocity, normObject) : 0.0f;
        if (objectFirst && intoObject < ImpactSpeed)
        {
            // pressed against it, or already moving away as the ground's slope turns the step back
            // into it: no hit, the boat slides along the object as along the ground; a bounce
            // here would throw it into the object again and hit on every frame
            if (intoObject > 0)
                m_velocity += intoObject*normObject;
            delta -= posObject - m_position;
            path += glm::length(posObject - m_position);
            m_position = posObject;
            const float toward = glm::dot(delta, normObject) - 1e-3f;
            if (toward < 0)
                delta -= toward*normObject;
            count++;
            continue;
        }
        else if (objectFirst)
        {
            ActiveObject *activeObject = dynamic_cast<ActiveObject *>(colObject);
            if (activeObject)
            {
                m_scenario->setPlayerRamming(true);
                activeObject->damage(m_kineticStrength, m_shockStrength, posObject - normObject*1.5f, this);
                m_scenario->setPlayerRamming(false);
            }
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

    const float ceiling = m_scenario->surfaceLevel() - SurfaceLimitDepth;
    if (m_position.z > ceiling)
    {
        m_position.z = ceiling;
        m_velocity.z = std::min(m_velocity.z, 0.0f);
    }

    // scraping along the ground rattles the cockpit
    if (grounded)
        m_scenario->headMotion().shake(std::min(0.4f, glm::length(vel)/40.0f));
    m_grounded = grounded;

    updateBox();
    m_speed = path/elapsedTime*3.6f;
    if (glm::dot(dir(), vel) < 0)
        m_speed = -m_speed;


    // torpedo lock: the target must stay locked and in front for the loaded torpedo's lock time,
    // which only runs once the torpedo is in and the magazine ready (appendix S)
    ActiveObject *target = m_scenario->target().locked();
    if (target != m_lockTarget)
    {
        m_lockTarget = target;
        m_lockTime = 0;
    }
    if (m_lockTarget && torpedoReady() && glm::dot(dir(), glm::normalize(m_lockTarget->center() - m_position)) > std::cos(glm::radians(40.0f)))
        m_lockTime += elapsedTime;
    else
        m_lockTime = 0;
    // a beep when locking starts, beep and the "target locked" voice when a homing torpedo would
    // follow
    const float progress = lockProgress();
    if (progress > 0 && m_lockTime <= elapsedTime)
        sfx::SampleMap::get(sfx::Sample::LockOn).play();
    if (progress >= 1 && !m_lockAnnounced)
    {
        sfx::SampleMap::get(sfx::Sample::TorpedoLockOn).play();
        sfx::SampleMap::get(sfx::Sample::Voice_TargetLocked).play();
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

    // the voice "lock-on located" when the first unit turns on the player; the symbol block
    // shows the marker under fire as long as any does
    const int attackers = m_scenario->playerAttackers();
    if (attackers > 0 && m_attackers == 0)
        sfx::SampleMap::get(sfx::Sample::Voice_LockOn).play();
    m_attackers = attackers;

    // critical: the voice once, then the alert beep with each blink of ALERT
    const bool critical = isCritical();
    if (critical && !m_critical)
        sfx::SampleMap::get(sfx::Sample::Voice_SystemCollapse).play();
    else if (critical && blink && !m_alertBlink)
        sfx::SampleMap::get(sfx::Sample::Alert).play();
    m_alertBlink = blink;
    m_critical = critical;

    // the radiation less than 60 m below the surface: the voice on entry and every 15 s, the
    // beep every 0.8 s (docs/ROADMAP.md, appendix D)
    const bool radiation = m_scenario->depth() < Scenario::RadiationAlertDepth;
    if (radiation && (!m_radiation || m_scenario->time() >= m_nextRadiationVoice))
    {
        sfx::SampleMap::get(sfx::Sample::Voice_SystemCollapse).play();
        m_nextRadiationVoice = m_scenario->time() + 15.0f;
    }
    if (radiation && (!m_radiation || m_scenario->time() >= m_nextRadiationBeep))
    {
        sfx::SampleMap::get(sfx::Sample::Alert).play();
        m_nextRadiationBeep = m_scenario->time() + 0.8f;
    }
    m_radiation = radiation;

    // the trigger is read before the gun counts its rearming time down, so the gun fires on the
    // frame after it is ready again (docs/ROADMAP.md, appendix K)
    if (m_firing && m_gun.ready())
        m_gun.fire(m_position, dir(), up(), right());
    m_gun.update(elapsedTime);
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



// --- the Fixer (appendix O)

namespace {

// a system is mended up to this much of its full state, and chosen for repair below it
const float FixerDone = 0.75f;
// one step a second, a 160th of the full state each
const float FixerStep = 1.0f/160;

} // namespace


void Player::cycleFixer()
{
    if (!m_fixer)
        return;
    // automatic, armour, gun, torpedo, turret 1, turret 2 (the turrets only if the boat has them)
    int next = m_fixerForced + 1;
    if (next == 4 && !m_turrets[0])
        next = 5;
    if (next == 5 && !m_turrets[1])
        next = 0;
    if (next > 5)
        next = 0;
    m_fixerForced = next;
    m_fixerNext = m_scenario->time() + 1.0f;
    sfx::SampleMap::get(sfx::Sample::Click).play();
}


// the first system below the threshold in the order armour (the side under repair; intact while none is),
// gun, torpedo, turrets; 0 if none is
int Player::fixerChoice(float threshold) const
{
    const float armour = m_fixerSide >= 0 && m_shieldMax[m_fixerSide] > 0 ? float(m_shield[m_fixerSide])/m_shieldMax[m_fixerSide] : 1.0f;
    if (armour < threshold)
        return 1;
    if (m_gun.defect() < threshold)
        return 2;
    if (m_magazineCondition < threshold)
        return 3;
    for (int i = 0; i < 2; i++)
        if (m_turrets[i] && m_turrets[i]->gun().defect() < threshold)
            return 4 + i;
    return 0;
}


void Player::updateFixer()
{
    const float now = m_scenario->time();
    if (now < m_fixerNext)
        return;
    m_fixerNext = now + 1.0f;

    // the forced system takes over; otherwise the worst one by the 25, 50 and 75 % marks
    if (m_fixerForced != 0 && m_fixerForced != m_fixerSystem)
    {
        m_fixerSystem = m_fixerForced;
        return;
    }
    if (m_fixerForced == 0)
    {
        for (float threshold : { 0.25f, 0.5f, FixerDone })
        {
            const int choice = fixerChoice(threshold);
            if (choice != 0)
            {
                m_fixerSystem = choice;
                break;
            }
        }
    }

    // a mended system hands over to the next one; the turrets only exist sometimes
    auto handOver = [&](int next) {
        m_fixerForced = 0;
        m_fixerSystem = next == 4 && !m_turrets[0] ? 1 : next == 5 && !m_turrets[1] ? 1 : next;
    };
    auto mend = [&](Gun &gun, int next) {
        if (gun.defect() >= FixerDone)
            handOver(next);
        else
            gun.setDefect(std::min(1.0f, gun.defect() + FixerStep));
    };
    switch (m_fixerSystem)
    {
    case 1: {
        // the worst side, a 160th of its full armour a second, until every side is at 75 %
        int worst = -1;
        bool allDone = true;
        for (int side = 0; side < 4; side++)
        {
            const float state = m_shieldMax[side] > 0 ? float(m_shield[side])/m_shieldMax[side] : 1.0f;
            allDone = allDone && state >= FixerDone;
            if (worst < 0 || state < float(m_shield[worst])/std::max(1, m_shieldMax[worst]))
                worst = side;
        }
        if (allDone)
        {
            m_fixerSide = -1;
            handOver(2);
            break;
        }
        m_fixerSide = worst;
        m_shield[worst] = std::min(m_shieldMax[worst], m_shield[worst] + std::max(1, m_shieldMax[worst]/160));
        break;
    }
    case 2:
        mend(m_gun, 3);
        break;
    case 3:
        if (m_magazineCondition >= FixerDone)
            handOver(4);
        else
            m_magazineCondition = std::min(1.0f, m_magazineCondition + FixerStep);
        break;
    case 4:
        if (m_turrets[0])
            mend(m_turrets[0]->gun(), 5);
        else
            m_fixerSystem = 1;
        break;
    case 5:
        if (m_turrets[1])
            mend(m_turrets[1]->gun(), 1);
        else
            m_fixerSystem = 1;
        break;
    default:
        break;
    }
    storeCondition();
}


void Player::storeCondition()
{
    for (int side = 0; side < 4; side++)
        m_boat->setDefect(game::Boat::Defect(game::Boat::DefectFArm + side), m_shieldMax[side] > 0 ? float(m_shield[side])/m_shieldMax[side] : 1.0f);
    m_boat->setDefect(game::Boat::DefectGun, m_gun.defect());
    m_boat->setDefect(game::Boat::DefectToMa, m_magazineCondition);
    if (m_turrets[0])
        m_boat->setDefect(game::Boat::DefectTur1, m_turrets[0]->gun().defect());
    if (m_turrets[1])
        m_boat->setDefect(game::Boat::DefectTur2, m_turrets[1]->gun().defect());
}


} // namespace fight
