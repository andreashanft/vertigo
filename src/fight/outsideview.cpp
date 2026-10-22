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

#include "outsideview.h"
#include "scenario.h"
#include "boats/player.h"

#include <glm/gtc/constants.hpp>
#include <glm/trigonometric.hpp>
#include <algorithm>
#include <cmath>


namespace fight {


namespace {

// the camera's distances from what it watches, in its radii (half its longest side): ZoomSteps
// of them, evenly apart
const float ZoomNear = 1.0f;
const float ZoomFar = 9.7f;
// the chase view's camera is behind the boat and this high above its course, not rolling with
// it, and looks at the course this far ahead of the boat (radii)
const float ChaseElevation = glm::radians(20.0f);
const float ChaseAhead = 0.9f;
// the look-at view's camera is a little nearer than the chase view's at the same zoom
const float LookAtNearer = 0.92f;
const float TurnStep = glm::two_pi<float>()/OutsideView::TurnSteps;
// the camera moves this share of the way to its place in a second
const float Follow = 10.0f;

// kept for the whole run: the zoom (0 the nearest, 4 at first) and the look-at view's place round
// the boat, steps to the camera's left (clockwise seen from above) from the boat's front and up
int g_zoom = 4;
int g_left = 0;
int g_up = 2;

} // namespace


OutsideView::OutsideView(Scenario *scenario) :
    m_scenario(scenario)
{
}


bool OutsideView::show(Mode mode, bool target)
{
    if (mode == Mode::Death || m_mode == Mode::Death)
        return false;
    if (mode != Mode::Cockpit && target && !m_scenario->target().locked())
        return false;
    m_mode = mode;
    m_target = mode != Mode::Cockpit && target;
    if (mode == Mode::Cockpit)
        return true;
    // at its place at once
    Vector3D place, at;
    aim(place, at);
    m_eye = place;
    lookAt(at);
    return true;
}


void OutsideView::startDeath()
{
    // the camera starts in the hull and moves out to its place beside it
    m_mode = Mode::Death;
    m_target = false;
    m_eye = m_scenario->player()->position();
    m_deathYaw = -glm::half_pi<float>();
    m_deathPitch = glm::quarter_pi<float>();
    m_deathDistance = 3.0f;
}


void OutsideView::zoom(int steps)
{
    g_zoom = std::clamp(g_zoom - steps, 0, ZoomSteps - 1);
}


void OutsideView::turn(int left, int up)
{
    g_left = ((g_left + left)%TurnSteps + TurnSteps)%TurnSteps;
    g_up = ((g_up + up)%TurnSteps + TurnSteps)%TurnSteps;
}


ActiveObject* OutsideView::subject() const
{
    if (m_target)
        return m_scenario->target().locked();
    return m_scenario->player();
}


void OutsideView::aim(Vector3D &place, Vector3D &at) const
{
    const ActiveObject *unit = subject();
    const SimpleBoat *boat = dynamic_cast<const SimpleBoat *>(unit);
    const Vector3D middle = boat ? boat->position() : unit->center();
    const Vector3D size = unit->box().dim();
    const float radius = boat ? boat->hullRadius() : 0.5f*std::max(size.x, std::max(size.y, size.z));
    const float distance = radius*(ZoomNear + (ZoomFar - ZoomNear)*g_zoom/float(ZoomSteps - 1));
    // a boat's nose, or for a tower or a building the way from the player's boat to it
    Vector3D dir = boat ? boat->dir() : middle - m_scenario->position();
    dir = glm::length(dir) > 1e-3f ? glm::normalize(dir) : m_heading;
    const Vector3D heading(dir.x, dir.y, 0);
    if (glm::length(heading) > 1e-3f)
        m_heading = glm::normalize(heading);

    if (m_mode == Mode::Chase)
    {
        // behind the boat on its course and above it, as if it didn't roll: past straight up
        // (looping) "above" is the other side
        Vector3D up = Vector3D(0, 0, 1) - dir*dir.z;
        up = glm::length(up) > 1e-3f ? glm::normalize(up) : -m_heading*glm::sign(dir.z);
        place = middle + distance*(std::sin(ChaseElevation)*up - std::cos(ChaseElevation)*dir);
        at = middle + dir*ChaseAhead*radius;
        return;
    }
    const Vector3D right(m_heading.y, -m_heading.x, 0);
    const float yaw = g_left*TurnStep;
    const float pitch = g_up*TurnStep;
    place = middle + LookAtNearer*distance*(std::cos(pitch)*(std::cos(yaw)*m_heading + std::sin(yaw)*right) + std::sin(pitch)*Vector3D(0, 0, 1));
    at = middle;
}


void OutsideView::update(float elapsedTime)
{
    if (m_mode == Mode::Cockpit)
        return;
    if (m_mode == Mode::Death)
    {
        updateDeath(elapsedTime);
        return;
    }
    if (!subject())
    {
        show(Mode::Cockpit, false);
        return;
    }
    Vector3D place, at;
    aim(place, at);
    m_eye += (place - m_eye)*std::min(1.0f, Follow*elapsedTime);
    lookAt(at);
}


// The death view (docs/ROADMAP.md, appendix R): the camera circles the hull at 45° a second, 45°
// above it and three hull radii away, in the hull's own frame, moving a tenth of a second's worth
// towards that place every frame and always looking at the hull. Once the hull is gone it comes
// down level and closes in to half a radius.
void OutsideView::updateDeath(float elapsedTime)
{
    const Player *player = m_scenario->player();
    m_deathYaw += glm::quarter_pi<float>()*elapsedTime;
    if (player->wreckGone())
    {
        m_deathPitch -= m_deathPitch*std::min(1.0f, 1.5f*elapsedTime);
        if (m_deathDistance > 0.5f)
            m_deathDistance -= 1.25f*elapsedTime;
    }
    const float d = m_deathDistance*player->hullRadius();
    const Vector3D offset(-d*std::cos(m_deathPitch)*std::sin(m_deathYaw), d*std::cos(m_deathPitch)*std::cos(m_deathYaw), d*std::sin(m_deathPitch));
    const Vector3D target = player->position();
    const Vector3D place = target + glm::transpose(player->orientation())*offset;
    m_eye += (place - m_eye)*std::min(1.0f, 10.0f*elapsedTime);
    lookAt(target);
}


void OutsideView::lookAt(const Vector3D &at)
{
    // (from the hull itself along the boat's nose)
    Vector3D forward = at - m_eye;
    forward = glm::length(forward) > 1e-3f ? glm::normalize(forward) : m_scenario->player()->dir();
    const Vector3D right = glm::cross(forward, Vector3D(0, 0, 1));
    if (glm::length(right) > 1e-3f)
        m_right = glm::normalize(right);
    const Vector3D up = glm::normalize(glm::cross(m_right, forward));
    m_matrixInverted = Matrix(Vector4D(glm::cross(forward, up), 0), Vector4D(up, 0), Vector4D(-forward, 0), Vector4D(m_eye, 1));
    m_matrix = glm::inverse(m_matrixInverted);
}


} // namespace fight
