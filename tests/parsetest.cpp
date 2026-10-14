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

// The parsers of txt/parse.hpp replace regular expressions. While Qt is linked (until step Q5 of
// the iPad port) they are compared with QRegularExpression here: on random texts of the
// characters that matter, and with the game data's directory as the argument also on every line
// of its text files.

#include "txt/parse.hpp"

#include <QDirIterator>
#include <QFile>
#include <QRegularExpression>

#include <iostream>
#include <map>
#include <random>


namespace {

int g_checks = 0;
int g_failures = 0;
std::map<std::string, int> g_kinds;


std::string describe(const String &s)
{
    std::string d = "\"";
    for (const char c : s.toLatin1()) {
        const unsigned char u = static_cast<unsigned char>(c);
        if (u >= 32 && u < 127) {
            d += c;
        } else {
            char hex[8];
            std::snprintf(hex, sizeof(hex), "\\x%02x", u);
            d += hex;
        }
    }
    return d + "\"";
}


// a match and its groups, as Qt finds them and as ours
void same(const char *kind, const String &text, const QRegularExpressionMatch &expected, bool matched, const std::vector<String> &groups)
{
    g_checks++;
    bool ok = expected.hasMatch() == matched;
    for (std::size_t i = 0; ok && matched && i < groups.size(); i++)
        ok = String::fromLatin1(expected.captured(int(i) + 1).toLatin1()) == groups[i];
    if (ok)
        return;
    g_failures++;
    if (++g_kinds[kind] > 5)
        return;
    std::cerr << "FAIL " << kind << " on " << describe(text) << ": Qt " << (expected.hasMatch() ? "matches" : "doesn't match");
    for (int i = 1; expected.hasMatch() && i <= expected.lastCapturedIndex(); i++)
        std::cerr << " " << describe(String::fromLatin1(expected.captured(i).toLatin1()));
    std::cerr << ", ours " << (matched ? "matches" : "doesn't match");
    for (std::size_t i = 0; matched && i < groups.size(); i++)
        std::cerr << " " << describe(groups[i]);
    std::cerr << std::endl;
}


void check(const String &text)
{
    const QString q = QString::fromLatin1(text.constData(), text.size());
    String a, b, c;

    static const QRegularExpression section("^\\s*\\[(.*)\\]");
    bool matched = txt::parseSection(text, a);
    same("section", text, section.match(q), matched, { a });

    static const QRegularExpression keyValue("([^=]*)=([^;]*)");
    matched = txt::parseKeyValue(text, a, b);
    same("keyValue", text, keyValue.match(q), matched, { a, b });

    static const QRegularExpression numberedText("#(\\d*).*\\{(.*)\\}");
    matched = txt::parseNumberedText(text, a, b);
    same("numberedText", text, numberedText.match(q), matched, { a, b });

    for (const char *prefix : { "station", "room", "" }) {
        const QRegularExpression numbered(QString("^%1(\\d*)$").arg(prefix));
        matched = txt::parseNumbered(text, prefix, a);
        same("numbered", text, numbered.match(q), matched, { a });
    }

    static const QRegularExpression tagged("(\\w): (.*)");
    matched = txt::parseTagged(text, a, b);
    same("tagged", text, tagged.match(q), matched, { a, b });

    static const QRegularExpression movie("d(\\d*)\\.mvi");
    matched = txt::parseMovieNumber(text, a);
    same("movie", text, movie.match(q), matched, { a });

    static const QRegularExpression position("(\\d*)\\/(\\d*)\\/(\\d*)\\/(\\d*)");
    String numbers[4];
    matched = txt::parsePosition(text, numbers);
    same("position", text, position.match(q), matched, { numbers[0], numbers[1], numbers[2], numbers[3] });

    g_checks++;
    static const QRegularExpression digits("\\d+");
    const String replaced = txt::replaceNumbers(text, "#");
    const String expected = String::fromLatin1(QString(q).replace(digits, "#").toLatin1());
    if (replaced != expected) {
        g_failures++;
        if (++g_kinds["replaceNumbers"] <= 5)
            std::cerr << "FAIL replaceNumbers on " << describe(text) << ": Qt " << describe(expected) << ", ours " << describe(replaced) << std::endl;
    }
}


void silence(QtMsgType, const QMessageLogContext &, const QString &)
{
}

} // namespace


int main(int argc, char *argv[])
{
    qInstallMessageHandler(silence);

    // every character alone, and random texts of those that matter (and some that only look so)
    for (int c = 0; c < 256; c++)
        check(String(1, char(c)));
    const std::string alphabet = std::string("#{}[]=;:/ \t\n\r\v\f.dmvistaonroTAM_xyz0123456789") + "\xa0\x85\xb2\xe9\xc4";
    std::mt19937 random(1);
    for (int i = 0; i < 400000; i++) {
        String text;
        const int length = std::uniform_int_distribution<int>(0, 24)(random);
        for (int j = 0; j < length; j++)
            text += alphabet[std::uniform_int_distribution<std::size_t>(0, alphabet.size() - 1)(random)];
        check(text);
    }
    // the shapes they are meant for
    for (const char *text : { "[Global]\n", "  [ station1 ] ; x\n", "key = value ; comment\n", "#12 x {Hallo} y}\n", "station12",
                              "station12\n", "room3", "T: Text", "xd36.mvi", "1/2/3/4", "a1/2/3/4/5", "condition: 12 of 345" })
        check(text);

    // every line of the game's text files, as DesFile and Dialog read them
    if (argc > 1) {
        QDirIterator it(QString::fromLocal8Bit(argv[1]), QDir::Files, QDirIterator::Subdirectories);
        int files = 0;
        while (it.hasNext()) {
            QFile file(it.next());
            const QString suffix = QFileInfo(file.fileName()).suffix().toLower();
            if (suffix != "des" && suffix != "txt" && suffix != "ini" && suffix != "sen" && suffix != "lan")
                continue;
            if (!file.open(QIODevice::ReadOnly))
                continue;
            files++;
            while (!file.atEnd()) {
                const QByteArray line = file.readLine();
                check(String::fromLatin1(line.constData(), int(line.size())));
            }
        }
        std::cout << files << " files" << std::endl;
    }

    for (const auto &kind : g_kinds)
        std::cout << kind.second << " failures of " << kind.first << std::endl;
    std::cout << g_checks << " checks, " << g_failures << " failures" << std::endl;
    return g_failures == 0 ? 0 : 1;
}
