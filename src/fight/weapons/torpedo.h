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

#ifndef FIGHT_TORPEDO_H
#define FIGHT_TORPEDO_H


#include "fight/objects/activeobject.h"
#include "util/string.hpp"
#include <vector>


namespace fight {


class Module;


// A torpedo type: GLOBWEAP.DES [TorpedoN] (name, lock) and TORPEDO.DES (body, kinematics, damage).
// Item numbers 1-22 are types 0-21.
struct TorpedoType
{
    String name;
    String shortName;
    String image;          // its picture in GFX/IMG/COCKPIT, e.g. stanley2
    int lockType = 0;       // 0 none (straight), 1 noise (homing)
    float lockDelay = 0;    // seconds the target must be locked before launch
    String model;
    float scale = 0;
    float noise = 0;
    float maxVelocity = 0;  // m/s
    float acceleration = 0; // m/s^2
    float retardation = 0;  // m/s^2
    float sideFriction = 0; // 1/s, of the speed across the nose
    // turning, yaw and pitch: the most angular acceleration and speed (rad/s^2, rad/s) and the
    // friction on the angular speed (1/s)
    Vector2D angleAcceleration;
    Vector2D angleVelocity;
    Vector2D angleFriction;
    float agility = 0;      // the agile homing types: the share of the velocity turned to the target per tick
    float influence = 1;    // the chance that a decoy can draw it away
    float lifeTime = 0;
    int sensor = 99;        // which decoys draw it: 3 buzzers, 4 hostile buzzers and torches, 99 none
    float sensorRange = 0;  // m, how far a homing torpedo finds a decoy
    int kineticStrength = 0;
    int shockStrength = 0;
    int kineticShield = 1;

    static const std::vector<TorpedoType>& all();
    static const TorpedoType* get(int type) { return type >= 0 && type < int(all().size()) ? &all()[type] : nullptr; }
    bool isTorch() const { return model == "estroker.mod"; } // leaves its torch, a decoy
    bool isCluster() const { return model == "ripper.mod"; } // bursts with area damage
    bool isAgile() const { return agile; }
    bool agile = false;     // Tiger Shark II, Man Eater II, Hammerhead, Flash Shark III, Leech I and II
    int index = 0;          // 0-21, the TORPEDO.DES entry
};


class Torpedo : public ActiveObject
{
public:
    // target: the unit a homing torpedo follows, nullptr to run straight
    Torpedo(Scenario *scenario, int type, const Vector3D &position, const Vector3D &direction, float speed, ActiveObject *owner, ActiveObject *target);

public:
    const TorpedoType& type() const { return m_type; }
    ActiveObject* owner() const { return m_owner; }
    ActiveObject* target() const { return m_target; }
    const Vector3D& dir() const { return m_dir; }
    // done: exploded or run out; the scenario removes it
    bool isDone() const { return !m_enabled; }
    // gone off but still at work (the Cluster Bomb's second ring of shards)
    bool isBursting() const { return m_burstTime >= 0; }

public:
    bool update(float elapsedTime);
    void draw();
    void destroy();
    bool intersect(const Vector3D &start, const Vector3D &dir, float radius, float &distance, Vector3D &normal);

private:
    void detonate(const Vector3D &position, ActiveObject *hit, bool ground = false);
    // takes the strength of what it hit off its armour, false if that is gone
    bool outlasts(int kinetic);
    // the Cluster Bomb's ring of shards at that distance (m), turned by that angle
    void shards(float distance, float turn);
    // where a homing torpedo makes for while the ground is between it and its goal
    Vector3D detour(const Vector3D &goal) const;
    float radius() const;
    bool targetAlive() const;
    void spend();
    // turns the nose towards the point within the type's angular limits
    void turnTowards(const Vector3D &point, float elapsedTime);
    // the speed along the nose towards this, the speed across it dying away
    void drive(float speed, float elapsedTime);

private:
    const TorpedoType &m_type;
    Module *m_model;
    ActiveObject *m_owner;
    ActiveObject *m_target;
    Vector3D m_dir;
    float m_speed;
    float m_time;
    float m_trailTime;
    Vector2D m_turn = Vector2D(0);  // angular speed, yaw and pitch (rad/s)
    float m_startDistance = 0;      // from the target at launch
    bool m_influenced;              // whether a decoy can still draw it away
    bool m_spent = false;           // its time is up: it sinks until it hits the ground
    int m_lure = 0;                 // the decoy that drew it away, 0 for none
    Object *m_inside = nullptr;     // what a torpedo that outlasts its hits (the Subsonics) is passing through
    bool m_blocked = false;         // the ground between it and its goal at the last look
    float m_nextLook = 0;           // when it looks again (every half second)
    float m_burstTime = -1;         // the Cluster Bomb's burst, -1 before it
    float m_burstHeading = 0;
};


} // namespace fight


#endif // FIGHT_TORPEDO_H
