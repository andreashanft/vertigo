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

#ifndef GFX_PIPELINE_H
#define GFX_PIPELINE_H

#include "util/geometry2d.h"
#include "util/geometry3d.h"

namespace gfx {

// What the game used of OpenGL's fixed-function pipeline, drawn with one shader that runs on
// OpenGL 3.2 core and OpenGL ES 3: the model-view matrix stack, a colour, per-vertex lighting
// with one spot light, linear fog, the alpha test and projective texture coordinates, plus a few
// point lights of a short reach, lit per fragment so that they show between the vertices. Depth,
// culling and blending are still set with plain OpenGL calls. Everything is drawn with the
// texture bound to unit 0, unless the vertices have no texture coordinates or the caller has no
// texture (OpenGL 1 drew in the colour alone without a texture image); then a white texture is
// bound instead.
class Pipeline
{
public:
    struct Vertex
    {
        Vector3D position;
        Vector2D texCoord;
    };

    // as glLight with the default material (ambient 0.2, diffuse 0.8, no specular light)
    struct Light
    {
        Vector3D position;      // in eye space
        Vector3D direction;     // in eye space
        Vector3D ambient;
        Vector3D diffuse;
        float cutoff;           // degrees
        float exponent;
        float constantAttenuation;
        float quadraticAttenuation;
    };

    // a glow of a short reach: the colour, faded with the square of the distance to nothing at
    // the radius, and lit per fragment, so a radius below the vertex spacing still shows
    struct PointLight
    {
        Vector3D position;      // in eye space
        Vector3D color;
        float radius;
    };
    static constexpr int MaxPointLights = 4;

    // linear in the distance along the view direction: the colour, multiplied with the gain,
    // blends into the fog's colour from start to end and the result is clamped. So a colour
    // with negative channels is a fog that takes a channel away, like water takes the red
    struct Fog
    {
        Vector3D color;
        float start;
        float end;
        float gain = 1;
    };

    // the game's shading tables: a brightness per vertex, an ambient part that falls off to
    // nothing at ambientEnd plus a light at the origin that falls off from lightFront to
    // lightBack, raised to lightPower, times the cosine between the normal and the direction to
    // the origin (not clamped, so a slope turned away takes from the ambient part). In the
    // fragment, the brightness interpolated between the vertices picks one of the rows, 0 at
    // nothing and rows - 1 at one and above, and row r blends the fog's colour into the colour
    // times the fog's gain by (r + 1)/rows, in place of the fog by distance. Point lights raise
    // the brightness by their strength instead of adding their colour
    struct TableShading
    {
        Vector3D origin;        // in eye space
        float ambient;
        float ambientEnd;
        float light;
        float lightFront;
        float lightBack;
        float lightPower;
        int rows;
    };

public:
    static void setProjection(const Matrix &matrix);
    static const Matrix& projection();
    // the top of the model-view stack
    static void setModelView(const Matrix &matrix);
    static const Matrix& modelView();
    static void pushMatrix();
    static void popMatrix();
    static void translate(const Vector3D &offset);
    static void translate(float x, float y, float z) { translate(Vector3D(x, y, z)); }
    static void rotate(float degrees, const Vector3D &axis);
    static void scale(float factor);
    static void multiply(const Matrix &matrix);

    // the colour without lighting, multiplied with the texture (white by default)
    static void setColor(const Vector4D &color);
    static const Vector4D& color();
    // the normal of vertices that have none, in model space (0, 0, 1 by default)
    static void setNormal(const Vector3D &normal);
    static void setLighting(bool enabled);
    static bool lighting();
    // the global ambient light, multiplied with the material's 0.2
    static void setAmbient(const Vector3D &ambient);
    static void setLight(const Light &light);
    // up to MaxPointLights, added to the lit colour; none by default and with a count of 0
    static void setPointLights(const PointLight *lights, int count);
    static void setFog(bool enabled);
    static void setFog(const Fog &fog);
    // replaces the fog's blend by distance while enabled; the fog's colour and gain still apply
    static void setTableShading(bool enabled);
    static void setTableShading(const TableShading &shading);
    static bool tableShading();
    // drops fragments without alpha
    static void setAlphaTest(bool enabled);
    // texture coordinates s/q and t/q from the model position: rows 0, 1 and 3 of the matrix give
    // s, t and q. Fragments with q below near and outside the texture are dropped.
    static void setTextureProjection(bool enabled, const Matrix &matrix = Matrix(1), float near = 0);

    // groups of four vertices, each a quad (two triangles 0 1 2 and 0 2 3)
    static void drawQuads(const Vertex *vertices, int count, bool textured = true);
    static void drawQuad(const RectF &dstRect, const RectF &texRect, bool textured = true);
    static void drawLines(const Vector3D *positions, int count);

    // for gfx::Mesh: binds the shader and sets its uniforms for a draw
    static void apply(bool textured);
    // the attribute locations of the shader
    enum Attribute { Position = 0, Normal = 1, TexCoord = 2 };

private:
    static void drawStream(unsigned int mode, const Vertex *vertices, int count, bool textured, bool quads);
};

} // namespace gfx

#endif // GFX_PIPELINE_H
