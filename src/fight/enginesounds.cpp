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

#include "enginesounds.h"
#include "scenario.h"
#include "boats/simpleboat.h"
#include "sfx/samplemap.h"

#include <glm/geometric.hpp>
#include <algorithm>
#include <vector>


namespace fight {


namespace {

// how many engines are heard at once, and how far
const size_t MaxEngines = 4;
const float MaxDistance = 300.0f;

}


EngineSounds::EngineSounds(Scenario *scenario) :
    m_scenario(scenario)
{
}


EngineSounds::~EngineSounds()
{
    for (auto &playing : m_playing)
        playing.second->stop(); // deletes the copy
}


void EngineSounds::update()
{
    // the nearest moving units, with the engine sound of their class
    struct Candidate
    {
        const Object *object;
        sfx::Sample sample;
        float distance;
    };
    std::vector<Candidate> candidates;
    for (const auto &record : m_scenario->objectRecords())
    {
        const Object *object = record.object;
        if (!object->isEnabled())
            continue;
        // a sleeping unit's engine is silent
        if (const ActiveObject *unit = dynamic_cast<const ActiveObject *>(object))
            if (unit->isAsleep())
                continue;
        sfx::Sample sample;
        if (record.type == 2063)
            sample = sfx::Sample::Engine_BigShip; // cargo ships
        else if (!dynamic_cast<const SimpleBoat *>(object))
            continue;
        else if (record.des.startsWith("bio"))
            sample = sfx::Sample::Engine_Biont;
        else if (record.type == 2049)
            sample = sfx::Sample::Engine_Enemy1;
        else
            sample = sfx::Sample::Engine_Enemy2; // bombers, tanks, crawlers
        const float distance = glm::length(object->position() - m_scenario->eye());
        if (distance < MaxDistance)
            candidates.push_back({ object, sample, distance });
    }
    std::sort(candidates.begin(), candidates.end(), [](const Candidate &a, const Candidate &b) { return a.distance < b.distance; });
    if (candidates.size() > MaxEngines)
        candidates.resize(MaxEngines);

    // stop the engines that are no longer among them
    for (auto it = m_playing.begin(); it != m_playing.end(); )
    {
        const bool near = std::any_of(candidates.begin(), candidates.end(), [&it](const Candidate &c) { return c.object == it->first; });
        if (near)
        {
            ++it;
            continue;
        }
        it->second->stop();
        it = m_playing.erase(it);
    }

    for (const Candidate &candidate : candidates)
    {
        const Vector3D &position = candidate.object->position();
        auto it = m_playing.find(candidate.object);
        if (it == m_playing.end())
        {
            sfx::Sound *sound = sfx::SampleMap::loopAt(candidate.sample, position.x, position.y, position.z);
            if (!sound)
                continue;
            it = m_playing.emplace(candidate.object, sound).first;
        }
        it->second->setPosition(position.x, position.y, position.z);
        // higher with speed, as for the player's engine
        float speed = 0;
        if (const ActiveObject *unit = dynamic_cast<const ActiveObject *>(candidate.object))
            speed = glm::length(unit->velocity());
        it->second->setPitch(1.0f + std::min(speed*0.036f, 1.0f));
    }
}


} // namespace fight
