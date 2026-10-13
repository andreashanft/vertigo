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

#include "orders.h"
#include "scenario.h"
#include "boats/simpleboat.h"
#include "objects/activeobject.h"

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <algorithm>
#include <cmath>


namespace fight {


namespace {

// a unit counts as having arrived within this horizontal distance (metres)
const float ArriveDistance = 20.0f;
// durations from this on mean "forever"
const int Forever = 100000;

float random()
{
    return qrand()/float(RAND_MAX);
}

}


QString OrderSlot::name(int type)
{
    static const char *names[] = { "none", "wait", "goto", "patrol", "attack", "escort", "ambush",
                                   "cardinalpatrol", "cardinalattack", "gotowitharrive", "gotowithinfiltrate", "explode" };
    return type >= 0 && type <= Explode ? names[type] : QString::number(type);
}


bool OrderSlot::isImplemented(int type)
{
    return type == Wait || type == Goto || type == Patrol || type == Escort || type == GotoWithArrive;
}


Orders::Orders(Scenario *scenario, SimpleBoat *boat, const std::vector<OrderSlot> &orderSlots) :
    m_scenario(scenario),
    m_boat(boat),
    m_slots(orderSlots),
    m_current(-1),
    m_startTime(0),
    m_pointHeight(0)
{
    if (!m_slots.empty())
        start(0);
}


void Orders::start(int slot)
{
    m_current = slot < int(m_slots.size()) ? slot : -1;
    m_startTime = m_scenario->time();
    if (m_current >= 0 && m_slots[m_current].type == OrderSlot::Patrol)
        pickPatrolPoint();
}


void Orders::next()
{
    start(m_current + 1);
}


float Orders::altitude(const Vector3D &point, int height) const
{
    Surface &surface = m_scenario->surface();
    const Vector3D &position = m_boat->position();

    // the highest ground under the unit, a little ahead of it, and at the point once it is close
    Vector2D way = Vector2D(point) - Vector2D(position);
    const float distance = glm::length(way);
    if (distance > 0.1f)
        way /= distance;
    float ground = surface.heightAt(position.x, position.y);
    for (float ahead : { 30.0f, 60.0f })
    {
        const Vector2D p = Vector2D(position) + way*std::min(ahead, distance);
        ground = std::max(ground, surface.heightAt(p.x, p.y));
    }
    if (distance < 60.0f)
        ground = std::max(ground, surface.heightAt(point.x, point.y));

    return ground + std::max(height*surface.scale().z, 3.0f);
}


void Orders::pickPatrolPoint()
{
    const OrderSlot &slot = m_slots[m_current];
    const Vector3D &scale = m_scenario->surface().scale();
    const float angle = random()*glm::two_pi<float>();
    const float radius = slot.rad*std::sqrt(random());
    m_point = Vector3D((slot.refx + 0.5f)*scale.x + radius*std::cos(angle), (slot.refy + 0.5f)*scale.y + radius*std::sin(angle), 0);
    m_pointHeight = slot.mih + int(random()*(slot.mah - slot.mih));
}


void Orders::update()
{
    // finish orderSlots whose break condition is met or whose time is up
    while (m_current >= 0)
    {
        const OrderSlot &slot = m_slots[m_current];
        if (slot.breakCondition && slot.breakCondition->isCompleted())
        {
            next();
            continue;
        }
        const bool timed = slot.type == OrderSlot::Wait || slot.type == OrderSlot::Patrol || slot.type == OrderSlot::Escort;
        if (timed && slot.dur < Forever && m_scenario->time() - m_startTime >= slot.dur)
        {
            next();
            continue;
        }
        break;
    }

    if (m_current < 0)
    {
        m_boat->hold();
        return;
    }

    const OrderSlot &slot = m_slots[m_current];
    const Vector3D &scale = m_scenario->surface().scale();
    const float speed = m_boat->maxVelocity()*slot.vel/100.0f;

    switch (slot.type)
    {
    case OrderSlot::Goto:
    case OrderSlot::GotoWithArrive:
        {
            Vector3D target((slot.refx + 0.5f)*scale.x, (slot.refy + 0.5f)*scale.y, 0);
            const float distance = glm::length(Vector2D(target) - Vector2D(m_boat->position()));
            if (distance < ArriveDistance)
            {
                if (slot.type == OrderSlot::GotoWithArrive)
                    m_boat->eventFinish()->complete();
                next();
                m_boat->hold();
                return;
            }
            target.z = altitude(target, slot.hei);
            // GotoWithArrive stops at the point, Goto passes through it
            const float arriveSpeed = slot.type == OrderSlot::GotoWithArrive ? std::min(speed, 2.0f + 0.3f*distance) : speed;
            m_boat->steerTo(target, arriveSpeed);
        }
        break;

    case OrderSlot::Patrol:
        if (glm::length(Vector2D(m_point) - Vector2D(m_boat->position())) < ArriveDistance)
            pickPatrolPoint();
        m_point.z = altitude(m_point, m_pointHeight);
        m_boat->steerTo(m_point, speed);
        break;

    case OrderSlot::Escort:
        {
            SimpleBoat *escorted = nullptr;
            for (const auto &record : m_scenario->objectRecords())
                if (record.id == slot.esc)
                    escorted = dynamic_cast<SimpleBoat *>(record.object);
            if (!escorted || escorted->eventDestroy()->isCompleted())
            {
                next();
                return;
            }
            if (!escorted->isEnabled())
            {
                m_boat->hold();
                return;
            }

            const Vector3D &offset = slot.offset;
            Vector3D target = escorted->position() + escorted->right()*offset.x + escorted->dir()*offset.y + escorted->up()*offset.z;
            if (slot.mah > 0)
            {
                const float ground = m_scenario->surface().heightAt(target.x, target.y);
                target.z = glm::clamp(target.z, ground + slot.mih*scale.z, ground + slot.mah*scale.z);
            }

            const float distance = glm::length(target - m_boat->position());
            const float escortedSpeed = glm::length(escorted->velocity());
            if (distance < 5.0f)
                // in place: fly along with the escorted unit
                m_boat->steerTo(m_boat->position() + escorted->dir()*50.0f + Vector3D(0, 0, target.z - m_boat->position().z), escortedSpeed);
            else
                m_boat->steerTo(target, std::min(std::max(speed, escortedSpeed), escortedSpeed + 0.5f*distance));
        }
        break;

    default:
        // Wait, and the orders that are not implemented yet
        m_boat->hold();
    }
}


} // namespace fight
