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

#ifndef FIGHT_SIMPLEBOAT_H
#define FIGHT_SIMPLEBOAT_H


#include "fight/objects/simpleobject.h"
#include "fight/weapons/gun.h"
#include <glm/mat3x3.hpp>
#include <memory>

namespace fight {


class SimpleBoat : public SimpleObject
{
public:
    SimpleBoat(Scenario *scenario, txt::DesFile &file, const ObjectInfo &info, float yaw);
    ~SimpleBoat();

public:
    const glm::mat3& orientation() const { return m_orientation; }
    const Vector3D& dir() const { return m_orientationInverted[1]; }
    const Vector3D& up() const { return m_orientationInverted[2]; }
    const Vector3D& right() const { return m_orientationInverted[0]; }

    float maxVelocity() const { return m_maxVelocity; }
    float minVelocity() const { return m_minVelocity; }

    float velocityTarget() const { return m_velocityTarget; }
    void setVelocityTarget(float velocityTarget) { m_velocityTarget = velocityTarget; }
    bool fullThrottle() const { return m_fullThrottle; }
    void setFullThrottle(bool fullThrottle) { m_fullThrottle = fullThrottle; }
    void setTurnVelocity(const Vector3D &turnVelocity) { m_turnVelocity = turnVelocity; }

    // steering for the AI orders: head for a point at a speed, or stay in place
    void steerTo(const Vector3D &point, float speed);
    void hold();

    // the [Attack] settings of the object file, distances in metres
    struct AttackSettings
    {
        float alignFront = 0.5f, alignBack = 0, alignLeft = 0.25f, alignRight = 0.25f;
        float minAlignRadius = 50, maxAlignRadius = 100;
        float minAlignTime = 4, maxAlignTime = 8;
        float minAttackTime = 6, maxAttackTime = 12;
        float hitProbability = 0.5f;
        float torpedoProbability = 0;
        float minFlightRadius = 25, maxFlightRadius = 30;
        float minFlightTime = 3, maxFlightTime = 6;
    };
    const AttackSettings& attackSettings() const { return m_attackSettings; }
    virtual bool isGroundVehicle() const { return false; }
    // whether patrols and escorts fly attack runs at hostiles they detect (cargo ships leave it to their turrets)
    virtual bool engagesHostiles() const { return true; }
    // where the gun points; units with a turret turn it with aimAt()
    virtual Vector3D gunDirection() const { return dir(); }

public:
    void setPosition(const Vector3D &pos);
    // moves the boat there, pointing its nose in the direction (with no roll)
    void place(const Vector3D &pos, const Vector3D &direction);
    bool update(float elapsedTime);
    void updateVelocity(float elapsedTime);
    void updateOrientation(float elapsedTime);
    void draw();
    void arm(int model, txt::DesFile &file);
    bool intersect(const Vector3D &start, const Vector3D &dir, float radius, float &distance, Vector3D &normal);

protected:
    void updateBox();
    // moves by delta, sliding along the terrain and the objects in the way
    void move(Vector3D delta);
    // the part of delta that doesn't run into another unit, a tower or a building: up to the
    // contact, then along it (mines, effects and the unit it docks on are left out)
    Vector3D slideAlongObjects(const Vector3D &position, const Vector3D &delta);
    // big ships push through; the others steer round them
    virtual bool collidesWithObjects() const { return true; }

protected:
    glm::mat3 m_orientation;
    glm::mat3 m_orientationInverted;

    float m_velocityTarget;
    bool m_fullThrottle;

    float m_maxVelocity;
    float m_minVelocity;
    float m_maxOrientationAcceleration;
    float m_maxOrientationRetardation;
    Vector3D m_orientationFriction;

    Vector3D m_angleVelocity;
    Vector3D m_turnVelocity;

    Vector3D m_maxAngleVelocity;
    Vector3D m_angleFriction;

    float m_collisionRadius; // for collisions with the terrain and other objects
    float m_currentFactor;   // how much the water current moves the boat
    float m_minClimbingAngle; // steepest dive and climb in radians (for ground vehicles: the most they tilt)
    float m_maxClimbingAngle;

    AttackSettings m_attackSettings;
};


} // namespace fight


#endif // FIGHT_SIMPLEBOAT_H
