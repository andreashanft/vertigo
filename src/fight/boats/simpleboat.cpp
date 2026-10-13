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
#include "fight/scenario.h"
#include "fight/module.h"
#include "fight/orders.h"
#include "txt/desfile.h"
#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>
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

    // a sphere that fits into the hull, so the boat can get close to the ground
    const Vector3D dim = m_localBox.dim();
    m_collisionRadius = glm::clamp(0.5f*std::min(dim.x, std::min(dim.y, dim.z)), 1.5f, 8.0f);

    // boats move, so collision results must not be cached
    m_static = false;
    updateBox();
}


SimpleBoat::~SimpleBoat()
{
}


void SimpleBoat::setOrders(Orders *orders)
{
    m_orders.reset(orders);
}


void SimpleBoat::steerTo(const Vector3D &point, float speed)
{
    const Vector3D way = point - m_position;
    const Vector3D local = m_orientation*way;

    // positive turns to the right (yaw) and pitch the nose down
    const float yawError = std::atan2(local.x, local.y);
    const float pitch = std::asin(glm::clamp(dir().z, -1.0f, 1.0f));
    const float targetPitch = glm::clamp(std::atan2(way.z, 40.0f), -0.6f, 0.6f);
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


void SimpleBoat::updateBox()
{
    m_box = m_localBox.transform(glm::translate(m_position) * Matrix(m_orientationInverted));
}


bool SimpleBoat::update(float elapsedTime)
{
    if (m_orders)
        m_orders->update();
    updateOrientation(elapsedTime);
    updateVelocity(elapsedTime);
    move(m_velocity*elapsedTime);

    return false;
}


// moves by delta, sliding along the terrain
void SimpleBoat::move(Vector3D delta)
{
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
    glPushMatrix();
    glTranslatef(m_position.x, m_position.y, m_position.z);
    glMultMatrixf(glm::value_ptr(Matrix(m_orientationInverted)));
    glScalef(m_scale, m_scale, m_scale);
    m_base->draw();
    glPopMatrix();
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
    Vector3D acceleration = velocity*m_orientationFriction;

    float orientationAcceleration = 0;
    if (m_velocityTarget > velocity.y + 0.1f)
        orientationAcceleration = m_maxOrientationAcceleration*(m_fullThrottle ? 2 : 1);
    if (m_velocityTarget < velocity.y - 0.1f)
        orientationAcceleration = m_maxOrientationRetardation*(m_fullThrottle ? 2 : 1);
    acceleration.y += orientationAcceleration;

    velocity += acceleration*elapsedTime;
    m_velocity = m_orientationInverted * velocity;
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
