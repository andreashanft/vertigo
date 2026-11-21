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


// How two units meet as spheres: fight::sphereContact

#include "check.hpp"

#include "fight/spherecontact.hpp"

#include <cmath>


namespace {

bool near(float actual, float expected, float tolerance = 1e-4f)
{
    return std::abs(actual - expected) <= tolerance;
}

bool near(const Vector3D &actual, const Vector3D &expected, float tolerance = 1e-4f)
{
    return glm::length(actual - expected) <= tolerance;
}

} // namespace


int main()
{
    const Vector3D east(1, 0, 0);
    float distance = -1;
    Vector3D normal(0);

    // a boat of 5.8 m heads for a mine of 1.6 m that is 20 m ahead: the two touch when their
    // centres are 7.4 m apart, and the boat is pushed back the way it came
    CHECK(fight::sphereContact(Vector3D(0), east, Vector3D(20, 0, 0), 7.4f, distance, normal));
    CHECK(near(distance, 12.6f));
    CHECK(near(normal, Vector3D(-1, 0, 0)));

    // the mine 5 m beside the boat's way: touched later and from the side
    CHECK(fight::sphereContact(Vector3D(0), east, Vector3D(20, 5, 0), 7.4f, distance, normal));
    CHECK(near(distance, 20 - std::sqrt(7.4f*7.4f - 25.0f)));
    CHECK(near(glm::length(normal), 1.0f));
    CHECK(near(normal.y, -5/7.4f));
    // the same above the boat: the height counts as the side does
    CHECK(fight::sphereContact(Vector3D(0), east, Vector3D(20, 0, 5), 7.4f, distance, normal));
    CHECK(near(normal.z, -5/7.4f));

    // 8 m beside the way the boat passes it, and a mine behind the boat is never met
    CHECK(!fight::sphereContact(Vector3D(0), east, Vector3D(20, 8, 0), 7.4f, distance, normal));
    CHECK(!fight::sphereContact(Vector3D(0), east, Vector3D(-20, 0, 0), 7.4f, distance, normal));

    // two that overlap already: a contact at once while the boat goes further in, none when it
    // moves out again
    CHECK(fight::sphereContact(Vector3D(0), east, Vector3D(5, 0, 0), 7.4f, distance, normal));
    CHECK(near(distance, 0.0f));
    CHECK(near(normal, Vector3D(-1, 0, 0)));
    CHECK(!fight::sphereContact(Vector3D(0), east, Vector3D(-5, 0, 0), 7.4f, distance, normal));

    // one centre on the other: away is back along the way
    CHECK(fight::sphereContact(Vector3D(1, 2, 3), east, Vector3D(1, 2, 3), 7.4f, distance, normal));
    CHECK(near(distance, 0.0f));
    CHECK(near(normal, Vector3D(-1, 0, 0)));

    return test::result();
}
