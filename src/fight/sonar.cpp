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

#include "sonar.h"
#include "scenario.h"
#include "objects/activeobject.h"
#include "objects/navpoint.h"
#include "weapons/torpedo.h"
#include "sfx/samplemap.h"

#include <glm/geometric.hpp>


namespace fight {


Sonar::Sonar(Scenario *scenario) :
    m_scenario(scenario),
    m_active(false),
    m_activatingDelay(0)
{
}


// what each sonar can do (docs/ROADMAP.md, appendix P): Spy I tells no friend from foe and shows
// no armor, weapons or activity of the target; Spy II tells friend from foe and shows the armor;
// Spy III and IV show everything. The ranges are not compared yet.
void Sonar::init(int sensor)
{
    const int grade = sensor >= 6145 && sensor <= 6148 ? sensor - 6144 : 1;
    m_delay = 14 - 2*grade;
    m_passiveRange = 140.0f + 20.0f*grade;
    m_passiveRangeMult = 10.0f*grade + 10.0f;
    m_activeRange = 160.0f;
    m_activeRangeMult = 20.0f;
    m_iff = grade >= 2;
    m_detectArmor = grade >= 2;
    m_detectWeapons = grade >= 3;
    m_detectActivity = grade >= 3;
}


void Sonar::activate()
{
    m_activatingDelay = m_delay;
}


void Sonar::deactivate()
{
    m_active = false;
    m_activatingDelay = 0;
    sfx::SampleMap::get(sfx::Sample::SonarDeactivated).play();
}


void Sonar::toggle()
{
    if (!m_active && m_activatingDelay <= 0)
        activate();
    else
        deactivate();
}


float Sonar::passiveRange(const ActiveObject *object) const
{
    return m_passiveRange + m_passiveRangeMult*object->noise();
}


float Sonar::activeRange(const ActiveObject *object) const
{
    return m_activeRange + m_activeRangeMult*object->noise();
}


bool Sonar::torpedoesBright() const
{
    return static_cast<int>(m_scenario->time()/0.15f)%2 == 0;
}


void Sonar::update(float elapsedTime)
{
    if (!m_active && m_activatingDelay > 0)
    {
        m_activatingDelay -= elapsedTime;
        if (m_activatingDelay <= 0)
        {
            m_active = true;
            sfx::SampleMap::get(sfx::Sample::SonarActivated).play();
        }
    }

    Target &target = m_scenario->target();
    bool lockNotLost = !target.isLocked();
    if (target.lockedNavPoint() != nullptr)
    {
        lockNotLost = true;
        target.update(glm::distance(target.lockedNavPoint()->position(), m_scenario->position()), true);
    }

    m_sonarEntries.clear();
    for (const auto &object_ptr : *m_scenario)
    {
        ActiveObject *object = dynamic_cast<ActiveObject *>(object_ptr.get());
        if (!object || !object->isEnabled() || !object->isContact())
            continue;
        if (!m_active && object->noise() <= 0.1f)
            continue;

        float passiveRange = this->passiveRange(object);
        float activeRange = this->activeRange(object);
        Vector3D dir = object->center() - m_scenario->position();
        float dist = glm::length(dir);
        bool passive = dist > activeRange || !m_active;
        if (dist > passiveRange && passive)
            continue;
        if (target.locked() == object)
        {
            lockNotLost = true;
            target.update(dist, passive);
            if (m_active && !object->isIdentified() && dist <= 35.0f)
                object->identify();
        }

        m_sonarEntries.emplace_back();
        SonarEntry &entry = m_sonarEntries.back();
        entry.object = object;
        entry.distance = dist;
        entry.isFriend = !m_iff || m_scenario->isFriend(Scenario::PlayerFaction, object->iff());
        entry.isNeutral = m_iff && !entry.isFriend && !m_scenario->isHostile(Scenario::PlayerFaction, object->iff());
        entry.isPassive = passive;
        entry.isTorpedo = false;
    }

    // every torpedo on its way is a contact, friend or foe, without being detected (KNOWLEDGE.md
    // 9.11); one that has gone off is no longer on its way, but stays the locked target while it
    // explodes
    for (const auto &torpedo : m_scenario->torpedoes())
    {
        if (torpedo->isDone() || (torpedo->isBursting() && target.locked() != torpedo.get()))
            continue;
        const float dist = glm::length(torpedo->center() - m_scenario->position());
        // a locked torpedo stays locked while it is on its way
        if (target.locked() == torpedo.get())
        {
            lockNotLost = true;
            target.update(dist, false);
        }
        m_sonarEntries.emplace_back();
        SonarEntry &entry = m_sonarEntries.back();
        entry.object = torpedo.get();
        entry.distance = dist;
        entry.isFriend = !m_iff || m_scenario->isFriend(Scenario::PlayerFaction, torpedo->iff());
        entry.isNeutral = false;
        entry.isPassive = false;
        entry.isTorpedo = true;
    }

    // a locked contact the radars no longer show (Alt+T, Alt+S) is lost as well
    for (const SonarEntry &entry : m_sonarEntries)
        if (entry.object == target.locked() && !isShown(entry))
            lockNotLost = false;

    if (!lockNotLost)
    {
        target.lockReset();
        sfx::SampleMap::get(sfx::Sample::TargetLost).play();
    }
}


bool Sonar::isShown(const SonarEntry &entry) const
{
    if (entry.isTorpedo)
        return !m_scenario->isFriend(Scenario::PlayerFaction, entry.object->iff()) || (m_friendlyTorpedoes && !m_onlyEnemies);
    return !m_onlyEnemies || (!entry.isFriend && !entry.isNeutral);
}


} // namespace fight
