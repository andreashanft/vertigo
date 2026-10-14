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

#include "activeobject.h"
#include "fight/scenario.h"
#include "sfx/samplemap.h"
#include "fight/effects/effectmanager.h"
#include "fight/weapons/unitgun.h"
#include "fight/orders.h"
#include "fight/weapons/torpedo.h"
#include "fight/boats/simpleboat.h"
#include "util/random.h"
#include <glm/geometric.hpp>
#include <algorithm>


namespace fight {


ActiveObject::ActiveObject(Scenario *scenario) :
    Object(scenario)
{
    m_shockShield = m_shockShieldMax = 10000000;
}


ActiveObject::ActiveObject(Scenario *scenario, txt::DesFile &file, const ObjectInfo &info) :
    Object(scenario, file),
    m_info(info),
    m_identified(false)
{
    file.setSection("noise");
    m_noise = file.value("level").toFloat();

    file.setSection("defense");
    m_kineticShield = m_kineticShieldMax = file.value("kineticshield0").toInt();
    m_shockShield = m_shockShieldMax = file.value("shockshield0", 10000000).toInt();
}


ActiveObject::~ActiveObject()
{
}


void ActiveObject::setOrders(Orders *orders)
{
    m_orders.reset(orders);
}


bool ActiveObject::fireTorpedo(ActiveObject *target, const Vector3D &position, const Vector3D &direction)
{
    const TorpedoType *type = TorpedoType::get(m_torpedoType);
    if (!type || m_torpedoCount <= 0 || m_scenario->time() < m_torpedoReady)
        return false;
    m_scenario->launchTorpedo(m_torpedoType, position, direction, glm::length(m_velocity), this, type->lockType != 0 ? target : nullptr);
    m_torpedoCount--;
    m_torpedoReady = m_scenario->time() + 6.0f;
    return true;
}


bool ActiveObject::update(float elapsedTime)
{
    if (m_orders)
        m_orders->update();
    return false;
}


void ActiveObject::damage(int kinetic, int shock, const Vector3D &position, ActiveObject *attacker)
{
    if (attacker && attacker != this)
        m_eventAttack.complete();
    const bool wasDestroyed = m_eventDestroy.isCompleted();
    m_kineticShield -= kinetic;
    m_shockShield -= shock;
    if (m_kineticShield < 0)
        destroy();
    else if (m_shockShield < 0)
        m_eventParalyze.complete();

    if (attacker && attacker != this && attacker == m_scenario->player())
        m_scenario->playerHit(this, !wasDestroyed && m_eventDestroy.isCompleted());
}


void ActiveObject::destroy()
{
    if (m_eventDestroy.isCompleted())
        return;

    // explosion sized to the object: small, medium or large flash
    const Vector3D dim = m_box.dim();
    const float size = std::max(dim.x, std::max(dim.y, dim.z));
    const Effects explosion = size < 8.0f ? Effects::Explosion_0 : size < 20.0f ? Effects::Explosion_1 : Effects::Explosion_2;
    m_scenario->effectManager().addEffect(explosion, center());
    const Vector3D at = center();
    sfx::SampleMap::playAt(size < 20.0f ? sfx::Sample::Explosion_Small : sfx::Sample::Explosion_Big, at.x, at.y, at.z);
    m_scenario->explosionAt(at, size < 8.0f ? 0.3f : size < 20.0f ? 0.6f : 1.0f);

    // debris (DEBRIS.DES): plates 0-2, engine 3, small parts 4-8, large parts 9-13, bodies 14-16
    auto random = [](int min, int max) { return min + util::random()%(max - min + 1); };
    auto direction = []() {
        const Vector3D d(util::random()/float(RAND_MAX) - 0.5f, util::random()/float(RAND_MAX) - 0.5f, 0.5f*util::random()/float(RAND_MAX));
        return glm::length(d) > 0.01f ? glm::normalize(d) : Vector3D(0, 0, 1);
    };
    auto debris = [this, &direction](int index) {
        m_scenario->effectManager().addDebris(Effects(int(Effects::Debris_0) + index), center(), direction());
    };
    const int pieces = size < 8.0f ? 4 : size < 20.0f ? 6 : 10;
    for (int i = 0; i < pieces; i++)
        debris(i % 3 == 0 ? random(0, 2) : size >= 8.0f && i % 3 == 1 ? random(9, 13) : random(4, 8));
    const SimpleBoat *vessel = dynamic_cast<const SimpleBoat *>(this);
    if (vessel)
    {
        debris(3);
        // crewed vessels
        if (!vessel->isGroundVehicle() || size >= 8.0f)
            for (int i = random(1, 3); i > 0; i--)
                debris(random(14, 16));
    }

    disable();
    m_eventDestroy.complete();
    if (m_scenario->target().locked() == this)
    {
        m_scenario->target().lockReset();
        sfx::SampleMap::get(sfx::Sample::TargetDestroyed).play();
    }
}


void ActiveObject::leaveMission()
{
    m_absent = true;
    disable();
}


void ActiveObject::identify()
{
    m_eventIdentify.complete();
    sfx::SampleMap::get(sfx::Sample::TargetIdentified).play();
}


} // namespace fight
