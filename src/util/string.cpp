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

#include "string.hpp"
#include <algorithm>
#include <charconv>
#include <climits>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <ostream>

namespace util {


namespace {

bool isSpace(char c)
{
    return String::isSpace(c);
}


int digitValue(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'z')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'Z')
        return c - 'A' + 10;
    return 99;
}


// numbers may have ASCII whitespace around them (not 0x85 and 0xa0, unlike trimmed())
String trimmedAscii(const String &text)
{
    int first = 0, last = text.size();
    auto ascii = [](char c) { return c == ' ' || (c >= 9 && c <= 13); };
    while (first < last && ascii(text[first]))
        first++;
    while (last > first && ascii(text[last - 1]))
        last--;
    return text.mid(first, last - first);
}


// the digits of an integer with an optional sign, as Qt's C locale reads them; false if there
// is anything else. Like Qt, this stops at a zero byte (some values in the data end with one).
bool parseInteger(const String &text, int base, bool &negative, unsigned long long &value)
{
    const int end = text.indexOf('\0');
    const String s = trimmedAscii(end >= 0 ? text.left(end) : text);
    int i = 0;
    negative = false;
    if (i < s.size() && (s[i] == '+' || s[i] == '-'))
        negative = s[i++] == '-';
    if ((base == 0 || base == 16) && i + 1 < s.size() && s[i] == '0' && (s[i + 1] == 'x' || s[i + 1] == 'X')) {
        base = 16;
        i += 2;
    } else if (base == 0) {
        base = i < s.size() && s[i] == '0' ? 8 : 10;
    }
    if (i >= s.size())
        return false;
    value = 0;
    for (; i < s.size(); i++) {
        const int digit = digitValue(s[i]);
        if (digit >= base)
            return false;
        if (value > (std::numeric_limits<unsigned long long>::max() - digit)/base)
            return false;
        value = value*base + digit;
    }
    return true;
}


// a decimal number, as Qt's C locale reads it: digits with an optional point and exponent,
// "inf" or "nan"; too large numbers are infinite, but not ok. Unlike integers, Qt allows all of
// QChar::isSpace() around it
bool parseDouble(const String &text, double &value)
{
    const String s = text.trimmed();
    if (s.isEmpty())
        return false;
    int i = 0;
    const bool negative = s[0] == '-';
    if (s[0] == '+' || s[0] == '-')
        i++;
    const String rest = s.mid(i).toLower();
    if (rest == "inf" || rest == "nan") {
        value = rest == "inf" ? std::numeric_limits<double>::infinity() : std::numeric_limits<double>::quiet_NaN();
        if (negative)
            value = -value;
        return true;
    }
    int digits = 0;
    const int start = i;
    while (i < s.size() && s[i] >= '0' && s[i] <= '9') {
        i++;
        digits++;
    }
    if (i < s.size() && s[i] == '.') {
        i++;
        while (i < s.size() && s[i] >= '0' && s[i] <= '9') {
            i++;
            digits++;
        }
    }
    if (digits == 0)
        return false;
    if (i < s.size() && (s[i] == 'e' || s[i] == 'E')) {
        i++;
        if (i < s.size() && (s[i] == '+' || s[i] == '-'))
            i++;
        int exponentDigits = 0;
        while (i < s.size() && s[i] >= '0' && s[i] <= '9') {
            i++;
            exponentDigits++;
        }
        if (exponentDigits == 0)
            return false;
    }
    if (i != s.size())
        return false;
    // from_chars takes no '+', and is independent of the C locale (Qt sets the user's)
    const char *begin = s.constData() + start;
    const std::from_chars_result result = std::from_chars(begin, s.constData() + s.size(), value);
    if (result.ec == std::errc::result_out_of_range) {
        // too large or too small: the power of ten of the first significant digit tells which
        int digit = 0, point = -1, firstSignificant = -1;
        int k = start;
        for (; k < s.size() && s[k] != 'e' && s[k] != 'E'; k++) {
            if (s[k] == '.') {
                point = digit;
                continue;
            }
            if (firstSignificant < 0 && s[k] != '0')
                firstSignificant = digit;
            digit++;
        }
        if (point < 0)
            point = digit;
        const long long exponent = k < s.size() ? std::strtoll(s.constData() + k + 1, nullptr, 10) : 0;
        const bool tooLarge = firstSignificant >= 0 && point - firstSignificant + exponent > 0;
        value = tooLarge ? std::numeric_limits<double>::infinity() : 0;
        if (negative)
            value = -value;
        return false;
    }
    if (result.ec != std::errc() || result.ptr != s.constData() + s.size())
        return false;
    if (negative)
        value = -value;
    return true;
}


// The exact decimal digits of a finite, positive double, without leading zeros, and the power of
// ten of the first one (every double has a finite decimal expansion, at most 767 significant
// digits)
std::string exactDigits(double value, int &exponent)
{
    char buffer[1024];
    const std::to_chars_result result = std::to_chars(buffer, buffer + sizeof(buffer), value, std::chars_format::scientific, 800);
    const std::string text(buffer, result.ptr);
    const size_t e = text.find('e');
    exponent = std::atoi(text.c_str() + e + 1);
    std::string digits = text.substr(0, 1) + text.substr(2, e - 2);
    digits.erase(digits.find_last_not_of('0') + 1);
    return digits.empty() ? "0" : digits;
}


// the first n digits (n >= 0), rounded half away from zero as Qt does (std::to_chars rounds
// exact halves to even: 2.5 to 2, Qt to 3); a carry raises the exponent, and for n = 0 gives "1"
std::string roundDigits(const std::string &digits, int n, int &exponent)
{
    std::string kept = digits.substr(0, size_t(n));
    kept.resize(size_t(n), '0');
    if (n < int(digits.size()) && digits[size_t(n)] >= '5') {
        int i = n - 1;
        while (i >= 0 && kept[size_t(i)] == '9')
            kept[size_t(i--)] = '0';
        if (i >= 0) {
            kept[size_t(i)]++;
        } else {
            kept.insert(0, "1");
            if (n > 0)
                kept.pop_back();
            exponent++;
        }
    }
    return kept;
}


std::string fixedText(double value, int precision)
{
    int exponent;
    const std::string digits = exactDigits(value, exponent);
    // digits up to the precision; none at all if the number is below half its last place
    const int n = exponent + 1 + precision;
    std::string kept;
    if (n >= 0) {
        kept = roundDigits(digits, n, exponent);
        if (int(kept.size()) < exponent + 1 + precision)
            kept += '0';
    } else {
        exponent = -precision - 1;
    }
    // as many digits as the integer part and the precision need
    std::string integer, fraction;
    const int integerDigits = exponent + 1;
    if (integerDigits > 0) {
        integer = kept.substr(0, size_t(integerDigits));
        integer.resize(size_t(integerDigits), '0');
        fraction = kept.size() > size_t(integerDigits) ? kept.substr(size_t(integerDigits)) : std::string();
    } else {
        integer = "0";
        fraction = std::string(size_t(-integerDigits), '0') + kept;
    }
    fraction.resize(size_t(precision), '0');
    return precision > 0 ? integer + "." + fraction : integer;
}


std::string exponentText(int exponent, bool upper)
{
    std::string text = upper ? "E" : "e";
    text += exponent < 0 ? '-' : '+';
    const std::string number = std::to_string(std::abs(exponent));
    return text + (number.size() < 2 ? "0" : "") + number;
}


std::string scientificText(double value, int precision, bool upper)
{
    int exponent;
    const std::string digits = exactDigits(value, exponent);
    const std::string kept = value == 0 ? std::string(size_t(precision + 1), '0') : roundDigits(digits, precision + 1, exponent);
    if (value == 0)
        exponent = 0;
    std::string text = kept.substr(0, 1);
    if (precision > 0)
        text += "." + kept.substr(1);
    return text + exponentText(exponent, upper);
}


// as printf's %g: fixed or scientific by the exponent after rounding, without trailing zeros
std::string generalText(double value, int precision, bool upper)
{
    if (precision == 0)
        precision = 1;
    int exponent = 0;
    if (value != 0) {
        const std::string digits = exactDigits(value, exponent);
        roundDigits(digits, precision, exponent);
    }
    std::string text = exponent < -4 || exponent >= precision ? scientificText(value, precision - 1, upper) : fixedText(value, precision - 1 - exponent);
    const size_t e = text.find_first_of("eE");
    std::string mantissa = text.substr(0, e);
    const std::string rest = e == std::string::npos ? std::string() : text.substr(e);
    if (mantissa.find('.') != std::string::npos) {
        mantissa.erase(mantissa.find_last_not_of('0') + 1);
        if (mantissa.back() == '.')
            mantissa.pop_back();
    }
    return mantissa + rest;
}


String formatDouble(double value, char format, int precision)
{
    if (precision < 0)
        precision = 6;
    const bool upper = format == 'E' || format == 'G';
    if (std::isnan(value))
        return upper ? "NAN" : "nan";
    if (std::isinf(value))
        return value < 0 ? (upper ? "-INF" : "-inf") : (upper ? "INF" : "inf");
    // Qt writes zero without a sign, also -0.0, but keeps the sign of what rounds to zero
    const bool negative = value < 0;
    value = std::fabs(value);
    std::string text;
    switch (format) {
    case 'f': case 'F': text = fixedText(value, precision); break;
    case 'e': case 'E': text = scientificText(value, precision, upper); break;
    default: text = generalText(value, precision, upper); break;
    }
    return String::fromLatin1((negative ? "-" : "") + text);
}


String formatInteger(long long value, bool isUnsigned, int base)
{
    if (base < 2 || base > 36)
        base = 10;
    char buffer[72];
    const std::to_chars_result result = isUnsigned ?
        std::to_chars(buffer, buffer + sizeof(buffer), (unsigned long long)value, base) :
        std::to_chars(buffer, buffer + sizeof(buffer), value, base);
    return String::fromLatin1(buffer, int(result.ptr - buffer));
}


// a number padded with zeros after its sign to the field width
String zeroPad(String number, int fieldWidth)
{
    const bool sign = number.startsWith('-') || number.startsWith('+');
    if (fieldWidth > number.size())
        number.insert(sign ? 1 : 0, String(fieldWidth - number.size(), '0'));
    return number;
}


struct ArgEscape
{
    int position;
    int length;
    int number;
};


// %0 to %99, a second digit always belonging to the number
std::vector<ArgEscape> findArgEscapes(const String &s)
{
    std::vector<ArgEscape> escapes;
    for (int i = 0; i + 1 < s.size(); i++) {
        if (s[i] != '%' || s[i + 1] < '0' || s[i + 1] > '9')
            continue;
        ArgEscape escape = { i, 2, s[i + 1] - '0' };
        if (i + 2 < s.size() && s[i + 2] >= '0' && s[i + 2] <= '9') {
            escape.number = escape.number*10 + s[i + 2] - '0';
            escape.length = 3;
        }
        escapes.push_back(escape);
        i += escape.length - 1;
    }
    return escapes;
}


String pad(const String &arg, int fieldWidth, char fill)
{
    const int width = std::abs(fieldWidth);
    if (arg.size() >= width)
        return arg;
    return fieldWidth > 0 ? String(width - arg.size(), fill) + arg : arg + String(width - arg.size(), fill);
}

} // namespace


bool String::isSpace(char c)
{
    const unsigned char u = static_cast<unsigned char>(c);
    return u == ' ' || (u >= 9 && u <= 13) || u == 0x85 || u == 0xa0;
}


String::String(const char *utf8)
{
    if (utf8)
        *this = fromUtf8(utf8);
}


String String::fromLatin1(const char *data, int size)
{
    String s;
    if (data)
        s.m_data.assign(data, size < 0 ? std::strlen(data) : size_t(size));
    return s;
}


String String::fromUtf8(const char *data, int size)
{
    String s;
    if (!data)
        return s;
    const int n = size < 0 ? int(std::strlen(data)) : size;
    s.m_data.reserve(size_t(n));
    for (int i = 0; i < n; ) {
        const unsigned char c = static_cast<unsigned char>(data[i]);
        if (c < 0x80) {
            s.m_data += char(c);
            i++;
            continue;
        }
        // the length of the sequence and the bits of its first byte
        int length = 0;
        unsigned int codePoint = 0;
        if (c >= 0xc2 && c <= 0xdf) {
            length = 2;
            codePoint = c & 0x1f;
        } else if (c >= 0xe0 && c <= 0xef) {
            length = 3;
            codePoint = c & 0x0f;
        } else if (c >= 0xf0 && c <= 0xf4) {
            length = 4;
            codePoint = c & 0x07;
        }
        bool valid = length > 0 && i + length <= n;
        for (int k = 1; valid && k < length; k++) {
            const unsigned char next = static_cast<unsigned char>(data[i + k]);
            valid = (next & 0xc0) == 0x80;
            codePoint = (codePoint << 6) | (next & 0x3f);
        }
        if (!valid) {
            // like Qt's replacement character, which is '?' in Latin-1
            s.m_data += '?';
            i++;
            continue;
        }
        s.m_data += codePoint <= 0xff ? char(codePoint) : '?';
        i += length;
    }
    return s;
}


String String::number(long long n, int base)
{
    return formatInteger(n, false, base);
}


String String::number(unsigned long long n, int base)
{
    return formatInteger((long long)n, true, base);
}


String String::number(double n, char format, int precision)
{
    return formatDouble(n, format, precision);
}


std::string String::toUtf8() const
{
    std::string utf8;
    utf8.reserve(m_data.size());
    for (char c : m_data) {
        const unsigned char u = static_cast<unsigned char>(c);
        if (u < 0x80) {
            utf8 += c;
        } else {
            utf8 += char(0xc0 | (u >> 6));
            utf8 += char(0x80 | (u & 0x3f));
        }
    }
    return utf8;
}


void String::chop(int n)
{
    if (n > 0)
        m_data.resize(size_t(std::max(0, size() - n)));
}


void String::truncate(int position)
{
    if (position < size())
        m_data.resize(size_t(std::max(0, position)));
}


String &String::insert(int position, const String &s)
{
    if (position < 0)
        position += size();
    if (position < 0)
        return *this;
    // beyond the end the string is extended with spaces, as in Qt
    if (position > size())
        m_data.resize(size_t(position), ' ');
    m_data.insert(size_t(position), s.m_data);
    return *this;
}


String &String::remove(int position, int n)
{
    if (position < 0)
        position += size();
    if (position < 0 || position >= size() || n <= 0)
        return *this;
    m_data.erase(size_t(position), size_t(std::min(n, size() - position)));
    return *this;
}


String &String::remove(char c)
{
    m_data.erase(std::remove(m_data.begin(), m_data.end(), c), m_data.end());
    return *this;
}


String &String::remove(const String &s)
{
    return replace(s, String());
}


String &String::replace(char before, char after)
{
    std::replace(m_data.begin(), m_data.end(), before, after);
    return *this;
}


String &String::replace(const String &before, const String &after)
{
    if (before.isEmpty())
        return *this;
    std::string result;
    size_t position = 0;
    for (size_t found; (found = m_data.find(before.m_data, position)) != std::string::npos; position = found + before.m_data.size())
        result.append(m_data, position, found - position).append(after.m_data);
    result.append(m_data, position, std::string::npos);
    m_data = result;
    return *this;
}


String String::left(int n) const
{
    if (n < 0 || n >= size())
        return *this;
    return fromLatin1(m_data.data(), n);
}


String String::right(int n) const
{
    if (n < 0 || n >= size())
        return *this;
    return fromLatin1(m_data.data() + size() - n, n);
}


String String::mid(int position, int n) const
{
    // QContainerImplHelper::mid()
    const int length = size();
    if (position > length)
        return String();
    if (position < 0) {
        if (n < 0 || n + position >= length)
            return *this;
        if (n + position <= 0)
            return String();
        n += position;
        position = 0;
    } else if (n < 0 || n > length - position) {
        n = length - position;
    }
    return fromLatin1(m_data.data() + position, n);
}


String String::section(char separator, int start, int end, int flags) const
{
    return section(String(1, separator), start, end, flags);
}


String String::section(const String &separator, int start, int end, int flags) const
{
    // QString::section()
    const StringList sections = split(separator);
    const int count = sections.size();
    const bool skipEmpty = flags & SectionSkipEmpty;
    int skip = 0;
    if (skipEmpty)
        for (const String &s : sections)
            if (s.isEmpty())
                skip++;
    if (start < 0)
        start += count - skip;
    if (end < 0)
        end += count - skip;
    if (start >= count || end < 0 || start > end)
        return String();

    String result;
    for (int i = 0, x = 0; x <= end && i < count; i++) {
        const String &s = sections[i];
        if (x >= start) {
            if (x > start && i > 0)
                result += separator;
            result += s;
        }
        if (!s.isEmpty() || !skipEmpty)
            x++;
    }
    return result;
}


StringList String::split(char separator, SplitBehavior behavior) const
{
    return split(String(1, separator), behavior);
}


StringList String::split(const String &separator, SplitBehavior behavior) const
{
    StringList list;
    if (separator.isEmpty()) {
        // as Qt: an empty part, every character, an empty part
        if (behavior == KeepEmptyParts)
            list << String();
        for (char c : m_data)
            list << String(1, c);
        if (behavior == KeepEmptyParts)
            list << String();
        return list;
    }
    size_t position = 0;
    while (true) {
        const size_t found = m_data.find(separator.m_data, position);
        const String part = fromLatin1(m_data.data() + position, int((found == std::string::npos ? m_data.size() : found) - position));
        if (behavior == KeepEmptyParts || !part.isEmpty())
            list << part;
        if (found == std::string::npos)
            break;
        position = found + separator.m_data.size();
    }
    return list;
}


String String::trimmed() const
{
    int first = 0, last = size();
    while (first < last && isSpace(m_data[size_t(first)]))
        first++;
    while (last > first && isSpace(m_data[size_t(last - 1)]))
        last--;
    return fromLatin1(m_data.data() + first, last - first);
}


String String::simplified() const
{
    String result;
    bool space = false;
    for (char c : trimmed().m_data) {
        if (isSpace(c)) {
            space = true;
            continue;
        }
        if (space)
            result.m_data += ' ';
        space = false;
        result.m_data += c;
    }
    return result;
}


String String::toLower() const
{
    String result = *this;
    for (char &c : result.m_data) {
        const unsigned char u = static_cast<unsigned char>(c);
        if ((u >= 'A' && u <= 'Z') || (u >= 0xc0 && u <= 0xde && u != 0xd7))
            c = char(u + 32);
    }
    return result;
}


String String::toUpper() const
{
    String result;
    result.m_data.reserve(m_data.size());
    for (char c : m_data) {
        const unsigned char u = static_cast<unsigned char>(c);
        if ((u >= 'a' && u <= 'z') || (u >= 0xe0 && u <= 0xfe && u != 0xf7))
            result.m_data += char(u - 32);
        else if (u == 0xdf)
            result.m_data += "SS";
        else if (u == 0xff || u == 0xb5)
            result.m_data += '?';
        else
            result.m_data += c;
    }
    return result;
}


String String::repeated(int times) const
{
    String result;
    for (int i = 0; i < times; i++)
        result += *this;
    return result;
}


bool String::endsWith(const String &s) const
{
    return s.m_data.size() <= m_data.size() && m_data.compare(m_data.size() - s.m_data.size(), s.m_data.size(), s.m_data) == 0;
}


int String::indexOf(char c, int from) const
{
    return indexOf(String(1, c), from);
}


int String::indexOf(const String &s, int from) const
{
    if (from < 0)
        from += size();
    if (from < 0 || from > size())
        return -1;
    const size_t found = m_data.find(s.m_data, size_t(from));
    return found == std::string::npos ? -1 : int(found);
}


int String::lastIndexOf(char c, int from) const
{
    return lastIndexOf(String(1, c), from);
}


int String::lastIndexOf(const String &s, int from) const
{
    if (from < 0)
        from += size();
    if (from < 0)
        return -1;
    const size_t found = m_data.rfind(s.m_data, size_t(from));
    return found == std::string::npos ? -1 : int(found);
}


int String::count(char c) const
{
    return int(std::count(m_data.begin(), m_data.end(), c));
}


int String::count(const String &s) const
{
    // overlapping, as Qt counts
    int n = 0;
    for (size_t found = m_data.find(s.m_data); found != std::string::npos; found = m_data.find(s.m_data, found + 1))
        n++;
    return n;
}


long long String::toLongLong(bool *ok, int base) const
{
    bool negative;
    unsigned long long value;
    const bool valid = parseInteger(*this, base, negative, value)
            && value <= (negative ? (unsigned long long)LLONG_MAX + 1 : (unsigned long long)LLONG_MAX);
    if (ok)
        *ok = valid;
    if (!valid)
        return 0;
    return negative ? (long long)(0 - value) : (long long)value;
}


unsigned long long String::toULongLong(bool *ok, int base) const
{
    bool negative;
    unsigned long long value;
    const bool valid = parseInteger(*this, base, negative, value) && !negative;
    if (ok)
        *ok = valid;
    return valid ? value : 0;
}


int String::toInt(bool *ok, int base) const
{
    bool valid;
    const long long value = toLongLong(&valid, base);
    valid = valid && value >= INT_MIN && value <= INT_MAX;
    if (ok)
        *ok = valid;
    return valid ? int(value) : 0;
}


unsigned int String::toUInt(bool *ok, int base) const
{
    bool valid;
    const unsigned long long value = toULongLong(&valid, base);
    valid = valid && value <= UINT_MAX;
    if (ok)
        *ok = valid;
    return valid ? (unsigned int)value : 0;
}


double String::toDouble(bool *ok) const
{
    double value = 0;
    const bool valid = parseDouble(*this, value);
    if (ok)
        *ok = valid;
    return valid || std::isinf(value) ? value : 0;
}


float String::toFloat(bool *ok) const
{
    // through double, as Qt does (QLocaleData::convertDoubleToFloat())
    bool valid;
    const double value = toDouble(&valid);
    if (valid && !std::isinf(value)) {
        if (std::fabs(value) > std::numeric_limits<float>::max()) {
            if (ok)
                *ok = false;
            return value < 0 ? -std::numeric_limits<float>::infinity() : std::numeric_limits<float>::infinity();
        }
        if (value != 0 && float(value) == 0) {
            if (ok)
                *ok = false;
            return 0;
        }
    }
    if (ok)
        *ok = valid;
    return float(value);
}


String String::replaceArg(const String &arg, int fieldWidth, char fill) const
{
    const std::vector<ArgEscape> escapes = findArgEscapes(*this);
    if (escapes.empty())
        return *this;
    int lowest = 100;
    for (const ArgEscape &escape : escapes)
        lowest = std::min(lowest, escape.number);
    const String padded = pad(arg, fieldWidth, fill);
    String result;
    int position = 0;
    for (const ArgEscape &escape : escapes) {
        if (escape.number != lowest)
            continue;
        result.m_data.append(m_data, size_t(position), size_t(escape.position - position));
        result += padded;
        position = escape.position + escape.length;
    }
    result.m_data.append(m_data, size_t(position), std::string::npos);
    return result;
}


String String::arg(const String &a, int fieldWidth, char fill) const
{
    return replaceArg(a, fieldWidth, fill);
}


String String::argInteger(long long a, bool isUnsigned, int fieldWidth, int base, char fill) const
{
    String number = formatInteger(a, isUnsigned, base);
    if (fill == '0' && fieldWidth > 0)
        number = zeroPad(number, fieldWidth);
    return replaceArg(number, fieldWidth, fill);
}


String String::arg(double a, int fieldWidth, char format, int precision, char fill) const
{
    String number = formatDouble(a, format, precision);
    if (fill == '0' && fieldWidth > 0 && std::isfinite(a))
        number = zeroPad(number, fieldWidth);
    return replaceArg(number, fieldWidth, fill);
}


String String::multiArg(std::initializer_list<const String *> args) const
{
    // the k-th lowest escape number takes the k-th argument
    const std::vector<ArgEscape> escapes = findArgEscapes(*this);
    std::vector<int> numbers;
    for (const ArgEscape &escape : escapes)
        numbers.push_back(escape.number);
    std::sort(numbers.begin(), numbers.end());
    numbers.erase(std::unique(numbers.begin(), numbers.end()), numbers.end());
    String result;
    int position = 0;
    for (const ArgEscape &escape : escapes) {
        const int index = int(std::lower_bound(numbers.begin(), numbers.end(), escape.number) - numbers.begin());
        if (index >= int(args.size()))
            continue;
        result.m_data.append(m_data, size_t(position), size_t(escape.position - position));
        result += *args.begin()[index];
        position = escape.position + escape.length;
    }
    result.m_data.append(m_data, size_t(position), std::string::npos);
    return result;
}


std::ostream &operator<<(std::ostream &stream, const String &s)
{
    return stream << s.toUtf8();
}


StringList StringList::withPrefix(const String &prefix) const
{
    StringList result;
    for (const String &s : *this)
        if (s.startsWith(prefix))
            result.append(s);
    return result;
}


bool StringList::contains(const String &s) const
{
    return std::find(begin(), end(), s) != end();
}


int StringList::indexOf(const String &s, int from) const
{
    for (int i = std::max(0, from); i < size(); i++)
        if (at(i) == s)
            return i;
    return -1;
}


int StringList::removeAll(const String &s)
{
    const int before = size();
    erase(std::remove(begin(), end(), s), end());
    return before - size();
}


String StringList::takeFirst()
{
    const String s = front();
    erase(begin());
    return s;
}


String StringList::join(const String &separator) const
{
    String result;
    for (int i = 0; i < size(); i++) {
        if (i > 0)
            result += separator;
        result += at(i);
    }
    return result;
}


} // namespace util
