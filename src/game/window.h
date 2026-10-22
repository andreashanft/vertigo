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

#include "input.hpp"
#include "util/geometry3d.h"

#include <deque>
#include <string>
#include <vector>

struct SDL_Cursor;
struct SDL_Window;
typedef struct SDL_GLContextState *SDL_GLContext;
union SDL_Event;

namespace game {

class Renderer;

// The game's window, with the one OpenGL context everything draws with. It exists (hidden)
// from the start, so that the context is there for loading before the first frame and for
// headless test runs.
class Window
{
public:
    // without focus for automated runs
    explicit Window(bool focusable = true);
    ~Window();

public:
    void show();
    // saves the settings and hides the window
    void close();
    bool isVisible() const { return m_shown && !m_closed; }

    void setRenderer(Renderer *renderer);
    Renderer* renderer() const { return m_renderer; }
    // the size in the window's coordinates (points), and how many pixels one has
    int width() const;
    int height() const;
    double devicePixelRatio() const;

    // what the platform reports; sends input on to the renderer
    void handle(const SDL_Event &event);
    // input that test runs make up, delivered before the next frame as if it came from the
    // platform
    enum class EventType { KeyPress, KeyRelease, MousePress, MouseRelease, MouseMove, TouchPress, TouchMove, TouchRelease };
    void post(EventType type, const input::KeyEvent &event);
    void post(EventType type, const input::MouseEvent &event);
    void post(EventType type, const input::TouchEvent &event);
    void deliverPosted();
    // test runs with a virtual game controller: only it counts, not those of the player
    void setOnlyGamepad(unsigned int id) { m_onlyGamepad = id; }

    // draws a frame (when shown)
    void render();
    // saves the next frame as an image file, a path in UTF-8
    void saveScreenshot(const std::string &fileName);
    // shows what has been drawn so far, e.g. a loading screen before a long load
    void swapBuffers();

private:
    void keyPressEvent(input::KeyEvent *event);
    void buttonEvent(input::Button button, bool pressed);
    void axisEvent(int axis, float value);
    void releaseGamepad();
    void touchEvent(EventType type, input::TouchEvent *event);
    void saveSettings();
    void loadSettings();
    void toggleFullScreen();
    void updateFrameRate();

private:
    struct Posted
    {
        EventType type;
        input::KeyEvent key;
        input::MouseEvent mouse;
        input::TouchEvent touch;
    };

    SDL_Window *m_window = nullptr;
    SDL_GLContext m_context = nullptr;
    SDL_Cursor *m_cursor = nullptr;
    bool m_shown = false;
    bool m_closed = false;
    // the app in the background (on the iPad), where nothing may draw: iOS ends apps that use
    // OpenGL there
    bool m_background = false;
    bool m_cursorVisible = true;
    int m_viewportWidth = 0;
    int m_viewportHeight = 0;
    int m_buttons = input::NoButton;
    // the game controllers' buttons held, their axes (SDL's order: the sticks' x and y, the
    // triggers; from -1 to 1) and the sticks beyond their dead zone
    bool m_padButtons[input::ButtonCount] = {};
    float m_axes[6] = {};
    Vector2D m_sticks[2] = { Vector2D(0), Vector2D(0) };
    unsigned int m_onlyGamepad = 0;
    // the last geometry outside full screen: x, y, width, height
    int m_geometry[4] = { 0, 0, 0, 0 };
    Renderer *m_renderer = nullptr;
    std::vector<std::string> m_screenshots;
    std::deque<Posted> m_posted;
};

} // namespace game

#endif // GAME_WINDOW_H
