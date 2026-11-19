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



// txt::DesFile on files as the game data has them: sections, values with comments, blocks of text,
// and the zero bytes that a few lines carry after their value

#include "check.hpp"

#include "txt/desfile.h"
#include "util/path.hpp"

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

using namespace std::string_literals;


int main()
{
    const std::string directory = util::fromPath(std::filesystem::temp_directory_path() / "vertigo-desfiletest");
    std::filesystem::remove_all(util::toPath(directory));
    util::makeDirectory(directory);
    const std::string path = util::joinPath(directory, "dialogue.des");

    // a dialogue's message with a zero byte after two values, behind a comment and on a line of
    // its own (the literal's suffix keeps the zero bytes)
    util::writeFile(path,
        ";\0-> Lisa X = nobody\r\n"
        "[Message4]\r\n"
        "Id \t\t= 9999201\r\n"
        "Type\t\t= mission2station\0\r\n"
        "Mission\t\t= m00201\0\r\n"
        "Station\t\t= 5 ; the fifth\0\r\n"
        "\0\r\n"
        "[B]\r\n"
        "{\r\n"
        "--- AMBUSH ---  \r\n"
        "\0\r\n"
        "}\r\n"s);

    txt::DesFile file(path);
    CHECK_EQUAL(file.sections().size(), 3);
    file.setSection("Global");
    CHECK_EQUAL(file.keys().size(), 0);

    file.setSection("Message4");
    CHECK_EQUAL(file.value("Id").toInt(), 9999201);
    // the values end at the zero byte, so they compare equal to the names they stand for
    CHECK_EQUAL(file.value("Type").toString(), "mission2station");
    CHECK_EQUAL(file.value("Mission").toString(), "m00201");
    CHECK_EQUAL(file.value("Mission").toString().size(), 6);
    CHECK_EQUAL(file.value("Station").toString(), "5");
    CHECK_EQUAL(file.keys().size(), 4);

    file.setSection("B");
    const StringList text = file.value("{text}").toStringList();
    CHECK_EQUAL(text.size(), 2);
    if (text.size() == 2) {
        CHECK_EQUAL(text[0], "--- AMBUSH ---");
        CHECK_EQUAL(text[1], "");
    }

    // a saved game that an earlier version wrote with the zero byte loads without it, and the
    // game writes it without
    const std::string savePath = util::joinPath(directory, "save.des");
    util::writeFile(savePath, "[station5]\nmission1 = m00201\0\n\n"s);
    txt::DesFile save(savePath);
    save.setSection("station5");
    CHECK_EQUAL(save.value("mission1").toString(), "m00201");
    CHECK(save.save(savePath));
    const std::vector<char> written = util::readFile(savePath);
    CHECK(!written.empty());
    CHECK(std::find(written.begin(), written.end(), '\0') == written.end());

    // a missing file leaves the sections empty
    txt::DesFile missing;
    CHECK(!missing.load(util::joinPath(directory, "missing.des")));
    CHECK_EQUAL(missing.keys().size(), 0);

    std::filesystem::remove_all(util::toPath(directory));
    return test::result();
}
