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

#include "bomber.h"
#include "fight/module.h"
#include "fight/orders.h"
#include "fight/scenario.h"
#include "fight/effects/effectmanager.h"
#include "fight/objects/mountedturret.h"
#include "sfx/samplemap.h"
#include "txt/desfile.h"
#include "util/random.h"
#include <glm/gtc/constants.hpp>


namespace fight {


Bomber::Bomber(Scenario *scenario, txt::DesFile &file, const ObjectInfo &info, float yaw, const String &desName) :
    SimpleBoat(scenario, file, info, yaw),
    m_bioBomber(desName.toLower() == "biobombr")
{
    // the turrets in order, up to the first missing section, at most 8: RelativePosition x and y on
    // the hull (model units), z +-1000: on top of it or under it, facing Alpha (counter-clockwise from
    // the bow); the bomber's faction
    ObjectInfo turretInfo = info;
    turretInfo.name = info.name;
    for (int i = 0; i < 8; i++)
    {
        file.setSection(String("Turret%1").arg(i));
        if (!file.contains("name"))
            break;
        const float x = file.value("RelativePositionX").toFloat();
        const float y = file.value("RelativePositionY").toFloat();
        const bool below = file.value("RelativePositionZ").toFloat() < 0;
        const float alpha = file.value("Alpha").toFloat();
        // the point of the hull under (or over) the mount
        const float sign = below ? -1.0f : 1.0f;
        float z = below ? m_base->box().minPoint().z : m_base->box().maxPoint().z;
        float distance;
        Vector3D normal;
        const float from = sign*(glm::abs(z) + 100.0f);
        if (m_base->intersect(Vector3D(x, y, from), Vector3D(0, 0, -sign), 0.0f, distance, normal))
            z = from - sign*distance;
        txt::DesFile turretFile(String("vfx:sobjects/%1.des").arg(file.value("name").toString()));
        m_turrets.push_back(new MountedTurret(scenario, this, turretFile, turretInfo, Vector3D(x, y, z)*m_scale, below, alpha));
    }
}


Bomber::~Bomber()
{
}


bool Bomber::update(float elapsedTime)
{
    if (m_waitingForTurrets)
    {
        // the hull hangs where it is until its last turret is gone, then blows up at both ends
        bool left = false;
        for (MountedTurret *turret : m_turrets)
        {
            if (!turret->isPresent())
                continue;
            turret->blowUpTurret();
            turret->think(elapsedTime);
            left = left || turret->isPresent();
        }
        if (left)
            return false;
        m_waitingForTurrets = false;
        const Vector3D toPlayer = m_scenario->player() ? m_scenario->player()->position() - m_position : Vector3D(0);
        const float radius = hullRadius();
        for (float end : { m_localBox.maxPoint().y, m_localBox.minPoint().y })
        {
            Vector3D at = m_position + m_orientationInverted*Vector3D(0, 0.5f*end, 0);
            const Vector3D way = m_scenario->player() ? m_scenario->player()->position() - at : toPlayer;
            if (glm::length(way) > 1e-3f)
                at += glm::normalize(way)*radius;
            m_scenario->effectManager().addEffect(Effects::Explosion_2, at);
            sfx::SampleMap::playAt(sfx::Sample::Explosion_Big, at.x, at.y, at.z);
        }
        SimpleBoat::wreckEnd();
        return false;
    }

    SimpleBoat::update(elapsedTime);
    for (MountedTurret *turret : m_turrets)
    {
        if (!turret->isPresent())
            continue;
        if (!turret->isEnabled())
            turret->enable();
        turret->think(elapsedTime);
    }
    return false;
}


void Bomber::orderStarted(const OrderSlot *slot)
{
    for (MountedTurret *turret : m_turrets)
        turret->setOrder(slot);
}


void Bomber::sleep()
{
    SimpleBoat::sleep();
    for (MountedTurret *turret : m_turrets)
        turret->reset();
}


bool Bomber::startTumbling()
{
    if (!SimpleBoat::startTumbling())
        return false;
    for (MountedTurret *turret : m_turrets)
        turret->paralyse();
    return true;
}


// half the turrets go up with the first blast, the rest with the fifth
void Bomber::wreckStepped(int step)
{
    const int half = int(m_turrets.size())/2;
    for (int i = 0; i < int(m_turrets.size()); i++)
        if ((step == 0 && i < half) || (step == 4 && i >= half))
            m_turrets[i]->blowUpTurret();
}


// other debris than a boat's, and none to three wreck bodies (each case one in four), none at all
// from the bio bomber
void Bomber::wreckLastDebris()
{
    const float fifth = glm::pi<float>()/5;
    for (int index : { 2, 17, 1, 3, 3 })
        wreckDebris(index, -fifth, fifth);
    if (m_bioBomber)
        return;
    const int roll = util::random() & 3;
    for (int body = 16 - roll; body <= 16 && roll < 3; body++)
        wreckDebris(body, 0, glm::half_pi<float>());
}


void Bomber::wreckEnd()
{
    m_waitingForTurrets = true;
}


} // namespace fight
