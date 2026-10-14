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

// util::Json replaces QJsonDocument for the glTF files of the 3D cockpits. While Qt is linked
// (until step Q5 of the iPad port) the two are compared here: on random documents, on broken
// ones (both must refuse them), and with a directory as the argument on the JSON of every .glb
// file in it.

#include "util/json.hpp"

#include <QDirIterator>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtEndian>

#include <cmath>
#include <cstring>
#include <iostream>
#include <map>
#include <random>


namespace {

int g_checks = 0;
int g_failures = 0;
std::map<std::string, int> g_kinds;
std::mt19937 g_random(1);


void fail(const std::string &kind, const std::string &what)
{
    g_failures++;
    if (++g_kinds[kind] <= 5)
        std::cerr << "FAIL " << kind << ": " << what << std::endl;
}


bool sameDouble(double a, double b)
{
    return std::memcmp(&a, &b, sizeof(a)) == 0 || (std::isnan(a) && std::isnan(b));
}


// the same value, read as the glTF loader reads it
void same(const QJsonValue &q, const util::Json &j, const std::string &path)
{
    g_checks++;
    if (q.toBool() != j.toBool() || q.toBool(true) != j.toBool(true))
        return fail("bool", path);
    if (!sameDouble(q.toDouble(), j.toDouble()) || !sameDouble(q.toDouble(-3.5), j.toDouble(-3.5)))
        return fail("double", path + ": Qt " + std::to_string(q.toDouble()) + ", ours " + std::to_string(j.toDouble()));
    if (q.toInt() != j.toInt() || q.toInt(-7) != j.toInt(-7))
        return fail("int", path + ": Qt " + std::to_string(q.toInt(-7)) + ", ours " + std::to_string(j.toInt(-7)));
    // (QVariant also converts booleans and texts, which offsets in a glTF file aren't)
    if (q.isDouble() && q.toVariant().toLongLong() != j.toInt64())
        return fail("int64", path + ": Qt " + std::to_string(q.toVariant().toLongLong()) + ", ours " + std::to_string(j.toInt64()));
    if (q.toString().toStdString() != j.toString())
        return fail("string", path + ": Qt \"" + q.toString().toStdString() + "\", ours \"" + j.toString() + "\"");
    if (q.isUndefined() != j.isUndefined())
        return fail("undefined", path);
    if (q.isArray()) {
        const QJsonArray a = q.toArray();
        if (j.type() != util::Json::Array || int(j.size()) != a.size())
            return fail("array", path);
        for (int i = 0; i < a.size(); i++)
            same(a[i], j[i], path + "[" + std::to_string(i) + "]");
        same(a.size() > 0 ? QJsonValue(QJsonValue::Undefined) : QJsonValue(QJsonValue::Undefined), j[int(a.size())], path + "[past the end]");
    } else if (q.isObject()) {
        const QJsonObject o = q.toObject();
        if (j.type() != util::Json::Object || int(j.size()) != o.size())
            return fail("object", path + ": Qt " + std::to_string(o.size()) + " members, ours " + std::to_string(j.size()));
        for (auto it = o.begin(); it != o.end(); ++it) {
            if (!j.contains(it.key().toStdString()))
                return fail("member", path + "." + it.key().toStdString());
            same(it.value(), j[it.key().toStdString()], path + "." + it.key().toStdString());
        }
        same(o["no such key"], j["no such key"], path + ".(missing)");
    } else if ((j.type() == util::Json::Array) != q.isArray() || (j.type() == util::Json::Object) != q.isObject()) {
        return fail("type", path);
    }
}


void compare(const QByteArray &text, const std::string &name)
{
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(text, &error);
    std::string ours;
    const util::Json json = util::Json::parse(text.toStdString(), &ours);
    g_checks++;
    if ((error.error == QJsonParseError::NoError) != ours.empty())
        return fail("accepted", name + ": Qt " + error.errorString().toStdString() + ", ours " + (ours.empty() ? "accepted" : ours) + " in " + text.left(200).toStdString());
    if (error.error != QJsonParseError::NoError)
        return;
    same(document.isArray() ? QJsonValue(document.array()) : QJsonValue(document.object()), json, name);
}


std::string randomString()
{
    static const char *pieces[] = { "a", "B", " ", "\\\"", "\\\\", "\\/", "\\n", "\\t", "\\r", "\\b", "\\f", "\\u0041", "\\u00e9",
                                    "\\u20ac", "\\ud83d\\ude00", "\xc3\xa4", "\xe2\x82\xac", "role", "hud", "_" };
    std::string s = "\"";
    const int length = std::uniform_int_distribution<int>(0, 6)(g_random);
    for (int i = 0; i < length; i++)
        s += pieces[std::uniform_int_distribution<int>(0, int(std::size(pieces)) - 1)(g_random)];
    return s + "\"";
}


std::string randomNumber()
{
    static const char *numbers[] = { "0", "-0", "1", "-1", "42", "2147483647", "2147483648", "-2147483648", "-2147483649",
                                     "9007199254740993", "9223372036854775807", "9223372036854775808", "-9223372036854775808",
                                     "18446744073709551616", "1.0", "1.5", "-2.5", "0.1", "1e2", "1E-2", "2.5e+3", "-0.0",
                                     "1e308", "1e309", "-1e309", "4.9e-324", "1e-400", "3.14159", "123456789012345678901234567890",
                                     "0.30000000000000004", "1e0", "100000000000000000000.0" };
    if (std::uniform_int_distribution<int>(0, 3)(g_random) == 0)
        return std::to_string(std::uniform_int_distribution<int>(-100000, 100000)(g_random));
    return numbers[std::uniform_int_distribution<int>(0, int(std::size(numbers)) - 1)(g_random)];
}


std::string randomValue(int depth)
{
    const int kind = std::uniform_int_distribution<int>(0, depth > 4 ? 4 : 6)(g_random);
    switch (kind) {
    case 0: return "true";
    case 1: return "false";
    case 2: return "null";
    case 3: return randomString();
    case 4: return randomNumber();
    case 5: {
        std::string s = "[";
        const int n = std::uniform_int_distribution<int>(0, 4)(g_random);
        for (int i = 0; i < n; i++)
            s += (i ? ", " : " ") + randomValue(depth + 1);
        return s + "]";
    }
    default: {
        std::string s = "{";
        const int n = std::uniform_int_distribution<int>(0, 4)(g_random);
        for (int i = 0; i < n; i++)
            s += (i ? ",\n\t" : "") + randomString() + " : " + randomValue(depth + 1);
        return s + "}";
    }
    }
}

} // namespace


int main(int argc, char *argv[])
{
    for (int i = 0; i < 20000; i++) {
        const std::string document = std::uniform_int_distribution<int>(0, 1)(g_random) ? "{\"x\": " + randomValue(0) + "}" : "[" + randomValue(0) + "]";
        compare(QByteArray::fromStdString(document), "random " + std::to_string(i));
    }
    // broken documents, which both refuse, and odd ones, which both read alike
    for (const char *text : { "", " ", "{", "[", "{]", "[}", "{\"a\"}", "{\"a\":}", "{\"a\" 1}", "[1,]", "[,1]", "{\"a\":1,}", "[01]",
                              "[1.]", "[.5]", "[1e]", "[+1]", "[-]", "[tru]", "[nul]", "[\"\\x\"]", "[\"\\u12\"]", "[\"a]", "[\"\x01\"]",
                              "{} x", "[] []", "1", "\"a\"", "true", "[1 2]", "{\"a\":1 \"b\":2}", "[1.]", "[.5]", "[\"\\q\"]", "[\"a\tb\"]", "[-.5e3]", "[1.e2]" })
        compare(QByteArray(text), std::string("broken ") + text);

    // the JSON chunk of each .glb file
    if (argc > 1) {
        QDirIterator it(QString::fromLocal8Bit(argv[1]), QStringList() << "*.glb", QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            QFile file(it.next());
            if (!file.open(QIODevice::ReadOnly))
                continue;
            const QByteArray glb = file.readAll();
            const quint32 length = qFromLittleEndian<quint32>(glb.constData() + 12);
            compare(glb.mid(20, int(length)), file.fileName().toStdString());
            std::cout << file.fileName().toStdString() << ": " << length << " bytes of JSON" << std::endl;
        }
    }

    for (const auto &kind : g_kinds)
        std::cout << kind.second << " failures of " << kind.first << std::endl;
    std::cout << g_checks << " checks, " << g_failures << " failures" << std::endl;
    return g_failures == 0 ? 0 : 1;
}
