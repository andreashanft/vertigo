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

#include "rgb.h"
#include <algorithm>
#include <cmath>

namespace gfx {


namespace {

// QColor works in 16 bits per channel: an 8-bit channel c is c*257. The arithmetic below is
// QColor's, so that the results are the same to the bit (where the compiler fuses the same
// multiplications and additions, as it does with its defaults)
const int Max = 65535;

int round(float f)
{
    return int(std::lround(f));
}


// hue in hundredths of a degree (Max if there is none), saturation and value 0-Max
struct Hsv
{
    int hue;
    int saturation;
    int value;
};


Hsv toHsv(Rgb c)
{
    const int r = red(c)*257, g = green(c)*257, b = blue(c)*257;
    const int max = std::max({ r, g, b }), min = std::min({ r, g, b });
    Hsv hsv;
    hsv.value = max;
    if (max == min) {
        hsv.hue = Max;
        hsv.saturation = 0;
        return hsv;
    }
    const float chroma = float(max - min);
    hsv.saturation = round(chroma/max*Max);
    // in sixths of the circle from red
    float hue;
    if (max == r) {
        hue = (g - b)/chroma;
        if (hue < 0)
            hue += 6;
    } else if (max == g) {
        hue = 2 + (b - r)/chroma;
    } else {
        hue = 4 + (r - g)/chroma;
    }
    hsv.hue = round(hue*6000);
    return hsv;
}


// 16 bits to 8, rounded
int to8(int c)
{
    return (c + 0x80 - ((c + 0x80) >> 8)) >> 8;
}


Rgb toRgb(const Hsv &hsv, int alpha)
{
    if (hsv.saturation == 0 || hsv.hue == Max) {
        const int v = to8(hsv.value);
        return rgba(v, v, v, alpha);
    }
    const float h = hsv.hue == 36000 ? 0.0f : hsv.hue/6000.0f;
    const float s = hsv.saturation/float(Max);
    const float v = hsv.value/float(Max);
    const int i = int(h);
    const float f = h - i;
    const float p = v*(1.0f - s);
    const float q = v*(1.0f - s*f);
    const float t = v*(1.0f - s*(1.0f - f));
    float r = 0, g = 0, b = 0;
    switch (i) {
    case 0: r = v; g = t; b = p; break;
    case 1: r = q; g = v; b = p; break;
    case 2: r = p; g = v; b = t; break;
    case 3: r = p; g = q; b = v; break;
    case 4: r = t; g = p; b = v; break;
    case 5: r = v; g = p; b = q; break;
    }
    return rgba(to8(round(r*Max)), to8(round(g*Max)), to8(round(b*Max)), alpha);
}

} // namespace


Rgb lighter(Rgb c, int factor)
{
    if (factor <= 0)
        return c;
    if (factor < 100)
        return darker(c, 10000/factor);
    Hsv hsv = toHsv(c);
    unsigned int v = unsigned(factor)*unsigned(hsv.value)/100;
    if (v > unsigned(Max)) {
        // past full brightness, whiter instead
        hsv.saturation = std::max(0, hsv.saturation - int(v - Max));
        v = Max;
    }
    hsv.value = int(v);
    return toRgb(hsv, alpha(c));
}


Rgb darker(Rgb c, int factor)
{
    if (factor <= 0)
        return c;
    if (factor < 100)
        return lighter(c, 10000/factor);
    Hsv hsv = toHsv(c);
    hsv.value = int(unsigned(hsv.value)*100/unsigned(factor));
    return toRgb(hsv, alpha(c));
}


} // namespace gfx
