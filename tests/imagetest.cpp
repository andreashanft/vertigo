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


// gfx::Image and the colour functions of gfx/rgb.h, as the game uses them

#include "check.hpp"

#include "gfx/image.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>


namespace {

// colours compare and print as 0xAARRGGBB
struct Hex
{
    gfx::Rgb c;
    bool operator==(const Hex &other) const { return c == other.c; }
};

std::ostream &operator<<(std::ostream &stream, Hex hex)
{
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", unsigned(hex.c));
    return stream << text;
}

#define CHECK_RGB(actual, expected) test::equal(Hex{ actual }, Hex{ expected }, #actual, __LINE__)


void testColors()
{
    using namespace gfx;
    CHECK_RGB(rgba(1, 2, 3, 4), 0x04010203);
    CHECK_RGB(rgb(1, 2, 3), 0xff010203);
    CHECK_EQUAL(red(0x04010203), 1);
    CHECK_EQUAL(green(0x04010203), 2);
    CHECK_EQUAL(blue(0x04010203), 3);
    CHECK_EQUAL(alpha(0x04010203), 4);
    CHECK_EQUAL(gray(rgb(255, 255, 255)), 255);
    CHECK_EQUAL(gray(rgb(0, 0, 0)), 0);

    // the factor the voxel radar uses: a channel's place between the darkest and the brightest
    // one stays
    CHECK_RGB(darker(rgb(130, 65, 13), 130), rgb(100, 50, 10));
    CHECK_RGB(lighter(rgb(100, 50, 10), 130), rgb(130, 65, 13));
    CHECK_RGB(darker(rgba(130, 65, 13, 77), 130), rgba(100, 50, 10, 77));
    CHECK_RGB(lighter(rgba(10, 20, 30, 40), 100), rgba(10, 20, 30, 40));
    // too bright to grow brighter: whiter
    CHECK_RGB(lighter(rgb(200, 100, 0), 150), rgb(255, 150, 45));
    CHECK_RGB(lighter(rgb(200, 200, 200), 130), rgb(255, 255, 255));
    CHECK_RGB(lighter(rgb(0, 0, 0), 130), rgb(0, 0, 0));
}


void testImages()
{
    using namespace gfx;
    CHECK(Image().isNull());
    CHECK(Image(0, 5, Image::RGB888).isNull());

    // indexed, with indices beyond the colour table
    Image indexed(2, 2, Image::Indexed8);
    CHECK(!indexed.isNull());
    CHECK_EQUAL(indexed.bytesPerLine(), 2);
    indexed.setColorTable({ rgb(255, 0, 0), rgba(0, 255, 0, 128) });
    indexed.setPixel(1, 0, 1);
    indexed.setPixel(0, 1, 5);
    indexed.setPixel(5, 5, 1);
    CHECK_RGB(indexed.pixel(0, 0), rgb(255, 0, 0));
    CHECK_RGB(indexed.pixel(1, 0), rgba(0, 255, 0, 128));
    CHECK_EQUAL(indexed.pixelIndex(0, 1), 5);
    CHECK_RGB(indexed.pixel(0, 1), 0);
    CHECK_RGB(indexed.pixel(-1, 0), 0);
    CHECK_RGB(indexed.pixel(2, 0), 0);
    CHECK_EQUAL(indexed.pixelIndex(0, 2), 0);

    const Image argb = indexed.convertToFormat(Image::ARGB32);
    CHECK_EQUAL(argb.format(), Image::ARGB32);
    CHECK_RGB(argb.pixel(0, 0), rgb(255, 0, 0));
    CHECK_RGB(argb.pixel(1, 0), rgba(0, 255, 0, 128));
    CHECK_RGB(argb.pixel(0, 1), 0);
    CHECK_RGB(argb.pixel(1, 1), rgb(255, 0, 0));
    const Image rgba8888 = indexed.convertToFormat(Image::RGBA8888);
    const std::uint8_t *bytes = rgba8888.constScanLine(0) + 4;
    CHECK_EQUAL(int(bytes[0]) << 24 | int(bytes[1]) << 16 | int(bytes[2]) << 8 | int(bytes[3]), 0x00ff0080);
    CHECK(argb.convertToFormat(Image::Indexed8).isNull());
    CHECK_EQUAL(indexed.convertToFormat(Image::Indexed8).pixelIndex(0, 1), 5);

    // without a colour table, grey levels
    Image grey(1, 1, Image::Indexed8);
    grey.setPixel(0, 0, 7);
    CHECK_RGB(grey.convertToFormat(Image::RGB888).pixel(0, 0), rgb(7, 7, 7));

    Image filled(3, 2, Image::RGB888);
    filled.fill(rgba(1, 2, 3, 4));
    CHECK_RGB(filled.pixel(2, 1), rgb(1, 2, 3));
    CHECK_RGB(filled.convertToFormat(Image::ARGB32).convertToFormat(Image::RGB888).pixel(1, 1), rgb(1, 2, 3));

    // parts reaching past the image are transparent there
    const Image part = argb.copy(-1, -1, 3, 3);
    CHECK_EQUAL(part.width(), 3);
    CHECK_RGB(part.pixel(0, 0), 0);
    CHECK_RGB(part.pixel(1, 1), argb.pixel(0, 0));
    CHECK_RGB(part.pixel(2, 2), argb.pixel(1, 1));
    CHECK_RGB(argb.flipped().pixel(0, 0), argb.pixel(0, 1));

    // scaled: the nearest pixel, or the average of those covered, their colours by their alpha
    const Image nearest = argb.scaled(4, 4);
    CHECK_RGB(nearest.pixel(3, 0), argb.pixel(1, 0));
    CHECK_RGB(nearest.pixel(1, 3), argb.pixel(0, 1));
    Image pair(2, 1, Image::ARGB32);
    pair.setPixel(0, 0, rgb(255, 255, 255));
    pair.setPixel(1, 0, rgb(0, 0, 0));
    CHECK_RGB(pair.scaled(1, 1, true).pixel(0, 0), rgb(128, 128, 128));
    pair.setPixel(0, 0, rgb(255, 0, 0));
    pair.setPixel(1, 0, 0);
    CHECK_RGB(pair.scaled(1, 1, true).pixel(0, 0), rgba(255, 0, 0, 128));
    Image even(3, 3, Image::RGB888);
    even.fill(rgb(10, 20, 30));
    CHECK_RGB(even.scaled(2, 2, true).pixel(1, 1), rgb(10, 20, 30));
}


std::vector<std::uint8_t> readFile(const std::filesystem::path &path)
{
    std::ifstream file(path, std::ios::binary);
    return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}


void testPng()
{
    using namespace gfx;
    const std::filesystem::path path = std::filesystem::temp_directory_path() / "vertigo-imagetest.png";
    Image image(3, 2, Image::ARGB32);
    for (int y = 0; y < 2; y++)
        for (int x = 0; x < 3; x++)
            image.setPixel(x, y, rgba(x*100, y*200, 50, 255 - x*100));
    for (const Image::Format format : { Image::RGBA8888, Image::RGB888 }) {
        const Image saved = image.convertToFormat(format);
        CHECK(saved.save(path.u8string()));
        const std::vector<std::uint8_t> data = readFile(path);
        const Image read = Image::fromData(data.data(), data.size());
        CHECK_EQUAL(read.format(), format);
        CHECK_EQUAL(read.width(), 3);
        for (int y = 0; y < 2; y++)
            for (int x = 0; x < 3; x++)
                CHECK_RGB(read.pixel(x, y), saved.pixel(x, y));
    }
    std::filesystem::remove(path);
    const std::uint8_t garbage[] = { 1, 2, 3 };
    CHECK(Image::fromData(garbage, sizeof(garbage)).isNull());
}

} // namespace


int main()
{
    testColors();
    testImages();
    testPng();
    return test::result();
}
