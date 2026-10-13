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

#include "debris.h"
#include "billboard.h"
#include "fight/scenario.h"


namespace fight {


namespace {

// Lift in DEBRIS.DES is negative (the pieces sink); this turns it into m/s^2
const float LiftScale = 4.0f;

}


Debris::Debris(Scenario *scenario, Billboard *billboard, const Vector3D &velocity) :
    Effect(scenario, billboard, qrand()%360, 1)
{
    m_velocity = velocity;
    m_permanent = true; // the animation loops; the piece lasts for its LifeTime
}


bool Debris::update(float elapsedTime)
{
    m_time += elapsedTime;
    if (m_time >= m_billboard->lifeTime())
    {
        disable();
        return true;
    }

    m_velocity += (-m_billboard->friction()*m_velocity + Vector3D(0, 0, m_billboard->lift()*LiftScale))*elapsedTime;
    m_position += m_velocity*elapsedTime;

    // settle on the ground
    const float ground = m_scenario->surface().heightAt(m_position.x, m_position.y) + 0.3f;
    if (m_position.z < ground)
    {
        m_position.z = ground;
        m_velocity = Vector3D(0);
    }
    return false;
}


} // namespace fight
