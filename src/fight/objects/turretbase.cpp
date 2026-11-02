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

#include "turret.h"
#include "turretbase.h"
#include "fight/scenario.h"
#include "fight/orders.h"
#include "fight/weapons/unitgun.h"
#include "txt/desfile.h"
#include "gfx/pipeline.h"
#include <glm/gtc/constants.hpp>
#include <glm/trigonometric.hpp>
#include <glm/gtx/transform.hpp>
#include <algorithm>
#include <cmath>


namespace fight {


namespace {

// tracking, the guns go from 5 degrees down to straight up, turned to an angle from 15 degrees down
const float MinTrackPitch = glm::radians(-5.0f);
const float MinTurnPitch = glm::radians(-15.0f);
const float MaxPitch = glm::half_pi<float>();
const float SoftDone = glm::radians(1.0f);
const float HardDone = glm::radians(0.5f);

}


TurretBase::TurretBase(Scenario *scenario, txt::DesFile &file, const ObjectInfo &info) :
    SimpleObject(scenario, file, info),
    m_yaw(0),
    m_pitch(0),
    m_targetYaw(0),
    m_targetPitch(0)
{
    m_static = false;

    for (const char *section : { "gunturret", "torpedoturret" })
    {
        file.setSection(section);
        if (!file.contains("name"))
            continue;
        m_torpedoTurret = String(section) == "torpedoturret";
        m_turretName = file.value("name").toString();
        txt::DesFile turretFile(String("vfx:sobjects/%1.des").arg(m_turretName));
        m_body.reset(new Turret(scenario, turretFile)); // TODO: TurretTorpedo
        m_turretPosition = Vector3D(file.value("RelativePositionX").toFloat(), file.value("RelativePositionY").toFloat(), file.value("RelativePositionZ").toFloat());
        m_body->setPosition(m_turretPosition);
        m_box.add(m_body->box().scale(m_scale));

        turretFile.setSection("kinematics");
        m_maxYawRate = turretFile.value("MaxAngleVelocityA", 1).toFloat();
        m_maxPitchRate = turretFile.value("MaxAngleVelocityB", 1).toFloat();
        m_yawAcceleration = turretFile.value("MaxAngleAccelerationA").toFloat();
        m_pitchAcceleration = turretFile.value("MaxAngleAccelerationB").toFloat();
        m_yawFriction = turretFile.value("AngleFrictionA").toFloat();
        m_pitchFriction = turretFile.value("AngleFrictionB").toFloat();
        m_softYaw = turretFile.value("SoftBodyAdjust", "yes").toString().trimmed().toLower() != "no";
        m_softPitch = turretFile.value("SoftArmRAdjust", "yes").toString().trimmed().toLower() != "no";
        turretFile.setSection("cluster");
        m_armOffset[0] = Vector3D(turretFile.value("ArmROffsetX").toFloat(), turretFile.value("ArmROffsetY").toFloat(), turretFile.value("ArmROffsetZ").toFloat());
        m_armOffset[1] = Vector3D(turretFile.value("ArmLOffsetX").toFloat(), turretFile.value("ArmLOffsetY").toFloat(), turretFile.value("ArmLOffsetZ").toFloat());
        turretFile.setSection("torpedomagazine");
        for (int i = 0; turretFile.contains(String("relative%1x").arg(i)); i++)
            m_tubes.push_back(Vector3D(turretFile.value(String("relative%1x").arg(i)).toFloat(), turretFile.value(String("relative%1y").arg(i)).toFloat(),
                                       turretFile.value(String("relative%1z").arg(i)).toFloat()));
    }
}


// the gun is in the turret's file: [StaticGun] and [VolleyGun], one on each arm, the barrels
// relative to the arm; the volley starts again whenever the aim is lost
void TurretBase::arm(int model, txt::DesFile &file)
{
    if (m_turretName.isEmpty())
        return;
    txt::DesFile turretFile(String("vfx:sobjects/%1.des").arg(m_turretName));
    const UnitGun::Mount mount = UnitGun::readMount(turretFile);
    if (mount.model < 0)
        return; // torpedo towers
    if (!UnitGun::isGun(model))
        model = mount.model;
    if (!UnitGun::isGun(model))
        return;
    std::vector<Vector3D> muzzles;
    for (const Vector3D &muzzle : mount.muzzles)
        muzzles.push_back(muzzle*m_scale);
    turretFile.setSection("cluster");
    const bool twoArms = turretFile.contains("arml") && turretFile.value("arml").toString() != "no";
    for (std::unique_ptr<UnitGun> *gun : { &m_unitGun, &m_armGun })
    {
        gun->reset(new UnitGun(m_scenario, this, model, muzzles, &turretFile));
        (*gun)->setRestartOnRelease(true);
        if (mount.ammunition >= 0)
            (*gun)->setAmmunition(mount.ammunition);
        if (!twoArms)
            break;
    }
}


void TurretBase::track(const Vector3D &point, bool bounded, float centre)
{
    const Vector3D way = point - pivot();
    const float yaw = std::atan2(way.x, way.y);
    if (bounded && glm::abs(std::remainder(yaw - centre, glm::two_pi<float>())) > glm::half_pi<float>())
    {
        aimAt(centre);
        return;
    }
    m_targetYaw = yaw;
    m_targetPitch = std::atan2(way.z, glm::length(Vector2D(way)));
    m_tracking = true;
}


bool TurretBase::arrived() const
{
    const bool softYaw = !m_tracking || m_softYaw, softPitch = !m_tracking || m_softPitch;
    const float yawError = glm::abs(std::remainder(m_targetYaw - m_yaw, glm::two_pi<float>()));
    const float low = m_tracking ? MinTrackPitch : MinTurnPitch;
    const float pitchError = m_torpedoTurret ? 0 : glm::abs(glm::clamp(m_targetPitch, low, MaxPitch) - m_pitch);
    return yawError <= (softYaw ? SoftDone : HardDone) && pitchError <= (softPitch ? SoftDone : HardDone);
}


Vector3D TurretBase::nextTube()
{
    const glm::mat3 turn = glm::mat3(glm::rotate(-m_yaw, Vector3D(0, 0, 1)));
    if (m_tubes.empty())
        return pivot() + aim()*4.0f;
    const Vector3D tube = m_tubes[m_nextTube];
    m_nextTube = (m_nextTube + 1) % int(m_tubes.size());
    return pivot() + turn*(tube*m_scale);
}


Vector3D TurretBase::aim() const
{
    return Vector3D(std::sin(m_yaw)*std::cos(m_pitch), std::cos(m_yaw)*std::cos(m_pitch), std::sin(m_pitch));
}


// a gun tower turns its gun to the front and lowers it to -14 degrees; a torpedo tower's turret
// stops where it is
bool TurretBase::startTumbling()
{
    m_tumbling = true;
    m_trigger = false;
    if (m_torpedoTurret)
        aimAt(m_yaw, m_pitch);
    else
        aimAt(0, -0.244f);
    m_scenario->disengagePlayer(this);
    return true;
}


namespace {

// one axis of the turn: soft (a damped spring) or hard (at the rate right onto the angle)
void turnAxis(float &angle, float &rate, float error, bool soft, float acceleration, float friction, float maxRate, float dt)
{
    if (soft)
    {
        float push = glm::abs(error) > SoftDone ? acceleration*error/glm::pi<float>() : 0.0f;
        push -= rate*std::min(friction, 1/dt);
        angle += rate*dt + 0.5f*push*dt*dt;
        rate = glm::clamp(rate + push*dt, -maxRate, maxRate);
    }
    else
    {
        rate = glm::abs(error) > HardDone ? glm::sign(error)*std::min(maxRate, glm::abs(error)/dt) : 0.0f;
        angle += rate*dt;
    }
}

}


bool TurretBase::update(float elapsedTime)
{
    if (m_tumbling)
        updateLaterExplosions();
    else
        ActiveObject::update(elapsedTime);
    if ((m_tumbling && updateGroundWreck(elapsedTime)) || !isEnabled())
        return false;
    // blowing up, the turret goes on at the target it had: tracking it, firing and launching at it
    // while it lives, until the tower is gone (docs/ROADMAP.md, appendix J)
    if (isGroundWrecking() && m_orders)
        m_orders->keepFiring();

    const float dt = std::max(elapsedTime, 1e-4f);
    const float low = m_tracking ? MinTrackPitch : MinTurnPitch;
    turnAxis(m_yaw, m_yawRate, std::remainder(m_targetYaw - m_yaw, glm::two_pi<float>()), !m_tracking || m_softYaw,
             m_yawAcceleration, m_yawFriction, m_maxYawRate, dt);
    m_yaw = std::remainder(m_yaw, glm::two_pi<float>());
    if (!m_torpedoTurret)
    {
        turnAxis(m_pitch, m_pitchRate, glm::clamp(m_targetPitch, low, MaxPitch) - m_pitch, !m_tracking || m_softPitch,
                 m_pitchAcceleration, m_pitchFriction, m_maxPitchRate, dt);
        if (m_pitch < low || m_pitch > MaxPitch)
        {
            m_pitch = glm::clamp(m_pitch, low, MaxPitch);
            m_pitchRate = 0;
        }
    }

    const Vector3D dir = aim();
    const Vector3D right = Vector3D(std::cos(m_yaw), -std::sin(m_yaw), 0);
    const glm::mat3 turn = glm::mat3(glm::rotate(-m_yaw, Vector3D(0, 0, 1)));
    if (m_unitGun)
        m_unitGun->update(elapsedTime, m_trigger, pivot() + turn*(m_armOffset[0]*m_scale), dir, glm::cross(right, dir), right);
    if (m_armGun)
        m_armGun->update(elapsedTime, m_trigger, pivot() + turn*(m_armOffset[1]*m_scale), dir, glm::cross(right, dir), right);
    return false;
}


void TurretBase::draw()
{
    gfx::Pipeline::pushMatrix();
    gfx::Pipeline::translate(m_position);
    gfx::Pipeline::scale(m_scale);
    m_base->draw();
    if (m_body)
    {
        // turn the turret around its position (the turret draws itself at that position)
        gfx::Pipeline::translate(m_turretPosition);
        gfx::Pipeline::rotate(-glm::degrees(m_yaw), Vector3D(0, 0, 1));
        gfx::Pipeline::translate(-m_turretPosition);
        m_body->setArmPitch(m_pitch);
        m_body->draw();
    }
    gfx::Pipeline::popMatrix();
}


bool TurretBase::intersect(const Vector3D &start, const Vector3D &dir, float radius, float &distance, Vector3D &normal)
{
    bool collision = m_base->intersect((start - m_position)/m_scale, dir, radius/m_scale, distance, normal);
    if (collision)
        distance *= m_scale;

    float d;
    Vector3D norm;
    if (m_body && m_body->intersect((start - m_position)/m_scale, dir, radius/m_scale, d, norm))
    {
        d *= m_scale;
        if (!collision || distance > d)
        {
            distance = d;
            normal = norm;
            collision = true;
        }
    }

    return collision;
}


} // namespace fight
