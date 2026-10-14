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


// util::Json, on documents of the shapes of the 3D cockpits' glTF, and broken ones

#include "check.hpp"

#include "util/json.hpp"

#include <string>


int main()
{
    std::string error;
    const util::Json json = util::Json::parse(
        "{ \"asset\": { \"version\": \"2.0\" },\n"
        "  \"bufferViews\": [ { \"buffer\": 0, \"byteOffset\": 4294967296, \"byteLength\": 12 } ],\n"
        "  \"nodes\": [ { \"translation\": [ 1.5, -2, 3e2 ], \"mesh\": 0, \"name\": \"HUD\" } ],\n"
        "  \"materials\": [ { \"alphaMode\": \"BLEND\", \"doubleSided\": true, \"extras\": null } ],\n"
        "  \"text\": \"a\\\"b\\\\c\\/d\\n\\t\\u0041\\u00e9\\u20ac\\ud83d\\ude00\\ud800x\\q\" }", &error);
    CHECK_EQUAL(error, "");
    CHECK_EQUAL(json.type(), util::Json::Object);
    CHECK_EQUAL(json.size(), 5u);
    CHECK_EQUAL(json["asset"]["version"].toString(), "2.0");
    CHECK(json.contains("nodes") && !json.contains("meshes"));

    const util::Json &view = json["bufferViews"][0];
    CHECK_EQUAL(view["buffer"].toInt(), 0);
    CHECK_EQUAL(view["byteOffset"].toInt64(), 4294967296LL);
    CHECK_EQUAL(view["byteOffset"].toInt(-1), -1);
    CHECK_EQUAL(view["byteLength"].toInt(), 12);

    const util::Json &node = json["nodes"][0];
    CHECK_EQUAL(node["translation"].size(), 3u);
    CHECK_EQUAL(node["translation"][0].toDouble(), 1.5);
    CHECK_EQUAL(node["translation"][1].toInt(), -2);
    CHECK_EQUAL(node["translation"][2].toInt(), 300);
    CHECK_EQUAL(node["translation"][0].toInt(7), 7);
    CHECK_EQUAL(node["name"].toString(), "HUD");
    CHECK_EQUAL(node["elements"].elements().size(), 0u);

    const util::Json &material = json["materials"][0];
    CHECK(material["doubleSided"].toBool());
    CHECK_EQUAL(material["extras"].type(), util::Json::Null);
    CHECK_EQUAL(material["alphaMode"].toString(), "BLEND");
    CHECK_EQUAL(material.members().size(), 3u);

    // what isn't there: undefined, or the default
    CHECK(json["missing"].isUndefined());
    CHECK(json["nodes"][1].isUndefined());
    CHECK(json["nodes"][-1].isUndefined());
    CHECK(json["asset"][0].isUndefined());
    CHECK(json["nodes"]["name"].isUndefined());
    CHECK_EQUAL(json["missing"].toInt(-1), -1);
    CHECK_EQUAL(json["missing"].toDouble(2.5), 2.5);
    CHECK(json["missing"].toBool(true));
    CHECK_EQUAL(json["missing"].toString(), "");
    CHECK_EQUAL(json["asset"].toInt64(), 0);

    // escapes, in UTF-8; a surrogate without its other half is U+FFFD, an unknown escape the character
    CHECK_EQUAL(json["text"].toString(), "a\"b\\c/d\n\tA\xc3\xa9\xe2\x82\xac\xf0\x9f\x98\x80\xef\xbf\xbdxq");

    CHECK_EQUAL(util::Json::parse("[]").type(), util::Json::Array);
    CHECK_EQUAL(util::Json::parse(" [ -0, 1e-2, 1E+2 ] ")[2].toInt(), 100);

    for (const char *broken : { "", " ", "{", "[", "{]", "[}", "{\"a\"}", "{\"a\":}", "{\"a\" 1}", "[1,]", "[,1]", "{\"a\":1,}",
                                "[1e]", "[-]", "[tru]", "[nul]", "[\"\\u12\"]", "[\"a]", "{} x", "[] []", "1", "\"a\"", "true",
                                "[1 2]", "{\"a\":1 \"b\":2}", "[1e999]" }) {
        error.clear();
        const util::Json value = util::Json::parse(broken, &error);
        test::equal(value.isUndefined() && !error.empty(), true, broken, __LINE__);
    }
    // deeper than the parser goes
    CHECK(util::Json::parse(std::string(5000, '[') + std::string(5000, ']')).isUndefined());
    return test::result();
}
