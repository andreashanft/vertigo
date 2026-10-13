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
#include <QString>
#include <vector>


namespace fight {


class Module;


// A torpedo type: GLOBWEAP.DES [TorpedoN] (name, lock) and TORPEDO.DES (body, kinematics, damage).
// Item numbers 1-22 are types 0-21.
struct TorpedoType
{
    QString name;
    QString shortName;
    int lockType = 0;       // 0 none (straight), 1 noise (homing)
    float lockDelay = 0;    // seconds the target must be locked before launch
    QString model;
    float scale = 0;
    float noise = 0;
    float maxVelocity = 0;  // m/s
    float acceleration = 0; // m/s^2
    float turnRate = 0;     // rad/s
    float lifeTime = 0;
    float sensorRange = 0;  // m, how far a homing torpedo finds its target
    int kineticStrength = 0;
    int shockStrength = 0;
    int kineticShield = 1;

    static const std::vector<TorpedoType>& all();
    static const TorpedoType* get(int type) { return type >= 0 && type < int(all().size()) ? &all()[type] : nullptr; }
    bool isDecoy() const { return model == "estroker.mod"; } // the Torch
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

public:
    bool update(float elapsedTime);
    void draw();
    void destroy();

private:
    void detonate(const Vector3D &position, ActiveObject *hit);

private:
    const TorpedoType &m_type;
    Module *m_model;
    ActiveObject *m_owner;
    ActiveObject *m_target;
    Vector3D m_dir;
    float m_speed;
    float m_time;
};


} // namespace fight


#endif // FIGHT_TORPEDO_H
