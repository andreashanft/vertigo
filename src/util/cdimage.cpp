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

#include "cdimage.hpp"
#include "path.hpp"

#include <algorithm>
#include <cstring>

namespace util {

namespace {

// the data of a sector
const std::size_t SectorData = 2048;
// directories as large as this are damage (the game's have a few sectors)
const std::uint32_t MaxDirectory = 1 << 22;
const int MaxDepth = 16;
// sectors read at once
const std::uint32_t Chunk = 256;

std::uint16_t le16(const unsigned char *bytes)
{
    return std::uint16_t(bytes[0] | bytes[1] << 8);
}

std::uint32_t le32(const unsigned char *bytes)
{
    return std::uint32_t(bytes[0]) | std::uint32_t(bytes[1]) << 8 | std::uint32_t(bytes[2]) << 16 | std::uint32_t(bytes[3]) << 24;
}

// the name of a directory record's file, empty if it can't be one's ("\0" and "\1" stand for
// the directory and its parent)
std::string recordName(const unsigned char *name, std::size_t length)
{
    std::string result(reinterpret_cast<const char *>(name), length);
    result = result.substr(0, result.find(';'));
    // "README." for a file without an extension
    if (!result.empty() && result.back() == '.')
        result.pop_back();
    if (result.empty() || result == "." || result == "..")
        return std::string();
    for (const char c : result)
        if (c == '/' || c == '\\' || c == ':' || static_cast<unsigned char>(c) < 0x20)
            return std::string();
    return result;
}

} // namespace


bool CdImage::open(const std::string &path)
{
    m_files.clear();
    m_image.close();
    m_image.clear();
    m_image.open(toPath(path), std::ios::binary);
    m_image.seekg(0, std::ios::end);
    const std::streamoff size = m_image.tellg();
    if (!m_image || size <= 0)
        return false;

    // plain sectors; raw ones of mode 1 (12 bytes of sync, 4 of header before the data) and of
    // mode 2 form 1 (8 more of subheader)
    static const std::size_t layouts[][2] = { { 2048, 0 }, { 2352, 16 }, { 2352, 24 } };
    std::vector<char> sector(SectorData);
    const unsigned char *data = reinterpret_cast<const unsigned char *>(sector.data());
    for (const auto &layout : layouts) {
        m_sectorSize = layout[0];
        m_dataOffset = layout[1];
        m_sectors = std::uint64_t(size) / m_sectorSize;
        // the volume descriptors from sector 16 on, up to the terminator (255); the primary one
        // (1) has the root directory
        for (std::uint32_t n = 16; n < 32 && readSectors(n, 1, sector.data()); n++) {
            if (std::memcmp(data + 1, "CD001", 5) != 0 || data[0] == 255)
                break;
            if (data[0] != 1 || le16(data + 128) != SectorData)
                continue;
            std::set<std::uint32_t> visited;
            readDirectory(le32(data + 156 + 2), le32(data + 156 + 10), std::string(), 0, visited);
            return true;
        }
    }
    m_image.close();
    return false;
}


bool CdImage::copy(const File &file, const std::string &path)
{
    std::ofstream out(toPath(path), std::ios::binary | std::ios::trunc);
    std::vector<char> buffer(Chunk * SectorData);
    std::uint32_t sector = file.sector;
    std::uint32_t left = file.size;
    while (left > 0 && out) {
        const std::uint32_t count = std::min<std::uint32_t>(left / SectorData + (left % SectorData != 0), Chunk);
        if (!readSectors(sector, count, buffer.data()))
            return false;
        const std::uint32_t bytes = std::min<std::uint32_t>(left, count * SectorData);
        out.write(buffer.data(), bytes);
        sector += count;
        left -= bytes;
    }
    out.close();
    return !out.fail();
}


bool CdImage::readSectors(std::uint32_t sector, std::uint32_t count, char *data)
{
    if (std::uint64_t(sector) + count > m_sectors)
        return false;
    m_image.clear();
    m_image.seekg(std::streamoff(std::uint64_t(sector) * m_sectorSize));
    if (m_sectorSize == SectorData)
        return bool(m_image.read(data, std::streamsize(std::uint64_t(count) * SectorData)));

    std::vector<char> raw(std::min(count, Chunk) * m_sectorSize);
    while (count > 0) {
        const std::uint32_t n = std::min(count, Chunk);
        if (!m_image.read(raw.data(), std::streamsize(n * m_sectorSize)))
            return false;
        for (std::uint32_t i = 0; i < n; i++, data += SectorData)
            std::memcpy(data, raw.data() + i * m_sectorSize + m_dataOffset, SectorData);
        count -= n;
    }
    return true;
}


void CdImage::readDirectory(std::uint32_t sector, std::uint32_t size, const std::string &path, int depth, std::set<std::uint32_t> &visited)
{
    if (depth > MaxDepth || size > MaxDirectory || !visited.insert(sector).second)
        return;
    std::vector<char> directory(std::size_t(size / SectorData + (size % SectorData != 0)) * SectorData);
    if (!readSectors(sector, std::uint32_t(directory.size() / SectorData), directory.data()))
        return;

    const unsigned char *data = reinterpret_cast<const unsigned char *>(directory.data());
    std::size_t position = 0;
    while (position < size) {
        const std::size_t length = data[position];
        // records don't cross sectors: the rest of one is zeros
        if (length == 0) {
            position = (position / SectorData + 1) * SectorData;
            continue;
        }
        const unsigned char *record = data + position;
        position += length;
        if (length < 34 || position > directory.size())
            break;
        const std::size_t nameLength = record[32];
        const unsigned flags = record[25];
        // (associated files are Mac resource forks)
        const std::string name = 33 + nameLength <= length && !(flags & 0x04) ? recordName(record + 33, nameLength) : std::string();
        if (name.empty())
            continue;
        if (flags & 0x02)
            readDirectory(le32(record + 2), le32(record + 10), path + name + "/", depth + 1, visited);
        else
            m_files.push_back({ path + name, le32(record + 2), le32(record + 10) });
    }
}

} // namespace util
