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

#ifndef GAME_WINDOW_H
#define GAME_WINDOW_H

#include <QCursor>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QStringList>
#include <QWindow>

namespace game {

class Renderer;

// The game's window, with the one OpenGL context everything draws with. The
// context is current from the start: on an offscreen surface while the window
// isn't shown (loading before the first frame, headless test runs), on the
// window while it draws.
class Window : public QWindow
{
public:
    Window();

public:
    void setRenderer(Renderer *renderer);
    Renderer* renderer() const { return m_renderer; }
    // saves the next frame as an image file
    void saveScreenshot(const QString &fileName);
    // shows what has been drawn so far, e.g. a loading screen before a long load
    void swapBuffers();
    QRect rect() const { return QRect(QPoint(0, 0), size()); }

protected:
    bool event(QEvent *event) override;
    void exposeEvent(QExposeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void moveEvent(QMoveEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void render();
    void saveSettings();
    void loadSettings();

private:
    QOpenGLContext m_context;
    QOffscreenSurface m_offscreen;
    QSize m_viewport;
    QRect m_geometry;   // the last one outside full screen
    QCursor m_cursor;
    Renderer *m_renderer;
    QStringList m_screenshots;
};

} // namespace game

#endif // GAME_WINDOW_H
