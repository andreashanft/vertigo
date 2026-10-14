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


// The parsers of txt/parse.hpp, on lines of the shapes the game data has

#include "check.hpp"

#include "txt/parse.hpp"


namespace {

// the parts a parser found, joined by '|', or "-" if it found nothing
String section(const String &line)
{
    String name;
    return txt::parseSection(line, name) ? name : "-";
}


String keyValue(const String &line)
{
    String key, value;
    return txt::parseKeyValue(line, key, value) ? key + "|" + value : "-";
}


String numberedText(const String &line)
{
    String number, text;
    return txt::parseNumberedText(line, number, text) ? number + "|" + text : "-";
}


String numbered(const String &name, const String &prefix)
{
    String number;
    return txt::parseNumbered(name, prefix, number) ? number : "-";
}


String tagged(const String &text)
{
    String tag, rest;
    return txt::parseTagged(text, tag, rest) ? tag + "|" + rest : "-";
}


String movieNumber(const String &name)
{
    String number;
    return txt::parseMovieNumber(name, number) ? number : "-";
}


String position(const String &text)
{
    String numbers[4];
    return txt::parsePosition(text, numbers) ? numbers[0] + "|" + numbers[1] + "|" + numbers[2] + "|" + numbers[3] : "-";
}

} // namespace


int main()
{
    CHECK_EQUAL(section("[Global]\r\n"), "Global");
    CHECK_EQUAL(section("  [ station1 ] ; x\n"), " station1 ");
    CHECK_EQUAL(section("[a]b]"), "a]b");
    CHECK_EQUAL(section("[a"), "-");
    CHECK_EQUAL(section("x [a]"), "-");
    CHECK_EQUAL(section(""), "-");

    CHECK_EQUAL(keyValue("key = value ; comment\n"), "key | value ");
    CHECK_EQUAL(keyValue("a=b"), "a|b");
    CHECK_EQUAL(keyValue("=x;y"), "|x");
    CHECK_EQUAL(keyValue("a=b=c"), "a|b=c");
    CHECK_EQUAL(keyValue("no value"), "-");

    // the last '{' before the last '}'
    CHECK_EQUAL(numberedText("#12 x {Hallo} y}\n"), "12|Hallo} y");
    CHECK_EQUAL(numberedText("#5 {a}{b}"), "5|b");
    CHECK_EQUAL(numberedText("#{x}"), "|x");
    CHECK_EQUAL(numberedText("#7 x"), "-");
    CHECK_EQUAL(numberedText("{x} #3"), "-");
    CHECK_EQUAL(numberedText("no hash {x}"), "-");

    CHECK_EQUAL(numbered("station12", "station"), "12");
    CHECK_EQUAL(numbered("station", "station"), "");
    CHECK_EQUAL(numbered("room3", "room"), "3");
    CHECK_EQUAL(numbered("station12x", "station"), "-");
    CHECK_EQUAL(numbered("xstation1", "station"), "-");

    CHECK_EQUAL(tagged("T: Text"), "T|Text");
    CHECK_EQUAL(tagged("condition: 12 of 345\n"), "n|12 of 345");
    CHECK_EQUAL(tagged("no tag"), "-");
    CHECK_EQUAL(tagged(": x"), "-");

    CHECK_EQUAL(movieNumber("gfx:mvi/film/d36.mvi"), "36");
    CHECK_EQUAL(movieNumber("dd5.mvi"), "5");
    CHECK_EQUAL(movieNumber("d.mvi"), "");
    CHECK_EQUAL(movieNumber("d5.mv"), "-");

    CHECK_EQUAL(position("1/2/3/4"), "1|2|3|4");
    CHECK_EQUAL(position("a1/2/3/4/5"), "1|2|3|4");
    CHECK_EQUAL(position(String("9/7/2/0") + String(1, '\0')), "9|7|2|0");
    CHECK_EQUAL(position("//3/"), "||3|");
    CHECK_EQUAL(position("1/2/3"), "-");

    CHECK_EQUAL(txt::replaceNumbers("condition: 12 of 345", "#"), "condition: # of #");
    CHECK_EQUAL(txt::replaceNumbers("text12", "#"), "text#");
    CHECK_EQUAL(txt::replaceNumbers("none", "#"), "none");
    return test::result();
}
