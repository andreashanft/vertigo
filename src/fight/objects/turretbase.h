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

#ifndef FIGHT_TURRETBASE_H
#define FIGHT_TURRETBASE_H


#include "simpleobject.h"
#include "util/string.hpp"


namespace fight {


class TurretBase : public SimpleObject
{
public:
    TurretBase(Scenario *scenario, txt::DesFile &file, const ObjectInfo &info);

public:
    void draw();
    bool update(float elapsedTime);
    bool intersect(const Vector3D &start, const Vector3D &dir, float radius, float &distance, Vector3D &normal);
    void arm(int model, txt::DesFile &file);
    void aimAt(const Vector3D &point);
    void aimAt(float yaw) { m_targetYaw = yaw; m_targetPitch = 0; }

    // the direction the turret points to
    Vector3D aim() const;

private:
    // an energy defence's collapse: a gun tower turns its gun to the front and lowers it, a
    // torpedo tower stops launching, for good (docs/ROADMAP.md, appendix J)
    bool startTumbling() override;
    // the point the turret turns around (world coordinates)
    Vector3D pivot() const { return m_position + m_turretPosition*m_scale; }

private:
    std::unique_ptr<Object> m_body;
    Vector3D m_turretPosition; // model units
    String m_turretName;
    float m_yaw;               // radians, 0 is +y, increasing to the right
    float m_pitch;
    float m_targetYaw;
    float m_targetPitch;
    float m_yawVelocity;       // the turret's turn rates (rad/s)
    float m_pitchVelocity;
};


} // namespace fight


#endif // FIGHT_TURRETBASE_H
