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

#include "mesh.h"
#include "pipeline.h"
#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>

namespace gfx {


Mesh::~Mesh()
{
    destroy();
}


void Mesh::create(int vertexCount, const Vector3D *positions, const Vector3D *normals, const Vector2D *texCoords,
                  int indexCount, const std::uint16_t *indices)
{
    m_indexType = GL_UNSIGNED_SHORT;
    create(vertexCount, positions, normals, texCoords, indexCount, indices, sizeof(std::uint16_t));
}


void Mesh::create(int vertexCount, const Vector3D *positions, const Vector3D *normals, const Vector2D *texCoords,
                  int indexCount, const std::uint32_t *indices)
{
    m_indexType = GL_UNSIGNED_INT;
    create(vertexCount, positions, normals, texCoords, indexCount, indices, sizeof(std::uint32_t));
}


void Mesh::create(int vertexCount, const Vector3D *positions, const Vector3D *normals, const Vector2D *texCoords,
                  int indexCount, const void *indices, int indexSize)
{
    const unsigned int indexType = m_indexType;
    destroy();
    m_indexType = indexType;
    m_indexSize = indexSize;
    m_indexCount = indexCount;
    m_textured = texCoords != nullptr;

    // one buffer: all positions, then all normals, then all texture coordinates
    QOpenGLExtraFunctions *gl = QOpenGLContext::currentContext()->extraFunctions();
    const GLsizeiptr positionSize = vertexCount*sizeof(Vector3D);
    const GLsizeiptr normalSize = normals ? vertexCount*sizeof(Vector3D) : 0;
    const GLsizeiptr texCoordSize = texCoords ? vertexCount*sizeof(Vector2D) : 0;
    gl->glGenVertexArrays(1, &m_vertexArray);
    gl->glGenBuffers(1, &m_vertices);
    gl->glGenBuffers(1, &m_indices);
    gl->glBindVertexArray(m_vertexArray);
    gl->glBindBuffer(GL_ARRAY_BUFFER, m_vertices);
    gl->glBufferData(GL_ARRAY_BUFFER, positionSize + normalSize + texCoordSize, nullptr, GL_STATIC_DRAW);
    gl->glBufferSubData(GL_ARRAY_BUFFER, 0, positionSize, positions);
    gl->glEnableVertexAttribArray(Pipeline::Position);
    gl->glVertexAttribPointer(Pipeline::Position, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    if (normals)
    {
        gl->glBufferSubData(GL_ARRAY_BUFFER, positionSize, normalSize, normals);
        gl->glEnableVertexAttribArray(Pipeline::Normal);
        gl->glVertexAttribPointer(Pipeline::Normal, 3, GL_FLOAT, GL_FALSE, 0, reinterpret_cast<void *>(positionSize));
    }
    if (texCoords)
    {
        gl->glBufferSubData(GL_ARRAY_BUFFER, positionSize + normalSize, texCoordSize, texCoords);
        gl->glEnableVertexAttribArray(Pipeline::TexCoord);
        gl->glVertexAttribPointer(Pipeline::TexCoord, 2, GL_FLOAT, GL_FALSE, 0, reinterpret_cast<void *>(positionSize + normalSize));
    }
    gl->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_indices);
    gl->glBufferData(GL_ELEMENT_ARRAY_BUFFER, GLsizeiptr(indexCount)*indexSize, indices, GL_STATIC_DRAW);
    gl->glBindVertexArray(0);
    gl->glBindBuffer(GL_ARRAY_BUFFER, 0);
    gl->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}


void Mesh::destroy()
{
    // the buffers went with the context otherwise
    QOpenGLContext *context = QOpenGLContext::currentContext();
    if (m_vertexArray != 0 && context)
    {
        QOpenGLExtraFunctions *gl = context->extraFunctions();
        gl->glDeleteVertexArrays(1, &m_vertexArray);
        gl->glDeleteBuffers(1, &m_vertices);
        gl->glDeleteBuffers(1, &m_indices);
    }
    m_vertexArray = m_vertices = m_indices = 0;
    m_indexCount = 0;
}


void Mesh::draw(int first, int count, bool textured)
{
    if (count < 0)
        count = m_indexCount - first;
    if (!isValid() || count <= 0)
        return;
    Pipeline::apply(m_textured && textured);
    QOpenGLExtraFunctions *gl = QOpenGLContext::currentContext()->extraFunctions();
    gl->glBindVertexArray(m_vertexArray);
    gl->glDrawElements(GL_TRIANGLES, count, m_indexType, reinterpret_cast<void *>(qintptr(first)*m_indexSize));
    gl->glBindVertexArray(0);
}


} // namespace gfx
