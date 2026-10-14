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

#include "value.h"
#include <charconv>
#include <cmath>

namespace txt {


namespace {

// the shortest text that reads back as the same number
String shortest(double d)
{
    char buffer[64];
    const std::to_chars_result result = std::to_chars(buffer, buffer + sizeof(buffer), d == 0 ? 0.0 : d);
    return String::fromLatin1(buffer, int(result.ptr - buffer));
}

} // namespace


String Value::toString() const
{
    switch (m_type) {
    case Type::String: return m_string;
    case Type::List: return m_list.size() == 1 ? m_list.first() : String();
    case Type::Int: return String::number(m_int);
    case Type::Float: return shortest(m_double);
    case Type::Double: return shortest(m_double);
    case Type::Bool: return m_int ? "true" : "false";
    case Type::Null: break;
    }
    return String();
}


StringList Value::toStringList() const
{
    switch (m_type) {
    case Type::String: return StringList{ m_string };
    case Type::List: return m_list;
    default: return StringList();
    }
}


int Value::toInt(bool *ok) const
{
    if (ok)
        *ok = true;
    switch (m_type) {
    case Type::String: return m_string.toInt(ok);
    case Type::Int:
    case Type::Bool: return m_int;
    case Type::Float:
    case Type::Double: return int(std::lround(m_double));
    case Type::List:
    case Type::Null: break;
    }
    if (ok)
        *ok = false;
    return 0;
}


double Value::toDouble(bool *ok) const
{
    if (ok)
        *ok = true;
    switch (m_type) {
    case Type::String: return m_string.toDouble(ok);
    case Type::Int:
    case Type::Bool: return m_int;
    case Type::Float:
    case Type::Double: return m_double;
    case Type::List:
    case Type::Null: break;
    }
    if (ok)
        *ok = false;
    return 0;
}


float Value::toFloat(bool *ok) const
{
    return float(toDouble(ok));
}


bool Value::toBool() const
{
    switch (m_type) {
    case Type::String: return !(m_string.isEmpty() || m_string == "0" || m_string.toLower() == "false");
    case Type::Int:
    case Type::Bool: return m_int != 0;
    case Type::Float:
    case Type::Double: return std::lround(m_double) != 0;
    case Type::List:
    case Type::Null: break;
    }
    return false;
}


} // namespace txt
