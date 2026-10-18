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
#include "fight/scenario.h"
#include "fight/module.h"
#include "fight/orders.h"
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
    m_angleFriction = Vector3D(-file.value("AngleFrictionB").toFloat(), -file.value("AngleFrictionC").toFloat(), -file.value("AngleFrictionA").toFloat());
    file.setSection("fluid");
    m_currentFactor = file.value("Current", 1).toFloat();
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
    steer(point, speed, 40.0f);
}


// the nose to the point's own direction, the pitch to its true elevation (docs/ROADMAP.md, appendix
// J): the guns fire along the nose
void SimpleBoat::pointAt(const Vector3D &point, float speed)
{
    steer(point, speed, glm::length(Vector2D(point - m_position)));
}


// the pitch makes for the point's height over this horizontal way
void SimpleBoat::steer(const Vector3D &point, float speed, float reach)
{
    const Vector3D way = point - m_position;
    const Vector3D local = m_orientation*way;

    // positive turns to the right (yaw) and pitch the nose down
    const float yawError = std::atan2(local.x, local.y);
    const float pitch = std::asin(glm::clamp(dir().z, -1.0f, 1.0f));
    const float targetPitch = glm::clamp(std::atan2(way.z, reach), -m_minClimbingAngle, m_maxClimbingAngle);
    m_turnVelocity = Vector3D(glm::clamp(2.0f*(pitch - targetPitch), -1.0f, 1.0f), 0, glm::clamp(1.5f*yawError, -1.0f, 1.0f));

    // slow down while turning towards a point to the side or behind
    m_velocityTarget = speed*std::max(0.3f, std::cos(std::min(glm::abs(yawError), glm::half_pi<float>())));
}


void SimpleBoat::hold()
{
    const float pitch = std::asin(glm::clamp(dir().z, -1.0f, 1.0f));
    m_turnVelocity = Vector3D(glm::clamp(2.0f*pitch, -1.0f, 1.0f), 0, 0);
    m_velocityTarget = 0;
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


// boats fire from the nose; their object files have no gun data
void SimpleBoat::arm(int model, txt::DesFile &file)
{
    if (UnitGun::isGun(model))
        m_unitGun.reset(new UnitGun(m_scenario, this, model, { Vector3D(0, m_localBox.maxPoint().y + 0.5f, 0) }));
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

    return false;
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
    auto obstacle = [docking](const Object *object) {
        return object != docking && !dynamic_cast<const Mine *>(object) && !dynamic_cast<const Effect *>(object);
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
    Vector3D acceleration = velocity*m_orientationFriction + m_orientation*(m_scenario->current(m_position)*m_currentFactor);

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


// the tumble after an Explode order or an energy defence's collapse (docs/ROADMAP.md, appendix J):
// the boat spins at random, loses its way and sinks, until it touches the ground or half a
// minute is over
bool SimpleBoat::startTumbling()
{
    if (this == m_scenario->player() || isGroundVehicle())
        return false;
    auto unit = [](float scale) { return scale*(2.0f*util::random()/float(RAND_MAX) - 1.0f); };
    m_spin = Vector3D(unit(1.5f), unit(1.5f), unit(1.5f));
    if (glm::length(m_spin) < 0.5f)
        m_spin = Vector3D(0, 0, 1.0f);
    m_tumbleEnd = m_scenario->time() + 30.0f;
    m_tumbling = true;
    m_trigger = false;
    m_scenario->disengagePlayer(this);
    return true;
}


void SimpleBoat::updateTumble(float elapsedTime)
{
    m_orientation *= glm::mat3(glm::rotate(m_spin.x*elapsedTime, Vector3D(1, 0, 0))) * glm::mat3(glm::rotate(m_spin.y*elapsedTime, Vector3D(0, 1, 0)))
                   * glm::mat3(glm::rotate(m_spin.z*elapsedTime, Vector3D(0, 0, 1)));
    m_orientationInverted = glm::transpose(m_orientation);
    // the way runs out and the boat sinks
    m_velocity *= std::exp(-0.5f*elapsedTime);
    m_velocity.z = std::max(m_velocity.z - 2.0f*elapsedTime, -4.0f);
    move(m_velocity*elapsedTime);
    const float ground = m_scenario->surface().heightAt(m_position.x, m_position.y);
    if (m_position.z - m_collisionRadius <= ground + 0.5f || m_scenario->time() >= m_tumbleEnd)
        vanish();
}


void SimpleBoat::updateOrientation(float elapsedTime)
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
// 70 % of its speed; the others stop, and the player's boat drifts on.
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
    else if (!player)
        m_velocity = Vector3D(0);
    m_tumbling = true;
    m_trigger = false;
    if (!player)
        m_scenario->disengagePlayer(this);
    nextWreckStep();
    return true;
}


void SimpleBoat::wreckExplosion(int type, float spread)
{
    const float r = hullRadius();
    const Vector3D at = m_position + Vector3D((uniform() - 0.5f)*r, (uniform() - 0.5f)*r*spread, uniform()*r*spread);
    m_scenario->effectManager().addEffect(Effects(int(Effects::Explosion_0) + type), at);
    sfx::SampleMap::playAt(type < 3 ? sfx::Sample::Explosion_Big : sfx::Sample::Explosion_Small, at.x, at.y, at.z);
    m_scenario->explosionAt(at, type < 3 ? 0.6f : 0.3f);
}


void SimpleBoat::wreckDebris(int index, float minElevation, float maxElevation)
{
    const float azimuth = between(-glm::pi<float>(), glm::pi<float>());
    const float elevation = between(minElevation, maxElevation);
    const Vector3D direction(std::cos(elevation)*std::sin(azimuth), std::cos(elevation)*std::cos(azimuth), std::sin(elevation));
    // 0.1 to 0.6 hull radii out, at the piece's own speed
    m_scenario->effectManager().addDebris(Effects(int(Effects::Debris_0) + index), m_position + direction*hullRadius()*between(0.1f, 0.6f), direction);
}


void SimpleBoat::nextWreckStep()
{
    const bool player = this == m_scenario->player();
    const float fifth = glm::pi<float>()/5;
    float delay = 0;
    switch (m_wreckStep)
    {
    case 0:
        wreckExplosion(0, 0.6f);
        wreckExplosion(3, 0.6f);
        wreckExplosion(3, 0.6f);
        if (player)
            for (int index : { 17, 17, 17, 1, 3, 3 })
                wreckDebris(index, -fifth, fifth);
        delay = 0.4f;
        break;
    case 1:
        wreckExplosion(4, 0.5f);
        delay = 0.5f;
        break;
    case 2:
        wreckExplosion(1, 0.4f);
        wreckExplosion(4, 0.4f);
        wreckExplosion(4, 0.7f);
        for (int index : { 2, 2, 1, 3, 3 })
            wreckDebris(index, fifth, 2*fifth);
        delay = 0.4f;
        break;
    case 3:
        wreckExplosion(5, 0.3f);
        delay = 0.3f;
        break;
    case 4:
        wreckExplosion(5, 0.6f);
        wreckExplosion(2, 0.6f);
        if (player)
            for (int index : { 17, 17, 17, 0, 1, 2 })
                wreckDebris(index, fifth, 2*fifth);
        // a boat that keeps its course goes up when it hits the ground, after five seconds at most
        delay = m_wreckRuns ? 5.0f : 0.4f;
        break;
    case 5:
    {
        wreckExplosion(2, 0.1f);
        wreckExplosion(2, 0.4f);
        for (int index : { 2, 2, 2, 1, 3, 3 })
            wreckDebris(index, -fifth, fifth);
        // the wreck: the player's boat leaves one body, another boat none to three
        const int roll = std::min(3, int(uniform()*4));
        if (player)
        {
            wreckDebris(roll == 3 ? 14 : roll == 2 ? 15 : 16, 0, glm::half_pi<float>());
            // and blows up at both ends of its hull
            for (float end : { m_localBox.maxPoint().y, m_localBox.minPoint().y })
            {
                const Vector3D at = m_position + m_orientationInverted*Vector3D(0, 0.5f*end, 0);
                m_scenario->effectManager().addEffect(Effects::Explosion_2, at);
                sfx::SampleMap::playAt(sfx::Sample::Explosion_Big, at.x, at.y, at.z);
            }
        }
        else
        {
            for (int body = 17 - roll; body <= 16; body++)
                wreckDebris(body, 0, glm::half_pi<float>());
        }
        m_wreckGone = true;
        m_wreckStep = 6;
        if (!player)
            vanish();
        return;
    }
    default:
        return;
    }
    m_wreckAcceleration = randomVector(30.0f)/32.0f;
    m_wreckSpinAcceleration = randomVector(0.2f);
    m_wreckNext = m_scenario->time() + delay;
    m_wreckStep++;
}


// between the bursts the hull drifts by its acceleration and spins up, a boat keeping its course
// spinning wildly (its spin drawn anew every frame)
void SimpleBoat::updateWreck(float elapsedTime)
{
    if (m_wreckGone)
        return;
    if (m_wreckRuns)
        m_wreckSpinAcceleration = randomVector(3.0f);
    else
        m_velocity += m_wreckAcceleration*elapsedTime;
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
