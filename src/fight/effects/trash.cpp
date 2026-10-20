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

#include "trash.h"
#include "billboard.h"
#include "fight/scenario.h"
#include "util/random.h"
#include "sfx/samplemap.h"

#include <glm/gtx/norm.hpp>


namespace fight {


Effects Trash::trashCollection[9] = { Effects::Trash_0, Effects::Trash_0, Effects::Trash_1, Effects::Trash_1, Effects::Trash_2, Effects::Trash_2, Effects::Trash_3, Effects::Trash_3, Effects::Trash_4 };


Trash::Trash(Scenario *scenario, Billboard *billboard, const Vector3D &position) :
    // between half and the full size
    Effect(scenario, billboard, static_cast<float>(util::random()%360), 0.5f + (util::random()%1001)/2000.0f)
{
    m_noise = billboard->noiseLevel();
    
    m_kineticShield = m_kineticShieldMax = billboard->kineticShield();
    m_kineticStrength = billboard->kineticStrength();
    m_shockStrength = billboard->shockStrength();

    m_scenario->collisionManager().addObject(this);
    m_box = m_billboard->box();

    Vector3D pos = position + Vector3D(util::random()%50 - 25, util::random()%50 - 25, util::random()%25 - 25);
    float height = m_scenario->surface().heightAt(pos.x, pos.y) + 5;
    if (pos.z < height)
        pos.z = height;
    Object::setPosition(pos);
}


bool Trash::intersect(const Vector3D &start, const Vector3D &dir, float radius, float &distance, Vector3D &normal)
{
    if (!m_billboard->intersect(start - m_position, dir, distance))
        return false;
    // the billboard collides like a sphere around its center
    const Vector3D hit = start + dir*distance - m_position;
    normal = glm::length2(hit) > 0 ? glm::normalize(hit) : -glm::normalize(dir);
    return true;
}


// trash goes out in a puff and leaves no debris (docs/ROADMAP.md, appendix Q)
void Trash::destroy()
{
    if (m_eventDestroy.isCompleted())
        return;
    m_scenario->effectManager().addEffect(Effects::Explosion_5, m_position, 0, glm::length2(m_box.dim()) > 15 ? 2 : 1);
    sfx::SampleMap::playAt(sfx::Sample::Explosion_Small, m_position.x, m_position.y, m_position.z);
    vanish();
    countDestroyed();
}


} // namespace fight
