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

#include "renderer.h"
#include "gfx/pipeline.h"
#include <qopengl.h>
#include <glm/vec4.hpp>
#include <glm/matrix.hpp>
#include <glm/gtc/matrix_transform.hpp>


namespace game {

Renderer::Renderer() :
    m_cursorVisible(true),
    m_window(NULL)
{

}


void Renderer::setWindow(Window *window)
{
    m_window = window;
}


void Renderer::showCursor()
{
    m_cursorVisible = true;
}


void Renderer::hideCursor()
{
    m_cursorVisible = false;
}


void Renderer::setRect(const Rect &rect)
{
    m_rect = rect;

    setupOrthographicMatrix(640, 480);
}


void Renderer::setupOrthographicMatrix(float w, float h)
{
    const float ratioW = width() / w;
    const float ratioH = height() / h;

    float left = 0.0f;
    float top = 0.0f;
    float scale;
    if (ratioW < ratioH) {
        scale = ratioW;
        top = ((height() / scale) - h) / 2;
    }
    else {
        scale = ratioH;
        left = ((width() / scale) - w) / 2;
    }

    m_rectOrtho = RectD(-left, -top, w + 2*left, h + 2*top);

    m_projectionMatrix = glm::ortho(m_rectOrtho.left(), m_rectOrtho.right(), m_rectOrtho.bottom(), m_rectOrtho.top());
    m_projectionMatrixInverted = glm::inverse(m_projectionMatrix);
}


void Renderer::setupGL(bool clear)
{
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    if (clear)
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glFrontFace(GL_CCW);
    gfx::Pipeline::setLighting(false);
    gfx::Pipeline::setFog(false);
    gfx::Pipeline::setProjection(m_projectionMatrix);
    gfx::Pipeline::setModelView(Matrix(1));
}


PointD Renderer::screenToImage(const PointD &pos)
{
    Vector4D p = m_projectionMatrixInverted * Vector4D(pos.x / width() * 2.0f - 1.0f, 1.0f - pos.y / height() * 2.0f, 0, 1);
    return PointD(p.x, p.y);
}


PointD Renderer::imageToScreen(const PointD &pos)
{
    const Vector4D p = m_projectionMatrix * Vector4D(pos.x, pos.y, 0, 1);
    return PointD((p.x + 1.0f)/2.0f*width(), (1.0f - p.y)/2.0f*height());
}


} // namespace game
