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
#include "fight/effects/effectmanager.h"
#include "fight/module.h"
#include "fight/orders.h"
#include "fight/objects/turret.h"
#include "fight/weapons/unitgun.h"
#include "txt/desfile.h"
#include "util/random.h"
#include "gfx/pipeline.h"

#include <glm/gtc/constants.hpp>
#include <glm/trigonometric.hpp>
#include <algorithm>
#include <cmath>


namespace fight {


namespace {

// The Gun modules (panztur*) have no turn rates in the data; like the GunTurret's (TANTUR2) and
// the turret models', theirs is 1.5 rad/s, faster than any hull turns (at most 1.1).
const float GunTurnRate = 1.5f;
const float MinPitch = -0.17f;
const float MaxPitch = 1.0f;
// a puff of dust behind each track this often at full speed (10 m/s), less often when slower
const float DustInterval = 0.15f;

}


Tank::Tank(Scenario *scenario, txt::DesFile &file, const ObjectInfo &info, float yaw) :
    SimpleBoat(scenario, file, info, yaw),
    m_heading(yaw),
    m_speed(0),
    m_gun(nullptr),
    m_turretRate(GunTurnRate),
    m_turretYaw(0),
    m_turretPitch(0),
    m_targetYaw(0),
    m_targetPitch(0),
    m_aimHold(0),
    m_dustTime(0)
{
    // the turret, in model coordinates like the base
    file.setSection("cluster");
    if (file.contains("gun"))
    {
        m_gun = scenario->moduleManager().get(file.value("gun").toString());
        m_gunOffset = Vector3D(file.value("GunOffsetX").toFloat(), file.value("GunOffsetY").toFloat(), file.value("GunOffsetZ").toFloat());
        m_pivot = m_gunOffset;
        m_localBox.add(m_gun->box().translate(m_gunOffset).scale(m_scale));
    }
    file.setSection("gunturret");
    if (file.contains("name"))
    {
        txt::DesFile turretFile(String("vfx:sobjects/%1.des").arg(file.value("name").toString()));
        m_turret.reset(new Turret(scenario, turretFile));
        m_turret->setPosition(Vector3D(file.value("RelativePositionX").toFloat(), file.value("RelativePositionY").toFloat(), file.value("RelativePositionZ").toFloat()));
        m_localBox.add(m_turret->box().scale(m_scale));
        m_pivot = m_turret->position();
        turretFile.setSection("kinematics");
        m_turretRate = turretFile.value("MaxAngleVelocityA", GunTurnRate).toFloat();
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
    Vector3D delta = slideAlongObjects(m_position, heading*m_speed*elapsedTime, std::abs(m_speed));
    delta.z = 0;
    setPosition(m_position + delta);
    m_velocity = elapsedTime > 0 ? (m_position - previous)/elapsedTime : Vector3D(0);

    updateTurret(elapsedTime);
    stirUpDust(elapsedTime);
    if (m_unitGun)
    {
        // the muzzles ([StaticGun] Relative*) sit on the gun module, as a tower's on its turret:
        // they turn with it around its pivot, which the turret aims from
        const glm::mat3 frame = m_orientationInverted*turretRotation();
        const Vector3D origin = m_position + m_orientationInverted*(m_pivot*m_scale);
        m_unitGun->update(elapsedTime, m_trigger, origin, frame[1], frame[2], frame[0]);
    }

    return false;
}


void Tank::stirUpDust(float elapsedTime)
{
    const float speed = std::abs(m_speed);
    if (speed < 1.0f)
        return;
    m_dustTime -= elapsedTime*std::min(speed/10.0f, 1.0f);
    if (m_dustTime > 0)
        return;
    m_dustTime += DustInterval*(0.7f + 0.6f*util::random()/float(RAND_MAX));

    // at the rear corners, on the ground; small, medium or large by the vehicle's size
    const Vector3D dim = m_localBox.dim();
    const int size = std::max(dim.x, dim.y) < 6.0f ? 0 : std::max(dim.x, dim.y) < 12.0f ? 1 : 2;
    for (const float side : { -1.0f, 1.0f })
    {
        const Vector3D local(side*0.4f*dim.x + 0.5f*(m_localBox.minPoint().x + m_localBox.maxPoint().x), m_localBox.minPoint().y, m_localBox.minPoint().z);
        Vector3D position = m_position + m_orientationInverted*local;
        position.z = ground(position.x, position.y);
        const Vector3D drift = -dir()*0.3f*speed + right()*side*0.5f;
        m_scenario->effectManager().addDust(position, size, drift);
    }
}


void Tank::aimAt(const Vector3D &point)
{
    if (!m_gun && !m_turret)
        return;
    const Vector3D way = m_orientation*(point - (m_position + m_orientationInverted*(m_pivot*m_scale)));
    m_targetYaw = std::atan2(way.x, way.y);
    m_targetPitch = glm::clamp(std::atan2(way.z, glm::length(Vector2D(way))), MinPitch, MaxPitch);
    m_aimHold = 1.0f;
}


Vector3D Tank::gunDirection() const
{
    return m_orientationInverted*turretRotation()[1];
}


glm::mat3 Tank::turretRotation() const
{
    // yaw to the right around z, then pitch up around the turned x axis
    const float cy = std::cos(m_turretYaw), sy = std::sin(m_turretYaw);
    const float cp = std::cos(m_turretPitch), sp = std::sin(m_turretPitch);
    const Vector3D right(cy, -sy, 0);
    const Vector3D forward(sy*cp, cy*cp, sp);
    return glm::mat3(right, forward, glm::cross(right, forward));
}


void Tank::updateTurret(float elapsedTime)
{
    m_aimHold -= elapsedTime;
    if (m_aimHold <= 0)
        m_targetYaw = m_targetPitch = 0;
    const float step = m_turretRate*elapsedTime;
    m_turretYaw += glm::clamp(std::remainder(m_targetYaw - m_turretYaw, glm::two_pi<float>()), -step, step);
    m_turretYaw = std::remainder(m_turretYaw, glm::two_pi<float>());
    m_turretPitch += glm::clamp(m_targetPitch - m_turretPitch, -step, step);
}


void Tank::draw()
{
    gfx::Pipeline::pushMatrix();
    gfx::Pipeline::translate(m_position);
    gfx::Pipeline::multiply(Matrix(m_orientationInverted));
    gfx::Pipeline::scale(m_scale);
    m_base->draw();
    // the turret turns around its pivot; only its yaw shows, the barrel's pitch isn't modelled
    if (m_gun)
    {
        gfx::Pipeline::pushMatrix();
        gfx::Pipeline::translate(m_gunOffset);
        gfx::Pipeline::rotate(-glm::degrees(m_turretYaw), Vector3D(0, 0, 1));
        m_gun->draw();
        gfx::Pipeline::popMatrix();
    }
    if (m_turret)
    {
        gfx::Pipeline::pushMatrix();
        gfx::Pipeline::translate(m_pivot);
        gfx::Pipeline::rotate(-glm::degrees(m_turretYaw), Vector3D(0, 0, 1));
        gfx::Pipeline::translate(-m_pivot);
        m_turret->draw();
        gfx::Pipeline::popMatrix();
    }
    gfx::Pipeline::popMatrix();
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
