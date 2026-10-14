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

#include "input.hpp"

#include <cmath>

namespace input {


Key parseKey(const String &text, int &modifiers)
{
    modifiers = NoModifier;
    StringList parts = text.split('+');
    // "+" itself, or a key after the last '+'
    if (text.endsWith("++"))
        return Key_None;
    for (int i = 0; i + 1 < parts.size(); i++) {
        const String modifier = parts[i].trimmed().toLower();
        if (modifier == "alt")
            modifiers |= AltModifier;
        else if (modifier == "ctrl")
            modifiers |= ControlModifier;
        else if (modifier == "shift")
            modifiers |= ShiftModifier;
        else if (modifier == "meta")
            modifiers |= MetaModifier;
        else
            return Key_None;
    }
    const String name = parts.isEmpty() ? String() : parts.last().trimmed();
    if (name.size() == 1) {
        const char c = name.toUpper()[0];
        if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || c == ' ' || c == '`' || c == '-' || c == '=' || c == '.')
            return Key(c);
        return Key_None;
    }
    const String lower = name.toLower();
    if (lower.size() >= 2 && lower[0] == 'f') {
        bool ok = false;
        const int n = lower.mid(1).toInt(&ok);
        if (ok && n >= 1 && n <= 12)
            return Key(Key_F1 + n - 1);
    }
    static const struct { const char *name; Key key; } names[] = {
        { "esc", Key_Escape }, { "escape", Key_Escape }, { "tab", Key_Tab }, { "backspace", Key_Backspace },
        { "return", Key_Return }, { "enter", Key_Enter }, { "space", Key_Space }, { "up", Key_Up }, { "down", Key_Down },
        { "left", Key_Left }, { "right", Key_Right },
    };
    for (const auto &entry : names)
        if (lower == entry.name)
            return entry.key;
    return Key_None;
}


KeyEvent alternative(const KeyEvent &event)
{
    if (!(event.modifiers() & ControlModifier) || (event.modifiers() & KeypadModifier))
        return event;
    const int modifiers = event.modifiers() & ~ControlModifier;
    const Key key = event.key();
    if (key == Key_Period)
        return KeyEvent(Key_Escape, modifiers);
    if (key >= Key_1 && key <= Key_9)
        return KeyEvent(Key(Key_F1 + (key - Key_1)), modifiers);
    if (key == Key_0)
        return KeyEvent(Key_F10, modifiers);
    if (key == Key_Minus)
        return KeyEvent(Key_F11, modifiers);
    if (key == Key_Equal)
        return KeyEvent(Key_F12, modifiers);
    return event;
}


} // namespace input
