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

#ifndef UTIL_BYTEREADER_HPP
#define UTIL_BYTEREADER_HPP

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>
#include <vector>

namespace util {

// Reads little-endian numbers from bytes: a number past the end reads as 0 (and so does every one
// after it)
class ByteReader
{
public:
    explicit ByteReader(const std::vector<char> &data) : m_data(data.data()), m_size(data.size()) {}
    ByteReader(const char *data, std::size_t size) : m_data(data), m_size(size) {}

public:
    bool atEnd() const { return m_position >= m_size; }
    std::size_t position() const { return m_position; }
    void seek(std::size_t position) { m_position = position; }
    void skip(std::size_t count) { m_position += count; }

    template <typename T>
    typename std::enable_if<std::is_integral<T>::value && !std::is_same<T, bool>::value, ByteReader&>::type
    operator >> (T &value)
    {
        typename std::make_unsigned<T>::type bits = 0;
        if (advance(sizeof(T))) {
            for (std::size_t i = 0; i < sizeof(T); i++)
                bits |= typename std::make_unsigned<T>::type(std::uint8_t(m_data[m_position - sizeof(T) + i])) << (8*i);
        }
        value = T(bits);
        return *this;
    }

    // up to count bytes, fewer at the end
    std::string read(std::size_t count)
    {
        const std::size_t start = std::min(m_position, m_size);
        const std::size_t length = std::min(count, m_size - start);
        m_position = start + length;
        return std::string(m_data + start, length);
    }

    // 4 bytes
    ByteReader& operator >> (float &value)
    {
        std::uint32_t bits;
        *this >> bits;
        std::memcpy(&value, &bits, sizeof(value));
        return *this;
    }

private:
    // whether there are as many bytes left; if not, the position goes to the end
    bool advance(std::size_t count)
    {
        if (m_position > m_size || m_size - m_position < count) {
            m_position = m_size;
            return false;
        }
        m_position += count;
        return true;
    }

private:
    const char *m_data;
    std::size_t m_size;
    std::size_t m_position = 0;
};

} // namespace util

#endif // UTIL_BYTEREADER_HPP
