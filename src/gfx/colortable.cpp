/***************************************************************************
 *  Copyright (C) 2010  Philipp Nordhus                                    *
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

#include "colortable.h"
#include "util/datapath.h"
#include "util/path.hpp"

namespace gfx {

namespace {

// the rows follow the header, the palette and 12 more bytes
const int ShadingOffset = 0x31a;

}

ColorTable::ColorTable(const String &filename)
{
    loadFromFile(filename);
}

bool ColorTable::loadFromFile(const String &filename)
{
    util::File file(String(filename).replace(":", "/").toUtf8().data());

    std::uint16_t type;
    file >> type;
    if (type == 0x0802) {
        file.skipBytes(12);
    } else {
        file.skipBytes(2);
    }
    load(file);
    return true;
}

void ColorTable::toRgb565()
{
    for (Rgb &color : *this)
        color = rgb(red(color) & 0xf8, green(color) & 0xfc, blue(color) & 0xf8);
}

std::vector<Rgb> ColorTable::loadShading(const String &filename)
{
    std::vector<Rgb> table;
    const std::vector<char> data = util::readFile(util::DataPath::resolve(filename));
    const int entries = ShadingRows*256;
    if (int(data.size()) < ShadingOffset + entries*2)
        return table;
    table.resize(entries);
    const std::uint8_t *colors = reinterpret_cast<const std::uint8_t *>(data.data()) + ShadingOffset;
    for (int i = 0; i < entries; i++) {
        const int c = colors[2*i] | (colors[2*i + 1] << 8);
        table[i] = rgb(((c >> 11) & 31)*255/31, ((c >> 5) & 63)*255/63, (c & 31)*255/31);
    }
    return table;
}


void ColorTable::load(util::File &file)
{
    clear();

    std::vector<char> data;
    data.resize(256 * 3);
    file.read(data.data(), data.size());
    for (std::size_t i = 0; i < data.size(); i += 3) {
        push_back(rgb(data[i + 0], data[i + 1], data[i + 2]));
    }
}

} // namespace gfx
