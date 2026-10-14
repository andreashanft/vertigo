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
#include "fight/weapons/unitgun.h"
#include "txt/desfile.h"
#include "gfx/pipeline.h"
#include <glm/gtc/constants.hpp>
#include <glm/trigonometric.hpp>
#include <cmath>


namespace fight {


TurretBase::TurretBase(Scenario *scenario, txt::DesFile &file, const ObjectInfo &info) :
    SimpleObject(scenario, file, info),
    m_yaw(0),
    m_pitch(0),
    m_targetYaw(0),
    m_targetPitch(0),
    m_yawVelocity(1),
    m_pitchVelocity(1)
{
    m_static = false;

    for (const char *section : { "gunturret", "torpedoturret" })
    {
        file.setSection(section);
        if (!file.contains("name"))
            continue;
        m_turretName = file.value("name").toString();
        txt::DesFile turretFile(String("vfx:sobjects/%1.des").arg(m_turretName));
        m_body.reset(new Turret(scenario, turretFile)); // TODO: TurretTorpedo
        m_turretPosition = Vector3D(file.value("RelativePositionX").toFloat(), file.value("RelativePositionY").toFloat(), file.value("RelativePositionZ").toFloat());
        m_body->setPosition(m_turretPosition);
        m_box.add(m_body->box().scale(m_scale));

        turretFile.setSection("kinematics");
        m_yawVelocity = turretFile.value("MaxAngleVelocityA", 1).toFloat();
        m_pitchVelocity = turretFile.value("MaxAngleVelocityB", 1).toFloat();
    }
}


// the gun is in the turret's file: [StaticGun] and [VolleyGun]
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
    m_unitGun.reset(new UnitGun(m_scenario, this, model, muzzles, &turretFile));
    if (mount.ammunition >= 0)
        m_unitGun->setAmmunition(mount.ammunition);
}


void TurretBase::aimAt(const Vector3D &point)
{
    const Vector3D way = point - pivot();
    m_targetYaw = std::atan2(way.x, way.y);
    m_targetPitch = std::atan2(way.z, glm::length(Vector2D(way)));
}


Vector3D TurretBase::aim() const
{
    return Vector3D(std::sin(m_yaw)*std::cos(m_pitch), std::cos(m_yaw)*std::cos(m_pitch), std::sin(m_pitch));
}


bool TurretBase::update(float elapsedTime)
{
    ActiveObject::update(elapsedTime);

    // turn towards the target at the turret's rates
    const float yawError = std::remainder(m_targetYaw - m_yaw, glm::two_pi<float>());
    m_yaw += glm::clamp(yawError, -m_yawVelocity*elapsedTime, m_yawVelocity*elapsedTime);
    m_pitch += glm::clamp(m_targetPitch - m_pitch, -m_pitchVelocity*elapsedTime, m_pitchVelocity*elapsedTime);

    if (m_unitGun)
    {
        const Vector3D dir = aim();
        const Vector3D right = Vector3D(std::cos(m_yaw), -std::sin(m_yaw), 0);
        m_unitGun->update(elapsedTime, m_trigger, pivot(), dir, glm::cross(right, dir), right);
    }
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
