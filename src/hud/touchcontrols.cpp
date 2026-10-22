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

#include "touchcontrols.h"

#include "gfx/font.h"
#include "gfx/image.h"
#include "gfx/pipeline.h"
#include "gfx/rgb.h"
#include "util/settings.hpp"

#include <glm/geometric.hpp>
#include <algorithm>
#include <cmath>


namespace hud {


namespace {

// the stick: how far from where the finger came down it is pushed fully, the knob's size, and
// how far it has to be pushed before it counts; the part of the window's width where it comes up
const float StickRadius = 56;
const float KnobRadius = 22;
const float StickDeadZone = 0.1f;
const float StickArea = 0.45f;
// where the stick is shown while it isn't used, from the bottom left corner
const Vector2D StickHint(100, -110);
// a button takes a finger that comes down a little outside it too
const float ButtonSlop = 1.2f;
// how long a button with a second key has to be held for it
const float HoldTime = 0.8f;
// a tap: lifted within this time (seconds), having moved less than this
const float TapTime = 0.3f;
const float TapSlop = 8;
// window coordinates (points) per view unit at most: an 11-inch iPad's, 834 high
const float MaxScale = 834/480.0f;

const Vector4D Green(0.35f, 1.0f, 0.35f, 1.0f);

// a white circle, filled or as a ring, with soft edges, for tinting
gfx::Image circle(bool ring)
{
    const int size = 128;
    const float outer = size/2.0f - 1;
    const float inner = ring ? outer - 7 : -1;
    gfx::Image image(size, size, gfx::Image::ARGB32);
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            const float d = std::hypot(x + 0.5f - size/2.0f, y + 0.5f - size/2.0f);
            const float a = std::clamp(outer - d + 0.5f, 0.0f, 1.0f)*std::clamp(d - inner + 0.5f, 0.0f, 1.0f);
            image.setPixel(x, y, gfx::rgba(255, 255, 255, int(a*255 + 0.5f)));
        }
    }
    return image;
}

} // namespace


TouchControls::TouchControls(gfx::Font &font, gfx::Font &fontPressed, const Handlers &handlers) :
    m_font(font),
    m_fontPressed(fontPressed),
    m_handlers(handlers),
    m_disc(circle(false)),
    m_ring(circle(true))
{
    using namespace input;
    // the weapons and the target under the right thumb, in an arc around FIRE (76 from it, from
    // left to top), the speed above them; the rarer orders in the top corners
    m_buttons = {
        { "FIRE", KeyEvent(Key_Space), Vector2D(-75, -75), 40, -1 },
        { "TORP", KeyEvent(Key_Tab), Vector2D(-148, -53), 26, -1 },
        { "BUZZ", KeyEvent(Key_D), Vector2D(-142, -111), 22, -1 },
        { "NEXT", KeyEvent(Key_Return), Vector2D(-104, -146), 22, -1 },
        { "LOCK", KeyEvent(Key_U), Vector2D(-49, -146), 26, -1 },
        { "+", KeyEvent(Key_S), Vector2D(-46, -258), 24, -1 },
        { "-", KeyEvent(Key_X), Vector2D(-46, -204), 24, -1 },
        { "STOP", KeyEvent(Key_C), Vector2D(-106, -231), 22, -1 },
        { "BOOST", KeyEvent(Key_Q), Vector2D(-106, -285), 22, -1 },
        // held: the debug overlay (Ctrl+D)
        { "MENU", KeyEvent(Key_Escape), Vector2D(30, 30), 22, -1, KeyEvent(Key_D, ControlModifier) },
        { "NAV", KeyEvent(Key_N), Vector2D(30, 82), 22, -1 },
        { "AUTO", KeyEvent(Key_A, AltModifier), Vector2D(30, 134), 22, -1 },
        { "SONAR", KeyEvent(Key_R), Vector2D(-30, 30), 22, -1 },
        { "TURR", KeyEvent(Key_O), Vector2D(-30, 82), 22, -1 },
        { "VIEW", KeyEvent(Key_V, AltModifier), Vector2D(-30, 134), 22, -1 },
    };
}


bool TouchControls::isMirrored()
{
    return util::Settings::boolValue("Touch/mirrored", false);
}


void TouchControls::setMirrored(bool mirrored)
{
    util::Settings::setValue("Touch/mirrored", mirrored);
}


bool TouchControls::isStickInverted()
{
    return util::Settings::boolValue("Touch/stickInverted", false);
}


void TouchControls::setStickInverted(bool inverted)
{
    util::Settings::setValue("Touch/stickInverted", inverted);
}


void TouchControls::setSize(double width, double height)
{
    if (height <= 0)
        return;
    m_scale = std::min(float(height/480), MaxScale);
    m_width = float(width/m_scale);
    m_height = float(height/m_scale);
}


Vector2D TouchControls::center(const Button &button) const
{
    const float x = m_mirrored ? -button.place.x : button.place.x;
    return Vector2D(x >= 0 ? x : m_width + x, button.place.y >= 0 ? button.place.y : m_height + button.place.y);
}


Vector2D TouchControls::toView(const PointD &position) const
{
    return Vector2D(position)/m_scale;
}


void TouchControls::press(long long finger, const PointD &position)
{
    const Vector2D at = toView(position);
    // the nearest button the finger is on
    Button *pressed = nullptr;
    float nearest = 0;
    for (Button &button : m_buttons) {
        const float distance = glm::distance(at, center(button))/button.radius;
        if (button.finger < 0 && distance <= ButtonSlop && (!pressed || distance < nearest)) {
            pressed = &button;
            nearest = distance;
        }
    }
    if (pressed) {
        pressed->finger = finger;
        // which of its keys is decided when it is let go or held long enough
        if (pressed->holdKey.key() != input::Key_None)
            pressed->held = 0;
        else
            m_handlers.press(pressed->key, int(pressed - m_buttons.data()));
        return;
    }
    if (m_tapFinger < 0) {
        m_tapFinger = finger;
        m_tapPosition = position;
        m_tapTime = 0;
    }
    if (m_stickFinger < 0 && (m_mirrored ? at.x > m_width*(1 - StickArea) : at.x < m_width*StickArea)) {
        m_stickFinger = finger;
        m_stickBase = m_stickAt = at;
        m_handlers.stick(Vector2D(0));
        return;
    }
    if (m_lookFinger < 0) {
        m_lookFinger = finger;
        m_handlers.lookStart(position);
    }
}


void TouchControls::move(long long finger, const PointD &position)
{
    if (finger == m_tapFinger && glm::distance(toView(position), toView(m_tapPosition)) > TapSlop)
        m_tapFinger = -1;
    // a finger that slides off its button keeps it pressed until it is lifted
    if (finger == m_stickFinger) {
        m_stickAt = toView(position);
        m_handlers.stick(stick());
    } else if (finger == m_lookFinger) {
        m_handlers.lookMove(position);
    }
}


void TouchControls::release(long long finger)
{
    for (Button &button : m_buttons) {
        if (button.finger != finger)
            continue;
        button.finger = -1;
        const int index = int(&button - m_buttons.data());
        if (button.holdKey.key() == input::Key_None) {
            m_handlers.release(button.key, index);
        } else if (button.held >= 0) {
            // a tap
            button.held = -1;
            m_handlers.press(button.key, index);
            m_handlers.release(button.key, index);
        }
    }
    if (finger == m_stickFinger) {
        m_stickFinger = -1;
        m_handlers.stick(Vector2D(0));
    }
    if (finger == m_lookFinger) {
        m_lookFinger = -1;
        m_handlers.lookEnd();
    }
    if (finger == m_tapFinger) {
        m_tapFinger = -1;
        m_handlers.tap(m_tapPosition);
    }
}


void TouchControls::update(float seconds)
{
    m_tapTime += seconds;
    if (m_tapTime > TapTime)
        m_tapFinger = -1;
    for (Button &button : m_buttons) {
        if (button.finger < 0 || button.held < 0)
            continue;
        button.held += seconds;
        if (button.held >= HoldTime) {
            button.held = -1;
            const int index = int(&button - m_buttons.data());
            m_handlers.press(button.holdKey, index);
            m_handlers.release(button.holdKey, index);
        }
    }
}


void TouchControls::reset()
{
    for (Button &button : m_buttons) {
        button.finger = -1;
        button.held = -1;
    }
    m_stickFinger = -1;
    m_lookFinger = -1;
    m_tapFinger = -1;
    m_mirrored = isMirrored();
    m_stickInverted = isStickInverted();
}


// beyond the dead zone the stick goes from 0 to 1, in proportion
Vector2D TouchControls::stick() const
{
    Vector2D offset = (m_stickAt - m_stickBase)/StickRadius;
    if (m_stickInverted)
        offset.y = -offset.y;
    const float length = glm::length(offset);
    if (length <= StickDeadZone)
        return Vector2D(0);
    return offset*((std::min(length, 1.0f) - StickDeadZone)/(1 - StickDeadZone)/length);
}


void TouchControls::drawCircle(gfx::Texture &texture, const Vector2D &center, float radius, const Vector4D &color)
{
    gfx::Pipeline::setColor(color);
    texture.draw(RectF(center.x - radius, center.y - radius, 2*radius, 2*radius));
}


void TouchControls::draw()
{
    for (const Button &button : m_buttons) {
        const bool pressed = button.finger >= 0;
        drawCircle(m_disc, center(button), button.radius, pressed ? Vector4D(Vector3D(Green)*0.6f, 0.45f) : Vector4D(0, 0, 0, 0.3f));
        drawCircle(m_ring, center(button), button.radius, Vector4D(Vector3D(Green), pressed ? 0.95f : 0.5f));
    }

    if (m_stickFinger >= 0) {
        // the knob follows the finger as far as the stick goes
        Vector2D knob = m_stickAt - m_stickBase;
        if (glm::length(knob) > StickRadius)
            knob *= StickRadius/glm::length(knob);
        drawCircle(m_disc, m_stickBase, StickRadius, Vector4D(0, 0, 0, 0.25f));
        drawCircle(m_ring, m_stickBase, StickRadius, Vector4D(Vector3D(Green), 0.6f));
        drawCircle(m_disc, m_stickBase + knob, KnobRadius, Vector4D(Vector3D(Green), 0.5f));
    } else {
        drawCircle(m_ring, Vector2D(m_mirrored ? m_width - StickHint.x : StickHint.x, m_height + StickHint.y), StickRadius, Vector4D(Vector3D(Green), 0.25f));
    }

    gfx::Pipeline::setColor(Vector4D(1));
    for (const Button &button : m_buttons) {
        const Vector2D c = center(button);
        gfx::Font &font = button.finger >= 0 ? m_fontPressed : m_font;
        const int height = font.height();
        font.draw(button.label, Rect(int(c.x - button.radius), int(std::lround(c.y - height/2.0f)), int(2*button.radius), height), true, false);
    }
}


} // namespace hud
