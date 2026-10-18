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

#include "particles.h"
#include "billboard.h"
#include "effectmanager.h"
#include "sfx/samplemap.h"
#include "fight/scenario.h"
#include "gfx/texture.h"
#include "util/random.h"

#include "gfx/gl.hpp"
#include <glm/gtx/norm.hpp>
#include <algorithm>


namespace fight {


namespace {

// Lift in DEBRIS.DES is in g and negative (the pieces sink); this turns it into m/s^2
const float DebrisLiftScale = 9.81f;
// bubbles disappear this far from the player (squared)
const float BubbleRange2 = 50.0f*50.0f;
// dust plays at this fraction of its speed, rising slowly
const float DustRate = 0.4f;
const float DustRise = 0.6f;
const int AmbientCapacity = Particles::Capacity*3/4;
// a piece of debris takes part in contacts once it is this old (s)
const float DebrisGrace = 1.0f;

// a piece of debris' radius
float debrisRadius(Billboard *billboard, float scale)
{
    return billboard->box().maxPoint().x*scale;
}

}


const float Particles::Range = 220.0f;


Particles::Particles(Scenario *scenario) :
    m_scenario(scenario)
{
    m_particles.reserve(Capacity);
}


int Particles::count(Motion motion) const
{
    return int(std::count_if(m_particles.begin(), m_particles.end(), [motion](const Particle &particle) { return particle.motion == motion; }));
}


bool Particles::add(Billboard *billboard, Motion motion, const Vector3D &position, const Vector3D &velocity, float angle, float scale, float time, float rate)
{
    const bool ambient = motion == Motion::Bubble || motion == Motion::Dust;
    if (int(m_particles.size()) >= (ambient ? AmbientCapacity : Capacity))
        return false;
    if (glm::distance2(position, m_scenario->position()) > Range*Range)
        return false;
    const float life = motion == Motion::Debris ? billboard->lifeTime()*(1 + 0.6f*util::random()/float(RAND_MAX)) : 0;
    m_particles.push_back(Particle{ billboard, motion, position, velocity, angle, scale, time, rate, billboard->kineticShield(), life });
    return true;
}


// the buzzer's torch is the only piece that makes noise
bool Particles::collides(const Particle &particle)
{
    return particle.motion == Motion::Debris && particle.billboard->noiseLevel() == 0;
}


bool Particles::hitDebris(const Vector3D &start, const Vector3D &end, float radius, float speed, int kinetic, DebrisHit &hit)
{
    const Vector3D way = end - start;
    const float length2 = glm::length2(way);
    Particle *nearest = nullptr;
    float nearestAlong = 2;
    for (Particle &particle : m_particles)
    {
        if (!collides(particle) || particle.time < DebrisGrace || particle.shield <= 0 || glm::length(particle.velocity) >= speed)
            continue;
        const float along = length2 > 0 ? glm::clamp(glm::dot(particle.position - start, way)/length2, 0.0f, 1.0f) : 0.0f;
        const float reach = radius + debrisRadius(particle.billboard, particle.scale);
        if (glm::distance2(start + way*along, particle.position) < reach*reach && along < nearestAlong)
        {
            nearest = &particle;
            nearestAlong = along;
        }
    }
    if (!nearest)
        return false;
    hit.kinetic = nearest->billboard->kineticStrength();
    hit.shock = nearest->billboard->shockStrength();
    hit.position = nearest->position;
    nearest->shield -= kinetic;
    return true;
}


void Particles::endDebris(const Particle &particle, int explosion, int sample)
{
    m_endings.push_back(Ending{ explosion, sample, particle.position });
}


bool Particles::update(Particle &particle, float elapsedTime)
{
    Billboard *billboard = particle.billboard;
    switch (particle.motion)
    {
    case Motion::Animation:
        particle.time += elapsedTime*particle.rate;
        return particle.time < billboard->duration();

    case Motion::Debris:
        {
            // the pieces go out in a puff when hit or old and in a ground impact on the ground,
            // each with its sound; the buzzer's torch rests on the ground until its time is up
            const bool ends = collides(particle);
            if (ends && particle.shield <= 0)
            {
                endDebris(particle, int(Effects::Explosion_4), int(sfx::Sample::Explosion_Small));
                return false;
            }
            particle.time += elapsedTime;
            if (particle.time >= particle.life)
            {
                if (ends)
                    endDebris(particle, int(Effects::Explosion_5), int(sfx::Sample::Explosion_Small));
                return false;
            }
            particle.velocity += (-billboard->friction()*particle.velocity + Vector3D(0, 0, billboard->lift()*DebrisLiftScale))*elapsedTime;
            particle.position += particle.velocity*elapsedTime;
            const float height = m_scenario->surface().heightAt(particle.position.x, particle.position.y);
            if (ends && particle.position.z - 0.5f*debrisRadius(billboard, particle.scale) < height)
            {
                particle.position.z = height;
                endDebris(particle, int(Effects::Explosion_12), int(sfx::Sample::Explosion_Ground));
                return false;
            }
            const float ground = height + 0.3f;
            if (particle.position.z < ground)
            {
                particle.position.z = ground;
                particle.velocity = Vector3D(0);
            }
            return true;
        }

    case Motion::Bubble:
        particle.time += elapsedTime;
        particle.position.z += billboard->velocity()*elapsedTime;
        return glm::distance2(m_scenario->position(), particle.position) <= BubbleRange2;

    case Motion::Dust:
        particle.time += elapsedTime*DustRate;
        particle.position += (particle.velocity + Vector3D(0, 0, DustRise))*elapsedTime;
        particle.velocity *= std::max(0.0f, 1.0f - 2.0f*elapsedTime);
        return particle.time < billboard->duration();
    }
    return false;
}


void Particles::update(float elapsedTime)
{
    // remove finished particles by moving the last one into their place
    for (size_t i = 0; i < m_particles.size(); )
    {
        if (update(m_particles[i], elapsedTime))
            i++;
        else
        {
            m_particles[i] = m_particles.back();
            m_particles.pop_back();
        }
    }
    // the debris' explosions join the pool now that the loop is done
    std::vector<Ending> endings;
    endings.swap(m_endings);
    for (const Ending &ending : endings)
    {
        m_scenario->effectManager().addEffect(Effects(ending.explosion), ending.position);
        sfx::SampleMap::playAt(sfx::Sample(ending.sample), ending.position.x, ending.position.y, ending.position.z);
    }
}


void Particles::draw()
{
    if (m_particles.empty())
        return;

    const glm::mat3 camera(m_scenario->cameraMatrixInverted());
    const Vector3D eye = m_scenario->position();
    const Vector3D forward = -camera[2];

    for (Batch &batch : m_batches)
        batch.vertices.clear();
    for (const Particle &particle : m_particles)
    {
        // culled beyond the fog and behind the camera
        const Vector3D way = particle.position - eye;
        const float distance2 = glm::length2(way);
        if (distance2 > Range*Range || (glm::dot(way, forward) < 0 && distance2 > 100.0f))
            continue;
        Vector3D corners[4];
        const Vector2D *texCoords;
        gfx::Texture *texture;
        particle.billboard->quad(particle.position, particle.angle, particle.scale, particle.time, camera, corners, texCoords, texture);
        const bool blendColor = particle.billboard->blendColor();
        auto batch = std::find_if(m_batches.begin(), m_batches.end(), [&](const Batch &batch) {
            return batch.texture->id() == texture->id() && batch.blendColor == blendColor;
        });
        if (batch == m_batches.end())
        {
            m_batches.push_back(Batch{ texture, blendColor, {} });
            batch = m_batches.end() - 1;
        }
        for (int i = 0; i < 4; i++)
            batch->vertices.push_back({ corners[i], texCoords[i] });
    }

    gfx::Pipeline::setAlphaTest(true);
    for (Batch &batch : m_batches)
    {
        if (batch.vertices.empty())
            continue;
        if (batch.blendColor)
            glBlendFunc(GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR);
        batch.texture->bind();
        gfx::Pipeline::drawQuads(batch.vertices.data(), int(batch.vertices.size()));
        if (batch.blendColor)
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
    gfx::Pipeline::setAlphaTest(false);
}


} // namespace fight
