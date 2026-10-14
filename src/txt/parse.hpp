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

#ifndef TXT_PARSE_HPP
#define TXT_PARSE_HPP

#include "util/string.hpp"

// The patterns of the game's text formats. They were regular expressions (in the comments), and
// match the same: digits and whitespace are ASCII ones, '.' is any character but '\n'.
namespace txt {

// "^\s*\[(.*)\]": the text between the '[' after leading whitespace and the line's last ']'
bool parseSection(const String &line, String &name);

// "([^=]*)=([^;]*)": what comes before the first '=', and after it up to a ';'
bool parseKeyValue(const String &line, String &key, String &value);

// "#(\d*).*\{(.*)\}": the digits after the first '#', and the text between the last '{' that a
// '}' follows and the last '}'
bool parseNumberedText(const String &line, String &number, String &text);

// "^<prefix>(\d*)$": the digits after the prefix, if nothing else follows
bool parseNumbered(const String &name, const String &prefix, String &number);

// "(\w): (.*)": the letter, digit or '_' before the first ": ", and the rest of the line
bool parseTagged(const String &text, String &tag, String &rest);

// "d(\d*)\.mvi": the digits of the first "d<digits>.mvi"
bool parseMovieNumber(const String &name, String &number);

// "(\d*)/(\d*)/(\d*)/(\d*)": the first four numbers between slashes
bool parsePosition(const String &text, String numbers[4]);

// "\d+" replaced
String replaceNumbers(const String &text, const String &with);

} // namespace txt

#endif // TXT_PARSE_HPP
