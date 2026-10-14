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

// util::String and txt::Value replace QString and QVariant. While Qt is linked (until step Q5 of
// the iPad port) they are compared with them here: on fixed and random cases, and with the game
// data's directory as the argument also on every line and value of its text files.

#include "util/string.hpp"
#include "txt/value.h"
#include "util/datetime.hpp"

#include <QDateTime>
#include <QDirIterator>
#include <QFile>
#include <QStringList>
#include <QVariant>

#include <cmath>
#include <cstring>
#include <iostream>
#include <random>


namespace {

// String and QString, as String converted them while Qt was linked: Latin-1, characters beyond
// it become '?'
QString toQt(const String &s)
{
    return QString::fromLatin1(s.constData(), s.size());
}


String fromQt(const QString &s)
{
    const QByteArray latin1 = s.toLatin1();
    return String::fromLatin1(latin1.constData(), int(latin1.size()));
}


QStringList toQt(const StringList &list)
{
    QStringList result;
    for (const String &s : list)
        result << toQt(s);
    return result;
}


int g_checks = 0;
int g_failures = 0;


void fail(const QString &what, const QString &expected, const QString &actual)
{
    if (++g_failures <= 100000)
        std::cerr << "FAIL " << what.toUtf8().constData() << ": Qt " << expected.toUtf8().constData()
                  << ", ours " << actual.toUtf8().constData() << std::endl;
}


QString describe(const QString &s)
{
    QString d = "\"";
    for (QChar c : s)
        d += c.unicode() >= 32 && c.unicode() < 127 ? QString(c) : QString("\\x%1").arg(int(c.unicode()), 2, 16, QChar('0'));
    return d + "\"";
}


// texts compare as Latin-1, which is how the game uses them
void same(const QString &what, const QString &expected, const String &actual)
{
    g_checks++;
    const QString latin1 = QString::fromLatin1(expected.toLatin1());
    if (latin1 != toQt(actual))
        fail(what, describe(latin1), describe(toQt(actual)));
}


void same(const QString &what, const QStringList &expected, const StringList &actual)
{
    g_checks++;
    QStringList latin1;
    for (const QString &s : expected)
        latin1 << QString::fromLatin1(s.toLatin1());
    if (latin1 != toQt(actual)) {
        QStringList a, b;
        for (const QString &s : latin1)
            a << describe(s);
        for (const String &s : actual)
            b << describe(toQt(s));
        fail(what, "(" + a.join(", ") + ")", "(" + b.join(", ") + ")");
    }
}


template <typename T>
void same(const QString &what, T expected, bool expectedOk, T actual, bool actualOk)
{
    g_checks++;
    const bool equal = expected == actual || (std::isnan(double(expected)) && std::isnan(double(actual)));
    if (!equal || expectedOk != actualOk)
        fail(what, QString::number(double(expected), 'g', 17) + (expectedOk ? " ok" : " not ok"),
             QString::number(double(actual), 'g', 17) + (actualOk ? " ok" : " not ok"));
}


void sameNumbers(const QString &text)
{
    const String s = fromQt(text);
    const QString what = describe(text);
    bool okQ, ok;
    int i = text.toInt(&okQ);
    same(what + ".toInt()", i, okQ, s.toInt(&ok), ok);
    unsigned u = text.toUInt(&okQ);
    same(what + ".toUInt()", u, okQ, s.toUInt(&ok), ok);
    long long l = text.toLongLong(&okQ);
    same(what + ".toLongLong()", l, okQ, s.toLongLong(&ok), ok);
    i = text.toInt(&okQ, 16);
    same(what + ".toInt(16)", i, okQ, s.toInt(&ok, 16), ok);
    i = text.toInt(&okQ, 0);
    same(what + ".toInt(0)", i, okQ, s.toInt(&ok, 0), ok);
    double d = text.toDouble(&okQ);
    same(what + ".toDouble()", d, okQ, s.toDouble(&ok), ok);
    float f = text.toFloat(&okQ);
    same(what + ".toFloat()", f, okQ, s.toFloat(&ok), ok);
}


void sameText(const QString &text)
{
    const String s = fromQt(text);
    const QString what = describe(text);
    same(what + ".trimmed()", text.trimmed(), s.trimmed());
    same(what + ".simplified()", text.simplified(), s.simplified());
    same(what + ".toLower()", text.toLower(), s.toLower());
    same(what + ".toUpper()", text.toUpper(), s.toUpper());
    for (const char *separator : { " ", ",", "\\", "=", "  " }) {
        same(what + ".split(" + separator + ")", text.split(separator), s.split(separator));
        same(what + ".split(" + separator + ", skip)", text.split(separator, Qt::SkipEmptyParts), s.split(separator, String::SkipEmptyParts));
    }
    for (int start = -3; start <= 3; start++) {
        for (int end = -2; end <= 2; end++) {
            same(what + QString(".section(' ', %1, %2)").arg(start).arg(end), text.section(' ', start, end), s.section(' ', start, end));
            same(what + QString(".section(' ', %1, %2, skip)").arg(start).arg(end),
                 text.section(' ', start, end, QString::SectionSkipEmpty), s.section(' ', start, end, String::SectionSkipEmpty));
        }
        same(what + QString(".section('\\\\', %1)").arg(start), text.section('\\', start), s.section('\\', start));
        same(what + QString(".section('.', %1, 0)").arg(start), text.section('.', start, 0), s.section('.', start, 0));
    }
    for (int position = -3; position <= text.size() + 2; position++) {
        same(what + QString(".left(%1)").arg(position), text.left(position), s.left(position));
        same(what + QString(".right(%1)").arg(position), text.right(position), s.right(position));
        same(what + QString(".mid(%1)").arg(position), text.mid(position), s.mid(position));
        for (int n = -2; n <= 3; n++)
            same(what + QString(".mid(%1, %2)").arg(position).arg(n), text.mid(position, n), s.mid(position, n));
        same(what + QString(".indexOf(' ', %1)").arg(position), QString::number(text.indexOf(' ', position)), String::number(s.indexOf(' ', position)));
    }
    sameNumbers(text);
}


void sameValue(const QVariant &q, const txt::Value &v, const QString &what)
{
    bool okQ, ok;
    same(what + ".toString()", q.toString(), v.toString());
    same(what + ".toStringList()", q.toStringList(), v.toStringList());
    int i = q.toInt(&okQ);
    same(what + ".toInt()", i, okQ, v.toInt(&ok), ok);
    float f = q.toFloat(&okQ);
    same(what + ".toFloat()", f, okQ, v.toFloat(&ok), ok);
    double d = q.toDouble(&okQ);
    same(what + ".toDouble()", d, okQ, v.toDouble(&ok), ok);
    same(what + ".toBool()", int(q.toBool()), true, int(v.toBool()), true);
}


void sameValue(const QString &text)
{
    sameValue(QVariant(text), txt::Value(fromQt(text)), "Value(" + describe(text) + ")");
}


const char *const NumberTexts[] = {
    "", " ", "0", "1", "-1", "+1", "12", " 12", "12 ", "\t12\n", "1 2", "12a", "a12", "0x10", "0X1f", "010", "08",
    "--1", "+-1", "-+1", "+", "-", "1.", ".5", "-.5", "+.5", "1.5", "10.5", "1e3", "1E3", "1e+3", "1e-3", "1e", "1e+",
    "e5", ".", "1..2", "1.2.3", "1,5", "1,000", "inf", "-inf", "+inf", "INF", "Inf", "nan", "NaN", "NAN", "infinity",
    "2147483647", "2147483648", "-2147483648", "-2147483649", "4294967295", "4294967296", "9223372036854775807",
    "9223372036854775808", "-9223372036854775808", "18446744073709551616", "1e38", "3.5e38", "1e39", "-1e39",
    "1e-45", "1e-46", "1e308", "1e309", "1e-320", "1e-400", "0.1", "0.2", "0.3", "1.0f", "0.000001", "123456789012",
    "\x85" "5\xa0", "\xa0" "7", "5\x85", "\xa0", "1\x0b", "\x0c" "2", "00", "-0", "+0", "-0.0", "0e0", "007", "1_000",
};

// values with a zero byte, as some in the data
const char NumbersWithZero[][8] = { "206\0", "1.5\0", "\0" "5", "7\0x", "12 \0", "" };

const double Doubles[] = {
    0, -0.0, 0.5, 1.5, 2.5, -2.5, 0.05, 0.15, 0.25, 0.35, -0.04, -0.05, 1e-5, 1.23456789, 123456.789, 1234567.0,
    1e15, 1e16, 1e17, 1e20, 1e21, 1e-4, 1e-7, 0.1, 1.0/3, 2.0/3, 99.95, 99.995, 1e100, -1e-100, 16.0, 3.0, 100.0,
    1e6, 999999.5, 0.000123456, 47.25, -273.15, 5.55e-10,
};

} // namespace


int main(int argc, char *argv[])
{
    // QString::arg() warns about texts without escapes, which are tested on purpose
    qInstallMessageHandler([](QtMsgType, const QMessageLogContext &, const QString &) {});

    // every character, alone and around text
    for (int c = 1; c < 256; c++) {
        const QString one = QString(QChar(c));
        sameText(one);
        sameText(QString("a") + one + "B");
        sameText(one + "x " + one + one);
    }

    for (const char *text : NumberTexts) {
        sameText(QString::fromLatin1(text));
        sameValue(QString::fromLatin1(text));
    }
    for (const char *text : NumbersWithZero) {
        const QString s = QString::fromLatin1(text, int(strlen(text)) + 1 + int(strlen(text + strlen(text) + 1)));
        sameNumbers(s);
        sameValue(s);
    }

    // random number-like texts
    std::mt19937 random(1);
    const char alphabet[] = "0123456789+-.eE x\t";
    for (int i = 0; i < 100000; i++) {
        QString text;
        const int length = int(random() % 10);
        for (int k = 0; k < length; k++)
            text += QChar(alphabet[random() % (sizeof(alphabet) - 1)]);
        sameNumbers(text);
    }

    // texts with separators
    for (const char *text : { "a b c", " a  b ", "a\\b\\c.d", "x.y.z", "a,,b", ",a,", "  ", "a  b  c", "key = value ; c",
                              "\\path\\to\\file.img", "one", "", "a\\", "\\a" })
        sameText(QString::fromLatin1(text));

    // arg() and number()
    const QStringList formats = { "%1", "%2 %1", "a%1b%1c", "%1%2", "%10 %1", "%01 %2", "%1 %9 %10 %99", "no escape",
                                  "%", "100%", "%%1", "%0 %1", "%1%" };
    for (const QString &format : formats) {
        const String f = fromQt(format);
        for (int width : { 0, 3, -3, 6, -6 }) {
            for (QChar fill : { QChar(' '), QChar('0'), QChar('*') }) {
                const QString what = describe(format) + QString(".arg(…, %1, '%2')").arg(width).arg(fill);
                for (const char *a : { "", "x", "abc", "longer text" })
                    same(what + a, format.arg(QString(a), width, fill), f.arg(String(a), width, fill.toLatin1()));
                for (int n : { 0, 5, -5, 42, -42, 12345, -123456, INT_MAX, INT_MIN }) {
                    same(what + QString::number(n), format.arg(n, width, 10, fill), f.arg(n, width, 10, fill.toLatin1()));
                    same(what + QString::number(n) + " hex", format.arg(n, width, 16, fill), f.arg(n, width, 16, fill.toLatin1()));
                }
                for (unsigned n : { 0u, 7u, 4000000000u })
                    same(what + QString::number(n), format.arg(n, width, 10, fill), f.arg(n, width, 10, fill.toLatin1()));
                for (long long n : { 0ll, -9000000000ll, 9000000000ll })
                    same(what + QString::number(n), format.arg(n, width, 10, fill), f.arg(n, width, 10, fill.toLatin1()));
                same(what + "char", format.arg(QChar('c'), width, fill), f.arg('c', width, fill.toLatin1()));
                for (double d : Doubles) {
                    for (char form : { 'f', 'e', 'g', 'E', 'G' }) {
                        for (int precision : { -1, 0, 1, 2, 3, 6, 10 }) {
                            const QString w = what + QString::number(d, 'g', 17) + " " + form + QString::number(precision);
                            same(w, format.arg(d, width, form, precision, fill), f.arg(d, width, form, precision, fill.toLatin1()));
                        }
                    }
                    same(what + QString::number(d, 'g', 17), format.arg(d), f.arg(d));
                }
            }
        }
        same(describe(format) + ".arg(a, b)", format.arg("first", "second"), f.arg(String("first"), String("second")));
        same(describe(format) + ".arg(a, b, c)", format.arg("1st", "2nd", "3rd"), f.arg(String("1st"), String("2nd"), String("3rd")));
    }
    for (double d : Doubles) {
        for (char form : { 'f', 'e', 'g' })
            for (int precision : { 0, 1, 2, 6 })
                same(QString("number(%1, %2, %3)").arg(d, 0, 'g', 17).arg(form).arg(precision),
                     QString::number(d, form, precision), String::number(d, form, precision));
        same(QString("number(%1)").arg(d, 0, 'g', 17), QString::number(d), String::number(d));
    }
    for (int n : { 0, 1, -1, 255, INT_MAX, INT_MIN })
        for (int base : { 2, 8, 10, 16, 36 })
            same(QString("number(%1, %2)").arg(n).arg(base), QString::number(n, base), String::number(n, base));

    // values of other types (the defaults passed to DesFile::value())
    for (int i : { 0, 1, -1, 7, 10000000 })
        sameValue(QVariant(i), txt::Value(i), QString("Value(%1)").arg(i));
    for (float f : { 0.0f, 1.0f, 0.5f, 0.1f, 2.7f, -2.5f, 1.5f, 1e20f, 3.25f })
        sameValue(QVariant(f), txt::Value(f), QString("Value(%1f)").arg(f));
    for (double d : { 0.0, 0.8, 0.5, 2.5, -1.5, 1e-5, 1e20, 123.456 })
        sameValue(QVariant(d), txt::Value(d), QString("Value(%1)").arg(d));
    for (bool b : { false, true })
        sameValue(QVariant(b), txt::Value(b), QString("Value(%1)").arg(b));
    sameValue(QVariant(), txt::Value(), "Value()");
    for (const QStringList &list : { QStringList(), QStringList{ "12" }, QStringList{ "a", "b" }, QStringList{ "" } }) {
        StringList l;
        for (const QString &s : list)
            l << fromQt(s);
        sameValue(QVariant(list), txt::Value(l), "Value(list " + list.join("|") + ")");
    }

    // UTF-8 literals
    same("String(\"Zerstört\")", QString("Zerstört"), String("Zerstört"));
    same("String(\"\\u20ac\")", QString("\u20ac"), String("\u20ac"));
    g_checks++;
    if (String::fromLatin1("\xe4" "b").toUtf8() != QString::fromLatin1("\xe4" "b").toUtf8().toStdString())
        fail("toUtf8()", "", "");

    // the game data: every line and every value of its text files
    // the times of saved games, read as QDateTime::fromString(…, Qt::ISODate) read them and
    // written as toString(Qt::ISODateWithMs) wrote them
    auto sameTime = [](const QString &text) {
        const QDateTime q = QDateTime::fromString(text, Qt::ISODate);
        const util::DateTime ours = util::DateTime::fromIso(fromQt(text));
        const bool equal = q.isValid() ? ours.isValid() && ours.year == q.date().year() && ours.month == q.date().month() && ours.day == q.date().day()
                && ours.hour == q.time().hour() && ours.minute == q.time().minute() && ours.second == q.time().second() && ours.millisecond == q.time().msec()
                : !ours.isValid();
        same("DateTime " + describe(text), q.isValid() ? q.toString(Qt::ISODateWithMs) : QString("invalid"), equal ? (q.isValid() ? ours.toIso() : String("invalid")) : String("different: ") + ours.toIso());
    };
    std::mt19937 times(2);
    for (int i = 0; i < 20000; i++) {
        const QDateTime t(QDate(1990 + int(times() % 60), 1 + int(times() % 12), 1 + int(times() % 28)), QTime(int(times() % 24), int(times() % 60), int(times() % 60), int(times() % 1000)));
        sameTime(t.toString(Qt::ISODateWithMs));
        sameTime(t.toString(Qt::ISODate));
    }
    // (not those with a time zone, which saved games never had)
    for (const char *text : { "2026-10-14T16:05:09", "2026-10-14T16:05", "2026-10-14 16:05:09.5", "2026-02-29T00:00:00.000",
                              "2024-02-29T00:00:00.000", "2026-13-01T00:00", "2026-00-10T00:00", "2026-10-00T00:00", "2026-10-14", "",
                              "garbage", "2026-10-14T16:60:00", "2026-10-14T16:05:60", "2026-10-14T16:05:09.1234", "2026-10-14T16:05:09.",
                              "2026-10-14T16:05:09.12", "2026-10-14T1:05:09", "26-10-14T16:05:09", "2026-10-14T16:05:09.000x",
                              "0000-10-14T16:05:09.000" })
        sameTime(QString::fromLatin1(text));

    if (argc > 1) {
        int files = 0, lines = 0;
        QDirIterator it(QString::fromLocal8Bit(argv[1]), QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString path = it.next();
            const QString suffix = QFileInfo(path).suffix().toLower();
            if (suffix != "des" && suffix != "txt" && suffix != "ini")
                continue;
            QFile file(path);
            if (!file.open(QFile::ReadOnly))
                continue;
            files++;
            while (!file.atEnd()) {
                const QString line = QString::fromLatin1(file.readLine());
                lines++;
                sameText(line);
                const int equals = line.indexOf('=');
                if (equals >= 0) {
                    QString value = line.mid(equals + 1);
                    const int comment = value.indexOf(';');
                    if (comment >= 0)
                        value.truncate(comment);
                    sameText(value.trimmed());
                    sameValue(value.trimmed());
                }
            }
        }
        std::cout << files << " data files, " << lines << " lines" << std::endl;
    }

    std::cout << g_checks << " checks, " << g_failures << " failures" << std::endl;
    return g_failures == 0 ? 0 : 1;
}
