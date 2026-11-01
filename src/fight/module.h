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

#ifndef FIGHT_MODULE_H
#define FIGHT_MODULE_H


#include "gfx/mesh.h"
#include "gfx/texture.h"
#include "boundingbox.h"
#include "collisionmesh.h"

#include <list>
#include "util/string.hpp"


namespace gfx { class TextureManager; }


namespace fight {


class Module
{
public:
    Module(gfx::TextureManager &texMan, const String &name);

public:
    void draw();
    const BoundingBox& box() const { return m_box; }
    // the farthest vertex from the origin, and the farthest from its vertical (model units)
    float radius() const { return m_radius; }
    float flatRadius() const { return m_flatRadius; }
    bool intersect(const Vector3D &start, const Vector3D &dir, float radius, float &distance, Vector3D &normal);
    // its triangles' corners, transformed, three by three
    void addTriangles(const Matrix &transform, std::vector<Vector3D> &corners) const;

private:
    struct Mesh
    {
        gfx::Texture texture;
        std::vector<Vector3D> vertices;
        std::vector<Vector3D> normals;
        std::vector<Vector2D> texCoords;
        std::vector<std::uint16_t> indices;
        gfx::Mesh buffers;
    };

    BoundingBox m_box;
    float m_radius = 0;
    float m_flatRadius = 0;
    std::list<Mesh> m_meshes;
    CollisionMesh m_collisionMesh;
};


} // namespace fight


#endif // FIGHT_MODULE_H
