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

#ifndef FIGHT_PROJECTILE_H
#define FIGHT_PROJECTILE_H


#include "effect.h"
#include "fight/collisionmanager.h"


namespace fight {


class ActiveObject;


class Projectile : public Effect
{
public:
    Projectile(Scenario *scenario, Billboard *billboard);

public:
    void setPosition(const Vector3D &pos);
    void setDirection(const Vector3D &direction);
    void setOwner(ActiveObject *owner);
    // the light an energy shot gives off; black for a shell
    void setLightColor(const Vector3D &color) { m_lightColor = color; }
    const Vector3D& lightColor() const { return m_lightColor; }
    // the Cluster Bomb's first shards leave a large puff every 0.35 s (docs/ROADMAP.md, appendix S)
    void setPuffs(bool puffs) { m_puffs = puffs; }
    bool update(float elapsedTime);

private:
    Vector3D m_originPos;
    Vector3D m_direction;
    Vector3D m_lightColor = Vector3D(0);
    CollisionCache m_collisionCache;
    ActiveObject *m_owner = nullptr; // never hit by its own shots
    int m_faction = -1;              // the owner's: it passes through that faction's units
    bool m_puffs = false;
    float m_nextPuff = PuffInterval;
    static constexpr float PuffInterval = 0.35f;
    static constexpr float PuffSize = 5.0f;   // m
};


} // namespace fight


#endif // FIGHT_PROJECTILE_H
