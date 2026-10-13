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

#ifndef FIGHT_EFFECTMANAGER_H
#define FIGHT_EFFECTMANAGER_H

#include "billboard.h"
#include "effect.h"

#include <map>
#include <list>
#include <memory>
#include "util/geometry3d.h"
#include "particles.h"

namespace fight {

class Scenario;
enum class Effects;
class Projectile;
class ActiveObject;

class EffectManager
{
public:
    EffectManager(Scenario *scenario);

public:
    Billboard* getBillboard(Effects effect);

    // an animation played once at the position
    void addEffect(Effects effect, const Vector3D &position, float angle = 0, float scale = 1);
    // a piece flying from the position in the direction, slowing down and sinking (DEBRIS.DES)
    void addDebris(Effects debris, const Vector3D &position, const Vector3D &direction);
    // ground dust (EXPLOSIO "bodenstaub") stirred up at the position, size 0-2 (small to large)
    void addDust(const Vector3D &position, int size, const Vector3D &velocity);
    Projectile* addProjectile(Effects projectile, const Vector3D &position, const Vector3D &direction, ActiveObject *owner = nullptr);

    const Particles& particles() const { return m_particles; }

    void update(float elapsedTime);
    void draw();

private:
    Scenario *m_scenario;
    std::map<Effects, Billboard> m_billboards;
    std::list<std::unique_ptr<Effect>> m_effects;  // projectiles
    Particles m_particles;
};

enum class Effects
{
    Explosion_0,
    Explosion_1,
    Explosion_2,
    Explosion_3,
    Explosion_4,
    Explosion_5,
    Explosion_6,
    Explosion_7,
    Explosion_8,
    Explosion_9,
    Explosion_10,
    Explosion_11,
    Explosion_12,
    Explosion_13,
    Explosion_14,
    Explosion_15,
    Explosion_16,
    Explosion_17,
    Explosion_18,
    Explosion_19,
    Explosion_20,
    Explosion_21,
    Explosion_22,
    Explosion_23,
    Explosion_24,
    Explosion_25,
    Explosion_26,
    Shoot_0,
    Shoot_PlasmaGun = Shoot_0,
    Shoot_PlasmaDeath,
    Shoot_DoomMortar,
    Shoot_MaximumImpactI,
    Shoot_MaximumImpact2,
    Shoot_Vendetta,
    Shoot_6,
    Shoot_7,
    Shoot_8,
    Debris_0,
    Debris_1,
    Debris_2,
    Debris_3,
    Debris_4,
    Debris_5,
    Debris_6,
    Debris_7,
    Debris_8,
    Debris_9,
    Debris_10,
    Debris_11,
    Debris_12,
    Debris_13,
    Debris_14,
    Debris_15,
    Debris_16,
    Debris_17,
    Debris_18,
    Debris_19,
    Debris_20,
    Debris_21,
    Debris_22,
    Trash_0,
    Trash_1,
    Trash_2,
    Trash_3,
    Trash_4,
    Bubble_0,
    Bubble_1,
    Bubble_2,
};

} // namespace fight

#endif // FIGHT_EFFECTMANAGER_H
