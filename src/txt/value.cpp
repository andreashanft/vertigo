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

// the shortest text that reads back as the same number, fixed or with an exponent, whichever is
// shorter (as QString::number() with QLocale::FloatingPointShortest)
String shortest(double d)
{
    if (!std::isfinite(d) || d == 0)
        return String::number(d);
    char buffer[64];
    const std::to_chars_result result = std::to_chars(buffer, buffer + sizeof(buffer), d, std::chars_format::scientific);
    const String scientific = String::fromLatin1(buffer, int(result.ptr - buffer));
    // the digits and the power of ten of the first one; the fixed form fills up with zeros
    const bool negative = scientific.startsWith('-');
    const int e = scientific.indexOf('e');
    const String digits = scientific.mid(negative ? 1 : 0, e - (negative ? 1 : 0)).remove('.');
    const int exponent = scientific.mid(e + 1).toInt();
    String fixed;
    if (exponent >= 0) {
        fixed = digits.left(exponent + 1) + String(std::max(0, exponent + 1 - digits.size()), '0');
        if (digits.size() > exponent + 1)
            fixed += "." + digits.mid(exponent + 1);
    } else {
        fixed = "0." + String(-exponent - 1, '0') + digits;
    }
    if (negative)
        fixed.prepend("-");
    return fixed.size() <= scientific.size() ? fixed : scientific;
}


// Qt's qRound64(), including its conversion of numbers out of range
long long round64(double d)
{
    return d >= 0.0 ? (long long)(d + 0.5) : (long long)(d - 0.5);
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
    // as QVariant: read as a 64-bit number and cut to 32 bits
    case Type::String: return int(m_string.toLongLong(ok));
    case Type::Int:
    case Type::Bool: return m_int;
    case Type::Float:
    case Type::Double: return int(round64(m_double));
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
    // as QVariant: read as a double, whatever the float makes of it
    return float(toDouble(ok));
}


bool Value::toBool() const
{
    switch (m_type) {
    case Type::String: return !(m_string.isEmpty() || m_string == "0" || m_string.toLower() == "false");
    case Type::Int:
    case Type::Bool: return m_int != 0;
    case Type::Float:
    case Type::Double: return round64(m_double) != 0;
    case Type::List:
    case Type::Null: break;
    }
    return false;
}


} // namespace txt
