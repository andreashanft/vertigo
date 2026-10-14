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

#ifndef GAME_RENDERER_H
#define GAME_RENDERER_H

#include "util/deferreddeletable.h"
#include "util/geometry2d.h"
#include "util/geometry3d.h"
#include "input.hpp"


namespace game {

class Window;

class Renderer : public util::DeferredDeletable
{
public:
    Renderer();

public:
    virtual void draw() = 0;
    virtual void activate() {}
    virtual void deactivate() {}
    // the app is interrupted or goes to the background (on the iPad): a mission pauses
    virtual void pause() {}

public:
    void setWindow(Window *window);
    virtual bool isCursorVisible() const { return m_cursorVisible; }
    // typing text, e.g. a new game's name: only then do keys go through the system's text input
    // (which on macOS takes held letters for its accent menu, on iOS shows the keyboard)
    virtual bool wantsTextInput() const { return false; }
    // where the typed text shows, in the 640x480 image (on iOS the window moves it above the
    // on-screen keyboard)
    virtual Rect textInputArea() const { return Rect(); }
    virtual void setRect(const Rect &rect);
    Rect rect() const { return m_rect; }
    int width() const { return m_rect.width; }
    int height() const { return m_rect.height; }
    const Matrix& projectionMatrix() const { return m_projectionMatrix; }
    const Matrix& projectionMatrixInverted() const { return m_projectionMatrixInverted; }
    RectD rectOrtho() const { return m_rectOrtho; }
    PointD screenToImage(const PointD &pos);
    PointD imageToScreen(const PointD &pos);

public:
    virtual void keyPressEvent(input::KeyEvent *) {}
    virtual void keyReleaseEvent(input::KeyEvent *) {}
    virtual void mousePressEvent(input::MouseEvent *) {}
    virtual void mouseReleaseEvent(input::MouseEvent *) {}
    virtual void mouseMoveEvent(input::MouseEvent *) {}
    // a game controller's buttons, by default the keys of input::menuKey(); and a stick, where it
    // is now (beyond its dead zone, else 0)
    virtual void buttonPressEvent(input::Button button);
    virtual void buttonReleaseEvent(input::Button button);
    virtual void stickEvent(input::Stick, const Vector2D &) {}

protected:
    void showCursor();
    void hideCursor();
    void setupOrthographicMatrix(float w, float h);
    void setupGL(bool clear = true);
    Window* window() const { return m_window; }

protected:
    Matrix m_projectionMatrix;
    Matrix m_projectionMatrixInverted;

private:
    bool m_cursorVisible;
    Rect m_rect;
    RectD m_rectOrtho;
    Window *m_window;
};

} // namespace game

#endif // GAME_RENDERER_H
