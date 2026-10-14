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

#ifndef GFX_FRAMEBUFFER_HPP
#define GFX_FRAMEBUFFER_HPP

namespace gfx {

// Drawing into a texture (RGBA, sampled with GL_NEAREST) with a depth buffer of its own
class Framebuffer
{
public:
    // needs a current OpenGL context; isValid() is false if the driver refuses it
    Framebuffer(int width, int height);
    ~Framebuffer();
    Framebuffer(const Framebuffer &) = delete;
    Framebuffer& operator=(const Framebuffer &) = delete;

public:
    bool isValid() const { return m_valid; }
    int width() const { return m_width; }
    int height() const { return m_height; }
    unsigned int texture() const { return m_texture; }
    // draws into it from now on
    void bind();
    // back to what was drawn into before bind() (the window's framebuffer isn't 0 everywhere)
    void release();

private:
    int m_width;
    int m_height;
    bool m_valid = false;
    unsigned int m_framebuffer = 0;
    unsigned int m_texture = 0;
    unsigned int m_depth = 0;
    int m_previous = 0;
};

} // namespace gfx

#endif // GFX_FRAMEBUFFER_HPP
