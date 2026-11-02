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

#include "mountedturret.h"
#include "fight/boats/simpleboat.h"
#include "fight/collisionmanager.h"
#include "fight/orders.h"
#include "fight/scenario.h"
#include "fight/weapons/unitgun.h"
#include "gfx/pipeline.h"
#include "txt/desfile.h"
#include "util/random.h"
#include <glm/gtc/constants.hpp>
#include <glm/gtx/transform.hpp>
#include <algorithm>
#include <cmath>
#include <limits>


namespace fight {


namespace {

// the turret turns within 90 degrees of its mount's forward, its guns from 2 degrees down to
// straight up while tracking (level to straight up when turned to an angle); it fires within
// about 14 degrees of the target and never with its guns at the lowest pitch
const float SectorHalf = glm::half_pi<float>();
const float MinTrackPitch = glm::radians(-2.0f);
const float MinTurnPitch = 0;
const float MaxPitch = glm::half_pi<float>();
const float AimCos = 0.97f;
// a soft turn leaves errors within a degree to the damping, a hard one lands within half a degree
const float SoftDone = glm::radians(1.0f);
const float HardDone = glm::radians(0.5f);
// the sweep: steps of 15 to 90 degrees, after a pause of 0.5 to 2.5 s once there
const float MinSweepStep = glm::radians(15.0f);
const float MaxSweepStep = glm::radians(90.0f);
const float MinSweepPause = 0.5f;
const float MaxSweepPause = 2.5f;
// durations from this on mean "forever"
const int Forever = 100000;

float uniform()
{
    return util::random()/float(RAND_MAX);
}

float between(float min, float max)
{
    return min + (max - min)*uniform();
}

// the turret's own explosions at the destruction sequence's steps: types (EXPLOSIO.DES) and
// spreads, and the pieces of debris (DEBRIS.DES) of the third and the last step
const std::vector<std::pair<int, float>> TurretBursts[6] = {
    { { 0, 0.6f }, { 3, 0.6f }, { 3, 0.6f } },
    { { 4, 0.5f } },
    { { 0, 0.4f }, { 3, 0.4f }, { 3, 0.7f } },
    { { 4, 0.3f } },
    { { 1, 0.6f } },
    { { 4, 0.1f }, { 1, 0.4f } },
};

} // namespace


MountedTurret::MountedTurret(Scenario *scenario, SimpleBoat *carrier, txt::DesFile &file, const ObjectInfo &info,
                             const Vector3D &position, bool below, float alpha) :
    ActiveObject(scenario, file, info),
    m_carrier(carrier),
    m_mountPosition(position),
    m_below(below),
    m_restYaw(below ? alpha : -alpha)
{
    m_model.reset(new Turret(scenario, file));
    file.setSection("size");
    m_modelScale = file.value("scale", 1).toFloat()/32.0f;
    file.setSection("cluster");
    m_armOffset[0] = Vector3D(file.value("ArmROffsetX").toFloat(), file.value("ArmROffsetY").toFloat(), file.value("ArmROffsetZ").toFloat())*m_modelScale;
    m_armOffset[1] = Vector3D(file.value("ArmLOffsetX").toFloat(), file.value("ArmLOffsetY").toFloat(), file.value("ArmLOffsetZ").toFloat())*m_modelScale;
    const bool twoArms = file.contains("arml") && file.value("arml").toString() != "no";

    file.setSection("kinematics");
    m_yawAcceleration = file.value("MaxAngleAccelerationA").toFloat();
    m_yawFriction = file.value("AngleFrictionA").toFloat();
    m_maxYawVelocity = file.value("MaxAngleVelocityA", 1).toFloat();
    m_maxPitchVelocity = file.value("MaxAngleVelocityB", 1).toFloat();
    m_softYaw = file.value("SoftBodyAdjust", "yes").toString().trimmed().toLower() != "no";

    // its own sensor
    file.setSection("sensor");
    m_info.sensor = file.value("sen", 1).toInt();
    m_info.sensorRange = file.value("senr", 100).toInt();
    m_info.sleeps = false;

    // a gun on each arm, each with all the barrels, firing in the volley's rhythm
    const UnitGun::Mount mount = UnitGun::readMount(file);
    if (UnitGun::isGun(mount.model))
    {
        std::vector<Vector3D> muzzles;
        for (const Vector3D &muzzle : mount.muzzles)
            muzzles.push_back(muzzle*m_modelScale);
        for (int arm = 0; arm < (twoArms ? 2 : 1); arm++)
        {
            m_guns[arm].reset(new UnitGun(scenario, this, mount.model, muzzles, &file));
            m_guns[arm]->setRestartOnRelease(true);
            if (mount.ammunition >= 0)
                m_guns[arm]->setAmmunition(mount.ammunition);
        }
    }

    m_sensing.reset(new Orders(scenario, this, {}));
    m_yaw = m_restYaw;
    m_pitch = 0;
    m_static = false;
    m_position = pivot();
    const float r = 0.5f*glm::length(m_model->box().dim())*m_modelScale;
    m_box = BoundingBox(m_position - Vector3D(r), m_position + Vector3D(r));
    disable();
    scenario->collisionManager().addObject(this);
}


MountedTurret::~MountedTurret()
{
}


Vector3D MountedTurret::pivot() const
{
    return m_carrier->position() + glm::transpose(m_carrier->orientation())*m_mountPosition;
}


// the mount's frame is the carrier's, turned over about its length for a turret under the hull
Vector3D MountedTurret::toWorldDirection(const Vector3D &mount) const
{
    const Vector3D local = m_below ? Vector3D(-mount.x, mount.y, -mount.z) : mount;
    return glm::transpose(m_carrier->orientation())*local;
}


Vector3D MountedTurret::toMountDirection(const Vector3D &world) const
{
    const Vector3D local = m_carrier->orientation()*world;
    return m_below ? Vector3D(-local.x, local.y, -local.z) : local;
}


Vector3D MountedTurret::aim() const
{
    return toWorldDirection(Vector3D(std::sin(m_yaw)*std::cos(m_pitch), std::cos(m_yaw)*std::cos(m_pitch), std::sin(m_pitch)));
}


bool MountedTurret::turnTo(float yaw, float pitch, bool tracking, float elapsedTime)
{
    pitch = glm::clamp(pitch, tracking ? MinTrackPitch : MinTurnPitch, MaxPitch);
    const float dt = std::max(elapsedTime, 1e-4f);
    const float yawError = std::remainder(yaw - m_yaw, glm::two_pi<float>());
    float pitchError = pitch - m_pitch;
    bool done;
    if (!tracking || m_softYaw)
    {
        float acceleration = glm::abs(yawError) > SoftDone ? m_yawAcceleration*yawError/glm::pi<float>() : 0.0f;
        acceleration -= m_yawVelocity*std::min(m_yawFriction, 1/dt);
        m_yaw += m_yawVelocity*dt + 0.5f*acceleration*dt*dt;
        m_yawVelocity = glm::clamp(m_yawVelocity + acceleration*dt, -m_maxYawVelocity, m_maxYawVelocity);
        done = glm::abs(yawError) <= SoftDone;
    }
    else
    {
        m_yawVelocity = glm::abs(yawError) > HardDone ? glm::sign(yawError)*std::min(m_maxYawVelocity, glm::abs(yawError)/dt) : 0.0f;
        m_yaw += m_yawVelocity*dt;
        done = glm::abs(yawError) <= HardDone;
    }
    if (tracking)
    {
        m_pitchVelocity = glm::abs(pitchError) > HardDone ? glm::sign(pitchError)*std::min(m_maxPitchVelocity, glm::abs(pitchError)/dt) : 0.0f;
        m_pitch += m_pitchVelocity*dt;
        done = done && glm::abs(pitchError) <= HardDone;
    }
    else
    {
        float acceleration = glm::abs(pitchError) > SoftDone ? m_yawAcceleration*pitchError/glm::pi<float>() : 0.0f;
        acceleration -= m_pitchVelocity*std::min(m_yawFriction, 1/dt);
        m_pitch += m_pitchVelocity*dt + 0.5f*acceleration*dt*dt;
        m_pitchVelocity = glm::clamp(m_pitchVelocity + acceleration*dt, -m_maxPitchVelocity, m_maxPitchVelocity);
        done = done && glm::abs(pitchError) <= SoftDone;
    }
    const float low = tracking ? MinTrackPitch : MinTurnPitch;
    if (m_pitch < low || m_pitch > MaxPitch)
    {
        m_pitch = glm::clamp(m_pitch, low, MaxPitch);
        m_pitchVelocity = 0;
    }
    m_yaw = std::remainder(m_yaw, glm::two_pi<float>());
    return done;
}


Vector3D MountedTurret::leadPoint(const ActiveObject *target) const
{
    Vector3D point = target->position();
    const float speed = m_guns[0] ? m_guns[0]->projectileSpeed() : 0;
    if (speed > 0)
    {
        // the earliest t with |d + v t| = s t
        const Vector3D d = point - pivot();
        const Vector3D v = target->velocity();
        const float a = glm::dot(v, v) - speed*speed, b = 2*glm::dot(d, v), c = glm::dot(d, d);
        float t = -1;
        if (glm::abs(a) < 1e-6f)
            t = glm::abs(b) > 1e-6f ? -c/b : -1;
        else
        {
            const float disc = b*b - 4*a*c;
            if (disc >= 0)
            {
                const float root = std::sqrt(disc);
                const float t1 = (-b - root)/(2*a), t2 = (-b + root)/(2*a);
                t = std::min(t1, t2) > 0 ? std::min(t1, t2) : std::max(t1, t2);
            }
        }
        if (t > 0)
            point += v*t;
    }
    if (const SimpleBoat *boat = dynamic_cast<const SimpleBoat *>(target))
        if (boat->isGroundVehicle())
            point.z += 0.4f*boat->collisionRadius();
    return point;
}


ActiveObject* MountedTurret::search()
{
    if (ActiveObject *attacker = m_sensing->shooter(this, 1))
        if (ActiveObject *taken = m_sensing->engage(attacker))
            return taken;
    return m_sensing->engage(m_sensing->scan());
}


void MountedTurret::setTarget(ActiveObject *target)
{
    if (!target)
        m_sensing->engage(nullptr);
    m_target = target;
}


void MountedTurret::setOrder(const OrderSlot *slot)
{
    m_orderType = slot ? slot->type : OrderSlot::None;
    m_enemyId = slot ? slot->ene : -1;
    const float now = m_scenario->time();
    m_orderEnd = slot && slot->dur < Forever ? now + slot->dur : std::numeric_limits<float>::max();
    if (m_state == State::Paralysed || isDying())
        return;
    setTarget(nullptr);
    m_sweepYaw = 0;
    m_sweepDirection = 0;
    m_sweepTurning = true;
    switch (m_orderType)
    {
    case OrderSlot::Patrol:
        m_state = State::Sweep;
        break;
    case OrderSlot::Goto:
    case OrderSlot::GotoWithArrive:
    case OrderSlot::GotoWithInfiltrate:
    case OrderSlot::Escort:
        // sweeping for as long as the order lasts
        m_orderEnd = std::numeric_limits<float>::max();
        m_state = State::Sweep;
        break;
    case OrderSlot::Attack:
        m_state = State::Attack;
        break;
    case OrderSlot::Wait:
        m_state = State::Wait;
        break;
    default:
        m_state = State::Rest;
    }
}


void MountedTurret::reset()
{
    if (m_state == State::Paralysed || isDying())
        return;
    setTarget(nullptr);
    m_state = State::Rest;
}


void MountedTurret::paralyse()
{
    if (isDying())
        return;
    m_shockShield = 0;
    setTarget(nullptr);
    m_state = State::Paralysed;
    m_trigger = false;
}


void MountedTurret::blowUpTurret()
{
    if (isDying())
        return;
    setTarget(nullptr);
    m_trigger = false;
    m_explodeStep = 0;
    m_explodeStart = m_scenario->time();
}


void MountedTurret::damage(int kinetic, int shock, const Vector3D &position, ActiveObject *attacker)
{
    if (isDying())
        return;
    // a hit on a turret is an attack on its bomber
    if (attacker && attacker != this && attacker != m_carrier)
        m_carrier->noteAttacked();
    if (kinetic != 0)
    {
        m_hitTime = m_scenario->time();
        m_lastAttacker = attacker != this ? attacker : nullptr;
    }
    m_kineticShield -= kinetic;
    m_shockShield -= shock;
    if (m_kineticShield < 0)
        blowUpTurret();
    else if (m_shockShield < 0 && m_state != State::Paralysed)
        paralyse();
    if (attacker && attacker == m_scenario->player())
        m_scenario->playerHit(this, isExploding());
}


void MountedTurret::updateExplosion()
{
    const float t = m_scenario->time() - m_explodeStart;
    const Vector3D centre = pivot();
    const float radius = std::max(0.5f, 0.5f*glm::length(m_model->box().dim())*m_modelScale);
    const float fifth = glm::pi<float>()/5;
    while (m_explodeStep >= 0 && m_explodeStep < 6 && t >= WreckTimes[m_explodeStep])
    {
        const int step = m_explodeStep++;
        for (const auto &burst : TurretBursts[step])
            wreckBurst(burst.first, burst.second, centre, radius);
        if (step == 2)
            for (int index : { 17, 2, 1 })
                wreckDebris(index, fifth, 2*fifth, centre, radius);
        if (step == 5)
        {
            for (int index : { 0, 17, 2, 1 })
                wreckDebris(index, 0, fifth, centre, radius);
            // gone, without counting as one of the bomber's squadron
            m_gone = true;
            vanish();
        }
    }
}


void MountedTurret::fire(float elapsedTime)
{
    const glm::mat3 toWorld = glm::transpose(m_carrier->orientation());
    const glm::mat3 flip = m_below ? glm::mat3(glm::vec3(-1, 0, 0), glm::vec3(0, 1, 0), glm::vec3(0, 0, -1)) : glm::mat3(1);
    const glm::mat3 body = toWorld*flip*glm::mat3(glm::rotate(-m_yaw, Vector3D(0, 0, 1)));
    const Vector3D dir = aim();
    Vector3D right = body*Vector3D(1, 0, 0);
    const Vector3D up = glm::normalize(glm::cross(right, dir));
    right = glm::cross(dir, up);
    // aligned within 14 degrees from between the arms, but not with the guns at their lowest
    bool trigger = false;
    if (m_target && isAliveTarget())
    {
        const Vector3D way = leadPoint(m_target) - pivot();
        trigger = glm::length(way) > 0.1f && m_pitch > MinTrackPitch + 1e-4f && glm::dot(dir, glm::normalize(way)) > AimCos;
    }
    m_trigger = trigger;
    for (int arm = 0; arm < 2; arm++)
        if (m_guns[arm])
            m_guns[arm]->update(elapsedTime, trigger, pivot() + body*m_armOffset[arm], dir, up, right);
}


bool MountedTurret::isAliveTarget() const
{
    return m_target && m_sensing->isAlive(m_target);
}


void MountedTurret::think(float elapsedTime)
{
    if (m_gone)
        return;
    m_position = pivot();
    const float r = 0.5f*glm::length(m_model->box().dim())*m_modelScale;
    m_box = BoundingBox(m_position - Vector3D(r), m_position + Vector3D(r));
    if (isExploding())
    {
        updateExplosion();
        return;
    }

    const float now = m_scenario->time();
    ActiveObject *enemy = m_orderType == OrderSlot::Attack ? m_sensing->findUnit(m_enemyId) : nullptr;
    const bool enemyHere = enemy && m_sensing->isHostile(enemy) && m_sensing->detects(enemy, 1);
    bool tracking = false;
    switch (m_state)
    {
    case State::Paralysed:
        // level, along its mount, for good
        turnTo(m_restYaw, glm::radians(-14.0f), false, elapsedTime);
        break;

    case State::Rest:
    case State::Wait:
        // no search, no fire
        turnTo(m_restYaw, 0, false, elapsedTime);
        break;

    case State::Sweep:
        if (ActiveObject *target = search())
        {
            setTarget(target);
            m_state = State::Fire;
            tracking = true;
        }
        else if (m_sweepTurning)
        {
            if (turnTo(m_restYaw + m_sweepYaw, 0, false, elapsedTime))
            {
                m_sweepTurning = false;
                m_waitEnd = now + between(MinSweepPause, MaxSweepPause);
            }
        }
        else
        {
            turnTo(m_restYaw + m_sweepYaw, 0, false, elapsedTime);
            if (now >= m_waitEnd)
            {
                // a step either way, back from the half circle's edges
                const float sign = m_sweepDirection != 0 ? float(m_sweepDirection) : (uniform() < 0.5f ? -1.0f : 1.0f);
                m_sweepYaw += sign*between(MinSweepStep, MaxSweepStep);
                m_sweepDirection = 0;
                if (m_sweepYaw > SectorHalf)
                {
                    m_sweepYaw = SectorHalf;
                    m_sweepDirection = -1;
                }
                else if (m_sweepYaw < -SectorHalf)
                {
                    m_sweepYaw = -SectorHalf;
                    m_sweepDirection = 1;
                }
                m_sweepTurning = true;
            }
        }
        break;

    case State::Attack:
        {
            // the enemy first, else whoever hit it or what the sensor finds; meanwhile it faces
            // its mount's forward
            ActiveObject *target = enemyHere ? m_sensing->engage(enemy) : nullptr;
            if (!target)
                target = search();
            if (target)
            {
                setTarget(target);
                m_state = State::Fire;
                tracking = true;
            }
            else
                turnTo(m_restYaw, 0, false, elapsedTime);
        }
        break;

    case State::Fire:
        tracking = true;
        switch (m_orderType)
        {
        case OrderSlot::Attack:
            {
                // chosen anew every frame
                ActiveObject *target = enemyHere ? m_sensing->engage(enemy) : nullptr;
                if (!target)
                    target = search();
                setTarget(target);
                if (!target)
                {
                    m_state = State::Attack;
                    tracking = false;
                }
            }
            break;
        case OrderSlot::Patrol:
        case OrderSlot::Escort:
            if (ActiveObject *attacker = m_sensing->shooter(this, 1))
                if (attacker != m_target && m_sensing->engage(attacker))
                    m_target = attacker;
            if (!m_target || !m_sensing->isHostile(m_target) || !m_sensing->detects(m_target, 1))
            {
                setTarget(nullptr);
                m_state = State::Sweep;
                m_sweepTurning = true;
                tracking = false;
            }
            break;
        default:
            // Goto and the like: the first target, for as long as the order lasts
            break;
        }
        break;
    }

    if (tracking && m_target)
    {
        if (isAliveTarget())
        {
            const Vector3D way = toMountDirection(leadPoint(m_target) - pivot());
            const float yaw = std::atan2(way.x, way.y);
            const float pitch = std::atan2(way.z, glm::length(Vector2D(way)));
            // beyond its half of the sky it turns back to its mount's forward
            if (glm::abs(std::remainder(yaw - m_restYaw, glm::two_pi<float>())) > SectorHalf)
                turnTo(m_restYaw, 0, false, elapsedTime);
            else
                turnTo(yaw, pitch, true, elapsedTime);
        }
    }

    // the sweeping and attacking orders run out
    if ((m_state == State::Sweep || m_state == State::Attack || m_state == State::Fire) && now > m_orderEnd)
    {
        setTarget(nullptr);
        m_state = State::Rest;
    }
    if (m_state != State::Fire)
        m_target = nullptr;
    fire(elapsedTime);
}


void MountedTurret::draw()
{
    if (m_gone)
        return;
    gfx::Pipeline::pushMatrix();
    gfx::Pipeline::translate(m_carrier->position());
    gfx::Pipeline::multiply(Matrix(glm::transpose(m_carrier->orientation())));
    gfx::Pipeline::translate(m_mountPosition);
    if (m_below)
        gfx::Pipeline::rotate(180, Vector3D(0, 1, 0));
    gfx::Pipeline::rotate(-glm::degrees(m_yaw), Vector3D(0, 0, 1));
    gfx::Pipeline::scale(m_modelScale);
    m_model->setArmPitch(m_pitch);
    m_model->draw();
    gfx::Pipeline::popMatrix();
}


bool MountedTurret::intersect(const Vector3D &start, const Vector3D &dir, float radius, float &distance, Vector3D &normal)
{
    if (m_gone)
        return false;
    // into the model's own coordinates
    const glm::mat3 flip = m_below ? glm::mat3(glm::vec3(-1, 0, 0), glm::vec3(0, 1, 0), glm::vec3(0, 0, -1)) : glm::mat3(1);
    const glm::mat3 toModel = glm::transpose(glm::mat3(glm::rotate(-m_yaw, Vector3D(0, 0, 1))))*flip*m_carrier->orientation();
    if (!m_model->intersect(toModel*(start - pivot())/m_modelScale, toModel*dir, radius/m_modelScale, distance, normal))
        return false;
    distance *= m_modelScale;
    normal = glm::transpose(toModel)*normal;
    return true;
}


} // namespace fight
