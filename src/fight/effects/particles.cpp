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
#include "fight/scenario.h"
#include "gfx/texture.h"

#include <qopengl.h>
#include <glm/gtx/norm.hpp>
#include <algorithm>


namespace fight {


namespace {

// Lift in DEBRIS.DES is negative (the pieces sink); this turns it into m/s^2
const float DebrisLiftScale = 4.0f;
// bubbles disappear this far from the player (squared)
const float BubbleRange2 = 50.0f*50.0f;
// dust plays at this fraction of its speed, rising slowly
const float DustRate = 0.4f;
const float DustRise = 0.6f;
const int AmbientCapacity = Particles::Capacity*3/4;

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


bool Particles::add(Billboard *billboard, Motion motion, const Vector3D &position, const Vector3D &velocity, float angle, float scale, float time)
{
    const bool ambient = motion == Motion::Bubble || motion == Motion::Dust;
    if (int(m_particles.size()) >= (ambient ? AmbientCapacity : Capacity))
        return false;
    if (glm::distance2(position, m_scenario->position()) > Range*Range)
        return false;
    m_particles.push_back(Particle{ billboard, motion, position, velocity, angle, scale, time });
    return true;
}


bool Particles::update(Particle &particle, float elapsedTime) const
{
    Billboard *billboard = particle.billboard;
    switch (particle.motion)
    {
    case Motion::Animation:
        particle.time += elapsedTime;
        return particle.time < billboard->duration();

    case Motion::Debris:
        {
            particle.time += elapsedTime;
            if (particle.time >= billboard->lifeTime())
                return false;
            particle.velocity += (-billboard->friction()*particle.velocity + Vector3D(0, 0, billboard->lift()*DebrisLiftScale))*elapsedTime;
            particle.position += particle.velocity*elapsedTime;
            const float ground = m_scenario->surface().heightAt(particle.position.x, particle.position.y) + 0.3f;
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
}


void Particles::draw()
{
    if (m_particles.empty())
        return;

    const glm::mat3 camera(m_scenario->cameraMatrixInverted());
    const Vector3D eye = m_scenario->position();
    const Vector3D forward = -camera[2];

    for (Batch &batch : m_batches)
    {
        batch.vertices.clear();
        batch.texCoords.clear();
    }
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
            m_batches.push_back(Batch{ texture, blendColor, {}, {} });
            batch = m_batches.end() - 1;
        }
        batch->vertices.insert(batch->vertices.end(), corners, corners + 4);
        batch->texCoords.insert(batch->texCoords.end(), texCoords, texCoords + 4);
    }

    glEnable(GL_TEXTURE_2D);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_NOTEQUAL, 0.0);
    for (Batch &batch : m_batches)
    {
        if (batch.vertices.empty())
            continue;
        if (batch.blendColor)
            glBlendFunc(GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR);
        batch.texture->bind();
        glVertexPointer(3, GL_FLOAT, 0, batch.vertices.data());
        glTexCoordPointer(2, GL_FLOAT, 0, batch.texCoords.data());
        glDrawArrays(GL_QUADS, 0, GLsizei(batch.vertices.size()));
        if (batch.blendColor)
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
    glDisable(GL_ALPHA_TEST);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
}


} // namespace fight
