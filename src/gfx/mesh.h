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

#ifndef GFX_MESH_H
#define GFX_MESH_H

#include "util/geometry3d.h"
#include <QtGlobal>

namespace gfx {

// Triangles in buffers on the GPU, drawn with gfx::Pipeline. Without texture coordinates they are
// drawn untextured, without normals with the pipeline's normal.
class Mesh
{
public:
    Mesh() = default;
    ~Mesh();
    Mesh(const Mesh &) = delete;
    Mesh& operator=(const Mesh &) = delete;

public:
    bool isValid() const { return m_vertexArray != 0; }
    // normals and texture coordinates may be null; needs a current OpenGL context
    void create(int vertexCount, const Vector3D *positions, const Vector3D *normals, const Vector2D *texCoords,
                int indexCount, const quint16 *indices);
    void create(int vertexCount, const Vector3D *positions, const Vector3D *normals, const Vector2D *texCoords,
                int indexCount, const quint32 *indices);
    void destroy();
    // count indices from first on, all by default; untextured also when there is no texture
    void draw(int first = 0, int count = -1, bool textured = true);

private:
    void create(int vertexCount, const Vector3D *positions, const Vector3D *normals, const Vector2D *texCoords,
                int indexCount, const void *indices, int indexSize);

private:
    unsigned int m_vertexArray = 0;
    unsigned int m_vertices = 0;
    unsigned int m_indices = 0;
    unsigned int m_indexType = 0;
    int m_indexSize = 0;
    int m_indexCount = 0;
    bool m_textured = false;
};

} // namespace gfx

#endif // GFX_MESH_H
