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

#ifndef FIGHT_HEADMOTION_H
#define FIGHT_HEADMOTION_H


#include "util/geometry3d.h"


namespace fight {


// The pilot's head moving inside the cockpit: held by a spring (the neck), it lags behind the
// boat's accelerations and turns, is thrown about when the boat bounces off something and
// rattles on hits and nearby explosions. Only the 3D cockpit shows it, as parallax between the
// instruments and the window; the simulation never looks at it.
// Everything is in the boat's camera frame (x right, y up, -z forward), in metres and radians.
class HeadMotion
{
public:
    // 0 turns the effect off, 1 is the default
    void setStrength(float strength) { m_strength = strength; }
    float strength() const { return m_strength; }

    // the boat's velocity changed abruptly (world frame, m/s), e.g. bouncing off a rock
    void jolt(const Vector3D &velocityChange);
    // a short rattle; strength 0..1 (a torpedo hit is 1)
    void shake(float strength);

    // basis: the boat's camera axes in world coordinates (right, up, -dir)
    void update(float elapsedTime, const Vector3D &velocity, const glm::mat3 &basis);
    void reset();

    const Vector3D& offset() const { return m_offset; }
    // pitch (about x), yaw (about y), roll (about z)
    const Vector3D& angles() const { return m_angles; }
    // the head's pose in the boat's camera frame
    Matrix transform() const;

private:
    float m_strength = 1;
    float m_time = 0;
    bool m_first = true;
    Vector3D m_lastVelocity;
    glm::mat3 m_lastBasis;
    Vector3D m_lastTurn;

    Vector3D m_position;        // spring-mounted head, translation
    Vector3D m_velocity;
    Vector3D m_rotation;        // and rotation (pitch, yaw, roll)
    Vector3D m_rotationVelocity;
    float m_rattle = 0;         // envelope of the rattle

    Vector3D m_offset;          // position and rotation including the rattle
    Vector3D m_angles;
};


} // namespace fight


#endif // FIGHT_HEADMOTION_H
