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

#ifndef FIGHT_PARTICLES_H
#define FIGHT_PARTICLES_H


#include "gfx/pipeline.h"
#include "util/geometry3d.h"
#include <vector>


namespace gfx { class Texture; }


namespace fight {


class Billboard;
class Scenario;


// Lightweight sprites: explosions, debris, bubbles, torpedo trails and dust. They live in one pool
// with a fixed size and are drawn in batches, one per texture. Ambient particles (bubbles, dust)
// only take the first three quarters of the pool, so they never crowd out explosions. Particles
// beyond the fog are neither spawned nor drawn.
class Particles
{
public:
    enum class Motion
    {
        Animation,  // plays its animation once, in place
        Debris,     // flies off, slows down, sinks and settles on the ground until its LifeTime ends
        Bubble,     // rises (the billboard's Velocity) until it is far from the player; loops
        Dust,       // plays its animation slowly while drifting up
    };

    static const int Capacity = 3000;
    static const float Range;   // beyond this distance from the camera nothing is spawned or drawn

public:
    explicit Particles(Scenario *scenario);

public:
    // false if the particle wasn't added (pool full, or out of range)
    bool add(Billboard *billboard, Motion motion, const Vector3D &position, const Vector3D &velocity = Vector3D(0), float angle = 0, float scale = 1, float time = 0, float rate = 1);
    int count() const { return int(m_particles.size()); }
    int count(Motion motion) const;

    void update(float elapsedTime);
    void draw();

private:
    struct Particle
    {
        Billboard *billboard;
        Motion motion;
        Vector3D position;
        Vector3D velocity;
        float angle;
        float scale;
        float time;
        float rate;     // the speed of the animation
    };

    struct Batch
    {
        gfx::Texture *texture;
        bool blendColor;
        std::vector<gfx::Pipeline::Vertex> vertices;
    };

    // advances the particle, false when it is done
    bool update(Particle &particle, float elapsedTime) const;

private:
    Scenario *m_scenario;
    std::vector<Particle> m_particles;
    std::vector<Batch> m_batches;  // kept between frames to reuse their memory
};


} // namespace fight


#endif // FIGHT_PARTICLES_H
