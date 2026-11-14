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

#ifndef FIGHT_POSEAHEAD_HPP
#define FIGHT_POSEAHEAD_HPP

#include "util/geometry3d.h"

#include <glm/gtc/quaternion.hpp>

namespace fight {


// A camera's place and heading a part of a step past the mission's last step (StepClock::ahead()):
// it goes on along the way it came in that step and turns on as it turned. The poses are the
// camera's inverted matrices, its axes and its place, before the step and after it. With no part
// of a step the pose is the one after the step, bit for bit.
inline Matrix poseAhead(const Matrix &before, const Matrix &after, float steps)
{
    if (steps == 0)
        return after;
    const glm::quat from = glm::quat_cast(glm::mat3(before));
    const glm::quat to = glm::quat_cast(glm::mat3(after));
    // (beyond 1 the turn from the one to the other goes on)
    Matrix pose(glm::mat3_cast(glm::normalize(glm::slerp(from, to, 1 + steps))));
    pose[3] = after[3] + (after[3] - before[3])*steps;
    return pose;
}


} // namespace fight

#endif // FIGHT_POSEAHEAD_HPP
