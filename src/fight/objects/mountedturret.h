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

#ifndef FIGHT_MOUNTEDTURRET_H
#define FIGHT_MOUNTEDTURRET_H


#include "activeobject.h"
#include "turret.h"
#include <memory>


namespace fight {


class SimpleBoat;
class UnitGun;
struct OrderSlot;


// A gun turret on a bomber's hull ([TurretN] of the bomber's file, TURRET*.DES), a unit of its own
// (docs/ROADMAP.md, appendix J): it has its own shields and sensor and is hit, paralysed and blown up
// on its own; its hits count as attacks on the bomber. It follows the bomber's order: silent on Wait,
// Ambush, the cardinal orders or none; sweeping its half of the sky and fighting what it finds on
// Patrol, Goto, GotoWithArrive, GotoWithInfiltrate and Escort; on Attack the order's enemy first.
// It turns within 90 degrees of its mount's forward, leads its target and fires both arms' guns
// within 14 degrees of it. The bomber moves it and runs it after its own move.
class MountedTurret : public ActiveObject
{
public:
    // position: on the carrier's hull, in its frame (metres); below: hangs under the hull, upside
    // down; alpha: the [TurretN] Alpha, counter-clockwise from the bow
    MountedTurret(Scenario *scenario, SimpleBoat *carrier, txt::DesFile &file, const ObjectInfo &info,
                  const Vector3D &position, bool below, float alpha);
    ~MountedTurret();

public:
    // the carrier's order, whenever it takes up a slot
    void setOrder(const OrderSlot *slot);
    // runs the turret after the carrier has moved
    void think(float elapsedTime);
    // the carrier sleeps: the turret lets go of its target and turns back to rest
    void reset();
    // the carrier is paralysed: so is the turret, for good
    void paralyse();
    // blows the turret up (its own two seconds of explosions), unless it already is
    void blowUpTurret();
    // still there (not blown up and gone)
    bool isPresent() const { return !m_gone; }
    bool isExploding() const { return m_explodeStep >= 0; }
    const Vector3D& mountPosition() const { return m_mountPosition; }
    void setMountPosition(const Vector3D &position) { m_mountPosition = position; }

    bool update(float elapsedTime) { return false; }
    void draw();
    bool intersect(const Vector3D &start, const Vector3D &dir, float radius, float &distance, Vector3D &normal);
    void damage(int kinetic, int shock, const Vector3D &position, ActiveObject *attacker = nullptr);
    void destroy() { blowUpTurret(); }
    bool isContact() const { return false; }
    bool isDying() const override { return m_gone || isExploding(); }

private:
    enum class State { Rest, Wait, Sweep, Attack, Fire, Paralysed };

    // the turret's pivot and directions in the world
    Vector3D pivot() const;
    Vector3D toWorldDirection(const Vector3D &mount) const;
    Vector3D toMountDirection(const Vector3D &world) const;
    Vector3D aim() const;
    // turns towards the angles (mount frame), the yaw soft or hard by SoftBodyAdjust when tracking;
    // true once there
    bool turnTo(float yaw, float pitch, bool tracking, float elapsedTime);
    // where to shoot at: the exact intercept with the guns' shot speed, the target's position without
    // one; a ground unit a little higher
    Vector3D leadPoint(const ActiveObject *target) const;
    // a hostile to fight: whoever hit the turret, else what the sensor finds
    ActiveObject* search();
    void fire(float elapsedTime);
    bool isAliveTarget() const;
    void updateExplosion();
    void setTarget(ActiveObject *target);

private:
    SimpleBoat *m_carrier;
    std::unique_ptr<Turret> m_model;
    std::unique_ptr<UnitGun> m_guns[2];  // one for each arm
    Vector3D m_armOffset[2];             // the arms' offsets from the pivot (metres)
    std::unique_ptr<Orders> m_sensing;   // its sensor and the target lists, through an Orders without slots
    Vector3D m_mountPosition;            // on the carrier (metres, its frame)
    bool m_below;
    float m_restYaw;                     // the mount's forward, in the mount frame
    float m_modelScale;

    // turning: yaw soft (an acceleration of MaxAngleAccelerationA times the error over pi, damped by
    // AngleFrictionA) unless SoftBodyAdjust is no, pitch at MaxAngleVelocityB
    float m_yaw, m_pitch;
    float m_yawVelocity = 0, m_pitchVelocity = 0;
    float m_yawAcceleration, m_yawFriction, m_maxYawVelocity, m_maxPitchVelocity;
    bool m_softYaw;

    State m_state = State::Rest;
    int m_orderType = 0;
    int m_enemyId = -1;
    float m_orderEnd = 0;          // the order's time for the turret's sweep or attack (forever: a large number)
    float m_waitEnd = 0;
    float m_sweepYaw = 0;          // the sweep's angle from the mount's forward
    int m_sweepDirection = 0;
    bool m_sweepTurning = true;    // turning to the sweep's angle (else waiting until m_waitEnd)
    ActiveObject *m_target = nullptr;

    int m_explodeStep = -1;
    float m_explodeStart = 0;
    bool m_gone = false;
};


} // namespace fight


#endif // FIGHT_MOUNTEDTURRET_H
