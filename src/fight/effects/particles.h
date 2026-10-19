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
// with a fixed size and are drawn from the farthest to the nearest, in batches of the same
// texture. Ambient particles (bubbles, dust) only take the first three quarters of the pool, so
// they never crowd out explosions. Particles beyond the fog are neither spawned nor drawn.
class Particles
{
public:
    enum class Motion
    {
        Animation,  // plays its animation once, in place
        Debris,     // flies off, slows down and sinks; goes out on the ground, at the end of its
                    // LifeTime or when it is hit (docs/ROADMAP.md, appendix Q)
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

    // A piece of debris in the way of something moving from start to end with that radius at that
    // speed (m/s): the nearest piece that is more than a second old and slower than the mover. The
    // piece takes kinetic (and goes when its armour is gone); its own strength and where it was
    // hit come back. False if nothing was hit.
    struct DebrisHit
    {
        int kinetic;
        int shock;
        Vector3D position;
    };
    bool hitDebris(const Vector3D &start, const Vector3D &end, float radius, float speed, int kinetic, DebrisHit &hit);
    // where the pieces that make noise are (the buzzer's torch), which draw torpedoes
    void decoys(std::vector<Vector3D> &positions) const;

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
        int shield;     // debris: its armour
        float life;     // debris: how long it lasts, 1 to 1.6 times its LifeTime
    };

    struct Batch
    {
        gfx::Texture *texture;
        bool blendColor;
        std::vector<gfx::Pipeline::Vertex> vertices;
    };

    // advances the particle, false when it is done
    bool update(Particle &particle, float elapsedTime);
    // a piece of debris goes out in an explosion of its own, with its sound
    void endDebris(const Particle &particle, int explosion, int sample);

private:
    Scenario *m_scenario;
    std::vector<Particle> m_particles;
    std::vector<std::pair<float, const Particle *>> m_order;  // the ones drawn, by distance squared
    std::vector<Batch> m_batches;  // kept between frames to reuse their memory
    struct Ending
    {
        int explosion;
        int sample;
        Vector3D position;
    };
    std::vector<Ending> m_endings;  // the debris that went out this frame, spawned after the update
};


} // namespace fight


#endif // FIGHT_PARTICLES_H
