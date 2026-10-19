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
#include "gfx/pipeline.h"
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
    // the piece of debris something moving runs into (Particles::hitDebris)
    bool hitDebris(const Vector3D &start, const Vector3D &end, float radius, float speed, int kinetic, Particles::DebrisHit &hit)
    {
        return m_particles.hitDebris(start, end, radius, speed, kinetic, hit);
    }
    // a puff of ground dust stirred up at the position, scale times its [Size] Scale
    void addDust(const Vector3D &position, const Vector3D &velocity, float scale);
    Projectile* addProjectile(Effects projectile, const Vector3D &position, const Vector3D &direction, ActiveObject *owner = nullptr);

    const Particles& particles() const { return m_particles; }

    // the glow of up to count energy shots near the position, those close to the ground first,
    // as lights with their positions in the world; none when no shot is within 150 m
    int pointLights(const Vector3D &position, gfx::Pipeline::PointLight *lights, int count);

    void update(float elapsedTime);
    void draw();

private:
    // whether a shot glows (an energy weapon's) rather than being a shell
    static bool glows(Effects projectile);

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
