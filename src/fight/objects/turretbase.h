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
#include "turret.h"
#include <memory>
#include <vector>
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
    // tracks the point (docs/ROADMAP.md, appendix J): within 90 degrees of the centre if bounded,
    // else the turret turns back to the centre; the guns from 5 degrees down to straight up
    void track(const Vector3D &point, bool bounded = false, float centre = 0);
    void aimAt(const Vector3D &point) override { track(point); }
    // turns to the yaw, the guns level (from 15 degrees down to straight up when told a pitch)
    void aimAt(float yaw, float pitch = 0) { m_targetYaw = yaw; m_targetPitch = pitch; m_tracking = false; }
    // the turret has got there: within a degree (half a degree turning at a fixed rate)
    bool arrived() const;
    bool isTorpedoTurret() const { return m_torpedoTurret; }
    // where the next torpedo leaves the turret ([TorpedoMagazine] RelativeN in turn), world
    Vector3D nextTube();

    // the direction the turret points to
    Vector3D aim() const;
    // between the muzzles (the pivot)
    Vector3D muzzles() const { return pivot(); }

private:
    // an energy defence's collapse: a gun tower turns its gun to the front and lowers it, a
    // torpedo tower stops launching, for good (docs/ROADMAP.md, appendix J)
    bool startTumbling() override;
    // the point the turret turns around (world coordinates)
    Vector3D pivot() const { return m_position + m_turretPosition*m_scale; }

private:
    std::unique_ptr<Turret> m_body;
    Vector3D m_turretPosition; // model units
    String m_turretName;
    float m_yaw;               // radians, 0 is +y, increasing to the right
    float m_pitch;
    float m_targetYaw;
    float m_targetPitch;
    bool m_tracking = false;
    // the turret's turning (docs/ROADMAP.md, appendix J): soft, an angular acceleration of
    // MaxAngleAcceleration times the error over pi damped by AngleFriction, or hard, at
    // MaxAngleVelocity right onto the angle; tracking by SoftBodyAdjust and SoftArmRAdjust, turns to
    // an angle always soft
    float m_yawRate = 0, m_pitchRate = 0;
    float m_maxYawRate = 1, m_maxPitchRate = 1;
    float m_yawAcceleration = 0, m_pitchAcceleration = 0;
    float m_yawFriction = 0, m_pitchFriction = 0;
    bool m_softYaw = true, m_softPitch = true;
    bool m_torpedoTurret = false;     // turns only about the vertical
    std::unique_ptr<UnitGun> m_armGun;  // the second arm's gun (the first is m_unitGun)
    Vector3D m_armOffset[2];          // the arms' offsets from the pivot (model units)
    std::vector<Vector3D> m_tubes;    // model units
    int m_nextTube = 0;
};


} // namespace fight


#endif // FIGHT_TURRETBASE_H
