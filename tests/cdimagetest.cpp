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

// util::CdImage, on small images of each layout (plain sectors, raw ones of mode 1 and of mode
// 2), with names and directories a damaged image could have

#include "check.hpp"

#include "util/cdimage.hpp"
#include "util/path.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace {

const std::size_t Sector = 2048;

void put32(std::string &data, std::size_t offset, std::uint32_t value)
{
    for (int i = 0; i < 4; i++)
        data[offset + i] = char(value >> (8 * i));
}


// a directory record: the name as on the CD ("\0" for the directory itself, "\1" for its parent)
std::string record(const std::string &name, std::uint32_t sector, std::uint32_t size, int flags = 0)
{
    std::string record(33 + name.size() + (name.size() % 2 == 0), '\0');
    record[0] = char(record.size());
    put32(record, 2, sector);
    put32(record, 10, size);
    record[25] = char(flags);
    record[32] = char(name.size());
    record.replace(33, name.size(), name);
    return record;
}


// the sectors' data: a primary volume descriptor, the root directory in sector 18 and files
std::vector<std::string> sectors()
{
    std::vector<std::string> sectors(27, std::string(Sector, '\0'));
    const int Directory = 2, Associated = 4;
    sectors[16].replace(0, 6, "\1CD001");
    sectors[16][128] = char(Sector & 0xff);
    sectors[16][129] = char(Sector >> 8);
    sectors[16].replace(156, 34, record(std::string(1, '\0'), 18, Sector, Directory));
    sectors[17].replace(0, 6, "\xff" "CD001");
    sectors[18] = record(std::string(1, '\0'), 18, Sector, Directory) + record(std::string(1, '\1'), 18, Sector, Directory)
        + record("README.TXT;1", 22, 5) + record("SF", 19, Sector, Directory)
        // a loop back to the root, a resource fork, names that would leave the directory
        + record("LOOP", 18, Sector, Directory) + record("ASSOC.TXT;1", 22, 5, Associated)
        + record("A/B;1", 22, 5) + record("..;1", 22, 5) + record("C:D;1", 22, 5)
        // a file that goes beyond the end of the image
        + record("TRUNC.DAT;1", 26, 5000);
    sectors[19] = record(std::string(1, '\0'), 19, Sector, Directory) + record(std::string(1, '\1'), 18, Sector, Directory)
        + record("GFX", 20, 2 * Sector, Directory) + record("NOEXT.;1", 0, 0);
    // a directory of two sectors, the first one ending in zeros
    sectors[20] = record(std::string(1, '\0'), 20, 2 * Sector, Directory) + record(std::string(1, '\1'), 19, Sector, Directory)
        + record("BIG.DAT;1", 23, 3000);
    sectors[21] = record("SECOND.DAT;1", 25, 10);
    sectors[22] = "hello";
    for (std::size_t i = 0; i < 3000; i++)
        sectors[23 + i / Sector][i % Sector] = char(i * 7);
    sectors[25] = "0123456789";
    for (std::string &sector : sectors)
        sector.resize(Sector, '\0');
    return sectors;
}


// the image of the sectors with the layout: 2048 bytes a sector, or raw 2352 with the data at 16
// (mode 1) or 24 (mode 2 form 1)
std::string image(std::size_t sectorSize, std::size_t dataOffset)
{
    std::string image;
    for (const std::string &data : sectors()) {
        std::string sector(sectorSize, '\0');
        if (sectorSize != Sector)
            sector.replace(0, 12, std::string("\0\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\0", 12));
        sector.replace(dataOffset, Sector, data);
        image += sector;
    }
    return image;
}


std::string content(const std::string &path)
{
    const std::vector<char> data = util::readFile(path);
    return std::string(data.begin(), data.end());
}

} // namespace


int main()
{
    const std::string directory = util::fromPath(std::filesystem::temp_directory_path() / "vertigo-cdimagetest");
    std::filesystem::remove_all(util::toPath(directory));
    util::makeDirectory(directory);
    const std::string imagePath = util::joinPath(directory, "game.gog");
    const std::string copyPath = util::joinPath(directory, "copy");

    static const std::size_t layouts[][2] = { { 2048, 0 }, { 2352, 16 }, { 2352, 24 } };
    for (const auto &layout : layouts) {
        util::writeFile(imagePath, image(layout[0], layout[1]));
        util::CdImage image;
        CHECK(image.open(imagePath));
        const std::vector<util::CdImage::File> &files = image.files();
        CHECK_EQUAL(files.size(), 5u);
        if (files.size() != 5)
            continue;
        CHECK_EQUAL(files[0].path, "README.TXT");
        CHECK_EQUAL(files[1].path, "SF/GFX/BIG.DAT");
        CHECK_EQUAL(files[2].path, "SF/GFX/SECOND.DAT");
        CHECK_EQUAL(files[3].path, "SF/NOEXT");
        CHECK_EQUAL(files[4].path, "TRUNC.DAT");
        CHECK_EQUAL(files[1].size, 3000u);

        CHECK(image.copy(files[0], copyPath));
        CHECK_EQUAL(content(copyPath), "hello");
        CHECK(image.copy(files[1], copyPath));
        std::string big = sectors()[23] + sectors()[24];
        big.resize(3000);
        CHECK(content(copyPath) == big);
        CHECK(image.copy(files[2], copyPath));
        CHECK_EQUAL(content(copyPath), "0123456789");
        CHECK(image.copy(files[3], copyPath));
        CHECK_EQUAL(content(copyPath), "");
        CHECK(!image.copy(files[4], copyPath));
    }

    // not an image: too short, or without the volume descriptor
    util::CdImage image;
    util::writeFile(imagePath, std::string(16 * Sector, '\0'));
    CHECK(!image.open(imagePath));
    util::writeFile(imagePath, std::string(32 * 2352, '\0'));
    CHECK(!image.open(imagePath));
    CHECK(!image.open(util::joinPath(directory, "missing.gog")));
    CHECK_EQUAL(image.files().size(), 0u);

    std::filesystem::remove_all(util::toPath(directory));
    return test::result();
}
