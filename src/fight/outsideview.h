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

#ifndef FIGHT_OUTSIDEVIEW_H
#define FIGHT_OUTSIDEVIEW_H


#include "util/geometry3d.h"


namespace fight {


class Scenario;
class ActiveObject;


// The views from outside the boat (docs/ROADMAP.md, appendix V): the chase view behind the
// player's boat or the locked target, the look-at view round it, and the death view (appendix R)
// once the player's boat is destroyed. The camera stays level, follows its place like a spring and
// looks at what it watches. Its distance and its place round the boat go in steps and are kept for
// the whole run, the same for the player's boat and the target.
class OutsideView
{
public:
    enum class Mode { Cockpit, Chase, LookAt, Death };

    // the distances, nearest first
    static const int ZoomSteps = 19;
    // a full turn round the boat, either way
    static const int TurnSteps = 16;

public:
    explicit OutsideView(Scenario *scenario);

public:
    Mode mode() const { return m_mode; }
    // the view watches the locked target instead of the player's boat
    bool watchesTarget() const { return m_target; }
    // back to the cockpit (Mode::Cockpit), or a view of the player's boat or of the target, which
    // the camera starts at; false (and nothing changes) for the target's without a locked unit
    bool show(Mode mode, bool target);
    // the player's boat is destroyed: the death view starts in its hull
    void startDeath();
    // nearer (positive) or further, as far as the steps go
    static void zoom(int steps);
    // the look-at view's camera round the boat: to the camera's left and up (positive)
    static void turn(int left, int up);
    // moves the camera towards its place; the target's views go back to the cockpit once the
    // target is gone or lost
    void update(float elapsedTime);

    // the camera, and where it is
    const Matrix& matrix() const { return m_matrix; }
    const Matrix& matrixInverted() const { return m_matrixInverted; }
    const Vector3D& eye() const { return m_eye; }

private:
    ActiveObject* subject() const;
    // where the camera belongs and the point it looks at
    void aim(Vector3D &place, Vector3D &at) const;
    void updateDeath(float elapsedTime);
    // looks at the point from the eye, level
    void lookAt(const Vector3D &at);

private:
    Scenario *m_scenario;
    Mode m_mode = Mode::Cockpit;
    bool m_target = false;
    Vector3D m_eye = Vector3D(0);
    // the last level heading and right, for a boat or a camera pointing straight up or down
    mutable Vector3D m_heading = Vector3D(0, 1, 0);
    Vector3D m_right = Vector3D(1, 0, 0);
    Matrix m_matrix = Matrix(1);
    Matrix m_matrixInverted = Matrix(1);
    // the death view's heading and elevation round the hull and its distance in hull radii
    float m_deathYaw = 0;
    float m_deathPitch = 0;
    float m_deathDistance = 0;
};


} // namespace fight


#endif // FIGHT_OUTSIDEVIEW_H
