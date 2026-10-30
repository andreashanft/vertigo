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

#ifndef GFX_VECTORFONT_H
#define GFX_VECTORFONT_H

#include "image.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace gfx {

// The silver font (DPLARGE) redrawn as vectors with the font editor in data/font, as it saves it
// (vertigo-mono.json): each glyph a set of polygons, filled even-odd, in a cell of 11 x 12 units
// whose units are the game glyph's pixels, and one style for all of them: an outline, light and
// shade bands, grain and the game's palette. A glyph is drawn at any scale over the box of the
// game's glyph it replaces, so text keeps the game's widths and places. It looks as the editor
// shows it. The glyphs are drawn for the large font; the medium and small silver fonts take them
// shrunk to their height.
class VectorFont
{
public:
    // the texels per game pixel the glyphs are drawn with when they replace the game's
    static const int Scale = 8;

    // the editor's file; false (and no glyphs) on a syntax error
    bool parse(const std::string &text);

    // whether the character (its CP437 code) has a glyph
    bool has(int code) const;
    // The glyph for the character, over the box of the game's glyph (w x h pixels), at scale
    // texels per pixel, RGBA8888. The game's glyph gives the grain when the style takes it from
    // there; the palette is the game's glyph colours, for the style that snaps to them. The
    // cell's height takes size times the box's, centred on the box; a box less than 12 pixels
    // high is a smaller font's, as many pixels narrower as it is lower than the large font's.
    // Above 1, gamma brightens the glyph's inside (not its outline): its colours are raised to
    // the power of 1/gamma.
    Image render(int code, int w, int h, int scale, const Image &original = Image(),
                 const std::vector<Rgb> &palette = {}, double size = 1, double gamma = 1) const;

    // the grain's random value of a cell, 0 to 1, as the editor makes it
    static double random(std::uint32_t glyph, std::uint32_t x, std::uint32_t y, std::uint32_t seed);

public:
    struct Point { double x, y; };
    using Ring = std::vector<Point>;

private:
    struct Glyph
    {
        std::vector<Ring> rings;
        int id = 0;         // the editor's: the character's CP437 code
        int ox = 0;         // where the game's glyph is in the cell
        int oy = 0;
    };

    struct Style
    {
        bool outline = true;
        Rgb outlineColor = 0xff25201a;
        double outlineWidth = 1.25;
        std::string join = "bevel";
        bool bevel = true;
        Rgb lit = 0xffd4d2e4;
        double litBand = 0.75;
        Rgb mid = 0xffa9a5ad;
        Rgb shade = 0xff8c8890;
        double shadeBand = 0.75;
        bool grain = true;
        bool grainOriginal = false;
        double grainCell = 1;
        double grainAmount = 0.5;
        std::string blend = "overlay";
        int seed = 1;
        bool crisp = true;
        bool palette = false;
    };

    std::map<int, Glyph> m_glyphs;
    Style m_style;
};

} // namespace gfx

#endif // GFX_VECTORFONT_H
