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

#ifndef TXT_VALUE_H
#define TXT_VALUE_H

#include "util/string.hpp"

namespace txt {

// A value of a DesFile: the text after "key =", the lines of a "{ }" block, or a default the
// code passes. Text converts to numbers as String does, a number to text in its shortest form, a
// double to int or bool rounded; a list is text only if it has one line, and never a number.
class Value
{
public:
    Value() = default;
    Value(const String &s) : m_type(Type::String), m_string(s) {}
    Value(const char *s) : Value(String(s)) {}
    Value(const StringList &list) : m_type(Type::List), m_list(list) {}
    Value(int i) : m_type(Type::Int), m_int(i) {}
    Value(float f) : m_type(Type::Float), m_double(f) {}
    Value(double d) : m_type(Type::Double), m_double(d) {}
    Value(bool b) : m_type(Type::Bool), m_int(b) {}

public:
    bool isNull() const { return m_type == Type::Null; }
    String toString() const;
    StringList toStringList() const;
    int toInt(bool *ok = nullptr) const;
    float toFloat(bool *ok = nullptr) const;
    double toDouble(bool *ok = nullptr) const;
    // false for no value, an empty text, "0" and "false" (in any case), and numbers rounding to zero
    bool toBool() const;

private:
    enum class Type { Null, String, List, Int, Float, Double, Bool };

    Type m_type = Type::Null;
    String m_string;
    StringList m_list;
    int m_int = 0;
    double m_double = 0;
};

} // namespace txt

#endif // TXT_VALUE_H
