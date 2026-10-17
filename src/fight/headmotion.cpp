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

#include "headmotion.h"

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtx/transform.hpp>
#include <algorithm>
#include <cmath>


namespace fight {


namespace {

// the neck: about 1.4 Hz for the head's position, 1.8 Hz for its rotation, both well damped
const float PositionOmega = 2*glm::pi<float>()*1.4f;
const float PositionDamping = 0.45f;
const float RotationOmega = 2*glm::pi<float>()*1.8f;
const float RotationDamping = 0.4f;

// how far the head lags behind accelerations: full throttle (about 7 m/s²) moves it 1 cm
const float LinearLag = 0.12f;
const float MaxAcceleration = 8.0f;     // m/s², larger ones are collisions and come as jolts
// the turn rates change in steps (the boat relaxes them 30 times a second, appendix L of the
// roadmap), so the rate is smoothed over this time before the head reacts to its change
const float AngularLag = 0.35f;
const float MaxAngularAcceleration = 3.0f;
const float TurnSmoothing = 0.1f;

// bouncing off something at 20 km/h (a velocity change of about 6 m/s) throws the head 3 cm
const float JoltPosition = 0.07f;
const float JoltNod = 0.12f;
const float JoltRoll = 0.08f;

// the rattle: a few millimetres and about a degree, fading in a third of a second
const float RattlePosition = 0.008f;
const float RattleAngle = glm::radians(1.0f);
const float RattleDecay = 0.35f;

const float MaxOffset = 0.08f;
const float MaxAngle = 0.1f;

// smooth noise in -1..1 for the rattle
float noise(float t, float seed)
{
    const float pi2 = 2*glm::pi<float>();
    return (std::sin(pi2*11.3f*t + seed) + 0.6f*std::sin(pi2*17.9f*t + 2*seed) + 0.4f*std::sin(pi2*29.1f*t + 3*seed))/2.0f;
}

Vector3D clampLength(const Vector3D &v, float max)
{
    const float length = glm::length(v);
    return length > max ? v*(max/length) : v;
}

}


void HeadMotion::jolt(const Vector3D &velocityChange)
{
    // into the camera frame; the head keeps going while the boat is stopped or pushed
    const Vector3D change = glm::transpose(m_lastBasis)*velocityChange;
    m_velocity -= change*JoltPosition;
    // hitting something in front nods the head forward, from the side tilts it
    m_rotationVelocity.x -= (change.z + 0.4f*change.y)*JoltNod;
    m_rotationVelocity.z += change.x*JoltRoll;
    shake(std::min(1.0f, glm::length(velocityChange)/10.0f));
}


void HeadMotion::shake(float strength)
{
    m_rattle = std::min(1.5f, std::max(m_rattle, strength));
}


void HeadMotion::reset()
{
    const float strength = m_strength;
    *this = HeadMotion();
    m_strength = strength;
}


void HeadMotion::update(float elapsedTime, const Vector3D &velocity, const glm::mat3 &basis)
{
    if (elapsedTime <= 0)
        return;
    m_time += elapsedTime;

    // the boat's turn rate from the change of its axes (camera frame)
    const glm::mat3 delta = glm::transpose(m_lastBasis)*basis;
    const Vector3D rawTurn = m_first ? Vector3D(0) :
        Vector3D(delta[1][2] - delta[2][1], delta[2][0] - delta[0][2], delta[0][1] - delta[1][0])/(2*elapsedTime);
    const Vector3D turn = m_first ? rawTurn : m_lastTurn + (rawTurn - m_lastTurn)*std::min(1.0f, elapsedTime/TurnSmoothing);
    Vector3D acceleration(0), angularAcceleration(0);
    if (!m_first)
    {
        acceleration = clampLength(glm::transpose(basis)*(velocity - m_lastVelocity)/elapsedTime, MaxAcceleration);
        angularAcceleration = clampLength((turn - m_lastTurn)/elapsedTime, MaxAngularAcceleration);
    }
    m_first = false;
    m_lastVelocity = velocity;
    m_lastBasis = basis;
    m_lastTurn = turn;

    // semi-implicit Euler is stable here: omega*dt stays far below 1 at 50 steps per second
    const Vector3D force = -PositionOmega*PositionOmega*m_position - 2*PositionDamping*PositionOmega*m_velocity - acceleration*LinearLag;
    m_velocity += force*elapsedTime;
    m_position = clampLength(m_position + m_velocity*elapsedTime, MaxOffset);

    const Vector3D torque = -RotationOmega*RotationOmega*m_rotation - 2*RotationDamping*RotationOmega*m_rotationVelocity - angularAcceleration*AngularLag;
    m_rotationVelocity += torque*elapsedTime;
    m_rotation = clampLength(m_rotation + m_rotationVelocity*elapsedTime, MaxAngle);

    m_rattle *= std::exp(-elapsedTime/RattleDecay);
    const Vector3D rattlePosition = m_rattle*RattlePosition*Vector3D(noise(m_time, 0.0f), noise(m_time, 1.7f), noise(m_time, 3.1f));
    const Vector3D rattleAngles = m_rattle*RattleAngle*Vector3D(noise(m_time, 4.3f), noise(m_time, 5.9f), noise(m_time, 7.7f));

    m_offset = m_strength*clampLength(m_position + rattlePosition, MaxOffset);
    m_angles = m_strength*clampLength(m_rotation + rattleAngles, MaxAngle);
}


Matrix HeadMotion::transform() const
{
    return glm::translate(m_offset) * glm::rotate(m_angles.y, Vector3D(0, 1, 0)) * glm::rotate(m_angles.x, Vector3D(1, 0, 0)) * glm::rotate(m_angles.z, Vector3D(0, 0, 1));
}


} // namespace fight
