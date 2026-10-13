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

#include "tank.h"
#include "fight/scenario.h"
#include "fight/module.h"
#include "fight/orders.h"
#include "fight/objects/turret.h"
#include "fight/weapons/unitgun.h"
#include "txt/desfile.h"

#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cmath>


namespace fight {


Tank::Tank(Scenario *scenario, txt::DesFile &file, const ObjectInfo &info, float yaw) :
    SimpleBoat(scenario, file, info, yaw),
    m_heading(yaw),
    m_speed(0),
    m_gun(nullptr)
{
    // the turret, in model coordinates like the base
    file.setSection("cluster");
    if (file.contains("gun"))
    {
        m_gun = scenario->moduleManager().get(file.value("gun").toString());
        m_gunOffset = Vector3D(file.value("GunOffsetX").toFloat(), file.value("GunOffsetY").toFloat(), file.value("GunOffsetZ").toFloat());
        m_localBox.add(m_gun->box().translate(m_gunOffset).scale(m_scale));
    }
    file.setSection("gunturret");
    if (file.contains("name"))
    {
        txt::DesFile turretFile(QString("vfx:sobjects/%1.des").arg(file.value("name").toString()));
        m_turret.reset(new Turret(scenario, turretFile));
        m_turret->setPosition(Vector3D(file.value("RelativePositionX").toFloat(), file.value("RelativePositionY").toFloat(), file.value("RelativePositionZ").toFloat()));
        m_localBox.add(m_turret->box().scale(m_scale));
    }

    m_groundOffset = -m_localBox.minPoint().z;
}


Tank::~Tank()
{
}


void Tank::arm(int model, txt::DesFile &file)
{
    const UnitGun::Mount mount = UnitGun::readMount(file);
    if (mount.model < 0)
        return; // crawlers have no gun
    if (!UnitGun::isGun(model))
        model = mount.model;
    if (!UnitGun::isGun(model))
        return;
    std::vector<Vector3D> muzzles;
    for (const Vector3D &muzzle : mount.muzzles)
        muzzles.push_back(muzzle*m_scale);
    m_unitGun.reset(new UnitGun(m_scenario, this, model, muzzles, &file));
    if (mount.ammunition >= 0)
        m_unitGun->setAmmunition(mount.ammunition);
}


float Tank::ground(float x, float y) const
{
    return m_scenario->surface().heightAt(x, y);
}


void Tank::setPosition(const Vector3D &pos)
{
    m_position = Vector3D(pos.x, pos.y, ground(pos.x, pos.y) + m_groundOffset);
    alignToGround();
    updateBox();
}


// up follows the ground under the vehicle, forward follows the heading
void Tank::alignToGround()
{
    const float d = std::max(1.5f, 0.35f*m_localBox.dim().y);
    const float dx = (ground(m_position.x + d, m_position.y) - ground(m_position.x - d, m_position.y))/(2*d);
    const float dy = (ground(m_position.x, m_position.y + d) - ground(m_position.x, m_position.y - d))/(2*d);
    Vector3D up = glm::normalize(Vector3D(-dx, -dy, 1));
    // tilt at most MaxClimbingAngle
    const float tilt = std::acos(glm::clamp(up.z, -1.0f, 1.0f));
    if (tilt > m_maxClimbingAngle)
    {
        const Vector2D side = glm::normalize(Vector2D(up));
        up = Vector3D(side*std::sin(m_maxClimbingAngle), std::cos(m_maxClimbingAngle));
    }
    const Vector3D heading(std::sin(m_heading), std::cos(m_heading), 0);
    const Vector3D forward = glm::normalize(heading - glm::dot(heading, up)*up);

    m_orientationInverted = glm::mat3(glm::cross(forward, up), forward, up);
    m_orientation = glm::transpose(m_orientationInverted);
}


bool Tank::update(float elapsedTime)
{
    if (m_orders)
        m_orders->update();

    // steerTo() and hold() set the turn rate and the target speed
    m_heading += m_turnVelocity.z*m_maxAngleVelocity.z*elapsedTime;
    if (m_velocityTarget > m_speed)
        m_speed = std::min(m_velocityTarget, m_speed + m_maxOrientationAcceleration*elapsedTime);
    else
        m_speed = std::max(m_velocityTarget, m_speed + m_maxOrientationRetardation*elapsedTime);

    const Vector3D heading(std::sin(m_heading), std::cos(m_heading), 0);
    const Vector3D previous = m_position;
    setPosition(m_position + heading*m_speed*elapsedTime);
    m_velocity = elapsedTime > 0 ? (m_position - previous)/elapsedTime : Vector3D(0);

    // TODO: aim the turret (the gun fires along the heading for now)
    if (m_unitGun)
        m_unitGun->update(elapsedTime, m_trigger, m_position, dir(), up(), right());

    return false;
}


void Tank::draw()
{
    glPushMatrix();
    glTranslatef(m_position.x, m_position.y, m_position.z);
    glMultMatrixf(glm::value_ptr(Matrix(m_orientationInverted)));
    glScalef(m_scale, m_scale, m_scale);
    m_base->draw();
    if (m_gun)
    {
        glPushMatrix();
        glTranslatef(m_gunOffset.x, m_gunOffset.y, m_gunOffset.z);
        m_gun->draw();
        glPopMatrix();
    }
    if (m_turret)
    {
        glPushMatrix();
        m_turret->draw();
        glPopMatrix();
    }
    glPopMatrix();
}


bool Tank::intersect(const Vector3D &start, const Vector3D &dir, float radius, float &distance, Vector3D &normal)
{
    // in model coordinates; the nearest of base and turret
    const Vector3D localStart = m_orientation*(start - m_position)/m_scale;
    const Vector3D localDir = m_orientation*dir;
    const float localRadius = radius/m_scale;

    bool collision = m_base->intersect(localStart, localDir, localRadius, distance, normal);
    float d;
    Vector3D n;
    if (m_gun && m_gun->intersect(localStart - m_gunOffset, localDir, localRadius, d, n) && (!collision || d < distance))
    {
        distance = d;
        normal = n;
        collision = true;
    }
    if (m_turret && m_turret->intersect(localStart, localDir, localRadius, d, n) && (!collision || d < distance))
    {
        distance = d;
        normal = n;
        collision = true;
    }

    if (collision)
    {
        distance *= m_scale;
        normal = m_orientationInverted*normal;
    }
    return collision;
}


} // namespace fight
