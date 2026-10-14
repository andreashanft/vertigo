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

#ifndef GFX_RGB_H
#define GFX_RGB_H

#include <cstdint>

namespace gfx {

// a colour as 0xAARRGGBB, not premultiplied (QRgb's layout, which this replaces)
using Rgb = std::uint32_t;

constexpr int red(Rgb c) { return (c >> 16) & 0xff; }
constexpr int green(Rgb c) { return (c >> 8) & 0xff; }
constexpr int blue(Rgb c) { return c & 0xff; }
constexpr int alpha(Rgb c) { return c >> 24; }

constexpr Rgb rgba(int r, int g, int b, int a)
{
    return (Rgb(a & 0xff) << 24) | (Rgb(r & 0xff) << 16) | (Rgb(g & 0xff) << 8) | Rgb(b & 0xff);
}

constexpr Rgb rgb(int r, int g, int b)
{
    return rgba(r, g, b, 255);
}

// the brightness as the eye sees it, 0-255
constexpr int gray(Rgb c)
{
    return (red(c)*11 + green(c)*16 + blue(c)*5)/32;
}

// brighter or darker by a factor in percent, keeping the hue and the alpha: in HSV, the value
// times the factor (as QColor::lighter() and darker() do, to the bit)
Rgb lighter(Rgb c, int factor = 150);
Rgb darker(Rgb c, int factor = 200);

} // namespace gfx

#endif // GFX_RGB_H
