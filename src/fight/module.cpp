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

#include "module.h"
#include "gfx/texturemanager.h"
#include "util/bytereader.hpp"
#include "util/datapath.h"
#include "util/path.hpp"

#include <iostream>
#include <glm/geometric.hpp>


namespace fight {


Module::Module(gfx::TextureManager &texMan, const String &name)
{
    const std::string path = util::DataPath::resolve(name);
    std::vector<char> data;
    if (!util::readFile(path, data))
        std::cerr << "Cannot open " << path << std::endl;
    util::ByteReader stream(data);

    unsigned int i, j, k;
    std::uint32_t numFaces;
    std::uint32_t numVertices;
    std::map<String, Mesh*> meshes;

    stream.skip(2);
    stream >> numFaces;
    stream.skip(4);
    stream >> numVertices;
    stream.skip(4);

    std::vector<Vector3D> vertices;
    vertices.reserve(numVertices);

    for (i = 0; i < numVertices; i++) {
        std::uint32_t index;
        Vector3D v;
        stream >> index >> v.x >> v.y >> v.z;
        vertices.push_back(v);
        m_box.add(v);
    }

    for (i = 0; i < numFaces; i++) {
        std::uint32_t faceNumber;
        std::uint32_t type;
        std::uint32_t numVertices;
        std::uint32_t numVertices2;

        stream >> faceNumber;
        stream >> type;
        stream.skip(2);
        stream >> numVertices;

        if (type == 0) {
            stream.skip(8);
            stream.skip(numVertices * 12);
            stream.skip(4);
        } else if (type == 1) {
            stream.skip(32);
            stream >> numVertices2;
            stream.skip(4);

            std::vector<Vector2D> texCoords;
            texCoords.reserve(numVertices2 * 2);

            for (j = 0; j < numVertices2; j++) {
                std::uint16_t s;
                std::uint16_t t;

                stream.skip(2);
                stream >> s;
                stream.skip(2);
                stream >> t;

                texCoords.emplace_back(s / 255.0f, t / 255.0f);
            }
            std::uint32_t nameLength;
            stream >> nameLength;
            stream.skip(4);

            const std::string textureBytes = stream.read(nameLength);
            const String textureName = String::fromLatin1(textureBytes.data(), int(textureBytes.size()));

            std::vector<std::uint32_t> indices;
            indices.reserve(3 * numVertices2);
            for (j = 0; j < 3 * numVertices2; j++) {
                std::uint32_t index;
                stream >> index;
                indices.push_back(index);
            }

            Mesh *mesh;
            auto it = meshes.find(textureName);
            if (it != meshes.end())
                mesh = it->second;
            else
            {
                m_meshes.emplace_back();
                mesh = &m_meshes.back();
                meshes[textureName] = mesh;
                mesh->texture = texMan.getModule(textureName);
            }

            int i0 = mesh->vertices.size();

            for (j = 0; j < numVertices2; j++)
            {
                mesh->vertices.push_back(vertices[indices[3 * j]]);
                mesh->texCoords.push_back(texCoords[j]);
                if (j > 1)
                {
                    mesh->indices.push_back(i0);
                    mesh->indices.push_back(i0 + j - 1);
                    mesh->indices.push_back(i0 + j);

                    Vector3D u = mesh->vertices[i0 + j] - mesh->vertices[i0];
                    Vector3D v = mesh->vertices[i0 + j - 1] - mesh->vertices[i0];

                    Vector3D n = glm::cross(u, v);
                    float l = glm::length(n);
                    if (l > 1e-5)
                        n /= l;
                    else
                        n = Vector3D(0);

                    for (k = 0; k < 1 + (j == 2 ? 2 : 0); k++)
                        mesh->normals.push_back(n);
                }
            }
        }
    }

    for (const Mesh &mesh : m_meshes)
        m_collisionMesh.addTriangles(mesh.vertices, mesh.normals, mesh.indices);
}


void Module::draw()
{
    for (Mesh &mesh : m_meshes)
    {
        if (!mesh.buffers.isValid())
        {
            // faces with less than three corners add vertices but no normals
            mesh.normals.resize(mesh.vertices.size(), Vector3D(0));
            mesh.buffers.create(int(mesh.vertices.size()), mesh.vertices.data(), mesh.normals.data(), mesh.texCoords.data(),
                                int(mesh.indices.size()), mesh.indices.data());
        }
        mesh.texture.bind();
        mesh.buffers.draw(0, -1, mesh.texture.isValid());
    }
}


void Module::addTriangles(const Matrix &transform, std::vector<Vector3D> &corners) const
{
    for (const Mesh &mesh : m_meshes)
        for (const std::uint16_t index : mesh.indices)
            corners.push_back(Vector3D(transform*Vector4D(mesh.vertices[index], 1)));
}


bool Module::intersect(const Vector3D &start, const Vector3D &dir, float radius, float &distance, Vector3D &normal)
{
    return m_collisionMesh.intersect(start, dir, radius, distance, normal);
}


} // namespace fight
