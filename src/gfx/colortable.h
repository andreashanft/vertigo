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

#ifndef GFX_COLORTABLE_H
#define GFX_COLORTABLE_H

#include "rgb.h"
#include "util/file.h"
#include "util/string.hpp"

#include <initializer_list>
#include <vector>

namespace gfx {

class ColorTable : public std::vector<Rgb>
{
public:
    ColorTable() = default;
    ColorTable(std::initializer_list<Rgb> colors) : std::vector<Rgb>(colors) {}
    explicit ColorTable(const String &filename);

public:
    int size() const { return int(std::vector<Rgb>::size()); }
    ColorTable& operator<<(Rgb color) { push_back(color); return *this; }

    bool loadFromFile(const String &filename);
    void load(util::File &file);
    void toRgb565();

    // the shading table behind the palette of a .s16 file: ShadingRows rows of 256 RGB565
    // colours, one colour per palette entry, from the farthest (the water's colour) to the
    // nearest row. Empty if the file has no table
    static const int ShadingRows = 32;
    static std::vector<Rgb> loadShading(const String &filename);
};

} // namespace gfx

#endif // GFX_COLORTABLE_H
