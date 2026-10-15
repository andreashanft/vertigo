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

    // the audio tracks of cue sheets: after a data track in one image (as in GOG's game.ins;
    // its name in another case), with a pregap (index 0), and in files of their own
    const std::size_t Raw = 2352;
    util::writeFile(util::joinPath(directory, "CD.GOG"), std::string(10 * Raw, 'd') + std::string(3 * Raw, 'a') + std::string(Raw, 'p') + std::string(2 * Raw, 'b'));
    util::writeFile(util::joinPath(directory, "game.ins"), "FILE \"cd.gog\" BINARY\r\n  TRACK 01 MODE1/2352\r\n    INDEX 01 00:00:00\r\n"
                    "  TRACK 02 AUDIO\r\n    INDEX 01 00:00:10\r\n  TRACK 03 AUDIO\r\n    INDEX 00 00:00:13\r\n    INDEX 01 00:00:14\r\n");
    std::vector<util::CdImage::AudioTrack> tracks = util::CdImage::audioTracks(util::joinPath(directory, "game.ins"));
    CHECK_EQUAL(tracks.size(), 2u);
    if (tracks.size() == 2) {
        CHECK_EQUAL(tracks[0].number, 2);
        CHECK_EQUAL(util::fileName(tracks[0].file), "CD.GOG");
        CHECK_EQUAL(tracks[0].offset, 10 * Raw);
        CHECK_EQUAL(tracks[0].size, 3 * Raw);
        CHECK_EQUAL(tracks[1].number, 3);
        CHECK_EQUAL(tracks[1].offset, 14 * Raw);
        CHECK_EQUAL(tracks[1].size, 2 * Raw);

        // as a WAV file: the header, then the samples
        CHECK(util::CdImage::copyAudio(tracks[0], copyPath));
        const std::string wav = content(copyPath);
        CHECK_EQUAL(wav.size(), 44 + 3 * Raw);
        CHECK_EQUAL(wav.substr(0, 4), "RIFF");
        CHECK_EQUAL(wav.substr(8, 8), "WAVEfmt ");
        CHECK_EQUAL(wav.substr(36, 4), "data");
        CHECK(wav.substr(20, 16) == std::string("\1\0\2\0\x44\xac\0\0\x10\xb1\2\0\4\0\x10\0", 16));
        CHECK(wav.substr(44) == std::string(3 * Raw, 'a'));
        // the image ends before the track does
        CHECK(!util::CdImage::copyAudio({ 2, tracks[0].file, 15 * Raw, 2 * Raw }, copyPath));
    }

    util::writeFile(util::joinPath(directory, "data.bin"), std::string(4 * Raw, 'd'));
    util::writeFile(util::joinPath(directory, "track 2.bin"), std::string(5 * Raw + 3, 'a'));
    util::writeFile(util::joinPath(directory, "rip.cue"), "FILE \"data.bin\" BINARY\n  TRACK 01 MODE1/2352\n    INDEX 01 00:00:00\n"
                    "FILE \"track 2.bin\" BINARY\n  TRACK 02 AUDIO\n    INDEX 00 00:00:00\n    INDEX 01 00:00:02\n");
    tracks = util::CdImage::audioTracks(util::joinPath(directory, "rip.cue"));
    CHECK_EQUAL(tracks.size(), 1u);
    if (tracks.size() == 1) {
        CHECK_EQUAL(util::fileName(tracks[0].file), "track 2.bin");
        CHECK_EQUAL(tracks[0].offset, 2 * Raw);
        // whole samples
        CHECK_EQUAL(tracks[0].size, 3 * Raw);
    }
    CHECK_EQUAL(util::CdImage::audioTracks(util::joinPath(directory, "missing.cue")).size(), 0u);

    std::filesystem::remove_all(util::toPath(directory));
    return test::result();
}
