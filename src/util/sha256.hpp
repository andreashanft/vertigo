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

#ifndef UTIL_SHA256_HPP
#define UTIL_SHA256_HPP

#include <cstddef>
#include <cstdint>
#include <string>

namespace util {

// SHA-256, to check a download against the hash it should have
class Sha256
{
public:
    Sha256();

public:
    void add(const void *data, std::size_t size);
    // the hash in lower-case hex; the object is used up
    std::string hex();
    // of a file's contents, empty if it can't be read
    static std::string file(const std::string &path);

private:
    void block(const std::uint8_t *data);

private:
    std::uint32_t m_state[8];
    std::uint8_t m_buffer[64];
    std::size_t m_buffered = 0;
    std::uint64_t m_length = 0;
};

} // namespace util

#endif // UTIL_SHA256_HPP
