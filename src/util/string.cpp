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
#include <cerrno>
#include <charconv>
#include <clocale>
#include <cmath>
#include <cstring>
#include <ostream>
#include <type_traits>
#if defined(__APPLE__)
#include <xlocale.h>
#endif

namespace util {


namespace {

bool isSpace(char c)
{
    return String::isSpace(c);
}


// A number of the type T: the whole text but whitespace around it and a '+' sign, up to a zero
// byte (some values in the data end with one)
template <typename T>
bool parseNumber(const String &text, T &value)
{
    const char *first = text.constData();
    const char *last = first + std::strlen(first);
    while (first < last && isSpace(*first))
        first++;
    while (last > first && isSpace(last[-1]))
        last--;
    if (last - first > 1 && first[0] == '+' && first[1] != '-')
        first++;
    const char *end = nullptr;
    if constexpr (std::is_floating_point<T>::value) {
        end = readDouble(first, last, value);
    } else {
        const std::from_chars_result result = std::from_chars(first, last, value);
        end = result.ec == std::errc() ? result.ptr : nullptr;
    }
    return first < last && end == last;
}


template <typename T>
T toNumber(const String &text, bool *ok)
{
    T value = 0;
    const bool valid = parseNumber(text, value);
    if (ok)
        *ok = valid;
    return valid ? value : 0;
}


String formatDouble(double value, char format, int precision)
{
    if (precision < 0)
        precision = 6;
    const std::chars_format chars = format == 'f' || format == 'F' ? std::chars_format::fixed
            : format == 'e' || format == 'E' ? std::chars_format::scientific : std::chars_format::general;
    // (-0.0 without its sign)
    std::vector<char> buffer(size_t(precision) + 400);
    const std::to_chars_result result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value == 0 ? 0.0 : value, chars, precision);
    String text = String::fromLatin1(buffer.data(), int(result.ptr - buffer.data()));
    return format >= 'A' && format <= 'Z' ? text.toUpper() : text;
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
    return c == ' ' || (c >= 9 && c <= 13);
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
            // a broken sequence is a character unknown to Latin-1
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
    m_data.insert(size_t(std::clamp(position, 0, size())), s.m_data);
    return *this;
}


String &String::remove(int position, int n)
{
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
    // what lies before the start counts towards n
    if (position < 0) {
        if (n >= 0)
            n = std::max(0, n + position);
        position = 0;
    }
    if (position >= size())
        return String();
    return fromLatin1(m_data.data() + position, n < 0 || n > size() - position ? size() - position : n);
}


String String::section(char separator, int start, int end, int flags) const
{
    return section(String(1, separator), start, end, flags);
}


String String::section(const String &separator, int start, int end, int flags) const
{
    const StringList sections = split(separator, flags & SectionSkipEmpty ? SkipEmptyParts : KeepEmptyParts);
    if (start < 0)
        start += sections.size();
    if (end < 0)
        end += sections.size();
    start = std::max(start, 0);
    end = std::min(end, sections.size() - 1);
    String result;
    for (int i = start; i <= end; i++)
        result += (i > start ? separator : String()) + sections[i];
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
        if (behavior == KeepEmptyParts || !isEmpty())
            list << *this;
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


String String::toLower() const
{
    String result = *this;
    for (char &c : result.m_data)
        if (c >= 'A' && c <= 'Z')
            c = char(c - 'A' + 'a');
    return result;
}


String String::toUpper() const
{
    String result = *this;
    for (char &c : result.m_data)
        if (c >= 'a' && c <= 'z')
            c = char(c - 'a' + 'A');
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
    // overlapping ones too
    int n = 0;
    for (size_t found = m_data.find(s.m_data); found != std::string::npos; found = m_data.find(s.m_data, found + 1))
        n++;
    return n;
}


long long String::toLongLong(bool *ok) const
{
    return toNumber<long long>(*this, ok);
}


unsigned long long String::toULongLong(bool *ok) const
{
    return toNumber<unsigned long long>(*this, ok);
}


int String::toInt(bool *ok) const
{
    return toNumber<int>(*this, ok);
}


unsigned int String::toUInt(bool *ok) const
{
    return toNumber<unsigned int>(*this, ok);
}


double String::toDouble(bool *ok) const
{
    return toNumber<double>(*this, ok);
}


float String::toFloat(bool *ok) const
{
    // rounded to a double first, as it always was (rounding the text to a float directly gives
    // the other neighbour in rare cases)
    return float(toDouble(ok));
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


const char *readDouble(const char *first, const char *last, double &value)
{
    // as std::from_chars: no whitespace or '+' first, and of "0x…" only the 0 (strtod() would
    // read hexadecimal numbers); strtod() needs the end marked
    std::string text(first, last);
    if (text.empty() || isSpace(text[0]) || text[0] == '+')
        return nullptr;
    const std::size_t digits = text[0] == '-' ? 1 : 0;
    if (text.compare(digits, 2, "0x") == 0 || text.compare(digits, 2, "0X") == 0)
        text.resize(digits + 1);
    char *end = nullptr;
    errno = 0;
#if defined(_WIN32)
    static const _locale_t c = _create_locale(LC_NUMERIC, "C");
    value = _strtod_l(text.c_str(), &end, c);
#else
    static const locale_t c = newlocale(LC_NUMERIC_MASK, "C", locale_t(0));
    value = strtod_l(text.c_str(), &end, c);
#endif
    // (too large, or too small for even the smallest double; strtod() also flags those smaller
    // than normal ones)
    if (end == text.c_str() || (errno == ERANGE && (value == 0 || std::isinf(value))))
        return nullptr;
    return first + (end - text.c_str());
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
