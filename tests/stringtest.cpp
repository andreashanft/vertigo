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


// util::String, txt::Value and util::DateTime, as the game uses them

#include "check.hpp"

#include "txt/value.h"
#include "util/datetime.hpp"
#include "util/string.hpp"


namespace {

String withZero(const char *before, const char *after)
{
    return String(before) + String(1, '\0') + String(after);
}


void testNumbers()
{
    bool ok = false;
    CHECK_EQUAL(String("12").toInt(&ok), 12);
    CHECK(ok);
    CHECK_EQUAL(String(" \t-7 \r\n").toInt(), -7);
    CHECK_EQUAL(String("+5").toInt(), 5);
    CHECK_EQUAL(String("+-5").toInt(&ok), 0);
    CHECK(!ok);
    CHECK_EQUAL(String("12a").toInt(&ok), 0);
    CHECK(!ok);
    CHECK_EQUAL(String("10.5").toInt(&ok), 0);
    CHECK(!ok);
    CHECK_EQUAL(String("").toInt(&ok), 0);
    CHECK(!ok);
    CHECK_EQUAL(String("2147483647").toInt(), 2147483647);
    CHECK_EQUAL(String("2147483648").toInt(&ok), 0);
    CHECK(!ok);
    CHECK_EQUAL(String("2147483648").toLongLong(), 2147483648LL);
    CHECK_EQUAL(String("4294967295").toUInt(), 4294967295u);
    CHECK_EQUAL(String("-1").toUInt(&ok), 0u);
    CHECK(!ok);
    CHECK_EQUAL(String("18446744073709551615").toULongLong(), 18446744073709551615ULL);
    // the data has values ending with a zero byte, the rest doesn't count
    CHECK_EQUAL(withZero("206 ", "").toInt(), 206);
    CHECK_EQUAL(withZero("7", "x").toInt(), 7);
    CHECK_EQUAL(withZero("", "5").toInt(&ok), 0);
    CHECK(!ok);
    CHECK_EQUAL(withZero("1.5", "").toDouble(), 1.5);
    // only ASCII whitespace: in code page 437, 0xa0 is 'á'
    CHECK_EQUAL(String::fromLatin1("\xa0" "7").toInt(&ok), 0);
    CHECK(!ok);

    CHECK_EQUAL(String("1.5").toDouble(), 1.5);
    CHECK_EQUAL(String(" -.5 ").toDouble(), -0.5);
    CHECK_EQUAL(String("1.").toDouble(), 1.0);
    CHECK_EQUAL(String("1e3").toDouble(), 1000.0);
    CHECK_EQUAL(String("2.5E-1").toDouble(), 0.25);
    CHECK_EQUAL(String("+3").toDouble(), 3.0);
    CHECK_EQUAL(String("1,5").toDouble(&ok), 0.0);
    CHECK(!ok);
    CHECK_EQUAL(String("1e").toDouble(&ok), 0.0);
    CHECK(!ok);
    CHECK_EQUAL(String("1e999").toDouble(&ok), 0.0);
    CHECK(!ok);
    CHECK_EQUAL(String(".").toDouble(&ok), 0.0);
    CHECK(!ok);
    // through a double, as always
    CHECK_EQUAL(String("0.1").toFloat(), float(0.1));
    CHECK_EQUAL(String("2.7").toFloat(&ok), 2.7f);
    CHECK(ok);

    CHECK_EQUAL(String::number(42), "42");
    CHECK_EQUAL(String::number(-5), "-5");
    CHECK_EQUAL(String::number(4000000000u), "4000000000");
    CHECK_EQUAL(String::number(255, 16), "ff");
    CHECK_EQUAL(String::number(0.1), "0.1");
    CHECK_EQUAL(String::number(1234567.0), "1.23457e+06");
    CHECK_EQUAL(String::number(1e-5), "1e-05");
    CHECK_EQUAL(String::number(2.5, 'f', 0), "2");
    CHECK_EQUAL(String::number(1.0/3, 'e', 2), "3.33e-01");
    CHECK_EQUAL(String::number(1.0/3, 'E', 2), "3.33E-01");
}


void testArg()
{
    CHECK_EQUAL(String("%1-%2").arg("a").arg("b"), "a-b");
    CHECK_EQUAL(String("%2 %1").arg("x"), "%2 x");
    CHECK_EQUAL(String("%1%1").arg("x"), "xx");
    CHECK_EQUAL(String("%10 %1").arg("x"), "%10 x");
    CHECK_EQUAL(String("no escape").arg("x"), "no escape");
    CHECK_EQUAL(String("%2 %1").arg(String("a"), String("b")), "b a");
    CHECK_EQUAL(String("%1/%2/%3").arg(String("1"), String("2"), String("3")), "1/2/3");
    CHECK_EQUAL(String("[%1]").arg("ab", 4), "[  ab]");
    CHECK_EQUAL(String("[%1]").arg("ab", -4, '*'), "[ab**]");
    CHECK_EQUAL(String("%1").arg('c'), "c");
    CHECK_EQUAL(String("tex_%1").arg(7, 3, 10, '0'), "tex_007");
    CHECK_EQUAL(String("%1").arg(-7, 4, 10, '0'), "-007");
    CHECK_EQUAL(String("%1").arg(-7, 4), "  -7");
    CHECK_EQUAL(String("%1").arg(255, 0, 16), "ff");
    CHECK_EQUAL(String("%1").arg(4000000000u), "4000000000");
    CHECK_EQUAL(String("%1").arg(-9000000000LL), "-9000000000");
    // doubles as the traces print them: correctly rounded, so exact halves to even
    CHECK_EQUAL(String("%1").arg(229.26, 0, 'f', 1), "229.3");
    CHECK_EQUAL(String("%1").arg(229.25, 0, 'f', 1), "229.2");
    CHECK_EQUAL(String("%1").arg(-0.04, 0, 'f', 1), "-0.0");
    CHECK_EQUAL(String("%1").arg(-0.0, 0, 'f', 1), "0.0");
    CHECK_EQUAL(String("%1").arg(1.5, 6, 'f', 2, '0'), "001.50");
    CHECK_EQUAL(String("%1").arg(0.5), "0.5");
}


void testText()
{
    CHECK_EQUAL(String(" \t a b \r\n").trimmed(), "a b");
    CHECK_EQUAL(String::fromLatin1("\x85" "x\xa0").trimmed(), String::fromLatin1("\x85" "x\xa0"));
    CHECK_EQUAL(String::fromLatin1("AbC\xc4\x8e").toLower(), String::fromLatin1("abc\xc4\x8e"));
    CHECK_EQUAL(String::fromLatin1("abc\xe1\x84").toUpper(), String::fromLatin1("ABC\xe1\x84"));

    CHECK_EQUAL(String("a,,b").split(',').join('|'), "a||b");
    CHECK_EQUAL(String("a,,b").split(',', String::SkipEmptyParts).join('|'), "a|b");
    CHECK_EQUAL(String(",a,").split(',').size(), 3);
    CHECK_EQUAL(String("").split(',').size(), 1);
    CHECK_EQUAL(String("").split(',', String::SkipEmptyParts).size(), 0);
    CHECK_EQUAL(String("a  b").split("  ").join('|'), "a|b");

    CHECK_EQUAL(String("  3082 x").section(' ', 0, 0, String::SectionSkipEmpty), "3082");
    CHECK_EQUAL(String("\\path\\to\\file.img").section('\\', -1), "file.img");
    CHECK_EQUAL(String("file.img").section('.', 0, 0), "file");
    CHECK_EQUAL(String("x.y.z").section('.', 1), "y.z");
    CHECK_EQUAL(String("x.y.z").section('.', -2, -2), "y");
    CHECK_EQUAL(String("x.y.z").section('.', 5), "");

    const String hello("hello");
    CHECK_EQUAL(hello.mid(1, 3), "ell");
    CHECK_EQUAL(hello.mid(3), "lo");
    CHECK_EQUAL(hello.mid(9), "");
    CHECK_EQUAL(hello.mid(-2, 4), "he");
    CHECK_EQUAL(hello.mid(-2), "hello");
    CHECK_EQUAL(hello.mid(2, 99), "llo");
    CHECK_EQUAL(hello.left(2), "he");
    CHECK_EQUAL(hello.left(-1), "hello");
    CHECK_EQUAL(hello.left(9), "hello");
    CHECK_EQUAL(hello.right(2), "lo");
    CHECK_EQUAL(hello.right(9), "hello");
    CHECK_EQUAL(hello.indexOf('l'), 2);
    CHECK_EQUAL(hello.indexOf('l', 3), 3);
    CHECK_EQUAL(hello.indexOf("lo"), 3);
    CHECK_EQUAL(hello.indexOf('x'), -1);
    CHECK_EQUAL(hello.lastIndexOf('l'), 3);
    CHECK_EQUAL(hello.lastIndexOf('l', 2), 2);
    CHECK_EQUAL(hello.lastIndexOf("he"), 0);
    CHECK_EQUAL(String("aaa").count("aa"), 2);
    CHECK_EQUAL(hello.count('l'), 2);
    CHECK(hello.startsWith("he") && hello.endsWith("lo") && hello.contains("ell") && !hello.contains("x"));
    CHECK(!String("lo").endsWith("hello"));

    String s("a.b.c");
    CHECK_EQUAL(s.replace(".", "::"), "a::b::c");
    CHECK_EQUAL(s.remove(':'), "abc");
    CHECK_EQUAL(s.insert(1, "-"), "a-bc");
    CHECK_EQUAL(s.insert(99, "!"), "a-bc!");
    CHECK_EQUAL(s.remove(1, 1), "abc!");
    s.chop(1);
    CHECK_EQUAL(s, "abc");
    s.truncate(1);
    CHECK_EQUAL(s, "a");
    CHECK_EQUAL(String("vfx\\surface").replace('\\', '/'), "vfx/surface");

    // UTF-8 in and out, as Latin-1
    CHECK_EQUAL(String("Zerstört").toLatin1(), "Zerst\xf6rt");
    CHECK_EQUAL(String("€").toLatin1(), "?");
    CHECK_EQUAL(String::fromUtf8("a\xff" "b").toLatin1(), "a?b");
    CHECK_EQUAL(String::fromLatin1("\xe4" "b").toUtf8(), "\xc3\xa4" "b");

    StringList list{ "station1", "room2", "station3" };
    CHECK_EQUAL(list.withPrefix("station").join(','), "station1,station3");
    CHECK_EQUAL(list.indexOf("room2"), 1);
    CHECK(list.contains("room2") && !list.contains("room"));
    CHECK_EQUAL(list.removeAll("room2"), 1);
    CHECK_EQUAL(list.value(5, "none"), "none");
}


void testValue()
{
    bool ok = false;
    CHECK_EQUAL(txt::Value("12").toInt(&ok), 12);
    CHECK(ok);
    CHECK_EQUAL(txt::Value("abc").toInt(&ok), 0);
    CHECK(!ok);
    CHECK_EQUAL(txt::Value(" 1.5 ").toFloat(), 1.5f);
    CHECK_EQUAL(txt::Value("2.5").toDouble(), 2.5);
    CHECK(!txt::Value("false").toBool() && !txt::Value("FALSE").toBool() && !txt::Value("0").toBool() && !txt::Value("").toBool());
    CHECK(txt::Value("x").toBool() && txt::Value("1").toBool());
    // the defaults the code passes
    CHECK_EQUAL(txt::Value(7).toString(), "7");
    CHECK_EQUAL(txt::Value(2.5).toInt(), 3);
    CHECK_EQUAL(txt::Value(-2.5).toInt(), -3);
    CHECK(!txt::Value(0.4).toBool() && txt::Value(0.5).toBool());
    CHECK_EQUAL(txt::Value(0.5f).toString(), "0.5");
    CHECK_EQUAL(txt::Value(123.456).toString(), "123.456");
    CHECK_EQUAL(txt::Value(1e20).toString(), "1e+20");
    CHECK_EQUAL(txt::Value(-0.0).toString(), "0");
    CHECK_EQUAL(txt::Value(true).toString(), "true");
    CHECK_EQUAL(txt::Value(true).toInt(), 1);
    // lists: text with one line, never numbers
    CHECK_EQUAL(txt::Value(StringList{ "12" }).toString(), "12");
    CHECK_EQUAL(txt::Value(StringList{ "a", "b" }).toString(), "");
    CHECK_EQUAL(txt::Value(StringList{ "a", "b" }).toStringList().size(), 2);
    CHECK_EQUAL(txt::Value(StringList{ "12" }).toInt(&ok), 0);
    CHECK(!ok);
    CHECK(txt::Value().isNull() && !txt::Value("").isNull());
    CHECK_EQUAL(txt::Value().toString(), "");
}


void testDateTime()
{
    const util::DateTime time = util::DateTime::fromIso("2026-10-13T10:37:52.563");
    CHECK(time.isValid());
    CHECK_EQUAL(time.year, 2026);
    CHECK_EQUAL(time.month, 10);
    CHECK_EQUAL(time.day, 13);
    CHECK_EQUAL(time.hour, 10);
    CHECK_EQUAL(time.minute, 37);
    CHECK_EQUAL(time.second, 52);
    CHECK_EQUAL(time.millisecond, 563);
    CHECK_EQUAL(time.toIso(), "2026-10-13T10:37:52.563");
    CHECK_EQUAL(util::DateTime::fromIso("2026-10-13T10:37:52").toIso(), "2026-10-13T10:37:52.000");
    CHECK(util::DateTime::fromIso("2024-02-29T00:00:00").isValid());
    for (const char *text : { "", "garbage", "2026-02-29T00:00:00", "2026-13-01T00:00:00", "2026-10-00T00:00:00", "2026-10-14",
                              "2026-10-14T16:60:00", "2026-10-14T24:00:00", "2026-10-14 16:05:09", "2026-10-14T16:05:09.12",
                              "2026-10-14T16:05:09.1234", "0000-10-14T16:05:09" })
        test::equal(util::DateTime::fromIso(text).isValid(), false, text, __LINE__);
    CHECK(util::DateTime::fromIso("2026-10-13T10:37:52.563") < util::DateTime::fromIso("2026-10-13T10:37:52.564"));
    CHECK(util::DateTime::fromIso("2027-01-01T00:00:00") > util::DateTime::fromIso("2026-12-31T23:59:59.999"));
    const util::DateTime now = util::DateTime::now();
    CHECK(now.isValid());
    CHECK(util::DateTime::fromIso(now.toIso()) == now);
}

} // namespace


int main()
{
    testNumbers();
    testArg();
    testText();
    testValue();
    testDateTime();
    return test::result();
}
