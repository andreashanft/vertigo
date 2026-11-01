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

// The GunTurret (TANTUR2) turns at its rate, 1.5 rad/s if the data has none, within 90 degrees of
// the front, its gun from 5 degrees down to straight up (docs/ROADMAP.md, appendix J)
const float GunTurnRate = 1.5f;
const float MinPitch = -glm::radians(5.0f);
const float MaxPitch = glm::half_pi<float>();
// a fixed Gun module (panztur*) only tilts, to the target's elevation over the hull's plane, and
// only at targets ahead, from 5 degrees down to straight up; otherwise it goes back to level
// (from 15 degrees down); it turns as a damped spring by the hull's MaxAngleAccelerationB,
// AngleFrictionB and MaxAngleVelocityB, leaving a degree to the damping (docs/ROADMAP.md,
// appendix J)
const float GunMinPitch = -glm::radians(5.0f);
const float GunRestMinPitch = -glm::radians(15.0f);
const float GunMaxPitch = glm::half_pi<float>();
const float GunDeadZone = glm::radians(1.0f);
// while it drives, a puff of dust behind one of its tracks every 0.2 to 0.3 s (a crawler 0.16 to
// 0.3 s), the larger the faster it goes (docs/ROADMAP.md, appendix H)
const float DustInterval = 0.2f;
const float CrawlerDustInterval = 0.16f;
const float DustMaxInterval = 0.3f;
// a paralysed one brakes for 5 s, then it stays as it is
const float ParalysedBrake = 5.0f;

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
        m_turretRate = m_maxAngleVelocity.x;
    }
    file.setSection("gunturret");
    if (file.contains("name"))
    {
        m_turretName = file.value("name").toString();
        txt::DesFile turretFile(String("vfx:sobjects/%1.des").arg(m_turretName));
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


// a turret tank (TANK2) fires its turret's gun ([StaticGun] and [VolleyGun] of TANTUR2), not its
// own file's; the others their [StaticGun] on the gun module
void Tank::arm(int model, txt::DesFile &file)
{
    txt::DesFile turretFile(String("vfx:sobjects/%1.des").arg(m_turretName.isEmpty() ? String("gvehicle") : m_turretName));
    txt::DesFile &gunFile = m_turret ? turretFile : file;
    const UnitGun::Mount mount = UnitGun::readMount(gunFile);
    if (mount.model < 0)
        return; // crawlers have no gun
    if (!UnitGun::isGun(model))
        model = mount.model;
    if (!UnitGun::isGun(model))
        return;
    std::vector<Vector3D> muzzles;
    for (const Vector3D &muzzle : mount.muzzles)
        muzzles.push_back(muzzle*m_scale);
    m_unitGun.reset(new UnitGun(m_scenario, this, model, muzzles, &gunFile));
    m_unitGun->setCountFirst(!m_turret);
    m_unitGun->setRestartOnRelease(true);
    if (mount.ammunition >= 0)
        m_unitGun->setAmmunition(mount.ammunition);
}


float Tank::ground(float x, float y) const
{
    return m_scenario->surface().heightAt(x, y);
}


// the vehicle stands on the ground under its base's four corners (docs/ROADMAP.md, appendix J):
// at their mean height, tilted by the difference front to back and right to left, without limit
void Tank::setPosition(const Vector3D &pos)
{
    m_position = Vector3D(pos.x, pos.y, 0);
    auto vertex = [this](const Vector2D &at) { return ground(at.x, at.y); };
    const BoundingBox base = m_base->box().scale(m_scale);
    const Vector2D heading(std::sin(m_heading), std::cos(m_heading)), side(std::cos(m_heading), -std::sin(m_heading));
    auto corner = [&](float x, float y) { return vertex(Vector2D(pos) + side*x + heading*y); };
    const float minX = base.minPoint().x, maxX = base.maxPoint().x, minY = base.minPoint().y, maxY = base.maxPoint().y;
    const float backLeft = corner(minX, minY), backRight = corner(maxX, minY), frontLeft = corner(minX, maxY), frontRight = corner(maxX, maxY);
    m_corners = Vector4D(backLeft, backRight, frontLeft, frontRight);
    m_position.z = 0.25f*(backLeft + backRight + frontLeft + frontRight) + m_groundOffset;
    alignToGround();
    updateBox();
}


void Tank::alignToGround()
{
    const BoundingBox base = m_base->box().scale(m_scale);
    const float length = std::max(0.1f, base.dim().y), width = std::max(0.1f, base.dim().x);
    const float pitch = std::atan((0.5f*(m_corners.z + m_corners.w) - 0.5f*(m_corners.x + m_corners.y))/length);
    const float roll = std::atan((0.5f*(m_corners.y + m_corners.w) - 0.5f*(m_corners.x + m_corners.z))/width);
    const Vector3D heading(std::sin(m_heading), std::cos(m_heading), 0), side(std::cos(m_heading), -std::sin(m_heading), 0);
    const Vector3D forward = glm::normalize(heading*std::cos(pitch) + Vector3D(0, 0, std::sin(pitch)));
    Vector3D right = side*std::cos(roll) + Vector3D(0, 0, std::sin(roll));
    const Vector3D up = glm::normalize(glm::cross(right, forward));
    right = glm::cross(forward, up);
    m_orientationInverted = glm::mat3(right, forward, up);
    m_orientation = glm::transpose(m_orientationInverted);
}


bool Tank::startTumbling()
{
    m_tumbling = true;
    m_trigger = false;
    m_paralysedEnd = m_scenario->time() + ParalysedBrake;
    m_scenario->disengagePlayer(this);
    return true;
}


bool Tank::update(float elapsedTime)
{
    // destroyed or paralysed, it brakes to a stop where it is and no longer turns
    const bool wrecking = isGroundWrecking();
    if (wrecking || m_tumbling)
    {
        if (wrecking || m_scenario->time() < m_paralysedEnd)
        {
            hold();
            m_turnVelocity = Vector3D(0);
            drive(elapsedTime);
        }
        else
            m_velocity = Vector3D(0);
        if (wrecking)
            updateGroundWreck(elapsedTime);
        return false;
    }

    if (m_orders)
        m_orders->update();
    drive(elapsedTime);
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


void Tank::drive(float elapsedTime)
{
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
}


void Tank::stirUpDust(float elapsedTime)
{
    const float speed = std::abs(m_speed);
    if (speed < 0.1f || m_maxVelocity <= 0)
        return;
    m_dustTime -= elapsedTime;
    if (m_dustTime > 0)
        return;
    const bool crawler = m_groundWreck == GroundWreck::Crawler;
    const float interval = crawler ? CrawlerDustInterval : DustInterval;
    m_dustTime = interval + (DustMaxInterval - interval)*util::random()/float(RAND_MAX);

    // at the rear, 0.6 to 1.1 of the half width to the left or the right, on the ground, where it
    // stays; as big as the vehicle is high times its share of its top speed and 0.8 to 1.0
    const Vector3D dim = m_localBox.dim();
    const float side = util::random()%2 ? 1.0f : -1.0f;
    const float out = (0.6f + 0.5f*util::random()/float(RAND_MAX))*0.5f*dim.x;
    const Vector3D local(side*out + 0.5f*(m_localBox.minPoint().x + m_localBox.maxPoint().x), m_localBox.minPoint().y, m_localBox.minPoint().z);
    Vector3D position = m_position + m_orientationInverted*local;
    position.z = ground(position.x, position.y);
    const float size = std::min(speed/m_maxVelocity, 1.0f)*m_localBox.maxPoint().z*(0.8f + 0.2f*util::random()/float(RAND_MAX));
    m_scenario->effectManager().addDust(position, size, crawler);
}


void Tank::aimAt(const Vector3D &point)
{
    if (!m_gun && !m_turret)
        return;
    const Vector3D way = m_orientation*(point - (m_position + m_orientationInverted*(m_pivot*m_scale)));
    if (m_turret)
    {
        // behind the hull's side, back to the front
        m_targetYaw = std::atan2(way.x, way.y);
        m_targetPitch = glm::clamp(std::atan2(way.z, glm::length(Vector2D(way))), MinPitch, MaxPitch);
        if (glm::abs(m_targetYaw) > glm::half_pi<float>())
            m_targetYaw = m_targetPitch = 0;
    }
    else
    {
        // the gun points along the hull; behind it, it levels out
        if (way.y <= 0)
            return;
        m_targetPitch = std::atan2(way.z, glm::length(Vector2D(way)));
    }
    m_aimHold = m_turret ? 1.0f : 0.0f;
    m_aiming = true;
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
    if (m_gun)
    {
        // aimed this frame, or back to level
        const bool aiming = m_aiming;
        m_aiming = false;
        if (!aiming)
            m_targetPitch = 0;
        const float dt = std::max(elapsedTime, 1e-4f);
        const float error = m_targetPitch - m_turretPitch;
        float acceleration = glm::abs(error) > GunDeadZone ? m_maxAngleAcceleration.x*error/glm::pi<float>() : 0.0f;
        acceleration -= m_gunRate*std::min(-m_angleFriction.x, 1/dt);
        m_turretPitch += m_gunRate*dt + 0.5f*acceleration*dt*dt;
        m_gunRate = glm::clamp(m_gunRate + acceleration*dt, -m_maxAngleVelocity.x, m_maxAngleVelocity.x);
        const float low = aiming ? GunMinPitch : GunRestMinPitch;
        if (m_turretPitch < low || m_turretPitch > GunMaxPitch)
        {
            m_turretPitch = glm::clamp(m_turretPitch, low, GunMaxPitch);
            m_gunRate = 0;
        }
        return;
    }
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
    // the gun tilts around its origin; the turret turns around its pivot and tilts its arm
    if (m_gun)
    {
        gfx::Pipeline::pushMatrix();
        gfx::Pipeline::translate(m_gunOffset);
        gfx::Pipeline::rotate(glm::degrees(m_turretPitch), Vector3D(1, 0, 0));
        m_gun->draw();
        gfx::Pipeline::popMatrix();
    }
    if (m_turret)
    {
        gfx::Pipeline::pushMatrix();
        gfx::Pipeline::translate(m_pivot);
        gfx::Pipeline::rotate(-glm::degrees(m_turretYaw), Vector3D(0, 0, 1));
        gfx::Pipeline::translate(-m_pivot);
        m_turret->setArmPitch(m_turretPitch);
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
