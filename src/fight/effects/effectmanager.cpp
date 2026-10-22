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

#include "effectmanager.h"
#include "projectile.h"
#include "fight/scenario.h"
#include "util/random.h"
#include <algorithm>
#include <glm/gtx/norm.hpp>
#include <vector>


namespace fight {


namespace {

const int Bubbles = 100;
// an energy shot lights the ground and the objects this far around it (m), with this strength
// (1 is the sprite's colour at full brightness right under the ball), and is looked for within
// this distance of the player
const float LightRadius = 20.0f;
const float LightStrength = 1.0f;
const float LightRange = 150.0f;
// the Torch's torch: 700 game units of reach and a brightness of 2.11 against the shots' 5,
// flickering between 0.5 and 1.5 of it every frame (docs/ROADMAP.md, appendix S); four times as
// strong here, as it hovers well above the ground and Vertigo's lights fall off with the square
// of the distance where the original's fall off linearly
const float TorchRadius = 700/32.0f;
const float TorchStrength = 2.11f/5*4;

float random()
{
    return util::random()/float(RAND_MAX);
}

}


EffectManager::EffectManager(Scenario *scenario) :
    m_scenario(scenario),
    m_particles(scenario)
{
    txt::DesFile file;

    // the explosions, the shots and the torpedo bubbles (21-23) are additive; the ground dust (18-20,
    // 24-26), the debris, the trash and the ambient bubbles (BUBBLE.DES) cover what is behind them
    file.load("vfx:sobjects/explosio.des");
    for (int i = 0; i < 27; i++)
        m_billboards.emplace(std::piecewise_construct,
            std::forward_as_tuple((Effects)((int)Effects::Explosion_0 + i)),
            std::forward_as_tuple(m_scenario->textureManager(), file, i, !(i >= 18 && i <= 20) && i < 24));
    file.load("vfx:sobjects/shoot.des");
    for (int i = 0; i < 9; i++)
        m_billboards.emplace(std::piecewise_construct,
            std::forward_as_tuple((Effects)((int)Effects::Shoot_0 + i)),
            std::forward_as_tuple(m_scenario->textureManager(), file, i));
    file.load("vfx:sobjects/debris.des");
    for (int i = 0; i < 23; i++)
        m_billboards.emplace(std::piecewise_construct,
            std::forward_as_tuple((Effects)((int)Effects::Debris_0 + i)),
            std::forward_as_tuple(m_scenario->textureManager(), file, i, false));
    file.load("vfx:sobjects/trash.des");
    for (int i = 0; i < 5; i++)
        m_billboards.emplace(std::piecewise_construct,
            std::forward_as_tuple((Effects)((int)Effects::Trash_0 + i)),
            std::forward_as_tuple(m_scenario->textureManager(), file, i, false));
    file.load("vfx:sobjects/bubble.des");
    for (int i = 0; i < 3; i++)
        m_billboards.emplace(std::piecewise_construct,
            std::forward_as_tuple((Effects)((int)Effects::Bubble_0 + i)),
            std::forward_as_tuple(m_scenario->textureManager(), file, i, false));
}


Billboard* EffectManager::getBillboard(Effects effect)
{
    return &m_billboards.at(effect);
}


void EffectManager::addEffect(Effects effect, const Vector3D &position, float angle, float scale)
{
    // the explosions differ from one to the next: the size by up to 30 % and the speed of the
    // animation by up to 25 %, and all but the ones on the ground are rolled at random; the
    // torpedoes' bubbles keep their frames' size whatever their [Size] Scale
    const int index = int(effect) - int(Effects::Explosion_0);
    float rate = 1;
    if (index >= 0 && index <= 26)
    {
        if (index >= 21 && index <= 23)
            scale /= getBillboard(effect)->sizeScale();
        else
        {
            scale *= 0.7f + 0.6f*random();
            rate = 1/(0.75f + 0.5f*random());
            const bool onGround = (index >= 12 && index <= 14) || (index >= 18 && index <= 20);
            if (!onGround && angle == 0)
                angle = 360*random();
        }
    }
    m_particles.add(getBillboard(effect), Particles::Motion::Animation, position, Vector3D(0), angle, scale, 0, rate);
}


void EffectManager::addDebris(Effects debris, const Vector3D &position, const Vector3D &direction)
{
    Billboard *billboard = getBillboard(debris);
    const float speed = billboard->minVelocity() + (billboard->maxVelocity() - billboard->minVelocity())*random();
    m_particles.add(billboard, Particles::Motion::Debris, position, direction*speed, util::random()%360);
}


void EffectManager::addBody(Effects body, const Vector3D &position)
{
    m_particles.add(getBillboard(body), Particles::Motion::Body, position, Vector3D(0), util::random()%360);
}


void EffectManager::addDecoy(Effects debris, Particles::Decoy kind, int faction, const Vector3D &position, const Vector3D &velocity)
{
    m_particles.addDecoy(getBillboard(debris), kind, faction, position, velocity, util::random()%360);
}


void EffectManager::addDust(const Vector3D &position, float size, bool groundOnly)
{
    // the large ground dust, upright, or (a tank's, at random) the other large dust, rolled; like
    // the other explosions 0.7 to 1.3 of the given size (the sprite scale, in metres), its frames
    // 0.75 to 1.25 of their time
    const bool ground = groundOnly || util::random()%2;
    const Effects dust = ground ? Effects::Explosion_20 : Effects::Explosion_26;
    Billboard *billboard = getBillboard(dust);
    m_particles.add(billboard, Particles::Motion::Dust, position, Vector3D(0), ground ? 0 : 360*random(),
                    size/std::max(billboard->sizeScale(), 1e-3f)*(0.7f + 0.6f*random()), 0, 1/(0.75f + 0.5f*random()));
}


Projectile* EffectManager::addProjectile(Effects projectile, const Vector3D &position, const Vector3D &direction, ActiveObject *owner)
{
    Billboard *billboard = getBillboard(projectile);
    Projectile *object = new Projectile(m_scenario, billboard);
    m_effects.emplace_back(object);
    object->setPosition(position);
    object->setDirection(direction);
    object->setOwner(owner);
    if (glows(projectile))
        object->setLightColor(billboard->lightColor());
    object->setPuffs(projectile == Effects::Shoot_7);
    object->setHugsGround(projectile == Effects::Shoot_7 || projectile == Effects::Shoot_8);
    return object;
}


bool EffectManager::glows(Effects projectile)
{
    return projectile == Effects::Shoot_PlasmaGun || projectile == Effects::Shoot_PlasmaDeath || projectile == Effects::Shoot_DoomMortar;
}


int EffectManager::pointLights(const Vector3D &position, gfx::Pipeline::PointLight *lights, int count)
{
    // the shots whose light reaches the ground first (a gun firing a dozen shots a second has the
    // nearest ones still high up), then the rest, each by the distance to the player; a shot's
    // light reaches a few metres, so none beyond the fog matters
    std::vector<std::pair<float, gfx::Pipeline::PointLight>> candidates;
    auto consider = [&](const Vector3D &p, const Vector3D &color, float radius) {
        const float distance = glm::distance(position, p);
        if (distance > LightRange)
            return;
        const bool aboveGround = p.z - m_scenario->surface().heightAt(p.x, p.y) > radius;
        candidates.emplace_back(distance + (aboveGround ? LightRange : 0), gfx::Pipeline::PointLight{ p, color, radius });
    };
    for (const auto &effect : m_effects)
    {
        const Projectile *shot = static_cast<const Projectile *>(effect.get());
        if (shot->lightColor() != Vector3D(0))
            consider(shot->position(), shot->lightColor()*LightStrength, LightRadius);
    }
    m_decoys.clear();
    m_particles.decoys(m_decoys);
    for (const Particles::DecoyInfo &decoy : m_decoys)
        if (decoy.kind == Particles::Decoy::Torch)
            consider(decoy.position, getBillboard(Effects::Debris_22)->lightColor()*TorchStrength*(0.5f + random()), TorchRadius);
    const int n = std::min(count, int(candidates.size()));
    std::partial_sort(candidates.begin(), candidates.begin() + n, candidates.end(),
                      [](const auto &a, const auto &b) { return a.first < b.first; });
    for (int i = 0; i < n; i++)
        lights[i] = candidates[i].second;
    return n;
}


void EffectManager::update(float elapsedTime)
{
    m_effects.remove_if([elapsedTime](const std::unique_ptr<Effect> &effect) { return effect->update(elapsedTime); });
    m_particles.update(elapsedTime);

    // bubbles around the eye, each at its own height of a rise from 50 m below
    Billboard *bubble = getBillboard(Effects::Bubble_0);
    for (int i = m_particles.count(Particles::Motion::Bubble); i < Bubbles; i++)
    {
        Vector3D position = m_scenario->eye() + Vector3D(util::random()%100 - 50, util::random()%100 - 50, -50);
        const float time = glm::distance2(m_scenario->eye(), position)/200.0f*random();
        position.z += time*bubble->velocity();
        if (glm::distance2(m_scenario->eye(), position) <= 50.0f*50.0f &&
            !m_particles.add(bubble, Particles::Motion::Bubble, position, Vector3D(0), 0, 2, time))
            break;
    }
}


void EffectManager::draw()
{
    m_particles.draw();
    for (const auto &effect : m_effects)
        effect->draw();
}


} // namespace fight
