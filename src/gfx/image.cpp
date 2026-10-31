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

#include "image.h"

#include <algorithm>
#include <cstring>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_NO_STDIO
#if defined(__ARM_NEON) && !defined(STBI_NEON)
#define STBI_NEON
#endif
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_WRITE_STATIC
// file names in UTF-8 on Windows too
#define STBIW_WINDOWS_UTF8
#include "stb_image_write.h"

namespace gfx {


Image::Image(int width, int height, Format format)
{
    if (width <= 0 || height <= 0 || format == Invalid)
        return;
    m_format = format;
    m_width = width;
    m_height = height;
    m_data.assign(std::size_t(width)*height*bytesPerPixel(), 0);
}


int Image::bytesPerPixel() const
{
    switch (m_format) {
    case Indexed8: return 1;
    case RGB888: return 3;
    case ARGB32:
    case RGBA8888: return 4;
    case Invalid: break;
    }
    return 0;
}


Rgb Image::pixel(int x, int y) const
{
    if (!valid(x, y))
        return 0;
    const std::uint8_t *p = constScanLine(y) + x*bytesPerPixel();
    switch (m_format) {
    case Indexed8:
        return *p < m_colorTable.size() ? m_colorTable[*p] : 0;
    case RGB888:
        return rgb(p[0], p[1], p[2]);
    case ARGB32: {
        Rgb c;
        std::memcpy(&c, p, sizeof(c));
        return c;
    }
    case RGBA8888:
        return rgba(p[0], p[1], p[2], p[3]);
    case Invalid:
        break;
    }
    return 0;
}


int Image::pixelIndex(int x, int y) const
{
    return valid(x, y) && m_format == Indexed8 ? constScanLine(y)[x] : 0;
}


void Image::setPixel(int x, int y, std::uint32_t indexOrRgb)
{
    if (!valid(x, y))
        return;
    std::uint8_t *p = scanLine(y) + x*bytesPerPixel();
    switch (m_format) {
    case Indexed8:
        *p = std::uint8_t(indexOrRgb);
        break;
    case RGB888:
        p[0] = red(indexOrRgb);
        p[1] = green(indexOrRgb);
        p[2] = blue(indexOrRgb);
        break;
    case ARGB32:
        std::memcpy(p, &indexOrRgb, sizeof(indexOrRgb));
        break;
    case RGBA8888:
        p[0] = red(indexOrRgb);
        p[1] = green(indexOrRgb);
        p[2] = blue(indexOrRgb);
        p[3] = alpha(indexOrRgb);
        break;
    case Invalid:
        break;
    }
}


void Image::fill(std::uint32_t indexOrRgb)
{
    if (m_format == Indexed8) {
        std::fill(m_data.begin(), m_data.end(), std::uint8_t(indexOrRgb));
        return;
    }
    for (int x = 0; x < m_width; x++)
        setPixel(x, 0, indexOrRgb);
    for (int y = 1; y < m_height; y++)
        std::memcpy(scanLine(y), constScanLine(0), bytesPerLine());
}


Image Image::copy(int x, int y, int width, int height) const
{
    if (isNull())
        return Image();
    Image image(width, height, m_format);
    if (image.isNull())
        return image;
    image.m_colorTable = m_colorTable;
    const int x0 = std::max(x, 0), x1 = std::min(x + width, m_width);
    const int y0 = std::max(y, 0), y1 = std::min(y + height, m_height);
    const int bpp = bytesPerPixel();
    for (int line = y0; line < y1 && x0 < x1; line++)
        std::memcpy(image.scanLine(line - y) + (x0 - x)*bpp, constScanLine(line) + x0*bpp, std::size_t(x1 - x0)*bpp);
    return image;
}


Image Image::scaled(int width, int height, bool smooth) const
{
    if (isNull() || width <= 0 || height <= 0)
        return Image();
    if (width == m_width && height == m_height)
        return *this;

    if (!smooth || m_format == Indexed8) {
        Image image(width, height, m_format);
        image.m_colorTable = m_colorTable;
        const int bpp = bytesPerPixel();
        for (int y = 0; y < height; y++) {
            const int sy = int((y + 0.5)*m_height/height);
            for (int x = 0; x < width; x++) {
                const int sx = int((x + 0.5)*m_width/width);
                std::memcpy(image.scanLine(y) + x*bpp, constScanLine(sy) + sx*bpp, bpp);
            }
        }
        return image;
    }

    // each target pixel is the average of the source pixels it covers, weighted by how much of
    // them; the colours weighted by their alpha
    const Image source = m_format == ARGB32 ? *this : convertToFormat(ARGB32);
    Image image(width, height, ARGB32);
    auto spans = [](int from, int to) {
        // for each target pixel the first source pixel and the weights of those it covers
        std::vector<std::pair<int, std::vector<double>>> result(to);
        const double step = double(from)/to;
        for (int i = 0; i < to; i++) {
            const double begin = i*step, end = (i + 1)*step;
            const int first = int(begin);
            std::vector<double> weights;
            for (int s = first; s < end && s < from; s++)
                weights.push_back((std::min(end, s + 1.0) - std::max(begin, double(s)))/step);
            result[i] = std::make_pair(first, weights);
        }
        return result;
    };
    const auto columns = spans(m_width, width);
    const auto rows = spans(m_height, height);
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            double r = 0, g = 0, b = 0, a = 0;
            for (std::size_t j = 0; j < rows[y].second.size(); j++)
                for (std::size_t i = 0; i < columns[x].second.size(); i++) {
                    const Rgb c = source.pixel(columns[x].first + int(i), rows[y].first + int(j));
                    const double w = rows[y].second[j]*columns[x].second[i]*alpha(c);
                    r += red(c)*w;
                    g += green(c)*w;
                    b += blue(c)*w;
                    a += w;
                }
            image.setPixel(x, y, a > 0 ? rgba(int(r/a + 0.5), int(g/a + 0.5), int(b/a + 0.5), int(a + 0.5)) : 0);
        }
    }
    return image.convertToFormat(m_format);
}


namespace {

// a row of colours from or to the formats without an index
void readRow(const std::uint8_t *in, Image::Format format, int width, Rgb *out)
{
    switch (format) {
    case Image::RGB888:
        for (int x = 0; x < width; x++, in += 3)
            out[x] = rgb(in[0], in[1], in[2]);
        break;
    case Image::ARGB32:
        std::memcpy(out, in, std::size_t(width)*4);
        break;
    case Image::RGBA8888:
        for (int x = 0; x < width; x++, in += 4)
            out[x] = rgba(in[0], in[1], in[2], in[3]);
        break;
    case Image::Indexed8:
    case Image::Invalid:
        break;
    }
}


void writeRow(const Rgb *in, Image::Format format, int width, std::uint8_t *out)
{
    switch (format) {
    case Image::RGB888:
        for (int x = 0; x < width; x++, out += 3) {
            out[0] = red(in[x]);
            out[1] = green(in[x]);
            out[2] = blue(in[x]);
        }
        break;
    case Image::ARGB32:
        std::memcpy(out, in, std::size_t(width)*4);
        break;
    case Image::RGBA8888:
        for (int x = 0; x < width; x++, out += 4) {
            out[0] = red(in[x]);
            out[1] = green(in[x]);
            out[2] = blue(in[x]);
            out[3] = alpha(in[x]);
        }
        break;
    case Image::Indexed8:
    case Image::Invalid:
        break;
    }
}

} // namespace


Image Image::convertToFormat(Format format) const
{
    if (isNull() || format == Invalid || format == Indexed8)
        return format == m_format ? *this : Image();
    if (format == m_format)
        return *this;

    Image image(m_width, m_height, format);
    std::vector<Rgb> row(m_width);
    if (m_format == Indexed8) {
        Rgb colors[256];
        for (int i = 0; i < 256; i++) {
            if (m_colorTable.empty())
                colors[i] = rgb(i, i, i);
            else
                colors[i] = i < m_colorTable.size() ? m_colorTable[i] : 0;
        }
        for (int y = 0; y < m_height; y++) {
            const std::uint8_t *index = constScanLine(y);
            for (int x = 0; x < m_width; x++)
                row[x] = colors[index[x]];
            writeRow(row.data(), format, m_width, image.scanLine(y));
        }
        return image;
    }

    // without alpha the colour stays as it is
    for (int y = 0; y < m_height; y++) {
        readRow(constScanLine(y), m_format, m_width, row.data());
        writeRow(row.data(), format, m_width, image.scanLine(y));
    }
    return image;
}


Image Image::flipped() const
{
    Image image = *this;
    for (int y = 0; y < m_height; y++)
        std::memcpy(image.scanLine(y), constScanLine(m_height - 1 - y), bytesPerLine());
    return image;
}


Image& Image::toRgb565()
{
    m_colorTable.toRgb565();
    return *this;
}


bool Image::save(const std::string &fileName) const
{
    switch (m_format) {
    case RGB888:
        return stbi_write_png(fileName.c_str(), m_width, m_height, 3, constBits(), bytesPerLine()) != 0;
    case RGBA8888:
        return stbi_write_png(fileName.c_str(), m_width, m_height, 4, constBits(), bytesPerLine()) != 0;
    case Indexed8:
    case ARGB32:
        return convertToFormat(RGBA8888).save(fileName);
    case Invalid:
        break;
    }
    return false;
}


bool Image::saveJpeg(const std::string &fileName, int quality) const
{
    if (m_format != RGB888)
        return !isNull() && convertToFormat(RGB888).saveJpeg(fileName, quality);
    return stbi_write_jpg(fileName.c_str(), m_width, m_height, 3, constBits(), quality) != 0;
}


bool Image::saveAlpha(const std::string &fileName) const
{
    const Image rgba = convertToFormat(RGBA8888);
    if (rgba.isNull())
        return false;
    std::vector<std::uint8_t> alpha(std::size_t(m_width)*m_height);
    for (int y = 0; y < m_height; y++)
        for (int x = 0; x < m_width; x++)
            alpha[std::size_t(y)*m_width + x] = rgba.constScanLine(y)[4*x + 3];
    return stbi_write_png(fileName.c_str(), m_width, m_height, 1, alpha.data(), m_width) != 0;
}


Image Image::fromData(const std::uint8_t *data, std::size_t size)
{
    int width, height, channels;
    if (size > std::size_t(INT32_MAX) || !stbi_info_from_memory(data, int(size), &width, &height, &channels))
        return Image();
    // grey levels as colours, and with alpha if there is
    const bool hasAlpha = channels == 2 || channels == 4;
    stbi_uc *pixels = stbi_load_from_memory(data, int(size), &width, &height, &channels, hasAlpha ? 4 : 3);
    if (pixels == nullptr)
        return Image();
    Image image(width, height, hasAlpha ? RGBA8888 : RGB888);
    if (!image.isNull())
        std::memcpy(image.bits(), pixels, image.m_data.size());
    stbi_image_free(pixels);
    return image;
}


Image Image::load(const String &filename, const ColorTable &colorTable)
{
    util::File file(String(filename).replace(":", "/").toUtf8().data());
    std::uint16_t type;
    file >> type;
    return load(file, Type(type), colorTable);
}

Image Image::load(const String &filename, int w, int h, bool colorKey)
{
    util::File file(String(filename).replace(":", "/").toUtf8().data());

    Image image(w, h, colorKey ? ARGB32 : RGB888);

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            std::uint16_t value;
            file >> value;

            const std::uint8_t r = (value >> 8) & 0xf8;
            const std::uint8_t g = (value >> 3) & 0xfc;
            const std::uint8_t b = (value << 3) & 0xf8;
            if (colorKey && r == 0 && g == 0 && b == 0)
                image.setPixel(x, y, rgba(0, 0, 0, 0));
            else
                image.setPixel(x, y, rgb(r, g, b));
        }
    }

    return image;
}

Image Image::loadPCX(const String &filename)
{
    util::File file(String(filename).replace(":", "/").toUtf8().data());

    struct
    {
//        std::uint8_t manufacturer;
//        std::uint8_t version;
        std::uint8_t encoding;
        std::uint8_t bitsPerPixel;
        std::int16_t xMin, yMin, xMax, yMax;
//        std::int16_t hDpi, vDpi;
//        std::uint8_t colormap[48];
//        std::uint8_t reserved;
        std::uint8_t nPlanes;
        std::int16_t bytesPerLine;
//        std::int16_t paletteInfo;
//        std::int16_t hScreenSize;
//        std::int16_t vScreenSize;
//        std::uint8_t filler[54];
    } header;

    file.skipBytes(2);
    file >> header.encoding >> header.bitsPerPixel;
    file >> header.xMin >> header.yMin >> header.xMax >> header.yMax;
    file.skipBytes(53);
    file >> header.nPlanes >> header.bytesPerLine;
    file.skipBytes(60);

    const int width = (header.xMax - header.xMin) + 1;
    const int height = (header.yMax - header.yMin) + 1;

    ASSERT(header.bitsPerPixel == 8);
    ASSERT(header.nPlanes == 1);
    ASSERT(header.encoding == 1);

    Image image(width, height, Indexed8);

    // a line can be longer than the image, and a run can reach past the line
    std::vector<std::uint8_t> scanLine(std::max<int>(header.bytesPerLine, 0) + 64);
    for (int y = 0; y < height; y++) {
        int x = 0;
        while (x < header.bytesPerLine) {
            std::uint8_t c;
            file >> c;
            if ((c & 0xc0) == 0xc0) {
                const int count = c & 0x3f;
                file >> c;
                for (int i = 0; i < count; i++)
                    scanLine[x++] = c;
            } else {
                scanLine[x++] = c;
            }
        }
        std::memcpy(image.scanLine(y), scanLine.data(), std::min<int>(width, header.bytesPerLine));
    }

    std::uint8_t c;
    do {
        file >> c;
    } while (c != 12);

    gfx::ColorTable colorTable;
    colorTable.load(file);
    colorTable[0] = rgba(0, 0, 0, 0);

    image.setColorTable(colorTable);

    return image;
}

Image Image::loadCursor(const String &filename, const ColorTable &colorTable)
{
    const Image image = load(filename, colorTable);

    Image cursor(image.width(), image.height(), ARGB32);
    for (int y = 0; y < image.height(); y++) {
        for (int x = 0; x < image.width(); x++) {
            if (image.pixelIndex(x, y) != 0)
                cursor.setPixel(x, y, image.pixel(x, y));
        }
    }
    return cursor;
}

Image Image::load(util::File &file, Type type, const ColorTable &colorTable)
{
    std::uint32_t width;
    std::uint32_t height;
    std::int32_t x;
    std::int32_t y;

    file >> width >> height >> x >> y;

    Image image;
    switch (type) {
    case Bitmap:
        image = decodeImage(file, width, height, colorTable[0]);
        break;

    case PaletteRLE:
        image = decodeImage(file, width, height, colorTable);
        break;

    case Palette: {
        const std::vector<char> data = file.read(std::size_t(width) * height);
        image = Image(width, height, Indexed8);
        if (!image.isNull())
            std::memcpy(image.bits(), data.data(), data.size());
        image.setColorTable(colorTable);
        break;
    }

    case RGB565:
        image = decodeImage(file, width, height);
        break;
    }

    return image;
}

Image Image::decodeImage(util::File &file, int width, int height, Rgb color)
{
    std::uint32_t length;
    file >> length;
    const std::size_t offset = file.position();

    Image image(width, height, ARGB32);

    for (int y = 0; y < height; y++) {
        int x = 0;
        while (x < width && !file.atEnd()) {
            std::uint16_t transparent = 0;
            std::uint16_t opaque = 0;
            file >> transparent >> opaque;

            for (unsigned int i = 0; i < transparent; i++)
                image.setPixel(x++, y, rgba(0, 0, 0, 0));

            for (unsigned int i = 0; i < opaque; i++)
                image.setPixel(x++, y, color);
        }
    }
    file.seek(offset + length);
    return image;
}

Image Image::decodeImage(util::File &file, int width, int height, const ColorTable &colorTable)
{
    std::uint32_t length;
    file >> length;
    const std::size_t offset = file.position();

    Image image(width, height, ARGB32);

    for (int y = 0; y < height; y++) {
        int x = 0;
        while (x < width && !file.atEnd()) {
            std::uint16_t transparent = 0;
            std::uint16_t opaque = 0;
            file >> transparent >> opaque;

            for (unsigned int i = 0; i < transparent; i++)
                image.setPixel(x++, y, rgba(0, 0, 0, 0));

            for (unsigned int i = 0; i < opaque; i++) {
                std::uint8_t index;
                file >> index;
                image.setPixel(x++, y, colorTable[index]);
            }
        }
    }
    file.seek(offset + length);
    return image;
}

Image Image::decodeImage(util::File &file, int width, int height)
{
    std::uint32_t length;
    file >> length;
    const std::size_t offset = file.position();

    Image image(width, height, ARGB32);

    for (int y = 0; y < height; y++) {
        int x = 0;
        while (x < width && !file.atEnd()) {
            std::uint16_t transparent = 0;
            std::uint16_t opaque = 0;
            file >> transparent >> opaque;

            for (unsigned int i = 0; i < transparent; i++)
                image.setPixel(x++, y, rgba(0, 0, 0, 0));

            for (unsigned int i = 0; i < opaque; i++) {
                std::uint16_t value;
                file >> value;

                const std::uint8_t r = (value >> 8) & 0xf8;
                const std::uint8_t g = (value >> 3) & 0xfc;
                const std::uint8_t b = (value << 3) & 0xf8;

                image.setPixel(x++, y, rgb(r, g, b));
            }
        }
    }
    file.seek(offset + length);
    return image;
}

} // namespace gfx
