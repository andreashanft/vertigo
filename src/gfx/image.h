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

#ifndef GFX_IMAGE_H
#define GFX_IMAGE_H

#include "colortable.h"
#include "util/file.h"
#include "util/string.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace gfx {

// An image in memory, in one of the formats the game uses, with the rows one after the other
// without gaps. Pixels outside the image read as 0 and aren't written; indices beyond the colour
// table are transparent black.
class Image
{
public:
    enum Format {
        Invalid,
        Indexed8,   // a byte per pixel, the index into the colour table
        RGB888,     // red, green and blue bytes
        ARGB32,     // an Rgb per pixel (so in the machine's byte order)
        RGBA8888,   // red, green, blue and alpha bytes, as OpenGL takes them
    };
    // the types of the game's image files
    enum Type { Bitmap = 0x0000, PaletteRLE = 0x0400, Palette = 0x0401, RGB565 = 0x0413 };

public:
    Image() = default;
    // filled with 0; without a size it is a null image
    Image(int width, int height, Format format);

public:
    bool isNull() const { return m_format == Invalid; }
    int width() const { return m_width; }
    int height() const { return m_height; }
    Format format() const { return m_format; }
    int bytesPerPixel() const;
    int bytesPerLine() const { return m_width*bytesPerPixel(); }
    std::uint8_t* bits() { return m_data.data(); }
    const std::uint8_t* constBits() const { return m_data.data(); }
    std::uint8_t* scanLine(int y) { return m_data.data() + std::size_t(y)*bytesPerLine(); }
    const std::uint8_t* constScanLine(int y) const { return m_data.data() + std::size_t(y)*bytesPerLine(); }
    const ColorTable& colorTable() const { return m_colorTable; }
    void setColorTable(const ColorTable &colorTable) { m_colorTable = colorTable; }
    bool valid(int x, int y) const { return x >= 0 && x < m_width && y >= 0 && y < m_height; }

    // the colour of a pixel, 0 outside the image or the colour table
    Rgb pixel(int x, int y) const;
    // the colour table index of a pixel, 0 in the other formats
    int pixelIndex(int x, int y) const;
    // the index into the colour table, or the colour
    void setPixel(int x, int y, std::uint32_t indexOrRgb);
    void fill(std::uint32_t indexOrRgb);

    // a part of the image; where it reaches past the image it is 0
    Image copy(int x, int y, int width, int height) const;
    // the pixel nearest to each one's centre, or with smooth the average of the pixels covered
    Image scaled(int width, int height, bool smooth = false) const;
    // to a format without an index (an empty colour table is grey levels)
    Image convertToFormat(Format format) const;
    // upside down
    Image flipped() const;
    Image& toRgb565();

    // as PNG
    bool save(const std::string &fileName) const;
    // as JPEG, quality 1 to 100
    bool saveJpeg(const std::string &fileName, int quality) const;
    // a PNG or JPEG image; a null image if it can't be decoded
    static Image fromData(const std::uint8_t *data, std::size_t size);

public:
    static Image load(const String &filename, const ColorTable &colorTable = ColorTable());
    static Image load(const String &filename, int w, int h, bool colorKey = false);
    static Image load(util::File& file, Type type, const ColorTable &colorTable);
    static Image loadPCX(const String &filename);
    // where index 0 is, transparent
    static Image loadCursor(const String &filename, const ColorTable &colorTable);

private:
    static Image decodeImage(util::File& file, int width, int height, Rgb color);
    static Image decodeImage(util::File& file, int width, int height, const ColorTable &colorTable);
    static Image decodeImage(util::File& file, int width, int height);

private:
    Format m_format = Invalid;
    int m_width = 0;
    int m_height = 0;
    std::vector<std::uint8_t> m_data;
    ColorTable m_colorTable;
};

} // namespace gfx

#endif // GFX_IMAGE_H
