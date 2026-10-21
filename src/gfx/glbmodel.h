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

#ifndef GFX_GLBMODEL_H
#define GFX_GLBMODEL_H


#include "image.h"
#include "util/geometry3d.h"
#include "util/json.hpp"

#include <string>
#include <vector>


namespace gfx {


// Reads binary glTF 2.0 files (.glb) as far as the 3D cockpits (data/3D-cockpits) need it:
// triangle meshes with positions, normals and texture coordinates (also quantized, as with
// KHR_mesh_quantization), the node hierarchy with its transforms, materials with a base colour
// and texture, embedded JPEG or PNG images, and the extras of the nodes and the asset.
class GlbModel
{
public:
    struct Mesh
    {
        std::string name;           // of the node
        util::Json extras;          // the node's glTF extras, e.g. "role" and "des_section"
        std::string material;
        int image = -1;             // base colour image, -1 for none
        Vector4D color = Vector4D(1);
        bool blend = false;         // alphaMode BLEND
        std::vector<Vector3D> positions;    // node transforms applied
        std::vector<Vector3D> normals;      // the same, unit length; empty without them
        std::vector<Vector2D> texCoords;    // of the base colour texture; empty without one
        std::vector<std::uint32_t> indices;       // triangles
    };

public:
    // a path in UTF-8
    bool load(const std::string &fileName);
    const std::string& error() const { return m_error; }

    const std::vector<Mesh>& meshes() const { return m_meshes; }
    const std::vector<Image>& images() const { return m_images; }
    // asset.extras
    const util::Json& extras() const { return m_extras; }

private:
    bool fail(const std::string &error);

private:
    std::string m_error;
    std::vector<Mesh> m_meshes;
    std::vector<Image> m_images;
    util::Json m_extras;
};


} // namespace gfx


#endif // GFX_GLBMODEL_H
