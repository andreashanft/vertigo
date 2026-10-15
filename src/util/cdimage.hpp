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

#ifndef UTIL_CDIMAGE_HPP
#define UTIL_CDIMAGE_HPP

#include <cstdint>
#include <fstream>
#include <set>
#include <string>
#include <vector>

namespace util {

// The files of a CD image with an ISO 9660 file system: of 2048-byte sectors (.iso), or the raw
// 2352-byte sectors of a data track (MODE1/2352 like GOG's game.gog, or MODE2/2352). Names are as
// on the CD, without the version (";1"); names that can't be a file's (with '/', "..") are left out.
class CdImage
{
public:
    struct File
    {
        std::string path; // "SF/GFX/MVI/FILM/D02.MVI"
        std::uint32_t sector;
        std::uint32_t size;
    };

    // false if it isn't such an image
    bool open(const std::string &path);
    // all files, in the order of the directories
    const std::vector<File>& files() const { return m_files; }
    // false if the image ends before the file does or the copy can't be written
    bool copy(const File &file, const std::string &path);

    // an audio track: 2352-byte sectors of 16-bit stereo samples at 44.1 kHz in an image file
    struct AudioTrack
    {
        int number;           // as on the CD (2 for the game's first, after the data track)
        std::string file;     // the image file
        std::uint64_t offset; // in bytes
        std::uint64_t size;
    };
    // the audio tracks of a cue sheet (a .cue, or a GOG installation's game.ins), whose image
    // files are next to it (in any case); none if it can't be read
    static std::vector<AudioTrack> audioTracks(const std::string &cueSheet);
    // a track as a WAV file; false if the image ends before the track does or the copy can't be
    // written
    static bool copyAudio(const AudioTrack &track, const std::string &path);

private:
    bool readSectors(std::uint32_t sector, std::uint32_t count, char *data);
    void readDirectory(std::uint32_t sector, std::uint32_t size, const std::string &path, int depth, std::set<std::uint32_t> &visited);

private:
    std::ifstream m_image;
    std::uint64_t m_sectors = 0;
    std::size_t m_sectorSize = 0;
    // where the 2048 bytes of a sector's data start
    std::size_t m_dataOffset = 0;
    std::vector<File> m_files;
};

} // namespace util

#endif // UTIL_CDIMAGE_HPP
