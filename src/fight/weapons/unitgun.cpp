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

#include "unitgun.h"
#include "txt/desfile.h"


namespace fight {


UnitGun::UnitGun(Scenario *scenario, ActiveObject *owner, int model, const std::vector<Vector3D> &muzzles, txt::DesFile *volleyFile) :
    m_gun(scenario, model, owner),
    m_phase(0),
    m_phaseTime(0)
{
    for (const Vector3D &muzzle : muzzles)
        m_gun.addMuzzle(muzzle);

    if (volleyFile)
    {
        volleyFile->setSection("volleygun");
        for (int i = 0; volleyFile->contains(String("state%1").arg(i)); i++)
        {
            const bool active = volleyFile->value(String("state%1").arg(i)).toString().trimmed().toLower() == "a";
            const float duration = volleyFile->value(String("duration%1").arg(i)).toFloat();
            volleyFile->value(String("tolerance%1").arg(i));
            if (duration > 0)
                m_volley.push_back({ active, duration });
        }
    }
}


UnitGun::Mount UnitGun::readMount(txt::DesFile &file)
{
    Mount mount;
    file.setSection("staticgun");
    if (!file.contains("model"))
        return mount;
    mount.model = file.value("model").toInt();
    mount.ammunition = file.value("ammunition", -1).toInt();
    for (int i = 0; file.contains(String("relative%1x").arg(i)); i++)
        mount.muzzles.push_back(Vector3D(file.value(String("relative%1x").arg(i)).toFloat(),
                                         file.value(String("relative%1y").arg(i)).toFloat(),
                                         file.value(String("relative%1z").arg(i)).toFloat()));
    return mount;
}


void UnitGun::update(float elapsedTime, bool trigger, const Vector3D &position, const Vector3D &dir, const Vector3D &up, const Vector3D &right)
{
    if (m_countFirst)
        m_gun.update(elapsedTime);
    if (trigger)
    {
        bool active = true;
        if (!m_volley.empty())
        {
            m_phaseTime += elapsedTime;
            while (m_phaseTime >= m_volley[m_phase].duration)
            {
                m_phaseTime -= m_volley[m_phase].duration;
                m_phase = (m_phase + 1) % m_volley.size();
            }
            active = m_volley[m_phase].active;
        }
        if (active && m_gun.ready())
            m_gun.fire(position, dir, up, right);
    }
    if (!m_countFirst)
        m_gun.update(elapsedTime);
}


} // namespace fight
