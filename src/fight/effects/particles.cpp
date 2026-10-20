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


// a decoy draws torpedoes, so it lives wherever it is
bool Particles::addDecoy(Billboard *billboard, Decoy kind, int faction, const Vector3D &position, const Vector3D &velocity, float angle)
{
    if (int(m_particles.size()) >= Capacity)
        return false;
    const float life = billboard->lifeTime()*(1 + 0.6f*util::random()/float(RAND_MAX));
    // clear of the ground, so that the Torch's torch, going off there, rises from it
    Vector3D start = position;
    start.z = std::max(start.z, m_scenario->surface().heightAt(start.x, start.y) + 0.5f*debrisRadius(billboard, 1) + 0.01f);
    m_particles.push_back(Particle{ billboard, Motion::Debris, start, velocity, angle, 1, 0, 1, billboard->kineticShield(), life, kind, faction, ++m_lastDecoy });
    return true;
}


void Particles::decoys(std::vector<DecoyInfo> &decoys) const
{
    for (const Particle &particle : m_particles)
        if (particle.decoy != Decoy::None)
            decoys.push_back(DecoyInfo{ particle.id, particle.decoy, particle.faction, particle.billboard->noiseLevel(), particle.position, particle.velocity });
}


bool Particles::hitDebris(const Vector3D &start, const Vector3D &end, float radius, float speed, int kinetic, DebrisHit &hit)
{
    const Vector3D way = end - start;
    const float length2 = glm::length2(way);
    Particle *nearest = nullptr;
    float nearestAlong = 2;
    for (Particle &particle : m_particles)
    {
        if (particle.motion != Motion::Debris || particle.time < DebrisGrace || particle.shield <= 0 || glm::length(particle.velocity) >= speed)
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
            // each with its sound, the buzzer's torch too
            if (particle.shield <= 0)
            {
                endDebris(particle, int(Effects::Explosion_4), int(sfx::Sample::Explosion_Small));
                return false;
            }
            particle.time += elapsedTime;
            if (particle.time >= particle.life)
            {
                endDebris(particle, int(Effects::Explosion_5), int(sfx::Sample::Explosion_Small));
                return false;
            }
            particle.velocity += (-billboard->friction()*particle.velocity + Vector3D(0, 0, billboard->lift()*DebrisLiftScale))*elapsedTime;
            particle.position += particle.velocity*elapsedTime;
            const float height = m_scenario->surface().heightAt(particle.position.x, particle.position.y);
            if (particle.position.z - 0.5f*debrisRadius(billboard, particle.scale) < height)
            {
                particle.position.z = height;
                endDebris(particle, int(Effects::Explosion_12), int(sfx::Sample::Explosion_Ground));
                return false;
            }
            return true;
        }

    case Motion::Bubble:
        particle.time += elapsedTime;
        particle.position.z += billboard->velocity()*elapsedTime;
        return glm::distance2(m_scenario->position(), particle.position) <= BubbleRange2;

    case Motion::Dust:
        particle.time += elapsedTime*particle.rate;
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

    // culled beyond the fog and behind the camera, the rest from the farthest to the nearest
    m_order.clear();
    for (const Particle &particle : m_particles)
    {
        const Vector3D way = particle.position - eye;
        const float distance2 = glm::length2(way);
        if (distance2 > Range*Range || (glm::dot(way, forward) < 0 && distance2 > 100.0f))
            continue;
        m_order.emplace_back(distance2, &particle);
    }
    std::sort(m_order.begin(), m_order.end(), [](const auto &a, const auto &b) { return a.first > b.first; });

    // in that order, a batch for each run of the same texture
    std::size_t batches = 0;
    for (const auto &entry : m_order)
    {
        const Particle &particle = *entry.second;
        Vector3D corners[4];
        const Vector2D *texCoords;
        gfx::Texture *texture;
        particle.billboard->quad(particle.position, particle.angle, particle.scale, particle.time, camera, corners, texCoords, texture);
        const bool additive = particle.billboard->additive();
        if (batches == 0 || m_batches[batches - 1].texture->id() != texture->id() || m_batches[batches - 1].additive != additive)
        {
            if (batches == m_batches.size())
                m_batches.emplace_back();
            m_batches[batches].texture = texture;
            m_batches[batches].additive = additive;
            m_batches[batches].vertices.clear();
            batches++;
        }
        for (int i = 0; i < 4; i++)
            m_batches[batches - 1].vertices.push_back({ corners[i], texCoords[i] });
    }

    // the sprites leave the depth buffer alone: the soft edges the filtering gives them only add
    // to what is behind them and don't hide the sprites further back, which come first
    gfx::Pipeline::setAlphaTest(true);
    glDepthMask(GL_FALSE);
    for (std::size_t i = 0; i < batches; i++)
    {
        const Batch &batch = m_batches[i];
        glBlendFunc(GL_SRC_ALPHA, batch.additive ? GL_ONE : GL_ONE_MINUS_SRC_ALPHA);
        batch.texture->bind();
        gfx::Pipeline::drawQuads(batch.vertices.data(), int(batch.vertices.size()));
    }
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_TRUE);
    gfx::Pipeline::setAlphaTest(false);
}


} // namespace fight
