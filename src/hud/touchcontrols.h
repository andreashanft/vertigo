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

#ifndef HUD_TOUCHCONTROLS_H
#define HUD_TOUCHCONTROLS_H


#include "game/input.hpp"
#include "gfx/texture.h"
#include "util/geometry2d.h"
#include "util/geometry3d.h"

#include <functional>
#include <vector>


namespace gfx { class Font; }


namespace hud {


// The controls of a mission on a touch screen, over the whole window: a stick that appears where
// a finger touches the left part, and buttons that stand for keys, along the edges and in the
// corners (where the cockpit art has its frame and the thumbs reach). A finger elsewhere turns
// the head in the 3D cockpit. Laid out in units of a view at least 480 high, the size of the
// HUD's text; on a larger screen they keep the size they have on an 11-inch iPad, in the
// corners. Mirrored, the stick is on the right and the buttons are the other way round. A short
// tap off the buttons is also passed on, for what lies behind the controls.
class TouchControls
{
public:
    struct Handlers
    {
        // a button pressed or let go: the key it stands for, and its number
        std::function<void(const input::KeyEvent &key, int button)> press;
        std::function<void(const input::KeyEvent &key, int button)> release;
        // where the stick is (x right, y down, each from -1 to 1)
        std::function<void(const Vector2D &stick)> stick;
        // a finger dragged elsewhere, in the window's coordinates
        std::function<void(const PointD &position)> lookStart;
        std::function<void(const PointD &position)> lookMove;
        std::function<void()> lookEnd;
        // a finger off the buttons lifted soon near where it came down (it may have worked the
        // stick or the head meanwhile), in the window's coordinates
        std::function<void(const PointD &position)> tap;
    };

public:
    TouchControls(gfx::Font &font, gfx::Font &fontPressed, const Handlers &handlers);

public:
    // the options, kept in the settings: mirrored, and the stick pushed up for down
    static bool isMirrored();
    static void setMirrored(bool mirrored);
    static bool isStickInverted();
    static void setStickInverted(bool inverted);

    // the window's size in its coordinates
    void setSize(double width, double height);
    void press(long long finger, const PointD &position);
    void move(long long finger, const PointD &position);
    void release(long long finger);
    // lets go of everything without telling, and takes the options, for a new mission
    void reset();
    // the time passed since the last call, for the buttons held
    void update(float seconds);
    // with the projection set to the window and the view's size
    void draw();
    // the view's size
    float width() const { return m_width; }
    float height() const { return m_height; }

private:
    struct Button
    {
        const char *label;
        input::KeyEvent key;
        // from the left (x >= 0) or right edge (x < 0), the top (y >= 0) or the bottom (y < 0)
        Vector2D place;
        float radius;
        long long finger;
        // held for a while, the button stands for this key instead, and for its own only when
        // let go before
        input::KeyEvent holdKey = input::KeyEvent();
        float held = -1;
    };

    Vector2D center(const Button &button) const;
    Vector2D toView(const PointD &position) const;
    Vector2D stick() const;
    void drawCircle(gfx::Texture &texture, const Vector2D &center, float radius, const Vector4D &color);

private:
    gfx::Font &m_font;
    gfx::Font &m_fontPressed;
    Handlers m_handlers;
    std::vector<Button> m_buttons;
    gfx::Texture m_disc;
    gfx::Texture m_ring;
    float m_scale = 1;      // window coordinates per view unit
    float m_width = 640;
    float m_height = 480;
    bool m_mirrored = false;
    bool m_stickInverted = false;
    long long m_stickFinger = -1;
    Vector2D m_stickBase = Vector2D(0);
    Vector2D m_stickAt = Vector2D(0);
    long long m_lookFinger = -1;
    long long m_tapFinger = -1;  // a finger that may still be a tap
    PointD m_tapPosition;
    float m_tapTime = 0;
};


} // namespace hud


#endif // HUD_TOUCHCONTROLS_H
