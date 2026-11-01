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

#include "mine.h"
#include <glm/gtx/transform.hpp>
#include <cmath>
#include <glm/gtc/constants.hpp>
#include "util/random.h"
#include "fight/scenario.h"
#include "fight/boats/player.h"
#include "fight/collisionmanager.h"
#include "fight/objects/building.h"
#include "fight/boats/cargoship.h"
#include "fight/effects/effectmanager.h"
#include "fight/effects/trash.h"
#include "sfx/samplemap.h"
#include "txt/desfile.h"
#include <glm/geometric.hpp>
#include <glm/gtx/norm.hpp>
#include <algorithm>


namespace fight {


namespace {

// an active mine sets off once the player is within 50 m of the place it was laid and gives up
// beyond 56 m; it moves at 0.3 times its MaxVelocity, keeping 1.5 times its radius above the
// ground
const float ChaseDistance = 50.0f;
const float GiveUpDistance = 56.0f;
const float SpeedFactor = 0.3f;
const float GroundClearance = 1.5f;
// the player's collision radius (as in Player::update)
const float PlayerRadius = 1.5f;

}


Mine::Mine(Scenario *scenario, txt::DesFile &file, const ObjectInfo &info) :
    SimpleObject(scenario, file, info)
{
    file.setSection("kinematics");
    m_speed = std::max(0.0f, file.value("maxvelocity").toFloat()/32.0f*SpeedFactor);
    // a mine that moves can't be remembered by the shots' collision cache
    m_static = !isActive();
    // every mine turns about the vertical at 0.5 to 1 rad/s from a random heading, and nods up and
    // down by up to 0.25 rad as the cosine of a phase that runs at up to 0.5 rad/s
    m_spin = 0.5f + 0.5f*util::random()/float(RAND_MAX);
    m_nodRate = 0.5f*util::random()/float(RAND_MAX);
    setYaw((2.0f*util::random()/float(RAND_MAX) - 1.0f)*glm::pi<float>());
}


void Mine::setPosition(const Vector3D &pos)
{
    SimpleObject::setPosition(pos);
    if (!m_chasing)
        m_home = pos;
}


bool Mine::update(float elapsedTime)
{
    ActiveObject::update(elapsedTime);
    if (m_eventDestroy.isCompleted())
        return false;
    setYaw(std::remainder(m_yaw + m_spin*elapsedTime, glm::two_pi<float>()));
    m_nodPhase = std::remainder(m_nodPhase + m_nodRate*elapsedTime, glm::two_pi<float>());
    const Vector3D right(std::cos(m_yaw), -std::sin(m_yaw), 0);
    m_tilt = glm::mat3(glm::rotate(0.25f*std::cos(m_nodPhase), right));
    if (!isActive())
        return false;

    Player *player = m_scenario->player();
    const float fromHome = glm::length(player->position() - m_home);
    if (m_chasing && fromHome > GiveUpDistance)
        m_chasing = false;
    else if (!m_chasing && fromHome < ChaseDistance)
        m_chasing = true;

    const Vector3D goal = m_chasing ? player->position() : m_home;
    const Vector3D way = goal - m_position;
    const float step = m_speed*elapsedTime;
    const float distance = glm::length(way);
    const float r = radius();
    if (distance > 1e-4f)
    {
        Vector3D next = distance <= step ? goal : m_position + way*(step/distance);
        next.z = std::max(next.z, m_scenario->surface().heightAt(next.x, next.y) + GroundClearance*r);
        // running into another faction's unit or a building on the way
        Vector3D contact, normal;
        const Vector3D from = center();
        auto other = [this, player](const Object *object) {
            if (dynamic_cast<const Building *>(object))
                return true;
            const ActiveObject *unit = dynamic_cast<const ActiveObject *>(object);
            return unit && unit != player && collidesWith(unit);
        };
        if (Object *hit = m_scenario->collisionManager().testCollision(from, from + next - m_position, r, contact, normal, nullptr, this, other))
        {
            collide(hit, contact);
            return false;
        }
        // debris slower than the mine
        Particles::DebrisHit debris;
        if (m_scenario->effectManager().hitDebris(from, from + next - m_position, r, m_speed, m_kineticStrength, debris))
        {
            damage(debris.kinetic, debris.shock, debris.position);
            if (m_eventDestroy.isCompleted())
                return false;
        }
        SimpleObject::setPosition(next);
    }

    // running into the player: both take the other's strength, as when the player runs into it
    if (m_chasing && glm::length(player->position() - center()) < r + PlayerRadius)
    {
        const Vector3D contact = center() + glm::normalize(player->position() - center())*r;
        player->damage(m_kineticStrength, m_shockStrength, contact);
        damage(player->kineticStrength(), player->shockStrength(), contact, player);
    }
    return false;
}


float Mine::radius() const
{
    return 0.5f*std::max(m_box.dim().x, std::max(m_box.dim().y, m_box.dim().z));
}


bool Mine::collidesWith(const ActiveObject *unit) const
{
    if (!isEnabled() || m_eventDestroy.isCompleted() || !unit || unit == this || !unit->isEnabled() || unit->contactFaction() == contactFaction())
        return false;
    // boats, tanks, towers, turrets and buildings, ship segments and trash, never another mine
    return (dynamic_cast<const SimpleObject *>(unit) && !dynamic_cast<const Mine *>(unit)) || dynamic_cast<const CargoSegment *>(unit) ||
           dynamic_cast<const Trash *>(unit);
}


void Mine::collide(Object *object, const Vector3D &contact)
{
    // a ground plan in the way: the mine goes up on it, whatever the plan's strength
    if (dynamic_cast<Building *>(object))
    {
        destroy();
        return;
    }
    const int kinetic = object->kineticStrength(), shock = object->shockStrength();
    if (ActiveObject *unit = dynamic_cast<ActiveObject *>(object))
        unit->damage(m_kineticStrength, m_shockStrength, contact);
    damage(kinetic, shock, contact);
}


// one explosion of the mine's kind and no debris: a puff for a passive mine, a flash for an
// active one
void Mine::destroy()
{
    if (m_eventDestroy.isCompleted())
        return;
    const Vector3D at = center();
    m_scenario->effectManager().addEffect(isActive() ? Effects::Explosion_1 : Effects::Explosion_5, at);
    // each explosion has its sound: the puff the small explosion's, the flash the big one's
    sfx::SampleMap::playAt(isActive() ? sfx::Sample::Explosion_Big : sfx::Sample::Explosion_Small, at.x, at.y, at.z);
    m_scenario->explosionAt(at, 0.3f);
    vanish();
    m_eventDestroy.complete();
    m_scenario->logEvent(name(), &m_eventDestroy, txt::Fight5);
}


} // namespace fight
