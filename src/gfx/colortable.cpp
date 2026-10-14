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

namespace gfx {

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
