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

#include "simpleboat.h"
#include "fight/collisionmanager.h"
#include "fight/effects/effect.h"
#include "fight/effects/effectmanager.h"
#include "fight/objects/mine.h"
#include "fight/objects/mountedturret.h"
#include "fight/scenario.h"
#include "fight/module.h"
#include "fight/orders.h"
#include "fight/weapons/torpedo.h"
#include "fight/weapons/unitgun.h"
#include "sfx/samplemap.h"
#include "txt/desfile.h"
#include "util/random.h"
#include "gfx/pipeline.h"
#include <glm/gtx/transform.hpp>
#include <glm/gtc/constants.hpp>
#include <cmath>
#include <glm/gtx/norm.hpp>
#include <algorithm>


namespace fight {


// a paralysed boat (docs/ROADMAP.md, appendix J): the most it turns about each axis (rad/s) and
// how long it turns and loses its way, braking at three times its retardation
const float ParalysedSpin = 0.227f;
const float ParalysedCoast = 5.0f;
const float ParalysedBrake = 3.0f;
// the AI's turn leaves errors within a degree to the damping (docs/ROADMAP.md, appendix J)
const float TurnDeadZone = glm::radians(1.0f);


SimpleBoat::SimpleBoat(Scenario *scenario, txt::DesFile &file, const ObjectInfo &info, float yaw) :
    SimpleObject(scenario, file, info),
    m_velocityTarget(0),
    m_fullThrottle(false),
    m_angleVelocity(0),
    m_turnVelocity(0)
{
    m_orientation = glm::mat3(glm::rotate(yaw, Vector3D(0, 0, 1)));
    m_orientationInverted = glm::transpose(m_orientation);

    file.setSection("kinematics");
    m_maxVelocity = file.value("MaxVelocity").toFloat() / 32.0f;
    m_minVelocity = file.value("MinVelocity").toFloat() / 32.0f;
    m_maxOrientationAcceleration = file.value("MaxOrientationAcceleration").toFloat() / 32.0f;
    m_maxOrientationRetardation = file.value("MaxOrientationRetardation").toFloat() / 32.0f;
    m_orientationFriction = Vector3D(-file.value("OrientationFrictionX").toFloat(), -file.value("OrientationFrictionY").toFloat(), -file.value("OrientationFrictionZ").toFloat());

    m_maxAngleVelocity = Vector3D(file.value("MaxAngleVelocityB").toFloat(), file.value("MaxAngleVelocityC").toFloat(), file.value("MaxAngleVelocityA").toFloat());
    m_maxAngleAcceleration = Vector3D(file.value("MaxAngleAccelerationB").toFloat(), file.value("MaxAngleAccelerationC").toFloat(), file.value("MaxAngleAccelerationA").toFloat());
    m_angleFriction = Vector3D(-file.value("AngleFrictionB").toFloat(), -file.value("AngleFrictionC").toFloat(), -file.value("AngleFrictionA").toFloat());
    file.setSection("fluid");
    m_currentFactor = file.value("Current", 1).toFloat();
    file.setSection("buzzermagazine");
    if (file.contains("Type"))
    {
        m_buzzers.type = file.value("Type").toInt();
        m_buzzers.relative = Vector3D(file.value("RelativeX").toFloat(), file.value("RelativeY").toFloat(), file.value("RelativeZ").toFloat());
        m_buzzers.range = file.value("EjectRange").toFloat();
        m_buzzers.probability = file.value("EjectProbability").toFloat();
        m_buzzers.speed = file.value("EjectVelocity").toFloat()/32.0f;
        m_buzzers.delay = file.value("EjectDelay").toFloat();
    }
    file.setSection("torpedomagazine");
    for (int i = 0; file.contains(String("relative%1x").arg(i)); i++)
        m_torpedoTubes.push_back(Vector3D(file.value(String("relative%1x").arg(i)).toFloat(), file.value(String("relative%1y").arg(i)).toFloat(),
                                          file.value(String("relative%1z").arg(i)).toFloat())*m_scale);
    file.setSection("kinematics");
    m_minClimbingAngle = glm::radians(file.value("MinClimbingAngle", 20).toFloat());
    m_maxClimbingAngle = glm::radians(file.value("MaxClimbingAngle", 20).toFloat());

    // a sphere that fits into the hull, so the boat can get close to the ground
    const Vector3D dim = m_localBox.dim();
    m_collisionRadius = glm::clamp(0.5f*std::min(dim.x, std::min(dim.y, dim.z)), 1.5f, 8.0f);

    file.setSection("attack");
    AttackSettings &a = m_attackSettings;
    auto setting = [&file](const char *key, float &value, float scale = 1) {
        if (file.contains(key))
            value = file.value(key).toFloat()*scale;
    };
    setting("AlignProbabilityFront", a.alignFront);
    setting("AlignProbabilityBack", a.alignBack);
    setting("AlignProbabilityLeft", a.alignLeft);
    setting("AlignProbabilityRight", a.alignRight);
    setting("MinAlignRadius", a.minAlignRadius, 1/32.0f);
    setting("MaxAlignRadius", a.maxAlignRadius, 1/32.0f);
    setting("MinAlignGamma", a.minAlignGamma);
    setting("MaxAlignGamma", a.maxAlignGamma);
    setting("MinAlignTime", a.minAlignTime);
    setting("MaxAlignTime", a.maxAlignTime);
    setting("AlignProbabilityTorpedoFire", a.alignTorpedo);
    setting("AttackProbabilityTurn", a.turnProbability);
    setting("AttackProbabilityHit", a.hitProbability);
    setting("AttackProbabilityTorpedoFire", a.attackTorpedo);
    setting("MinAttackTime", a.minAttackTime);
    setting("MaxAttackTime", a.maxAttackTime);
    setting("MinFlightRadius", a.minFlightRadius, 1/32.0f);
    setting("MaxFlightRadius", a.maxFlightRadius, 1/32.0f);
    setting("MinFlightTime", a.minFlightTime);
    setting("MaxFlightTime", a.maxFlightTime);
    setting("FlightPointProbabilityUL", a.flightUL);
    setting("FlightPointProbabilityUR", a.flightUR);
    setting("FlightPointProbabilityDL", a.flightDL);
    setting("FlightPointProbabilityDR", a.flightDR);
    setting("FlightProbabilityTorpedoFire", a.flightTorpedo);
    setting("FlightProbabilityHit", a.flightHit);
    setting("AlignProbabilityHook", a.hook);
    setting("AlignProbabilityCross", a.cross);
    setting("AlignProbabilityRetain", a.retain);
    setting("AlignProbabilityChange", a.change);
    setting("AlignProbabilityHit", a.alignHit);

    // boats move, so collision results must not be cached
    m_static = false;
    updateBox();
}


SimpleBoat::~SimpleBoat()
{
}


void SimpleBoat::steerTo(const Vector3D &point, float speed)
{
    steer(point, speed, glm::length(Vector2D(point - m_position)));
}


// the nose to the point's own direction, the pitch to its true elevation (docs/ROADMAP.md, appendix
// J): the guns fire along the nose
void SimpleBoat::pointAt(const Vector3D &point, float speed)
{
    steer(point, speed, glm::length(Vector2D(point - m_position)));
}


// the pitch makes for the point's height over this horizontal way; big ships climb no steeper
// than their climbing angles, boats at any angle
void SimpleBoat::steer(const Vector3D &point, float speed, float reach)
{
    const Vector3D way = point - m_position;
    const Vector3D local = m_orientation*way;

    // positive turns to the right (yaw) and pitch the nose down
    const float yawError = std::atan2(local.x, local.y);
    const float pitch = std::asin(glm::clamp(dir().z, -1.0f, 1.0f));
    float targetPitch = std::atan2(way.z, std::max(reach, 1e-3f));
    if (isBigShip())
        targetPitch = glm::clamp(targetPitch, -m_minClimbingAngle, m_maxClimbingAngle);
    m_turnVelocity = Vector3D(glm::clamp(2.0f*(pitch - targetPitch), -1.0f, 1.0f), 0, glm::clamp(1.5f*yawError, -1.0f, 1.0f));
    turnBy(yawError, pitch - targetPitch, speed);
}


void SimpleBoat::turnBy(float yawError, float pitchError, float speed)
{
    m_turnError = Vector3D(pitchError, 0, yawError);
    m_steered = true;
    m_velocityTarget = speed;
}


// the heading held, the pitch going for half its present value
void SimpleBoat::hold()
{
    const float pitch = std::asin(glm::clamp(dir().z, -1.0f, 1.0f));
    m_turnVelocity = Vector3D(glm::clamp(2.0f*pitch, -1.0f, 1.0f), 0, 0);
    turnBy(0, 0.5f*pitch, 0);
}


Vector3D SimpleBoat::nextTorpedoTube()
{
    if (m_torpedoTubes.empty())
        return m_position + dir()*(m_localBox.maxPoint().y + 1.0f);
    const Vector3D tube = m_torpedoTubes[m_nextTube];
    m_nextTube = (m_nextTube + 1) % int(m_torpedoTubes.size());
    return m_position + m_orientationInverted*tube;
}


void SimpleBoat::setPosition(const Vector3D &pos)
{
    m_position = pos;
    updateBox();
}


void SimpleBoat::place(const Vector3D &pos, const Vector3D &direction)
{
    const Vector3D forward = glm::normalize(direction);
    const Vector3D right = glm::normalize(glm::cross(forward, Vector3D(0, 0, 1)));
    m_orientationInverted = glm::mat3(right, forward, glm::cross(right, forward));
    m_orientation = glm::transpose(m_orientationInverted);
    m_velocity = Vector3D(0);
    setPosition(pos);
}


void SimpleBoat::updateBox()
{
    m_box = m_localBox.transform(glm::translate(m_position) * Matrix(m_orientationInverted));
}


// boats fire from their [StaticGun] mountings (the nose without any) in the rhythm of their
// [VolleyGun], which starts again whenever the target leaves the aim (docs/ROADMAP.md, appendix J);
// the mission's gun replaces the file's model, with that model's own ammunition
void SimpleBoat::arm(int model, txt::DesFile &file)
{
    if (!UnitGun::isGun(model))
        return;
    std::vector<Vector3D> muzzles;
    for (const Vector3D &muzzle : UnitGun::readMount(file).muzzles)
        muzzles.push_back(muzzle*m_scale);
    if (muzzles.empty())
        muzzles.push_back(Vector3D(0, m_localBox.maxPoint().y + 0.5f, 0));
    m_unitGun.reset(new UnitGun(m_scenario, this, model, muzzles, &file));
    m_unitGun->setRestartOnRelease(true);
}


bool SimpleBoat::update(float elapsedTime)
{
    if (isWrecking())
    {
        updateWreck(elapsedTime);
        return false;
    }
    if (m_tumbling)
    {
        updateTumble(elapsedTime);
        return false;
    }
    if (m_orders)
        m_orders->update();
    updateOrientation(elapsedTime);
    updateVelocity(elapsedTime);
    move(m_velocity*elapsedTime);
    if (m_unitGun)
        m_unitGun->update(elapsedTime, m_trigger, m_position, dir(), up(), right());
    // the buzzer magazine works only in patrols, attacks, ambushes and escorts
    if (m_orders && m_orders->usesBuzzers())
        updateBuzzers();

    return false;
}


// it looks every frame until the first buzzer, then every EjectDelay; the buzzer leaves its place (the tail if none is given) upwards and
// backwards at the magazine's speed on top of the boat's own velocity, of the boat's faction
void SimpleBoat::updateBuzzers()
{
    const float now = m_scenario->time();
    if (m_buzzers.type == 2 || now - m_buzzers.lastLook < m_buzzers.interval)
        return;
    m_buzzers.lastLook = now;
    const Torpedo *threat = m_scenario->nearestThreatTo(this);
    if (!threat || glm::length(threat->position() - m_position) >= m_buzzers.range || util::random()/float(RAND_MAX) >= m_buzzers.probability)
        return;
    const Vector3D local = m_buzzers.relative != Vector3D(0) ? m_buzzers.relative/32.0f : Vector3D(0, m_localBox.minPoint().y, 0);
    const Vector3D position = m_position + m_orientationInverted*local;
    const Vector3D velocity = glm::normalize(2.0f*up() - dir())*m_buzzers.speed + m_velocity;
    const bool works = util::random()/float(RAND_MAX) < (m_buzzers.type == 1 ? 0.9f : 0.4f);
    const Effects debris = m_buzzers.type == 1 ? (works ? Effects::Debris_18 : Effects::Debris_20) : (works ? Effects::Debris_19 : Effects::Debris_21);
    m_scenario->effectManager().addDecoy(debris, works ? Particles::Decoy::Buzzer : Particles::Decoy::None, iff(), position, velocity);
    m_buzzers.interval = m_buzzers.delay;
}


Vector3D SimpleBoat::slideAlongObjects(const Vector3D &position, const Vector3D &delta, float speed)
{
    if (glm::length2(delta) < 1e-8f)
        return delta;
    // debris slower than the unit
    Particles::DebrisHit debris;
    if (m_scenario->effectManager().hitDebris(position, position + delta, m_collisionRadius, speed < 0 ? glm::length(m_velocity) : speed, m_kineticStrength, debris))
        damage(debris.kinetic, debris.shock, debris.position);
    // another faction's mine in the way goes up (docs/ROADMAP.md, appendix Q); the unit carries on
    auto mine = [this](const Object *object) {
        const Mine *mine = dynamic_cast<const Mine *>(object);
        return mine && mine->collidesWith(this);
    };
    Vector3D contact, normal;
    if (Object *hit = m_scenario->collisionManager().testCollision(position, position + delta, m_collisionRadius, contact, normal, nullptr, this, mine))
        static_cast<Mine *>(hit)->collide(this, contact);
    if (!collidesWithObjects())
        return delta;
    const Object *docking = m_orders ? m_orders->dockingTarget() : nullptr;
    // (a bomber's turrets ride on its hull, which is the obstacle)
    auto obstacle = [docking](const Object *object) {
        return object != docking && !dynamic_cast<const Mine *>(object) && !dynamic_cast<const Effect *>(object) &&
               !dynamic_cast<const MountedTurret *>(object);
    };
    if (!m_scenario->collisionManager().testCollision(position, position + delta, m_collisionRadius, contact, normal, nullptr, this, obstacle))
        return delta;
    // already touching and moving away (or along it): free to go
    if (glm::dot(delta, normal) >= 0)
        return delta;
    Vector3D rest = delta - (contact - position);
    rest -= glm::dot(rest, normal)*normal;
    const float into = glm::dot(m_velocity, normal);
    if (into < 0)
        m_velocity -= into*normal;
    return contact - position + normal*1e-3f + rest;
}


void SimpleBoat::move(Vector3D delta)
{
    delta = slideAlongObjects(m_position, delta);
    Vector3D position = m_position;
    for (int i = 0; i < 5 && glm::length2(delta) > 1e-5f; i++)
    {
        Vector3D posSurface, normSurface;
        if (!m_scenario->surface().testCollision(position, position + delta, m_collisionRadius, posSurface, normSurface))
        {
            position += delta;
            break;
        }
        delta -= posSurface - position;
        position = posSurface;
        delta -= (glm::dot(delta, normSurface) - 1e-3f)*normSurface;
        // the ground stops the part of the movement that goes into it
        const float into = glm::dot(m_velocity, normSurface);
        if (into < 0)
            m_velocity -= into*normSurface;
    }
    setPosition(position);
}


void SimpleBoat::draw()
{
    gfx::Pipeline::pushMatrix();
    gfx::Pipeline::translate(m_position);
    gfx::Pipeline::multiply(Matrix(m_orientationInverted));
    if (m_bank != 0)
        gfx::Pipeline::rotate(glm::degrees(m_bank), Vector3D(0, 1, 0));
    gfx::Pipeline::scale(m_scale);
    m_base->draw();
    gfx::Pipeline::popMatrix();
}


bool SimpleBoat::intersect(const Vector3D &start, const Vector3D &dir, float radius, float &distance, Vector3D &normal)
{
    // test in the model's own coordinates
    if (m_base->intersect(m_orientation*(start - m_position)/m_scale, m_orientation*dir, radius/m_scale, distance, normal))
    {
        distance *= m_scale;
        normal = m_orientationInverted*normal;
        return true;
    }
    return false;
}


void SimpleBoat::updateVelocity(float elapsedTime)
{
    Vector3D velocity = m_orientation * m_velocity;
    // the water's drag, and a current's push: a steady push is held by the drag across the boat
    // and by the cap along its axis (docs/ROADMAP.md, appendix D)
    Vector3D acceleration = velocity*m_orientationFriction + m_orientation*(m_scenario->current(m_position)*m_currentFactor + m_push);

    // the booster pushes the boat to twice its top speed whatever the throttle says
    const float target = m_speedBoost > 1.0f ? m_maxVelocity*m_speedBoost : m_velocityTarget;
    float orientationAcceleration = 0;
    if (target > velocity.y + 0.1f)
        orientationAcceleration = m_maxOrientationAcceleration*(m_fullThrottle ? 2 : 1)*m_accelerationBoost;
    if (target < velocity.y - 0.1f)
        orientationAcceleration = m_maxOrientationRetardation*(m_fullThrottle ? 2 : 1);
    acceleration.y += orientationAcceleration;

    velocity += acceleration*elapsedTime;
    // nothing pushes the boat past twice its top speed along its axis
    velocity.y = glm::clamp(velocity.y, 2*m_minVelocity, 2*m_maxVelocity);
    m_velocity = m_orientationInverted * velocity;
}


// the drift after an energy defence's collapse (docs/ROADMAP.md, appendix J): the boat turns
// about its three axes at random rates up to 0.227 rad/s and brakes to a stop for 5 s, then stays
// still where it is, to be boarded or finished off
bool SimpleBoat::startTumbling()
{
    if (this == m_scenario->player() || isGroundVehicle())
        return false;
    auto unit = [](float scale) { return scale*(2.0f*util::random()/float(RAND_MAX) - 1.0f); };
    m_spin = Vector3D(unit(ParalysedSpin), unit(ParalysedSpin), unit(ParalysedSpin));
    m_tumbleEnd = m_scenario->time() + ParalysedCoast;
    m_tumbling = true;
    m_trigger = false;
    m_scenario->disengagePlayer(this);
    return true;
}


void SimpleBoat::updateTumble(float elapsedTime)
{
    // the boat turns and its way runs out, and then it hangs still in the water
    if (m_scenario->time() >= m_tumbleEnd)
    {
        m_velocity = Vector3D(0);
        return;
    }
    m_orientation *= glm::mat3(glm::rotate(m_spin.x*elapsedTime, Vector3D(1, 0, 0))) * glm::mat3(glm::rotate(m_spin.y*elapsedTime, Vector3D(0, 1, 0)))
                   * glm::mat3(glm::rotate(m_spin.z*elapsedTime, Vector3D(0, 0, 1)));
    m_orientationInverted = glm::transpose(m_orientation);
    // the engine brakes along the nose at three times its retardation, the water across it
    Vector3D local = m_orientation*m_velocity;
    local += local*m_orientationFriction*elapsedTime;
    const float brake = -ParalysedBrake*m_maxOrientationRetardation*elapsedTime;
    local.y -= glm::clamp(local.y, -brake, brake);
    m_velocity = m_orientationInverted*local;
    move(m_velocity*elapsedTime);
}


void SimpleBoat::updateOrientation(float elapsedTime)
{
    if (!m_steered || isBigShip())
    {
        m_angleVelocity += m_angleVelocity*m_angleFriction*elapsedTime;
        // turning in either direction
        const Vector3D turn = m_turnVelocity*m_maxAngleVelocity;
        if (glm::abs(m_angleVelocity.x) < glm::abs(turn.x))
            m_angleVelocity.x = turn.x;
        if (glm::abs(m_angleVelocity.z) < glm::abs(turn.z))
            m_angleVelocity.z = turn.z;
        m_orientation *= glm::mat3(glm::rotate(m_angleVelocity.x*elapsedTime, right())) * glm::mat3(glm::rotate(m_angleVelocity.z*elapsedTime, Vector3D(0, 0, 1)));
        m_orientationInverted = glm::transpose(m_orientation);
        return;
    }

    // the AI's turn (docs/ROADMAP.md, appendix J): an error beyond a degree accelerates the turn by
    // MaxAngleAcceleration times the error over pi, AngleFriction damps it, and the rate is held
    // to MaxAngleVelocity afterwards
    const float damping = elapsedTime > 0 ? 1/elapsedTime : 0;
    Vector3D turned(0);
    for (int axis : { 0, 2 })
    {
        const float error = m_turnError[axis];
        float acceleration = glm::abs(error) > TurnDeadZone ? m_maxAngleAcceleration[axis]*error/glm::pi<float>() : 0.0f;
        acceleration -= m_angleVelocity[axis]*std::min(-m_angleFriction[axis], damping);
        turned[axis] = m_angleVelocity[axis]*elapsedTime + 0.5f*acceleration*elapsedTime*elapsedTime;
        m_angleVelocity[axis] += acceleration*elapsedTime;
        const float most = glm::abs(m_maxAngleVelocity[axis]);
        m_angleVelocity[axis] = glm::clamp(m_angleVelocity[axis], -most, most);
    }
    m_orientation *= glm::mat3(glm::rotate(turned.x, right())) * glm::mat3(glm::rotate(turned.z, Vector3D(0, 0, 1)));
    m_orientationInverted = glm::transpose(m_orientation);

    // the hull banks into the turn by as many radians as it turns radians a second, following
    // the turn within about a second (docs/ROADMAP.md, appendix V); the boat itself stays level:
    // it steers, aims and is hit as before
    m_bank += (m_angleVelocity.z - m_bank)*std::min(1.0f, elapsedTime);
}


} // namespace fight


namespace fight {


namespace {

float uniform()
{
    return util::random()/float(RAND_MAX);
}

float between(float min, float max)
{
    return min + (max - min)*uniform();
}

Vector3D randomVector(float extent)
{
    return Vector3D(between(-extent, extent), between(-extent, extent), between(-extent, extent));
}

} // namespace


float SimpleBoat::hullRadius() const
{
    return 0.5f*std::max(m_localBox.dim().x, std::max(m_localBox.dim().y, m_localBox.dim().z));
}


// The destruction sequence (docs/ROADMAP.md, appendix R). A boat that keeps its course does so at
// 70 % of its speed; the others brake, and the player's boat drifts on.
bool SimpleBoat::startWreck()
{
    if (isGroundVehicle())
        return false;
    if (isWrecking())
        return true;
    const bool player = this == m_scenario->player();
    m_wreckStep = 0;
    m_wreckNext = m_scenario->time();
    m_wreckSpin = Vector3D(0);
    m_wreckRuns = !player && uniform() < 0.2f;
    if (m_wreckRuns)
        m_velocity *= 0.7f;
    m_tumbling = true;
    m_trigger = false;
    if (!player)
        m_scenario->disengagePlayer(this);
    nextWreckStep();
    return true;
}


void SimpleBoat::nextWreckStep()
{
    const bool player = this == m_scenario->player();
    const float fifth = glm::pi<float>()/5;
    wreckStepped(m_wreckStep);
    wreckBursts(m_wreckStep, m_position, hullRadius());
    // the next step after the times' gap, but see step 4
    float delay = m_wreckStep >= 0 && m_wreckStep < 6 ? WreckTimes[m_wreckStep + 1] - WreckTimes[m_wreckStep] : 0;
    switch (m_wreckStep)
    {
    case 0:
        if (player)
            for (int index : { 17, 17, 17, 1, 3, 3 })
                wreckDebris(index, -fifth, fifth);
        break;
    case 1:
    case 3:
        break;
    case 2:
        for (int index : { 2, 2, 1, 3, 3 })
            wreckDebris(index, fifth, 2*fifth);
        break;
    case 4:
        if (player)
            for (int index : { 17, 17, 17, 0, 1, 2 })
                wreckDebris(index, fifth, 2*fifth);
        // a boat that keeps its course goes up when it hits the ground, after five seconds at most
        if (m_wreckRuns)
            delay = 5.0f;
        break;
    case 5:
    {
        if (!player)
        {
            wreckLastDebris();
            m_wreckGone = true;
            m_wreckStep = 6;
            wreckEnd();
            return;
        }
        for (int index : { 2, 2, 2, 1, 3, 3 })
            wreckDebris(index, -fifth, fifth);
        // the wreck: the player's boat leaves one body
        const int roll = std::min(3, int(uniform()*4));
        wreckDebris(roll == 3 ? 14 : roll == 2 ? 15 : 16, 0, glm::half_pi<float>());
        // and blows up at both ends of its hull
        for (float end : { m_localBox.maxPoint().y, m_localBox.minPoint().y })
        {
            const Vector3D at = m_position + m_orientationInverted*Vector3D(0, 0.5f*end, 0);
            m_scenario->effectManager().addEffect(Effects::Explosion_2, at);
            sfx::SampleMap::playAt(sfx::Sample::Explosion_Big, at.x, at.y, at.z);
        }
        m_wreckGone = true;
        m_wreckStep = 6;
        return;
    }
    default:
        return;
    }
    if (wreckKicks())
    {
        m_wreckAcceleration = randomVector(30.0f)/32.0f;
        m_wreckSpinAcceleration = randomVector(0.2f);
    }
    m_wreckNext = m_scenario->time() + delay;
    m_wreckStep++;
}


// the last step's debris: a boat leaves none to three wreck bodies
void SimpleBoat::wreckLastDebris()
{
    const float fifth = glm::pi<float>()/5;
    for (int index : { 2, 2, 2, 1, 3, 3 })
        wreckDebris(index, -fifth, fifth);
    const int roll = std::min(3, int(uniform()*4));
    for (int body = 17 - roll; body <= 16; body++)
        wreckDebris(body, 0, glm::half_pi<float>());
}


void SimpleBoat::wreckEnd()
{
    vanish();
    // (not after an Explode order, which leaves the armour alone)
    if (m_kineticShield < 0)
        countDestroyed();
}


// between the bursts the hull drifts by its acceleration and spins up, a boat keeping its course
// spinning wildly (its spin drawn anew every frame); another boat's engine brakes its way along
// the nose, as it slows down for a stop (MaxOrientationRetardation), and the water its way across
void SimpleBoat::updateWreck(float elapsedTime)
{
    if (m_wreckGone)
        return;
    if (m_wreckRuns)
        m_wreckSpinAcceleration = randomVector(3.0f);
    else
    {
        m_velocity += m_wreckAcceleration*elapsedTime;
        if (this != m_scenario->player())
        {
            // the water's drag across the hull, and the engine against the way along it
            Vector3D local = m_orientation*m_velocity;
            local += local*m_orientationFriction*elapsedTime;
            m_velocity = m_orientationInverted*local;
            const float along = glm::dot(m_velocity, dir());
            const float brake = (along > 0 ? -m_maxOrientationRetardation : m_maxOrientationAcceleration)*elapsedTime;
            m_velocity -= dir()*glm::clamp(along, -brake, brake);
        }
    }
    m_wreckSpin += m_wreckSpinAcceleration*elapsedTime;
    m_orientation *= glm::mat3(glm::rotate(m_wreckSpin.x*elapsedTime, Vector3D(1, 0, 0))) * glm::mat3(glm::rotate(m_wreckSpin.y*elapsedTime, Vector3D(0, 1, 0)))
                   * glm::mat3(glm::rotate(m_wreckSpin.z*elapsedTime, Vector3D(0, 0, 1)));
    m_orientationInverted = glm::transpose(m_orientation);
    m_position += m_velocity*elapsedTime;
    const float r = hullRadius();
    const float ground = m_scenario->surface().heightAt(m_position.x, m_position.y);
    if (m_position.z - r <= ground)
    {
        if (m_wreckRuns && m_wreckStep <= 5)
        {
            m_wreckStep = 5;
            nextWreckStep();
            return;
        }
        m_position.z = ground + r;
        m_velocity.z = std::max(m_velocity.z, 0.0f);
    }
    updateBox();
    while (m_wreckStep <= 5 && m_scenario->time() >= m_wreckNext)
        nextWreckStep();
}


} // namespace fight
