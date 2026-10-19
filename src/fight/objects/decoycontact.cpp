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

#include "decoycontact.h"
#include "fight/scenario.h"


namespace fight {


DecoyContact::DecoyContact(Scenario *scenario, const Particles::DecoyInfo &decoy) :
    ActiveObject(scenario),
    m_decoy(decoy.id),
    m_kind(decoy.kind)
{
    m_info.iff = decoy.faction;
    m_info.name = isTorch() ? "Torch" : "Buzzer";
    m_noise = decoy.noise;
    m_kineticShield = m_kineticShieldMax = 1;
    m_velocity = Vector3D(0);
    follow();
}


bool DecoyContact::follow()
{
    if (!isEnabled())
        return false;
    if (!m_scenario->decoy(m_decoy, m_position, m_velocity))
    {
        disable();
        return false;
    }
    return true;
}


} // namespace fight
