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

#include "parse.hpp"

namespace txt {


namespace {

bool isDigit(char c)
{
    return c >= '0' && c <= '9';
}


bool isWord(char c)
{
    return isDigit(c) || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}


// \s without Unicode properties
bool isSpace(char c)
{
    return c == ' ' || (c >= '\t' && c <= '\r');
}


// the end of the line, where '.' stops matching
int lineEnd(const String &text, int from)
{
    const int newline = text.indexOf('\n', from);
    return newline < 0 ? text.size() : newline;
}


int digitsFrom(const String &text, int from)
{
    int end = from;
    while (end < text.size() && isDigit(text[end]))
        end++;
    return end;
}

} // namespace


bool parseSection(const String &line, String &name)
{
    int start = 0;
    while (start < line.size() && isSpace(line[start]))
        start++;
    if (start >= line.size() || line[start] != '[')
        return false;
    const int close = line.left(lineEnd(line, start + 1)).lastIndexOf(']');
    if (close <= start)
        return false;
    name = line.mid(start + 1, close - start - 1);
    return true;
}


bool parseKeyValue(const String &line, String &key, String &value)
{
    const int equals = line.indexOf('=');
    if (equals < 0)
        return false;
    key = line.left(equals);
    const int semicolon = line.indexOf(';', equals + 1);
    value = line.mid(equals + 1, semicolon < 0 ? -1 : semicolon - equals - 1);
    return true;
}


bool parseNumberedText(const String &line, String &number, String &text)
{
    // from the first '#' that the rest follows on the same line
    for (int hash = line.indexOf('#'); hash >= 0; hash = line.indexOf('#', hash + 1)) {
        const int digits = digitsFrom(line, hash + 1);
        const int end = lineEnd(line, hash + 1);
        const int close = line.left(end).lastIndexOf('}');
        const int open = close < 0 ? -1 : line.left(close).lastIndexOf('{');
        if (open < digits)
            continue;
        number = line.mid(hash + 1, digits - hash - 1);
        text = line.mid(open + 1, close - open - 1);
        return true;
    }
    return false;
}


bool parseNumbered(const String &name, const String &prefix, String &number)
{
    if (!name.startsWith(prefix))
        return false;
    int end = digitsFrom(name, prefix.size());
    // '$' also matches before a final '\n'
    if (end != name.size() && !(end == name.size() - 1 && name[end] == '\n'))
        return false;
    number = name.mid(prefix.size(), end - prefix.size());
    return true;
}


bool parseTagged(const String &text, String &tag, String &rest)
{
    for (int i = 0; i + 2 < text.size(); i++) {
        if (isWord(text[i]) && text[i + 1] == ':' && text[i + 2] == ' ') {
            tag = text.mid(i, 1);
            rest = text.mid(i + 3, lineEnd(text, i + 3) - i - 3);
            return true;
        }
    }
    return false;
}


bool parseMovieNumber(const String &name, String &number)
{
    for (int i = 0; i < name.size(); i++) {
        if (name[i] != 'd')
            continue;
        const int end = digitsFrom(name, i + 1);
        if (name.mid(end, 4) == ".mvi") {
            number = name.mid(i + 1, end - i - 1);
            return true;
        }
    }
    return false;
}


bool parsePosition(const String &text, String numbers[4])
{
    for (int start = 0; start <= text.size(); start++) {
        int at = start;
        int part = 0;
        for (; part < 4; part++) {
            const int end = digitsFrom(text, at);
            numbers[part] = text.mid(at, end - at);
            at = end;
            if (part < 3) {
                if (at >= text.size() || text[at] != '/')
                    break;
                at++;
            }
        }
        if (part == 4)
            return true;
    }
    return false;
}


String replaceNumbers(const String &text, const String &with)
{
    String result;
    for (int i = 0; i < text.size(); ) {
        if (isDigit(text[i])) {
            i = digitsFrom(text, i);
            result += with;
        } else {
            result += text[i++];
        }
    }
    return result;
}


} // namespace txt
