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

#include "torpedo.h"
#include "fight/scenario.h"
#include "fight/module.h"
#include "fight/effects/effectmanager.h"
#include "sfx/samplemap.h"
#include "txt/desfile.h"

#include <glm/geometric.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cmath>


namespace fight {


namespace {

// a torpedo explodes this close to its target (metres)
const float FuseDistance = 4.0f;
// the cluster torpedo bursts near any unit, damaging everything around it (TORPEDO.DES: "KineticStrength Area = 7600")
const float ClusterFuse = 25.0f;
const float ClusterRadius = 31.0f;
const int ClusterStrength = 7600;
// a bubble behind the torpedo this often (seconds)
const float TrailInterval = 0.06f;

}


const std::vector<TorpedoType>& TorpedoType::all()
{
    static std::vector<TorpedoType> types;
    if (!types.empty())
        return types;

    txt::DesFile weapons("vfx:sobjects/globweap.des");
    txt::DesFile bodies("vfx:sobjects/torpedo.des");
    for (int i = 0; i < 22; i++)
    {
        TorpedoType type;
        weapons.setSection(QString("torpedo%1").arg(i));
        type.name = weapons.value("longname").toString();
        type.shortName = weapons.value("shortname").toString();
        type.lockType = weapons.value("locktype", 0).toInt();
        type.lockDelay = weapons.value("lockdelay", 0).toFloat();

        bodies.setSection(QString("cluster%1").arg(i));
        type.model = bodies.value("name").toString().toLower();
        bodies.setSection(QString("size%1").arg(i));
        type.scale = bodies.value("scale").toFloat()/32.0f;
        bodies.setSection(QString("noise%1").arg(i));
        type.noise = bodies.value("level").toFloat();
        bodies.setSection(QString("sensor%1").arg(i));
        type.sensorRange = bodies.value("senr", 80).toFloat();
        bodies.setSection(QString("kinematics%1").arg(i));
        type.maxVelocity = bodies.value("maxvelocity").toFloat()/32.0f;
        if (type.maxVelocity <= 0)
            type.maxVelocity = 1500/32.0f; // one entry has no readable value
        type.acceleration = bodies.value("maxorientationacceleration", 600).toFloat()/32.0f;
        type.turnRate = std::min(bodies.value("maxanglevelocitya", 3).toFloat(), 6.0f);
        type.lifeTime = bodies.value("lifetime", 6).toFloat();
        bodies.setSection(QString("offense%1").arg(i));
        type.kineticStrength = bodies.value("kineticstrength").toInt();
        type.shockStrength = bodies.value("shockstrength").toInt();
        bodies.setSection(QString("defense%1").arg(i));
        type.kineticShield = bodies.value("kineticshield0", 1).toInt();
        types.push_back(type);
    }
    return types;
}


Torpedo::Torpedo(Scenario *scenario, int type, const Vector3D &position, const Vector3D &direction, float speed, ActiveObject *owner, ActiveObject *target) :
    ActiveObject(scenario),
    m_type(*TorpedoType::get(type)),
    m_model(scenario->moduleManager().get(m_type.model)),
    m_owner(owner),
    m_target(target),
    m_dir(glm::normalize(direction)),
    m_speed(speed),
    m_time(0),
    m_trailTime(0)
{
    m_position = position;
    m_noise = m_type.noise;
    m_kineticShield = m_kineticShieldMax = m_type.kineticShield;
    m_kineticStrength = m_type.kineticStrength;
    m_shockStrength = m_type.shockStrength;
    m_info.iff = owner ? owner->iff() : 0;
    m_box = BoundingBox(position - Vector3D(1.5f), position + Vector3D(1.5f));
    m_static = false;
}


bool Torpedo::update(float elapsedTime)
{
    if (!m_enabled)
        return false;

    m_time += elapsedTime;
    if (m_time >= m_type.lifeTime)
    {
        detonate(m_position, nullptr);
        return false;
    }

    // homing: towards the target, or a decoy that is closer
    Vector3D goal;
    bool homing = false;
    if (m_target && m_type.lockType != 0)
    {
        const bool alive = m_target == m_scenario->player() ? !m_scenario->playerDestroyed()
                                                             : m_target->isEnabled() && !m_target->eventDestroy()->isCompleted();
        if (alive)
        {
            goal = m_target->center();
            homing = true;
        }
        else
            m_target = nullptr;
    }
    Vector3D decoy;
    bool chasingDecoy = false;
    if (homing && m_scenario->decoyNear(m_position, m_type.sensorRange, this, decoy) &&
        glm::length(decoy - m_position) < glm::length(goal - m_position))
    {
        goal = decoy;
        chasingDecoy = true;
    }
    if (homing)
    {
        const Vector3D wanted = glm::normalize(goal - m_position);
        const float angle = std::acos(glm::clamp(glm::dot(m_dir, wanted), -1.0f, 1.0f));
        const float turn = m_type.turnRate*elapsedTime;
        m_dir = angle <= turn ? wanted : glm::normalize(m_dir + (wanted - m_dir)*(turn/angle));
        if (glm::length(goal - m_position) < FuseDistance)
        {
            detonate(m_position, chasingDecoy ? nullptr : m_target);
            return false;
        }
    }

    // the cluster torpedo bursts as soon as a unit (other than its launcher) is near
    if (m_type.isCluster())
    {
        auto near = [this](const ActiveObject *unit) {
            return unit && unit != m_owner && unit->isEnabled() && glm::length(unit->center() - m_position) < ClusterFuse;
        };
        bool burst = near(m_scenario->player()) && !m_scenario->playerDestroyed();
        for (const auto &record : m_scenario->objectRecords())
            burst = burst || near(dynamic_cast<const ActiveObject *>(record.object));
        if (burst)
        {
            detonate(m_position, nullptr);
            return false;
        }
    }

    // a trail of bubbles (EXPLOSIO.DES 21-23)
    m_trailTime += elapsedTime;
    if (m_trailTime >= TrailInterval)
    {
        m_trailTime = 0;
        m_scenario->effectManager().addEffect(Effects(int(Effects::Explosion_21) + qrand()%3), m_position - m_dir*1.5f);
    }

    m_speed = std::min(m_type.maxVelocity, m_speed + m_type.acceleration*elapsedTime);
    m_velocity = m_dir*m_speed;
    const Vector3D next = m_position + m_velocity*elapsedTime;

    Vector3D position, normal;
    if (m_scenario->surface().testCollision(m_position, next, 0.5f, position, normal))
    {
        detonate(position, nullptr);
        return false;
    }
    if (Object *hit = m_scenario->collisionManager().testCollision(m_position, next, 0.5f, position, normal, nullptr, m_owner))
    {
        detonate(position, dynamic_cast<ActiveObject *>(hit));
        return false;
    }

    setPosition(next);
    return false;
}


void Torpedo::detonate(const Vector3D &position, ActiveObject *hit)
{
    if (!m_enabled)
        return;
    disable();
    if (m_type.isCluster())
    {
        // everything near the burst, less further away
        auto blast = [this, &position](ActiveObject *unit) {
            if (!unit || unit == m_owner || !unit->isEnabled())
                return;
            const float distance = glm::length(unit->center() - position);
            if (distance < ClusterRadius)
                unit->damage(int(ClusterStrength*(1 - 0.5f*distance/ClusterRadius)), 0, position, m_owner);
        };
        if (!m_scenario->playerDestroyed())
            blast(m_scenario->player());
        for (const auto &record : m_scenario->objectRecords())
            blast(dynamic_cast<ActiveObject *>(record.object));
        m_scenario->effectManager().addEffect(Effects::Explosion_2, position);
        for (int i = 0; i < 5; i++)
            m_scenario->effectManager().addEffect(Effects::Explosion_9, position + Vector3D(qrand()%21 - 10, qrand()%21 - 10, qrand()%11 - 5));
        sfx::SampleMap::playAt(sfx::Sample::Explosion_Big, position.x, position.y, position.z);
        m_scenario->explosionAt(position, 1.0f);
        return;
    }
    if (hit)
        hit->damage(m_type.kineticStrength, m_type.shockStrength, position, m_owner);
    m_scenario->effectManager().addEffect(Effects::Explosion_1, position);
    sfx::SampleMap::playAt(sfx::Sample::Explosion_Small, position.x, position.y, position.z);
    m_scenario->explosionAt(position, 0.7f);
}


// a sphere of 1.5 m; only entering it counts, so the torpedo's own movement never hits itself
bool Torpedo::intersect(const Vector3D &start, const Vector3D &dir, float radius, float &distance, Vector3D &normal)
{
    const float size = 1.5f + radius;
    const Vector3D toCentre = m_position - start;
    if (glm::length(toCentre) <= size)
        return false;
    const float along = glm::dot(toCentre, dir);
    const float miss = glm::length(toCentre - dir*along);
    if (along < 0 || miss > size)
        return false;
    distance = along - std::sqrt(size*size - miss*miss);
    normal = glm::normalize(start + dir*distance - m_position);
    return true;
}


// shot down
void Torpedo::destroy()
{
    detonate(m_position, nullptr);
}


void Torpedo::draw()
{
    // the model's nose is +y
    Vector3D right = glm::cross(m_dir, Vector3D(0, 0, 1));
    right = glm::length(right) > 1e-3f ? glm::normalize(right) : Vector3D(1, 0, 0);
    const Vector3D up = glm::cross(right, m_dir);
    const Matrix orientation(Vector4D(right, 0), Vector4D(m_dir, 0), Vector4D(up, 0), Vector4D(0, 0, 0, 1));

    glPushMatrix();
    glTranslatef(m_position.x, m_position.y, m_position.z);
    glMultMatrixf(glm::value_ptr(orientation));
    glScalef(m_type.scale, m_type.scale, m_type.scale);
    m_model->draw();
    glPopMatrix();
}


} // namespace fight
