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

#ifndef FIGHT_TANK_H
#define FIGHT_TANK_H


#include "simpleboat.h"


namespace fight {


class Module;
class Turret;


// A ground vehicle (tanks and crawlers): drives on the ground, tilts with it
// (at most MaxClimbingAngle) and steers by turning only. Its fixed Gun module tilts up and down
// towards what the orders aim at, a GunTurret turns towards it as well; both go back to the front
// when the orders stop aiming. Moving, it stirs up dust behind its tracks.
class Tank : public SimpleBoat
{
public:
    Tank(Scenario *scenario, txt::DesFile &file, const ObjectInfo &info, float yaw);
    ~Tank();

public:
    void setPosition(const Vector3D &pos);
    bool update(float elapsedTime);
    void draw();
    void arm(int model, txt::DesFile &file);
    bool isGroundVehicle() const { return true; }
    // a crawler never leaves its orders to fight (docs/ROADMAP.md, appendix J)
    bool engagesHostiles() const { return m_groundWreck != GroundWreck::Crawler; }
    // an energy defence's collapse: it brakes to a stop and stays there for good
    bool startTumbling() override;
    bool intersect(const Vector3D &start, const Vector3D &dir, float radius, float &distance, Vector3D &normal);
    void aimAt(const Vector3D &point);
    Vector3D gunDirection() const;
    // a GunTurret (TANK2) rather than a fixed gun module
    bool hasTurret() const { return m_turret != nullptr; }

private:
    float ground(float x, float y) const;
    // turns and speeds up or brakes as steerTo() and hold() want, along the ground
    void drive(float elapsedTime);
    void alignToGround();
    void updateTurret(float elapsedTime);
    void stirUpDust(float elapsedTime);
    // the turret's yaw and pitch applied to model coordinates
    glm::mat3 turretRotation() const;

private:
    float m_heading;          // radians, 0 is +y, increasing to the right
    float m_speed;            // m/s along the heading
    float m_groundOffset;     // from the ground to the model's origin

    Module *m_gun;            // a fixed gun model (Cluster/Gun) ...
    Vector3D m_gunOffset;
    std::unique_ptr<Turret> m_turret; // ... or a turret (GunTurret)
    String m_turretName;
    Vector3D m_pivot;         // what the turret turns around (model coordinates)
    float m_turretRate;       // rad/s
    float m_turretYaw;        // relative to the hull, positive to the right
    float m_turretPitch;
    float m_targetYaw;
    float m_targetPitch;
    float m_aimHold;          // seconds until the turret returns to the front
    float m_dustTime;         // until the next puff of dust
    float m_paralysedEnd = 0; // when a paralysed one stops for good
    Vector4D m_corners;       // the ground under the base's corners: back left, back right, front left, front right
};


} // namespace fight


#endif // FIGHT_TANK_H
