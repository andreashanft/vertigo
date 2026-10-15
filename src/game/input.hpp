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

#ifndef GAME_INPUT_HPP
#define GAME_INPUT_HPP

#include "util/geometry2d.h"
#include "util/string.hpp"

// The keys and mouse buttons the game reacts to, and the events that bring them
namespace input {

// the key that produces the character, as the keyboard layout has it (so Z is where the layout
// has its Z); the digits, letters and function keys in order. Minus, Equal and Period are
// places, the two keys after 0 in the number row and the period's key of a US layout, for
// alternative()
enum Key
{
    Key_None = 0,
    Key_0 = '0', Key_1, Key_2, Key_3, Key_4, Key_5, Key_6, Key_7, Key_8, Key_9,
    Key_A = 'A', Key_B, Key_C, Key_D, Key_E, Key_F, Key_G, Key_H, Key_I, Key_J, Key_K, Key_L, Key_M,
    Key_N, Key_O, Key_P, Key_Q, Key_R, Key_S, Key_T, Key_U, Key_V, Key_W, Key_X, Key_Y, Key_Z,
    Key_Space = ' ',
    Key_QuoteLeft = '`',
    Key_Minus = '-', Key_Equal = '=', Key_Period = '.',
    Key_F1 = 0x100, Key_F2, Key_F3, Key_F4, Key_F5, Key_F6, Key_F7, Key_F8, Key_F9, Key_F10, Key_F11, Key_F12,
    Key_Escape = 0x200, Key_Tab, Key_Backspace, Key_Return, Key_Enter, Key_Up, Key_Down, Key_Left, Key_Right,
};

enum Modifier
{
    NoModifier = 0,
    ShiftModifier = 1,
    ControlModifier = 2,
    AltModifier = 4,
    MetaModifier = 8,
    // a key of the number pad
    KeypadModifier = 16,
};

enum MouseButton
{
    NoButton = 0,
    LeftButton = 1,
    RightButton = 2,
    MiddleButton = 4,
};

// a game controller's buttons by their place, as SDL names them: South is A on an Xbox controller
// and Cross on a PlayStation one; the triggers count as buttons
enum Button
{
    Button_None = -1,
    Button_South, Button_East, Button_West, Button_North,
    Button_Back, Button_Start, Button_LeftStick, Button_RightStick,
    Button_LeftShoulder, Button_RightShoulder,
    Button_Up, Button_Down, Button_Left, Button_Right,
    Button_LeftTrigger, Button_RightTrigger,
    ButtonCount
};

// a game controller's sticks: x to the right and y down, each from -1 to 1
enum Stick { LeftStick, RightStick };

// a key pressed or released, or text typed (then without a key)
class KeyEvent
{
public:
    KeyEvent(Key key = Key_None, int modifiers = NoModifier, const String &text = String()) : m_key(key), m_modifiers(modifiers), m_text(text) {}

    Key key() const { return m_key; }
    int modifiers() const { return m_modifiers; }
    // what the key typed, in Latin-1
    const String& text() const { return m_text; }

private:
    Key m_key;
    int m_modifiers;
    String m_text;
};

// a mouse button pressed or released, or the mouse moved, in the window's coordinates
class MouseEvent
{
public:
    MouseEvent(const PointD &position = PointD(0), MouseButton button = NoButton, int buttons = NoButton, bool fromTouch = false) :
        m_position(position), m_button(button), m_buttons(buttons), m_fromTouch(fromTouch) {}

    const PointD& position() const { return m_position; }
    // the one pressed or released
    MouseButton button() const { return m_button; }
    // those held down now
    int buttons() const { return m_buttons; }
    // made up by the system from a finger on the touch screen, so that taps click
    bool isFromTouch() const { return m_fromTouch; }

private:
    PointD m_position;
    MouseButton m_button;
    int m_buttons;
    bool m_fromTouch;
};

// a finger on a touch screen put down, moved or lifted, in the window's coordinates; the id tells
// the fingers apart while they touch
class TouchEvent
{
public:
    TouchEvent(long long id = 0, const PointD &position = PointD(0)) : m_id(id), m_position(position) {}

    long long id() const { return m_id; }
    const PointD& position() const { return m_position; }

private:
    long long m_id;
    PointD m_position;
};

// "Esc", "Alt+A", "Down", "F5", "2" (as --keys takes them; any case); Key_None if it isn't one
Key parseKey(const String &text, int &modifiers);

// The key a second binding stands for, for keyboards without Esc and function keys (an iPad's):
// Cmd+. (Ctrl+. on Windows and Linux) is Esc, as in iPadOS, and Cmd with the number row's keys
// 1 to 0, - and = is F1 to F12. Other keys are themselves.
KeyEvent alternative(const KeyEvent &event);

// "A", "LB", "RT", "Up", "Start", "L3" (as --pad takes them, by an Xbox controller's names; any
// case); Button_None if it isn't one
Button parseButton(const String &text);

// the key a button stands for outside missions: A is Return, B and Start are Esc, the d-pad the
// arrows (so a button starts a mission after its briefing and skips a movie)
KeyEvent menuKey(Button button);

} // namespace input

#endif // GAME_INPUT_HPP
