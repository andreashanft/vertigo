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


// gfx::VectorFont: the font editor's file, its grain as the editor makes it, and glyphs drawn
// with an outline, its corners, and grain

#include "check.hpp"

#include "gfx/vectorfont.h"

#include <cmath>
#include <string>


namespace {

// a font of one square glyph, 'A' (2,2 to 9,10 in the cell), in the given style
std::string font(const std::string &style)
{
    return "{ \"style\": " + style + ", \"glyphs\": ["
        " { \"id\": \"41\", \"ch\": \"A\", \"rings\": [[[2, 2], [9, 2], [9, 10], [2, 10]]], \"ox\": 0, \"oy\": 0 },"
        " { \"id\": \"99\", \"ch\": \"\\u00d6\", \"rings\": [[[2, 2], [9, 2], [9, 10]]], \"ox\": 0, \"oy\": 0 },"
        " { \"id\": \"42\", \"ch\": \"B\", \"rings\": [], \"ox\": 0, \"oy\": 0 } ] }";
}

const std::string Plain = "\"bevel\": { \"on\": false, \"mid\": \"#a0a0a0\" }, \"palette\": { \"on\": false }";

// a texel: its red and alpha
int red(const gfx::Image &image, int x, int y) { return image.constScanLine(y)[x*4]; }
int alpha(const gfx::Image &image, int x, int y) { return image.constScanLine(y)[x*4 + 3]; }

} // namespace


int main()
{
    // the editor's random values (its function, run in a browser)
    CHECK(std::abs(gfx::VectorFont::random(0x41, 0, 0, 1) - 0.045660909974) < 1e-9);
    CHECK(std::abs(gfx::VectorFont::random(0x41, 5, 7, 1) - 0.988279537994) < 1e-9);
    CHECK(std::abs(gfx::VectorFont::random(0xe1, 10, 11, 3) - 0.661687989408) < 1e-9);

    gfx::VectorFont vector;
    CHECK(!vector.parse("{ \"glyphs\": ["));
    CHECK(vector.parse(font("{ \"outline\": { \"on\": true, \"color\": \"#102030\", \"width\": 1, \"join\": \"bevel\" }, "
                            "\"grain\": { \"on\": false }, " + Plain + " }")));
    CHECK(vector.has(0x41));
    // Ö is at 0x95 in this font, and a glyph without a shape isn't there
    CHECK(vector.has(0x95));
    CHECK(!vector.has(0x99));
    CHECK(!vector.has(0x42));

    // two texels per unit: the inside, the outline a unit wide, and nothing past it
    gfx::Image image = vector.render(0x41, 11, 12, 2);
    CHECK_EQUAL(image.width(), 22);
    CHECK_EQUAL(image.height(), 24);
    CHECK_EQUAL(red(image, 11, 12), 0xa0);
    CHECK_EQUAL(alpha(image, 11, 12), 255);
    CHECK_EQUAL(red(image, 3, 12), 0x10);
    CHECK_EQUAL(alpha(image, 3, 12), 255);
    CHECK_EQUAL(alpha(image, 1, 12), 0);
    // half the texel across the edge
    CHECK_EQUAL(alpha(image, 17, 12), 255);
    CHECK_EQUAL(alpha(image, 2, 12), 255);
    // a bevelled corner leaves out its tip, a mitred one doesn't
    CHECK_EQUAL(alpha(image, 2, 2), 0);
    CHECK(vector.parse(font("{ \"outline\": { \"on\": true, \"color\": \"#102030\", \"width\": 1, \"join\": \"miter\" }, "
                            "\"grain\": { \"on\": false }, " + Plain + " }")));
    CHECK_EQUAL(alpha(vector.render(0x41, 11, 12, 2), 2, 2), 255);

    // grain alone, crisp cells of a unit: the cell's grey, 128 + 127 times the random value from -1 to 1
    CHECK(vector.parse(font("{ \"outline\": { \"on\": false }, \"grain\": { \"on\": true, \"source\": \"random\", \"cell\": \"1\", "
                            "\"amount\": 1, \"blend\": \"normal\", \"seed\": 1, \"crisp\": true }, " + Plain + " }")));
    image = vector.render(0x41, 11, 12, 1);
    CHECK_EQUAL(red(image, 5, 7), int(128 + (2*gfx::VectorFont::random(0x41, 5, 7, 1) - 1)*127));
    CHECK_EQUAL(alpha(image, 1, 1), 0);

    // over the game glyph's box only: a box from 3,0 three wide holds the cell's columns 3 to 5
    CHECK(vector.parse(font("{ \"outline\": { \"on\": false }, \"grain\": { \"on\": false }, " + Plain + " }")));
    std::string moved = font("{ \"outline\": { \"on\": false }, \"grain\": { \"on\": false }, " + Plain + " }");
    moved.replace(moved.find("\"ox\": 0"), 7, "\"ox\": 3");
    CHECK(vector.parse(moved));
    image = vector.render(0x41, 3, 12, 1);
    CHECK_EQUAL(image.width(), 3);
    CHECK_EQUAL(alpha(image, 0, 1), 0);
    CHECK_EQUAL(alpha(image, 0, 5), 255);

    return test::result();
}
