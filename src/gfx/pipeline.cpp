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

#include "pipeline.h"
#include "gl.hpp"
#include <SDL3/SDL_video.h>
#include "gfx/gl.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cstddef>
#include <iostream>
#include <vector>


namespace gfx {


namespace {

// Per-vertex lighting as in OpenGL 1.x: the scene colour (global ambient times the material's
// ambient) plus one spot light, attenuated and clamped. The point lights are added per fragment,
// from the eye-space position and normal passed on. The fog uses the distance along the view
// direction, as most fixed-function implementations did.
const char *VertexShader = R"(
in vec3 position;
in vec3 normal;
in vec2 texCoord;

uniform mat4 modelView;
uniform mat4 projection;
uniform mat3 normalMatrix;
uniform vec4 color;
uniform bool lighting;
uniform vec3 sceneColor;
uniform vec3 lightPosition;
uniform vec3 lightDirection;
uniform vec3 lightAmbient;
uniform vec3 lightDiffuse;
uniform float lightCosCutoff;
uniform float lightExponent;
uniform vec2 lightAttenuation;
uniform bool textureProjection;
uniform mat4 textureMatrix;

out vec4 vColor;
out vec3 vTexCoord;
out float vDepth;
out vec3 vEye;
out vec3 vNormal;

void main()
{
    vec4 eye = modelView*vec4(position, 1.0);
    gl_Position = projection*eye;
    vDepth = -eye.z;
    vEye = eye.xyz;
    vNormal = vec3(0.0);
    vTexCoord = textureProjection ? (textureMatrix*vec4(position, 1.0)).xyw : vec3(texCoord, 1.0);
    if (lighting)
    {
        // zero normals (degenerate faces) stay zero
        vec3 n = normalMatrix*normal;
        float size = length(n);
        n = size > 0.0 ? n/size : n;
        vNormal = n;
        vec3 toLight = lightPosition - eye.xyz;
        float d = max(length(toLight), 1e-6);
        vec3 l = toLight/d;
        float spot = dot(-l, lightDirection);
        float factor = spot >= lightCosCutoff ? pow(max(spot, 0.0), lightExponent)/(lightAttenuation.x + lightAttenuation.y*d*d) : 0.0;
        vColor = vec4(min(sceneColor + factor*(lightAmbient + max(dot(n, l), 0.0)*lightDiffuse), vec3(1.0)), 1.0);
    }
    else
        vColor = color;
}
)";

const char *FragmentShader = R"(
in vec4 vColor;
in vec3 vTexCoord;
in float vDepth;
in vec3 vEye;
in vec3 vNormal;

uniform sampler2D tex;
uniform bool textureProjection;
uniform float textureNear;
uniform bool alphaTest;
uniform bool fog;
uniform vec3 fogColor;
uniform vec2 fogRange;
uniform int pointLights;
uniform vec4 pointLightPosition[4];     // eye space, w = 1/radius^2
uniform vec3 pointLightColor[4];

out vec4 fragColor;

void main()
{
    vec4 c = vColor;
    if (pointLights > 0)
    {
        // faded with the square of the distance; a third of it regardless of the surface's
        // direction, the rest by the angle (a zero normal takes only that third)
        for (int i = 0; i < 4; i++)
        {
            if (i >= pointLights)
                break;
            vec3 toLight = pointLightPosition[i].xyz - vEye;
            float d2 = dot(toLight, toLight);
            float fade = max(1.0 - d2*pointLightPosition[i].w, 0.0);
            float facing = max(dot(vNormal, toLight*inversesqrt(max(d2, 1e-6))), 0.0);
            c.rgb += pointLightColor[i]*(fade*fade*(0.35 + 0.65*facing));
        }
        c.rgb = min(c.rgb, vec3(1.0));
    }
    if (textureProjection)
    {
        vec2 st = vTexCoord.xy/vTexCoord.z;
        c *= texture(tex, st);
        // nothing behind the near plane, where the projection mirrors, and nothing outside the texture
        if (vTexCoord.z < textureNear || any(lessThan(st, vec2(0.0))) || any(greaterThan(st, vec2(1.0))))
            discard;
    }
    else
        c *= texture(tex, vTexCoord.xy);
    if (alphaTest && c.a <= 0.0)
        discard;
    if (fog)
        c.rgb = mix(fogColor, c.rgb, clamp((fogRange.y - vDepth)/(fogRange.y - fogRange.x), 0.0, 1.0));
    fragColor = c;
}
)";

// quads are drawn from a stream of vertices with a fixed index buffer
const int MaxQuads = 16384;


struct State
{
    Matrix projection = Matrix(1);
    std::vector<Matrix> modelView = { Matrix(1) };
    Vector4D color = Vector4D(1);
    Vector3D normal = Vector3D(0, 0, 1);
    bool lighting = false;
    // OpenGL's defaults
    Vector3D ambient = Vector3D(0.2f);
    Pipeline::Light light = { Vector3D(0, 0, 1), Vector3D(0, 0, -1), Vector3D(0), Vector3D(1), 180, 0, 1, 0 };
    Pipeline::PointLight pointLights[Pipeline::MaxPointLights];
    int pointLightCount = 0;
    bool fog = false;
    Pipeline::Fog fogParameters = { Vector3D(0), 0, 1 };
    bool alphaTest = false;
    bool textureProjection = false;
    Matrix textureMatrix = Matrix(1);
    float textureNear = 0;
    // the light and the fog are uploaded on the next draw
    bool parametersChanged = true;
};

State& state()
{
    static State s;
    return s;
}


struct Uniforms
{
    int modelView, projection, normalMatrix, color, lighting, sceneColor;
    int lightPosition, lightDirection, lightAmbient, lightDiffuse, lightCosCutoff, lightExponent, lightAttenuation;
    int pointLights, pointLightPosition, pointLightColor;
    int textureProjection, textureMatrix, textureNear, alphaTest, fog, fogColor, fogRange;
};

// the shader and the buffers for drawing from client memory, made for the current context
struct Gpu
{
    SDL_GLContext context = nullptr;
    GLuint program = 0;
    Uniforms uniforms;
    GLuint streamArray = 0;
    GLuint streamBuffer = 0;
    GLuint quadIndices = 0;
    // for drawing without a texture (some drivers warn about texture 0 even if it isn't sampled)
    GLuint white = 0;
};

Gpu g_gpu;


GLuint compile(GLenum type, const std::string &source)
{
    const GLuint shader = glCreateShader(type);
    const char *text = source.c_str();
    glShaderSource(shader, 1, &text, nullptr);
    glCompileShader(shader);
    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok)
    {
        char log[4096];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        std::cerr << "Cannot compile the " << (type == GL_VERTEX_SHADER ? "vertex" : "fragment") << " shader: " << log << std::endl;
    }
    return shader;
}


Gpu& gpu()
{
    SDL_GLContext context = SDL_GL_GetCurrentContext();
    if (g_gpu.context == context)
        return g_gpu;

    g_gpu = Gpu();
    g_gpu.context = context;

    // GLSL 1.50 is OpenGL 3.2's
    const std::string header = gl::isES() ? "#version 300 es\nprecision highp float;\n" : "#version 150\n";
    const GLuint vertexShader = compile(GL_VERTEX_SHADER, header + VertexShader);
    const GLuint fragmentShader = compile(GL_FRAGMENT_SHADER, header + FragmentShader);
    const GLuint program = g_gpu.program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glBindAttribLocation(program, Pipeline::Position, "position");
    glBindAttribLocation(program, Pipeline::Normal, "normal");
    glBindAttribLocation(program, Pipeline::TexCoord, "texCoord");
    // OpenGL ES has only one output; OpenGL 3.2 binds it without a location of its own, too, but
    // only by convention
    if (!gl::isES() && glBindFragDataLocation)
        glBindFragDataLocation(program, 0, "fragColor");
    glLinkProgram(program);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    GLint ok = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok)
    {
        char log[4096];
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        std::cerr << "Cannot link the shader: " << log << std::endl;
    }

    Uniforms &u = g_gpu.uniforms;
    auto location = [program](const char *name) { return glGetUniformLocation(program, name); };
    u.modelView = location("modelView");
    u.projection = location("projection");
    u.normalMatrix = location("normalMatrix");
    u.color = location("color");
    u.lighting = location("lighting");
    u.sceneColor = location("sceneColor");
    u.lightPosition = location("lightPosition");
    u.lightDirection = location("lightDirection");
    u.lightAmbient = location("lightAmbient");
    u.lightDiffuse = location("lightDiffuse");
    u.lightCosCutoff = location("lightCosCutoff");
    u.lightExponent = location("lightExponent");
    u.lightAttenuation = location("lightAttenuation");
    u.pointLights = location("pointLights");
    u.pointLightPosition = location("pointLightPosition");
    u.pointLightColor = location("pointLightColor");
    u.textureProjection = location("textureProjection");
    u.textureMatrix = location("textureMatrix");
    u.textureNear = location("textureNear");
    u.alphaTest = location("alphaTest");
    u.fog = location("fog");
    u.fogColor = location("fogColor");
    u.fogRange = location("fogRange");
    glUseProgram(program);
    glUniform1i(location("tex"), 0);

    // the stream: positions and texture coordinates interleaved, normals from setNormal()
    glGenVertexArrays(1, &g_gpu.streamArray);
    glGenBuffers(1, &g_gpu.streamBuffer);
    glGenBuffers(1, &g_gpu.quadIndices);
    glBindVertexArray(g_gpu.streamArray);
    glBindBuffer(GL_ARRAY_BUFFER, g_gpu.streamBuffer);
    glEnableVertexAttribArray(Pipeline::Position);
    glVertexAttribPointer(Pipeline::Position, 3, GL_FLOAT, GL_FALSE, sizeof(Pipeline::Vertex), reinterpret_cast<void *>(offsetof(Pipeline::Vertex, position)));
    glEnableVertexAttribArray(Pipeline::TexCoord);
    glVertexAttribPointer(Pipeline::TexCoord, 2, GL_FLOAT, GL_FALSE, sizeof(Pipeline::Vertex), reinterpret_cast<void *>(offsetof(Pipeline::Vertex, texCoord)));
    std::vector<GLushort> indices;
    indices.reserve(MaxQuads*6);
    for (int i = 0; i < MaxQuads; i++)
        for (int corner : { 0, 1, 2, 0, 2, 3 })
            indices.push_back(GLushort(i*4 + corner));
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_gpu.quadIndices);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, GLsizeiptr(indices.size()*sizeof(GLushort)), indices.data(), GL_STATIC_DRAW);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    const std::uint32_t white = 0xffffffff;
    GLint bound = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &bound);
    glGenTextures(1, &g_gpu.white);
    glBindTexture(GL_TEXTURE_2D, g_gpu.white);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &white);
    glBindTexture(GL_TEXTURE_2D, GLuint(bound));

    state().parametersChanged = true;
    return g_gpu;
}

} // namespace


void Pipeline::setProjection(const Matrix &matrix)
{
    state().projection = matrix;
}


const Matrix& Pipeline::projection()
{
    return state().projection;
}


void Pipeline::setModelView(const Matrix &matrix)
{
    state().modelView.back() = matrix;
}


const Matrix& Pipeline::modelView()
{
    return state().modelView.back();
}


void Pipeline::pushMatrix()
{
    State &s = state();
    s.modelView.push_back(s.modelView.back());
}


void Pipeline::popMatrix()
{
    State &s = state();
    if (s.modelView.size() > 1)
        s.modelView.pop_back();
    else
        std::cerr << "Pipeline::popMatrix() without pushMatrix()" << std::endl;
}


void Pipeline::translate(const Vector3D &offset)
{
    Matrix &m = state().modelView.back();
    m = glm::translate(m, offset);
}


void Pipeline::rotate(float degrees, const Vector3D &axis)
{
    Matrix &m = state().modelView.back();
    m = glm::rotate(m, glm::radians(degrees), axis);
}


void Pipeline::scale(float factor)
{
    Matrix &m = state().modelView.back();
    m = glm::scale(m, Vector3D(factor));
}


void Pipeline::multiply(const Matrix &matrix)
{
    Matrix &m = state().modelView.back();
    m = m*matrix;
}


void Pipeline::setColor(const Vector4D &color)
{
    state().color = color;
}


const Vector4D& Pipeline::color()
{
    return state().color;
}


void Pipeline::setNormal(const Vector3D &normal)
{
    state().normal = normal;
}


void Pipeline::setLighting(bool enabled)
{
    state().lighting = enabled;
}


bool Pipeline::lighting()
{
    return state().lighting;
}


void Pipeline::setAmbient(const Vector3D &ambient)
{
    State &s = state();
    s.ambient = ambient;
    s.parametersChanged = true;
}


void Pipeline::setLight(const Light &light)
{
    State &s = state();
    s.light = light;
    s.parametersChanged = true;
}


void Pipeline::setPointLights(const PointLight *lights, int count)
{
    State &s = state();
    s.pointLightCount = std::clamp(count, 0, MaxPointLights);
    std::copy(lights, lights + s.pointLightCount, s.pointLights);
    s.parametersChanged = true;
}


void Pipeline::setFog(bool enabled)
{
    state().fog = enabled;
}


void Pipeline::setFog(const Fog &fog)
{
    State &s = state();
    s.fogParameters = fog;
    s.parametersChanged = true;
}


void Pipeline::setAlphaTest(bool enabled)
{
    state().alphaTest = enabled;
}


void Pipeline::setTextureProjection(bool enabled, const Matrix &matrix, float near)
{
    State &s = state();
    s.textureProjection = enabled;
    s.textureMatrix = matrix;
    s.textureNear = near;
}


void Pipeline::apply(bool textured)
{
    Gpu &g = gpu();
    State &s = state();
    const Uniforms &u = g.uniforms;
    glUseProgram(g.program);

    if (s.parametersChanged)
    {
        // the default material: ambient 0.2, diffuse 0.8
        const Light &light = s.light;
        glUniform3fv(u.sceneColor, 1, glm::value_ptr(s.ambient*0.2f));
        glUniform3fv(u.lightPosition, 1, glm::value_ptr(light.position));
        glUniform3fv(u.lightDirection, 1, glm::value_ptr(glm::normalize(light.direction)));
        glUniform3fv(u.lightAmbient, 1, glm::value_ptr(light.ambient*0.2f));
        glUniform3fv(u.lightDiffuse, 1, glm::value_ptr(light.diffuse*0.8f));
        // a cutoff of 180 degrees is a point light
        glUniform1f(u.lightCosCutoff, light.cutoff >= 180 ? -2.0f : std::cos(glm::radians(light.cutoff)));
        glUniform1f(u.lightExponent, light.exponent);
        glUniform2f(u.lightAttenuation, light.constantAttenuation, light.quadraticAttenuation);
        if (s.pointLightCount > 0)
        {
            Vector4D positions[MaxPointLights];
            Vector3D colors[MaxPointLights];
            for (int i = 0; i < s.pointLightCount; i++)
            {
                const PointLight &point = s.pointLights[i];
                const float radius = std::max(point.radius, 1e-3f);
                positions[i] = Vector4D(point.position, 1.0f/(radius*radius));
                colors[i] = point.color;
            }
            glUniform4fv(u.pointLightPosition, s.pointLightCount, glm::value_ptr(positions[0]));
            glUniform3fv(u.pointLightColor, s.pointLightCount, glm::value_ptr(colors[0]));
        }
        glUniform3fv(u.fogColor, 1, glm::value_ptr(s.fogParameters.color));
        glUniform2f(u.fogRange, s.fogParameters.start, s.fogParameters.end);
        s.parametersChanged = false;
    }

    const Matrix &modelView = s.modelView.back();
    glUniformMatrix4fv(u.modelView, 1, GL_FALSE, glm::value_ptr(modelView));
    glUniformMatrix4fv(u.projection, 1, GL_FALSE, glm::value_ptr(s.projection));
    glUniform4fv(u.color, 1, glm::value_ptr(s.color));
    glUniform1i(u.lighting, s.lighting);
    // the point lights only where lit; with none the fragment shader skips them
    glUniform1i(u.pointLights, s.lighting ? s.pointLightCount : 0);
    if (s.lighting)
    {
        const glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(modelView)));
        glUniformMatrix3fv(u.normalMatrix, 1, GL_FALSE, glm::value_ptr(normalMatrix));
    }
    // the colour alone without a texture
    if (!textured)
        glBindTexture(GL_TEXTURE_2D, g.white);
    glUniform1i(u.textureProjection, s.textureProjection);
    if (s.textureProjection)
    {
        glUniformMatrix4fv(u.textureMatrix, 1, GL_FALSE, glm::value_ptr(s.textureMatrix));
        glUniform1f(u.textureNear, s.textureNear);
    }
    glUniform1i(u.alphaTest, s.alphaTest);
    glUniform1i(u.fog, s.fog);
    // the value of the normal attribute where there is no array of normals
    glVertexAttrib3f(Normal, s.normal.x, s.normal.y, s.normal.z);
}


void Pipeline::drawStream(unsigned int mode, const Vertex *vertices, int count, bool textured, bool quads)
{
    if (count <= 0)
        return;
    apply(textured);
    Gpu &g = gpu();
    glBindVertexArray(g.streamArray);
    glBindBuffer(GL_ARRAY_BUFFER, g.streamBuffer);
    for (int first = 0; first < count; )
    {
        // a new buffer each time, so the driver needn't wait for the draws before
        const int n = quads ? std::min(count - first, MaxQuads*4)/4*4 : count;
        if (n == 0)
            break;
        glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(n*sizeof(Vertex)), vertices + first, GL_STREAM_DRAW);
        if (quads)
            glDrawElements(GL_TRIANGLES, n/4*6, GL_UNSIGNED_SHORT, nullptr);
        else
            glDrawArrays(mode, 0, n);
        first += n;
    }
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}


void Pipeline::drawQuads(const Vertex *vertices, int count, bool textured)
{
    drawStream(GL_TRIANGLES, vertices, count, textured, true);
}


void Pipeline::drawQuad(const RectF &dstRect, const RectF &texRect, bool textured)
{
    const Vertex vertices[4] = {
        { Vector3D(dstRect.left(), dstRect.top(), 0), Vector2D(texRect.left(), texRect.top()) },
        { Vector3D(dstRect.left(), dstRect.bottom(), 0), Vector2D(texRect.left(), texRect.bottom()) },
        { Vector3D(dstRect.right(), dstRect.bottom(), 0), Vector2D(texRect.right(), texRect.bottom()) },
        { Vector3D(dstRect.right(), dstRect.top(), 0), Vector2D(texRect.right(), texRect.top()) },
    };
    drawQuads(vertices, 4, textured);
}


void Pipeline::drawLines(const Vector3D *positions, int count)
{
    std::vector<Vertex> vertices(count);
    for (int i = 0; i < count; i++)
        vertices[i] = { positions[i], Vector2D(0) };
    drawStream(GL_LINES, vertices.data(), count, false, false);
}


} // namespace gfx
