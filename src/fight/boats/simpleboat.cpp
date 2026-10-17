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
#include "fight/objects/mine.h"
#include "fight/scenario.h"
#include "fight/module.h"
#include "fight/orders.h"
#include "fight/weapons/unitgun.h"
#include "txt/desfile.h"
#include "util/random.h"
#include "gfx/pipeline.h"
#include <glm/gtx/transform.hpp>
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
    const Vector3D way = point - m_position;
    const Vector3D local = m_orientation*way;

    // positive turns to the right (yaw) and pitch the nose down
    const float yawError = std::atan2(local.x, local.y);
    const float pitch = std::asin(glm::clamp(dir().z, -1.0f, 1.0f));
    const float targetPitch = glm::clamp(std::atan2(way.z, 40.0f), -m_minClimbingAngle, m_maxClimbingAngle);
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


Vector3D SimpleBoat::slideAlongObjects(const Vector3D &position, const Vector3D &delta)
{
    if (!collidesWithObjects() || glm::length2(delta) < 1e-8f)
        return delta;
    const Object *docking = m_orders ? m_orders->dockingTarget() : nullptr;
    auto obstacle = [docking](const Object *object) {
        return object != docking && !dynamic_cast<const Mine *>(object) && !dynamic_cast<const Effect *>(object);
    };
    Vector3D contact, normal;
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
