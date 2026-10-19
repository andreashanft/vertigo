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
#include "fight/effects/billboard.h"
#include "sfx/samplemap.h"
#include "txt/desfile.h"
#include "util/random.h"
#include "gfx/pipeline.h"

#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>
#include <glm/gtc/constants.hpp>
#include <algorithm>
#include <cmath>


namespace fight {


namespace {

// the Cluster Bomb's shards (SHOOT.DES 7 and 8, "KineticStrength Area = 7600" in TORPEDO.DES): a ring
// a metre out at the burst, another 6.25 m out a second later
const float ShardsNear = 1.0f;
const float ShardsFar = 6.25f;
const float ShardsDelay = 1.0f;
// a homing torpedo looks for the ground between it and its goal this often (s)
const float LookInterval = 0.5f;
// and climbs this far over it (m)
const float Climb = 31.25f;
// a bubble (EXPLOSIO.DES 22) behind the torpedo every two of its frames' time, from half a
// second after the launch (docs/ROADMAP.md, appendix S)
const float TrailInterval = 0.16f;
const float TrailStart = 0.5f;
// the agile types fly straight this long before they make for the target
const float AgileStart = 0.7f;
// the agile types' Agility is a share per tick, at the ticks the player's turning uses
const float AgilityTicks = 30.0f;
// a spent torpedo sinks at half a g
const float SpentSink = 0.5f*9.81f;

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
        weapons.setSection(String("torpedo%1").arg(i));
        type.name = weapons.value("longname").toString();
        type.shortName = weapons.value("shortname").toString();
        type.image = weapons.value("imagename").toString().section('\\', -1).section('.', 0, 0).toLower();
        type.lockType = weapons.value("locktype", 0).toInt();
        type.lockDelay = weapons.value("lockdelay", 0).toFloat();

        bodies.setSection(String("cluster%1").arg(i));
        type.model = bodies.value("name").toString().toLower();
        bodies.setSection(String("size%1").arg(i));
        type.scale = bodies.value("scale").toFloat()/32.0f;
        bodies.setSection(String("noise%1").arg(i));
        type.noise = bodies.value("level").toFloat();
        bodies.setSection(String("sensor%1").arg(i));
        type.sensorRange = bodies.value("senr", 80).toFloat();
        type.influence = bodies.value("influenceprobability", 1).toFloat();
        bodies.setSection(String("kinematics%1").arg(i));
        type.maxVelocity = bodies.value("maxvelocity").toFloat()/32.0f;
        if (type.maxVelocity <= 0)
            type.maxVelocity = 1500/32.0f; // one entry has no readable value
        type.acceleration = bodies.value("maxorientationacceleration", 600).toFloat()/32.0f;
        type.retardation = bodies.value("maxorientationretardation", 600).toFloat()/32.0f;
        type.sideFriction = bodies.value("orientationfrictionx", 1.5f).toFloat();
        type.angleAcceleration = Vector2D(bodies.value("maxangleaccelerationa", 15).toFloat(), bodies.value("maxangleaccelerationb", 10).toFloat());
        type.angleVelocity = Vector2D(bodies.value("maxanglevelocitya", 3).toFloat(), bodies.value("maxanglevelocityb", 2).toFloat());
        type.angleFriction = Vector2D(bodies.value("anglefrictiona", 1.5f).toFloat(), bodies.value("anglefrictionb", 1.5f).toFloat());
        type.agility = bodies.value("agility", 0.5f).toFloat();
        type.lifeTime = bodies.value("lifetime", 6).toFloat();
        type.agile = i == 7 || i == 9 || i == 10 || i == 13 || i == 19 || i == 20;
        type.index = i;
        bodies.setSection(String("offense%1").arg(i));
        type.kineticStrength = bodies.value("kineticstrength").toInt();
        type.shockStrength = bodies.value("shockstrength").toInt();
        bodies.setSection(String("defense%1").arg(i));
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
    m_target(m_type.lockType != 0 ? target : nullptr),
    m_dir(glm::normalize(direction)),
    m_speed(speed),
    m_time(0),
    m_trailTime(0)
{
    m_position = position;
    m_velocity = m_dir*speed;
    m_noise = m_type.noise;
    m_kineticShield = m_kineticShieldMax = m_type.kineticShield;
    m_kineticStrength = m_type.kineticStrength;
    m_shockStrength = m_type.shockStrength;
    m_info.iff = owner ? owner->iff() : 0;
    m_box = BoundingBox(position - Vector3D(1.5f), position + Vector3D(1.5f));
    m_static = false;
    if (m_target)
        m_startDistance = std::max(1.0f, glm::length(m_target->center() - position));
    m_influenced = util::random()/float(RAND_MAX) < m_type.influence;
}


bool Torpedo::targetAlive() const
{
    return m_target == m_scenario->player() ? !m_scenario->playerDestroyed()
                                            : m_target->isEnabled() && !m_target->eventDestroy()->isCompleted();
}


// (docs/ROADMAP.md, appendix S) Its time up, or its target gone, the torpedo's engine stops and
// its homing with it; it sinks until it hits the ground, where it goes off
void Torpedo::spend()
{
    m_spent = true;
    m_target = nullptr;
}


// Yaw and pitch each speed up by the type's angular acceleration times the error over pi (none
// within a degree), slowed by their friction and held to their top speed
void Torpedo::turnTowards(const Vector3D &point, float elapsedTime)
{
    const Vector3D way = point - m_position;
    const Vector2D flat(m_dir), wayFlat(way);
    const float heading = std::atan2(m_dir.y, m_dir.x);
    float pitch = std::asin(glm::clamp(m_dir.z, -1.0f, 1.0f));
    const float yawError = std::atan2(flat.x*wayFlat.y - flat.y*wayFlat.x, glm::dot(flat, wayFlat));
    const float pitchError = std::atan2(way.z, glm::length(wayFlat)) - pitch;
    const Vector2D error(yawError, pitchError);
    for (int axis = 0; axis < 2; axis++)
    {
        float acceleration = std::abs(error[axis]) > glm::radians(1.0f) ? m_type.angleAcceleration[axis]*error[axis]/glm::pi<float>() : 0;
        const float friction = m_type.angleFriction[axis]*elapsedTime;
        acceleration -= friction >= 1 ? m_turn[axis]/elapsedTime : m_turn[axis]*m_type.angleFriction[axis];
        m_turn[axis] = glm::clamp(m_turn[axis] + acceleration*elapsedTime, -m_type.angleVelocity[axis], m_type.angleVelocity[axis]);
    }
    pitch = glm::clamp(pitch + m_turn.y*elapsedTime, -glm::radians(89.0f), glm::radians(89.0f));
    const float yaw = heading + m_turn.x*elapsedTime;
    m_dir = Vector3D(std::cos(pitch)*std::cos(yaw), std::cos(pitch)*std::sin(yaw), std::sin(pitch));
}


// along the nose the speed makes for the wanted one at the type's acceleration or retardation;
// across it the speed dies away by the type's friction (OrientationFrictionX and Z)
void Torpedo::drive(float speed, float elapsedTime)
{
    float along = glm::dot(m_velocity, m_dir);
    Vector3D across = m_velocity - m_dir*along;
    if (speed > along)
        along = std::min(speed, along + m_type.acceleration*elapsedTime);
    else
        along = std::max(speed, along - m_type.retardation*elapsedTime);
    across *= std::max(0.0f, 1.0f - m_type.sideFriction*elapsedTime);
    m_velocity = m_dir*along + across;
}


bool Torpedo::update(float elapsedTime)
{
    if (!m_enabled)
        return false;

    m_time += elapsedTime;
    if (isBursting())
    {
        if (m_time >= m_burstTime + ShardsDelay)
        {
            shards(ShardsFar, glm::radians(30.0f));
            disable();
        }
        return false;
    }
    if (!m_spent && (m_time >= m_type.lifeTime || (m_target && !targetAlive())))
        spend();

    if (m_spent)
    {
        // no thrust; the nose follows the way it moves, a little below it, and the torpedo sinks,
        // keeping its way along the nose
        const Vector3D way = glm::length(m_velocity) > 1e-3f ? glm::normalize(m_velocity) : m_dir;
        turnTowards(m_position + glm::normalize(way + Vector3D(0, 0, -0.1f)), elapsedTime);
        const float along = glm::dot(m_velocity, m_dir);
        m_velocity = m_dir*along + (m_velocity - m_dir*along)*std::max(0.0f, 1.0f - m_type.sideFriction*elapsedTime);
        m_velocity.z -= SpentSink*elapsedTime;
    }
    else if (m_target)
    {
        // a decoy can draw it away once, and only with the type's InfluenceProbability
        Vector3D goal = m_target->center();
        Vector3D decoy;
        if (m_influenced && m_scenario->decoyNear(m_position, m_type.sensorRange, this, decoy))
        {
            m_influenced = false;
            m_decoy = true;
        }
        if (m_decoy)
        {
            if (m_scenario->decoyNear(m_position, m_type.sensorRange, this, decoy))
                goal = decoy;
            else
                m_decoy = false;
        }
        // every half second it looks whether the ground is between it and its goal, and while
        // it is, makes for a way over it (the agile types start looking once they turn)
        if (m_time >= m_nextLook && (!m_type.isAgile() || m_time >= AgileStart))
        {
            m_nextLook = m_time + LookInterval;
            Vector3D hit, normal;
            m_blocked = m_scenario->surface().testCollision(m_position, goal, 0.5f, hit, normal);
        }
        const Vector3D aim = m_blocked ? detour(goal) : goal;
        // it slows down closing in: from its top speed at the launch distance to a mix of the
        // target's speed and its own
        const float share = std::min(1.0f, glm::length(goal - m_position)/m_startDistance);
        const float targetSpeed = glm::length(m_target->velocity());
        const float vmax = m_type.maxVelocity;
        if (m_type.isAgile())
        {
            const float wanted = std::min(vmax, (targetSpeed + 0.4f*vmax)*(1 - share) + vmax*share);
            if (m_time < AgileStart)
                drive(vmax, elapsedTime);
            else
            {
                // the speed as wanted, the velocity turned by the Agility per tick
                float speed = glm::length(m_velocity);
                speed = wanted > speed ? std::min(wanted, speed + m_type.acceleration*elapsedTime)
                                       : std::max(wanted, speed - m_type.retardation*elapsedTime);
                const Vector3D way = aim - m_position;
                const float keep = std::pow(1.0f - m_type.agility, AgilityTicks*elapsedTime);
                const Vector3D current = glm::length(m_velocity) > 1e-3f ? glm::normalize(m_velocity) : m_dir;
                const Vector3D blended = current*keep + (glm::length(way) > 1e-3f ? glm::normalize(way) : m_dir)*(1 - keep);
                if (glm::length(blended) > 1e-3f)
                    m_dir = glm::normalize(blended);
                m_velocity = m_dir*speed;
            }
        }
        else
        {
            turnTowards(aim, elapsedTime);
            drive((1.5f*targetSpeed + 0.3f*vmax)*(1 - share) + vmax*share, elapsedTime);
        }
    }
    else
        drive(m_type.maxVelocity, elapsedTime);
    // the water's currents push it as they push the boats (docs/ROADMAP.md, appendix D)
    m_velocity += m_scenario->current(m_position)*elapsedTime;
    m_speed = glm::length(m_velocity);

    // a trail of bubbles while the engine runs
    if (!m_spent && m_time >= TrailStart)
    {
        m_trailTime += elapsedTime;
        if (m_trailTime >= TrailInterval)
        {
            m_trailTime -= TrailInterval;
            m_scenario->effectManager().addEffect(Effects::Explosion_22, m_position - m_dir*1.5f);
        }
    }

    const Vector3D next = m_position + m_velocity*elapsedTime;

    Vector3D position, normal;
    if (m_scenario->surface().testCollision(m_position, next, 0.5f, position, normal))
    {
        detonate(position + normal*0.5f, nullptr, true);
        return false;
    }
    Object *hit = m_scenario->collisionManager().testCollision(m_position, next, 0.5f, position, normal, nullptr, m_owner);
    // a piece of debris before that: with its armour of 1 the torpedo goes up there
    // (docs/ROADMAP.md, appendix Q)
    Particles::DebrisHit debris;
    if (m_scenario->effectManager().hitDebris(m_position, hit ? position : next, 0.5f, m_speed, m_type.kineticStrength, debris))
    {
        detonate(debris.position, nullptr);
        return false;
    }
    if (hit)
    {
        detonate(position, dynamic_cast<ActiveObject *>(hit));
        return false;
    }

    setPosition(next);
    return false;
}


// (docs/ROADMAP.md, appendix S) The Flash Sharks and Leeches go off on the ground in a large flash
// and elsewhere in a large ECM impact, sized to their hull (1.6, 2 or 2.4 times its radius); the
// Torch in a small large flash, leaving its burning torch, which draws torpedoes while it lasts;
// the Cluster Bomb in a large puff and two rings of shards; the others in a medium explosion
void Torpedo::detonate(const Vector3D &position, ActiveObject *hit, bool ground)
{
    if (!m_enabled || isBursting())
        return;
    if (hit)
        hit->damage(m_type.kineticStrength, m_type.shockStrength, position, m_owner);

    auto sized = [this, &position](Effects effect, float factor) {
        Billboard *billboard = m_scenario->effectManager().getBillboard(effect);
        const float sprite = billboard ? billboard->box().maxPoint().x : 1.0f;
        m_scenario->effectManager().addEffect(effect, position, 0, factor*radius()/std::max(sprite, 0.01f));
    };
    const Effects flash = ground ? Effects::Explosion_2 : Effects::Explosion_17;
    bool big = false;
    switch (m_type.index)
    {
    case 11: case 19: sized(flash, 1.6f); big = true; break;
    case 12: case 20: sized(flash, 2.0f); big = true; break;
    case 13: sized(flash, 2.4f); big = true; break;
    case 21:
        {
            sized(Effects::Explosion_2, 0.9f);
            m_scenario->effectManager().addDebris(Effects::Debris_22, position, Vector3D(0, 0, 1));
        }
        break;
    case 18:
        sized(Effects::Explosion_5, 0.9f);
        m_target = nullptr;
        m_burstTime = m_time;
        m_burstHeading = std::atan2(m_dir.y, m_dir.x);
        setPosition(position);
        shards(ShardsNear, 0);
        sfx::SampleMap::playAt(sfx::Sample::Explosion_Big, position.x, position.y, position.z);
        m_scenario->explosionAt(position, 1.0f);
        return;
    default:
        m_scenario->effectManager().addEffect(Effects::Explosion_1, position);
    }
    disable();
    sfx::SampleMap::playAt(big ? sfx::Sample::Explosion_Big : sfx::Sample::Explosion_Small, position.x, position.y, position.z);
    m_scenario->explosionAt(position, 0.7f);
}


// six shards (SHOOT.DES 7) 60 degrees apart, then twelve (SHOOT.DES 8) 30 degrees apart starting 15
// degrees on, each from that far out round the burst and flying outwards a little upwards
void Torpedo::shards(float distance, float turn)
{
    float angle = m_burstHeading + turn;
    auto shard = [this, distance, &angle](Effects type) {
        const Vector3D out(std::cos(angle), std::sin(angle), 0);
        m_scenario->effectManager().addProjectile(type, m_position + out*distance, glm::normalize(out + Vector3D(0, 0, 0.2f)), m_owner);
    };
    for (int i = 0; i < 6; i++, angle += glm::radians(60.0f))
        shard(Effects::Shoot_7);
    angle += glm::radians(15.0f);
    for (int i = 0; i < 12; i++, angle += glm::radians(30.0f))
        shard(Effects::Shoot_8);
}


float Torpedo::radius() const
{
    const Vector3D dim = m_model ? m_model->box().dim()*m_type.scale : Vector3D(1);
    return 0.5f*std::max(dim.x, std::max(dim.y, dim.z));
}


// Over the ground in its way: towards the goal, but well above the ground it would hit within a
// second and a half's way (and two hull radii), and climbing steeply while the ground three
// quarters of a second ahead comes within half a radius of its hull
Vector3D Torpedo::detour(const Vector3D &goal) const
{
    const Vector3D toGoal = glm::length(goal - m_position) > 1e-3f ? glm::normalize(goal - m_position) : m_dir;
    const float ahead = 2*radius() + 1.5f*m_speed;
    Vector3D aim = goal;
    Vector3D hit, normal;
    if (m_scenario->surface().testCollision(m_position, m_position + toGoal*ahead, 0.5f, hit, normal) ||
        m_scenario->surface().testCollision(m_position, m_position + m_dir*ahead, 0.5f, hit, normal))
    {
        aim = m_position + toGoal*ahead;
        aim.z = std::max(aim.z, hit.z + Climb);
    }
    const Vector3D probe = m_position + m_dir*(0.5f*m_scenario->surface().scale().x + 0.75f*m_speed);
    if (m_scenario->surface().heightAt(probe.x, probe.y) > m_position.z - 1.5f*radius())
        aim.z = std::max(aim.z, m_position.z + Climb);
    return aim;
}


// a sphere of 1.5 m; only entering it counts, so the torpedo's own movement never hits itself
bool Torpedo::intersect(const Vector3D &start, const Vector3D &dir, float radius, float &distance, Vector3D &normal)
{
    if (isBursting())
        return false;
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
    if (isBursting())
        return;
    // the model's nose is +y
    Vector3D right = glm::cross(m_dir, Vector3D(0, 0, 1));
    right = glm::length(right) > 1e-3f ? glm::normalize(right) : Vector3D(1, 0, 0);
    const Vector3D up = glm::cross(right, m_dir);
    const Matrix orientation(Vector4D(right, 0), Vector4D(m_dir, 0), Vector4D(up, 0), Vector4D(0, 0, 0, 1));

    gfx::Pipeline::pushMatrix();
    gfx::Pipeline::translate(m_position);
    gfx::Pipeline::multiply(orientation);
    gfx::Pipeline::scale(m_type.scale);
    m_model->draw();
    gfx::Pipeline::popMatrix();
}


} // namespace fight
