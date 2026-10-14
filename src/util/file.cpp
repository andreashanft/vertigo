/***************************************************************************
 *  Copyright (C) 2015  Philipp Nordhus                                    *
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

#include "file.h"

#include "datapath.h"
#include "path.hpp"
#include "error.h"

#include <iostream>

namespace util {

File::File()
{

}

File::File(const std::string &name)
{
    open(name);
}

void File::open(const std::string &name)
{
    m_name = DataPath::resolve(name);

    m_stream.close();
    // (a narrow name would be in the ANSI code page on Windows, not UTF-8)
    m_stream.open(toPath(m_name), std::ios_base::in | std::ios_base::binary);
    if (!m_stream.good())
        std::cerr << "Failed to open " << m_name << std::endl;
    ASSERT(m_stream.good());
}

void File::skipBytes(std::size_t numBytes)
{
    m_stream.seekg(numBytes, std::ios_base::cur);
}

void File::seek(std::size_t offset)
{
    m_stream.seekg(offset, std::ios_base::beg);
}

} // namespace util
