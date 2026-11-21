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


#ifndef FIGHT_SPHERECONTACT_HPP
#define FIGHT_SPHERECONTACT_HPP

#include "util/geometry3d.h"

#include <glm/geometric.hpp>
#include <algorithm>
#include <cmath>

namespace fight {

// Where a sphere that moves from start along dir (a unit vector) first touches a resting sphere
// about centre; reach is the two radii together. distance takes how far the moving sphere goes
// until then (0 if the two overlap already and it goes further in), normal the direction away
// from the resting sphere at the contact. False if the moving sphere never touches the other on
// its line, or overlaps it already and moves out of it.
inline bool sphereContact(const Vector3D &start, const Vector3D &dir, const Vector3D &centre, float reach, float &distance, Vector3D &normal)
{
    const Vector3D from = start - centre;
    const float along = glm::dot(dir, from);
    const float discriminant = along*along - (glm::dot(from, from) - reach*reach);
    if (discriminant < 0)
        return false;
    const float root = std::sqrt(discriminant);
    const float in = -along - root, out = -along + root;
    if (out < 0 || (in < 0 && out < -in))
        return false;
    distance = std::max(in, 0.0f);
    const Vector3D apart = from + dir*distance;
    normal = glm::dot(apart, apart) > 0 ? glm::normalize(apart) : -dir;
    return true;
}

} // namespace fight

#endif // FIGHT_SPHERECONTACT_HPP
