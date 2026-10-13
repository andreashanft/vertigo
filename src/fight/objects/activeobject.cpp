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


void ActiveObject::damage(int kinetic, int shock, const Vector3D &position, ActiveObject *attacker)
{
    if (attacker && attacker != this)
        m_eventAttack.complete();
    m_kineticShield -= kinetic;
    m_shockShield -= shock;
    if (m_kineticShield < 0)
    {
        destroy();
        return;
    }
    if (m_shockShield < 0)
    {
        m_eventParalyze.complete();
    }
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
    sfx::SampleMap::get(size < 20.0f ? sfx::Sample::Explosion_Small : sfx::Sample::Explosion_Big).playInstance();

    disable();
    m_eventDestroy.complete();
    if (m_scenario->target().locked() == this)
    {
        m_scenario->target().lockReset();
        sfx::SampleMap::get(sfx::Sample::TargetDestroyed).play();
    }
}


void ActiveObject::identify()
{
    m_eventIdentify.complete();
    sfx::SampleMap::get(sfx::Sample::TargetIdentified).play();
}


} // namespace fight
