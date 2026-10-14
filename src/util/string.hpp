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

#ifndef UTIL_STRING_HPP
#define UTIL_STRING_HPP

#include <functional>
#include <initializer_list>
#include <iosfwd>
#include <string>
#include <type_traits>
#include <vector>

// the bridge to Qt while it is still linked (until step Q5 of the iPad port)
#include <QString>
#include <QStringList>
class QRegularExpression;

namespace util {

class StringList;

// Text as the game uses it: one byte per character, the character code being the byte
// (Latin-1). The game data is 8-bit text (DOS code page 437) and the fonts are indexed by
// those bytes, so this keeps them as they are. The functions are the part of QString's
// interface the code uses and behave like it on such text (the comments name the cases
// where that isn't obvious). Literals and other UTF-8 text become Latin-1, characters
// outside it '?'. File paths, which can hold any character, aren't Strings.
class String
{
public:
    enum SplitBehavior { KeepEmptyParts, SkipEmptyParts };
    enum SectionFlag { SectionDefault = 0, SectionSkipEmpty = 1 };

public:
    String() = default;
    String(const char *utf8);
    String(int size, char c) : m_data(size_t(size > 0 ? size : 0), c) {}

    static String fromLatin1(const char *data, int size = -1);
    static String fromLatin1(const std::string &data) { return fromLatin1(data.data(), int(data.size())); }
    static String fromUtf8(const char *data, int size = -1);
    static String fromUtf8(const std::string &data) { return fromUtf8(data.data(), int(data.size())); }
    static String fromStdString(const std::string &utf8) { return fromUtf8(utf8); }
    static String fromLocal8Bit(const char *data) { return fromUtf8(data); }

    // QString::number(): as arg() below
    static String number(int n, int base = 10) { return number((long long)n, base); }
    static String number(unsigned int n, int base = 10) { return number((unsigned long long)n, base); }
    static String number(long n, int base = 10) { return number((long long)n, base); }
    static String number(unsigned long n, int base = 10) { return number((unsigned long long)n, base); }
    static String number(long long n, int base = 10);
    static String number(unsigned long long n, int base = 10);
    static String number(double n, char format = 'g', int precision = 6);
    // as QChar::isSpace(): 9 to 13, 32, 0x85 and 0xa0
    static bool isSpace(char c);

public:
    // sizes are signed as in Qt, so size() - 1 is -1 for an empty string
    int size() const { return int(m_data.size()); }
    int length() const { return size(); }
    bool isEmpty() const { return m_data.empty(); }
    char at(int i) const { return m_data[size_t(i)]; }
    char operator[](int i) const { return m_data[size_t(i)]; }
    char &operator[](int i) { return m_data[size_t(i)]; }
    char front() const { return m_data.front(); }
    char back() const { return m_data.back(); }
    // the Latin-1 bytes, zero-terminated
    const char *constData() const { return m_data.c_str(); }
    const char *data() const { return m_data.c_str(); }
    std::string::const_iterator begin() const { return m_data.begin(); }
    std::string::const_iterator end() const { return m_data.end(); }

    std::string toLatin1() const { return m_data; }
    std::string toUtf8() const;
    std::string toStdString() const { return toUtf8(); }

    void clear() { m_data.clear(); }
    void resize(int size) { m_data.resize(size_t(size > 0 ? size : 0)); }
    void chop(int n);
    void truncate(int position);
    String &append(const String &s) { m_data += s.m_data; return *this; }
    String &append(char c) { m_data += c; return *this; }
    String &prepend(const String &s) { m_data.insert(0, s.m_data); return *this; }
    String &insert(int position, const String &s);
    String &remove(int position, int n);
    String &remove(char c);
    String &remove(const String &s);
    String &replace(char before, char after);
    String &replace(const String &before, const String &after);
    String &operator+=(const String &s) { return append(s); }
    String &operator+=(const char *s) { return append(String(s)); }
    String &operator+=(char c) { return append(c); }

    // like QString, for indexes beyond the string as well
    String left(int n) const;
    String right(int n) const;
    String mid(int position, int n = -1) const;
    String section(char separator, int start, int end = -1, int flags = SectionDefault) const;
    String section(const String &separator, int start, int end = -1, int flags = SectionDefault) const;
    StringList split(char separator, SplitBehavior behavior = KeepEmptyParts) const;
    StringList split(const String &separator, SplitBehavior behavior = KeepEmptyParts) const;
    // whitespace: 9 to 13, 32, and like QChar::isSpace() also 0x85 and 0xa0 (in the game's code
    // page 'à' and 'á')
    String trimmed() const;
    String simplified() const;
    // as QString: 0xe0-0xfe (not 0xf7) and 0xc0-0xde (not 0xd7) change case; 0xdf (ß) becomes
    // "SS", 0xff and 0xb5 (µ) become characters outside Latin-1, here '?'
    String toLower() const;
    String toUpper() const;
    String repeated(int times) const;

    bool contains(char c) const { return m_data.find(c) != std::string::npos; }
    bool contains(const String &s) const { return m_data.find(s.m_data) != std::string::npos; }
    bool startsWith(char c) const { return !m_data.empty() && m_data.front() == c; }
    bool startsWith(const String &s) const { return m_data.compare(0, s.m_data.size(), s.m_data) == 0; }
    bool endsWith(char c) const { return !m_data.empty() && m_data.back() == c; }
    bool endsWith(const String &s) const;
    int indexOf(char c, int from = 0) const;
    int indexOf(const String &s, int from = 0) const;
    int lastIndexOf(char c, int from = -1) const;
    int lastIndexOf(const String &s, int from = -1) const;
    int count(char c) const;
    int count(const String &s) const;
    int count() const { return size(); }

    // as QString: the C locale, whitespace around the number allowed, nothing else; 0 and
    // ok = false if the text isn't a number of the type ("10.5".toInt() is 0)
    int toInt(bool *ok = nullptr, int base = 10) const;
    unsigned int toUInt(bool *ok = nullptr, int base = 10) const;
    long long toLongLong(bool *ok = nullptr, int base = 10) const;
    unsigned long long toULongLong(bool *ok = nullptr, int base = 10) const;
    double toDouble(bool *ok = nullptr) const;
    float toFloat(bool *ok = nullptr) const;

    // as QString::arg(): replaces every %1 to %99 with the lowest number in the text; a positive
    // field width pads on the left, a negative one on the right; a '0' fill pads numbers after
    // their sign
    String arg(const String &a, int fieldWidth = 0, char fill = ' ') const;
    String arg(const char *a, int fieldWidth = 0, char fill = ' ') const { return arg(String(a), fieldWidth, fill); }
    String arg(char a, int fieldWidth = 0, char fill = ' ') const { return arg(String(1, a), fieldWidth, fill); }
    template <typename T, typename std::enable_if<std::is_integral<T>::value && !std::is_same<T, char>::value && !std::is_same<T, bool>::value, int>::type = 0>
    String arg(T a, int fieldWidth = 0, int base = 10, char fill = ' ') const
    {
        if (std::is_signed<T>::value)
            return argInteger((long long)a, false, fieldWidth, base, fill);
        return argInteger((long long)(unsigned long long)a, true, fieldWidth, base, fill);
    }
    String arg(double a, int fieldWidth = 0, char format = 'g', int precision = -1, char fill = ' ') const;
    // %1, %2, ... at once, in the order of their numbers
    String arg(const String &a1, const String &a2) const { return multiArg({ &a1, &a2 }); }
    String arg(const String &a1, const String &a2, const String &a3) const { return multiArg({ &a1, &a2, &a3 }); }
    String arg(const String &a1, const String &a2, const String &a3, const String &a4) const { return multiArg({ &a1, &a2, &a3, &a4 }); }

    friend bool operator==(const String &a, const String &b) { return a.m_data == b.m_data; }
    friend bool operator!=(const String &a, const String &b) { return a.m_data != b.m_data; }
    friend bool operator<(const String &a, const String &b) { return a.m_data < b.m_data; }
    friend bool operator>(const String &a, const String &b) { return a.m_data > b.m_data; }
    friend bool operator<=(const String &a, const String &b) { return a.m_data <= b.m_data; }
    friend bool operator>=(const String &a, const String &b) { return a.m_data >= b.m_data; }
    friend bool operator==(const String &a, const char *b) { return a == String(b); }
    friend bool operator!=(const String &a, const char *b) { return !(a == String(b)); }
    friend bool operator==(const char *a, const String &b) { return String(a) == b; }
    friend bool operator!=(const char *a, const String &b) { return !(String(a) == b); }
    friend bool operator==(const String &a, char b) { return a.m_data.size() == 1 && a.m_data[0] == b; }
    friend bool operator!=(const String &a, char b) { return !(a == b); }
    friend String operator+(String a, const String &b) { return a.append(b); }
    friend String operator+(String a, const char *b) { return a.append(String(b)); }
    friend String operator+(const char *a, const String &b) { return String(a).append(b); }
    friend String operator+(String a, char b) { return a.append(b); }
    friend String operator+(char a, const String &b) { return String(1, a).append(b); }

    // as UTF-8
    friend std::ostream &operator<<(std::ostream &stream, const String &s);

    // while Qt is linked: QString takes any String; the other way round characters outside
    // Latin-1 become '?', so paths stay QStrings
    explicit String(const QString &s);
    operator QString() const { return QString::fromLatin1(m_data.data(), size()); }
    // all bytes (QByteArray also converts to a pointer, which would end at a zero byte)
    static String fromLatin1(const QByteArray &data) { return fromLatin1(data.constData(), int(data.size())); }
    static String fromUtf8(const QByteArray &data) { return fromUtf8(data.constData(), int(data.size())); }
    // regular expressions until step Q3
    String &replace(const QRegularExpression &expression, const String &after);
    String &remove(const QRegularExpression &expression) { return replace(expression, String()); }
    bool contains(const QRegularExpression &expression) const;
    StringList split(const QRegularExpression &expression, SplitBehavior behavior = KeepEmptyParts) const;

private:
    String argInteger(long long a, bool isUnsigned, int fieldWidth, int base, char fill) const;
    String replaceArg(const String &arg, int fieldWidth, char fill) const;
    String multiArg(std::initializer_list<const String *> args) const;

private:
    std::string m_data;

    friend struct std::hash<String>;
};


// QStringList's part in use, with signed sizes
class StringList : public std::vector<String>
{
public:
    StringList() = default;
    StringList(std::initializer_list<String> list) : std::vector<String>(list) {}

    int size() const { return int(std::vector<String>::size()); }
    int count() const { return size(); }
    int length() const { return size(); }
    bool isEmpty() const { return empty(); }
    const String &at(int i) const { return (*this)[size_t(i)]; }
    const String &operator[](int i) const { return std::vector<String>::operator[](size_t(i)); }
    String &operator[](int i) { return std::vector<String>::operator[](size_t(i)); }
    String value(int i, const String &defaultValue = String()) const { return i >= 0 && i < size() ? at(i) : defaultValue; }
    const String &first() const { return front(); }
    const String &last() const { return back(); }
    void append(const String &s) { push_back(s); }
    void append(const StringList &list) { insert(end(), list.begin(), list.end()); }
    void prepend(const String &s) { insert(begin(), s); }
    StringList &operator<<(const String &s) { push_back(s); return *this; }
    StringList &operator<<(const StringList &list) { append(list); return *this; }
    bool contains(const String &s) const;
    int indexOf(const String &s, int from = 0) const;
    int removeAll(const String &s);
    void removeAt(int i) { erase(begin() + i); }
    String takeFirst();
    String join(const String &separator) const;
    String join(char separator) const { return join(String(1, separator)); }

    // while Qt is linked
    explicit StringList(const QStringList &list);
    operator QStringList() const;
    StringList filter(const QRegularExpression &expression) const;
    // those starting with the prefix
    StringList withPrefix(const String &prefix) const;
};

} // namespace util

namespace std {
template <>
struct hash<util::String>
{
    size_t operator()(const util::String &s) const { return hash<string>()(s.m_data); }
};
}

using String = util::String;
using StringList = util::StringList;

#endif // UTIL_STRING_HPP
