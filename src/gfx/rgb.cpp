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


Rgb lighter(Rgb c, int factor)
{
    const float r = float(red(c)), g = float(green(c)), b = float(blue(c));
    const float max = std::max({ r, g, b }), min = std::min({ r, g, b });
    if (max == 0)
        return c;
    // in HSV: the value times the factor, and past full brightness less saturation instead
    float value = max/255*factor/100;
    float saturation = (max - min)/max;
    if (value > 1) {
        saturation = std::max(0.0f, saturation - (value - 1));
        value = 1;
    }
    // each channel keeps its place between the lowest and the highest
    auto channel = [&](float x) {
        const float place = max > min ? (x - min)/(max - min) : 1;
        return int(std::lround(255*value*(1 - saturation*(1 - place))));
    };
    return rgba(channel(r), channel(g), channel(b), alpha(c));
}


Rgb darker(Rgb c, int factor)
{
    // the value divided by the factor, which scales all channels alike
    auto channel = [&](int x) { return int(std::lround(x*100.0f/factor)); };
    return rgba(channel(red(c)), channel(green(c)), channel(blue(c)), alpha(c));
}


} // namespace gfx
