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

#ifndef FIGHT_BILLBOARD_H
#define FIGHT_BILLBOARD_H


#include <vector>
#include "util/geometry3d.h"
#include "gfx/texture.h"


namespace gfx { class TextureManager; }
namespace txt { class DesFile; }


namespace fight {


class BoundingBox;


class Billboard
{
public:
    Billboard(gfx::TextureManager &texMan, txt::DesFile &name, int index, bool additive = true);

public:
    // the frame shown at that time (the animation loops), facing the camera (its right, up and back axes)
    void quad(const Vector3D &position, float angle, float scale, float time, const glm::mat3 &camera,
              Vector3D corners[4], const Vector2D *&texCoords, gfx::Texture *&texture);
    void draw(const Vector3D &position, float angle, float scale, float time, const glm::mat3 &camera);
    // the sprite adds its colour to what is behind it, so overlapping sprites get brighter, up to
    // white; otherwise it covers it, with palette entry 0 left out (docs/ROADMAP.md, appendix H)
    bool additive() const { return m_additive; }
    // the colour of the light a glowing sprite gives off: its first frame's pixels averaged by
    // brightness, at full brightness; read from the image on the first call
    Vector3D lightColor();

    BoundingBox box();
    bool intersect(const Vector3D &start, const Vector3D &dir, float &distance);
    
    float duration() const { return m_stages.size()*m_displayTime; }
    // the [Size] Scale, the sprite's size in multiples of its frame's pixels at 1/32 m
    float sizeScale() const { return m_scale; }
    float range() const { return m_range; }
    float velocity() const { return m_velocity; }
    // debris: how long a piece lasts, how it slows down and sinks, and its start speed (m/s)
    float lifeTime() const { return m_lifeTime; }
    float friction() const { return m_friction; }
    float lift() const { return m_lift; }
    float minVelocity() const { return m_minVelocity; }
    float maxVelocity() const { return m_maxVelocity; }
    float collisionRadius() const { return m_collisionRadius; }
    float noiseLevel() const { return m_noiseLevel; }
    int kineticShield() const { return m_kineticShield; }
    int kineticStrength() const { return m_kineticStrength; }
    int shockStrength() const { return m_shockStrength; }

private:
    struct Stage
    {
        gfx::Texture texture;
        Vector2D texCoords[4];
        Vector2D scale;
        Vector2D offset;
    };

    float m_displayTime;
    std::vector<Stage> m_stages;
    String m_imageName;     // of the first stage
    float m_scale;
    bool m_additive;
    bool m_lightColorKnown = false;
    Vector3D m_lightColor = Vector3D(0);

    float m_range = 0;
    float m_velocity = 0;
    float m_lifeTime = 0;
    float m_friction = 0;
    float m_lift = 0;
    float m_minVelocity = 0;
    float m_maxVelocity = 0;
    float m_collisionRadius;
    float m_noiseLevel;
    int m_kineticShield;
    int m_kineticStrength;
    int m_shockStrength;
};


} // namespace fight


#endif // FIGHT_BILLBOARD_H
