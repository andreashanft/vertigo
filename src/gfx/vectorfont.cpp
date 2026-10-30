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

#include "vectorfont.h"
#include "util/json.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace gfx {

namespace {

using Point = VectorFont::Point;
using Ring = VectorFont::Ring;
using Color = std::array<double, 3>;

// the editor's cell, in units (the game glyph's pixels)
const int CellWidth = 11;
const int CellHeight = 12;
// samples per texel side; a texel takes the average of its samples
const int Samples = 4;
const double Pi = 3.14159265358979323846;

// a sample grid over a part of the cell: perUnit samples per unit from (x0, y0)
struct Mask
{
    Mask(int w, int h, double perUnit, double x0, double y0) :
        w(w), h(h), perUnit(perUnit), x0(x0), y0(y0), bits(std::size_t(w)*h, 0) {}
    bool at(int x, int y) const { return bits[std::size_t(y)*w + x] != 0; }

    int w, h;
    double perUnit, x0, y0;
    std::vector<std::uint8_t> bits;
};

// sets the samples whose centres lie inside the rings (even-odd), moved by (dx, dy)
void fill(Mask &mask, const std::vector<Ring> &rings, double dx = 0, double dy = 0)
{
    double top = 1e9, bottom = -1e9;
    for (const Ring &ring : rings)
        for (const Point &p : ring) {
            top = std::min(top, p.y + dy);
            bottom = std::max(bottom, p.y + dy);
        }
    const int first = std::max(0, int(std::floor((top - mask.y0)*mask.perUnit - 0.5)));
    const int last = std::min(mask.h - 1, int(std::ceil((bottom - mask.y0)*mask.perUnit - 0.5)));
    std::vector<double> xs;
    for (int j = first; j <= last; j++) {
        const double y = mask.y0 + (j + 0.5)/mask.perUnit - dy;
        xs.clear();
        for (const Ring &ring : rings)
            for (std::size_t i = 0; i < ring.size(); i++) {
                const Point &a = ring[i], &b = ring[(i + 1) % ring.size()];
                if ((a.y <= y) != (b.y <= y))
                    xs.push_back(a.x + (y - a.y)*(b.x - a.x)/(b.y - a.y) + dx);
            }
        std::sort(xs.begin(), xs.end());
        std::uint8_t *row = mask.bits.data() + std::size_t(j)*mask.w;
        for (std::size_t k = 0; k + 1 < xs.size(); k += 2) {
            const int from = std::max(0, int(std::ceil((xs[k] - mask.x0)*mask.perUnit - 0.5)));
            const int to = std::min(mask.w, int(std::ceil((xs[k + 1] - mask.x0)*mask.perUnit - 0.5)));
            if (from < to)
                std::fill(row + from, row + to, 1);
        }
    }
}

// the stroke along the rings, halfWidth to each side, as convex pieces: a quad along each edge
// and at each corner the join (bevel, miter up to four times the half width, or round), as SVG
// draws it
std::vector<Ring> stroke(const std::vector<Ring> &rings, double halfWidth, const std::string &join)
{
    std::vector<Ring> pieces;
    auto normal = [halfWidth](const Point &a, const Point &b) {
        const double dx = b.x - a.x, dy = b.y - a.y, length = std::hypot(dx, dy);
        return Point{ -dy/length*halfWidth, dx/length*halfWidth };
    };
    for (const Ring &input : rings) {
        // without repeated points
        Ring ring;
        for (const Point &p : input)
            if (ring.empty() || std::hypot(p.x - ring.back().x, p.y - ring.back().y) > 1e-9)
                ring.push_back(p);
        while (ring.size() > 1 && std::hypot(ring.front().x - ring.back().x, ring.front().y - ring.back().y) <= 1e-9)
            ring.pop_back();
        const std::size_t n = ring.size();
        if (n < 2)
            continue;
        for (std::size_t i = 0; i < n; i++) {
            const Point &a = ring[i], &b = ring[(i + 1) % n], &c = ring[(i + 2) % n];
            const Point n1 = normal(a, b), n2 = normal(b, c);
            pieces.push_back({ { a.x + n1.x, a.y + n1.y }, { b.x + n1.x, b.y + n1.y }, { b.x - n1.x, b.y - n1.y }, { a.x - n1.x, a.y - n1.y } });
            if (join == "round") {
                Ring disc;
                for (int k = 0; k < 24; k++)
                    disc.push_back({ b.x + halfWidth*std::cos(k*Pi/12), b.y + halfWidth*std::sin(k*Pi/12) });
                pieces.push_back(disc);
                continue;
            }
            for (const double side : { 1.0, -1.0 }) {
                const Point p1{ b.x + side*n1.x, b.y + side*n1.y }, p2{ b.x + side*n2.x, b.y + side*n2.y };
                const double dot = n1.x*n2.x + n1.y*n2.y, w2 = halfWidth*halfWidth;
                if (join == "miter" && w2 + dot > 1e-9) {
                    const double f = w2/(w2 + dot);
                    const Point m{ b.x + side*(n1.x + n2.x)*f, b.y + side*(n1.y + n2.y)*f };
                    if (std::hypot(m.x - b.x, m.y - b.y) <= 4*halfWidth) {
                        pieces.push_back({ b, p1, m, p2 });
                        continue;
                    }
                }
                pieces.push_back({ b, p1, p2 });
            }
        }
    }
    return pieces;
}

Rgb parseColor(const util::Json &value, Rgb defaultValue)
{
    const std::string &s = value.toString();
    if (s.size() != 7 || s[0] != '#')
        return defaultValue;
    return 0xff000000 | Rgb(std::stoul(s.substr(1), nullptr, 16));
}

Color color(Rgb c)
{
    return { red(c)/255.0, green(c)/255.0, blue(c)/255.0 };
}

enum class Blend { Normal, Multiply, Screen, Overlay, HardLight, SoftLight };

// a channel of the grain (source) over the glyph (backdrop), as CSS blends them
double blend(Blend mode, double b, double s)
{
    auto multiply = [](double x, double y) { return x*y; };
    auto screen = [](double x, double y) { return x + y - x*y; };
    switch (mode) {
    case Blend::Normal:
        return s;
    case Blend::Multiply:
        return multiply(b, s);
    case Blend::Screen:
        return screen(b, s);
    case Blend::Overlay:
        return b <= 0.5 ? multiply(s, 2*b) : screen(s, 2*b - 1);
    case Blend::HardLight:
        return s <= 0.5 ? multiply(b, 2*s) : screen(b, 2*s - 1);
    case Blend::SoftLight: {
        if (s <= 0.5)
            return b - (1 - 2*s)*b*(1 - b);
        const double d = b <= 0.25 ? ((16*b - 12)*b + 4)*b : std::sqrt(b);
        return b + (2*s - 1)*(d - b);
    }
    }
    return s;
}

Blend blendMode(const std::string &name)
{
    if (name == "multiply")
        return Blend::Multiply;
    if (name == "screen")
        return Blend::Screen;
    if (name == "overlay")
        return Blend::Overlay;
    if (name == "hard-light")
        return Blend::HardLight;
    if (name == "soft-light")
        return Blend::SoftLight;
    return Blend::Normal;
}

// the game glyph's own grain: each pixel's brightness against the glyph's mean (its pixels
// brighter than 50 of 255), spread two pixels past them
std::vector<double> originalGrain(const Image &original)
{
    const Image image = original.convertToFormat(Image::RGBA8888);
    const int w = image.width(), h = image.height();
    std::vector<double> lum(std::size_t(w)*h), grain(std::size_t(w)*h, 0);
    std::vector<std::uint8_t> have(std::size_t(w)*h, 0);
    double sum = 0;
    int count = 0;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            const std::uint8_t *p = image.constScanLine(y) + x*4;
            const std::size_t i = std::size_t(y)*w + x;
            lum[i] = (p[0]*30 + p[1]*59 + p[2]*11)/100.0;
            if (p[3] > 0 && lum[i] > 50) {
                have[i] = 1;
                sum += lum[i];
                count++;
            }
        }
    const double mean = count ? sum/count : 0;
    for (std::size_t i = 0; i < have.size(); i++)
        if (have[i])
            grain[i] = (lum[i] - mean)/64;
    for (int pass = 0; pass < 2; pass++) {
        std::vector<std::uint8_t> next = have;
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++) {
                const std::size_t i = std::size_t(y)*w + x;
                if (have[i])
                    continue;
                double s = 0;
                int n = 0;
                for (const auto &d : { std::array<int, 2>{ 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } }) {
                    const int X = x + d[0], Y = y + d[1];
                    if (X >= 0 && Y >= 0 && X < w && Y < h && have[std::size_t(Y)*w + X]) {
                        s += grain[std::size_t(Y)*w + X];
                        n++;
                    }
                }
                if (n) {
                    grain[i] = s/n;
                    next[i] = 1;
                }
            }
        have = next;
    }
    return grain;
}

} // namespace


bool VectorFont::parse(const std::string &text)
{
    m_glyphs.clear();
    const util::Json json = util::Json::parse(text);
    if (json.isUndefined())
        return false;

    const util::Json &style = json["style"];
    const util::Json &outline = style["outline"], &bevel = style["bevel"], &grain = style["grain"];
    Style s;
    s.outline = outline["on"].toBool(s.outline);
    s.outlineColor = parseColor(outline["color"], s.outlineColor);
    s.outlineWidth = outline["width"].toDouble(s.outlineWidth);
    if (outline.contains("join"))
        s.join = outline["join"].toString();
    s.bevel = bevel["on"].toBool(s.bevel);
    s.lit = parseColor(bevel["lit"], s.lit);
    s.litBand = bevel["litBand"].toDouble(s.litBand);
    s.mid = parseColor(bevel["mid"], s.mid);
    s.shade = parseColor(bevel["shade"], s.shade);
    s.shadeBand = bevel["shadeBand"].toDouble(s.shadeBand);
    s.grain = grain["on"].toBool(s.grain);
    s.grainOriginal = grain["source"].toString() == "original";
    // (the editor keeps the cell size as the text of its choice)
    if (grain.contains("cell"))
        s.grainCell = grain["cell"].type() == util::Json::String ? std::atof(grain["cell"].toString().c_str()) : grain["cell"].toDouble();
    if (!(s.grainCell > 0))
        s.grainCell = 1;
    s.grainAmount = grain["amount"].toDouble(s.grainAmount);
    if (grain.contains("blend"))
        s.blend = grain["blend"].toString();
    s.seed = int(grain["seed"].toDouble(s.seed));
    s.crisp = grain["crisp"].toBool(s.crisp);
    s.palette = style["palette"]["on"].toBool(s.palette);
    m_style = s;

    for (const util::Json &g : json["glyphs"].elements()) {
        Glyph glyph;
        glyph.id = int(std::strtol(g["id"].toString().c_str(), nullptr, 16));
        glyph.ox = g["ox"].toInt();
        glyph.oy = g["oy"].toInt();
        for (const util::Json &r : g["rings"].elements()) {
            Ring ring;
            for (const util::Json &p : r.elements())
                ring.push_back({ p[0].toDouble(), p[1].toDouble() });
            if (ring.size() >= 3)
                glyph.rings.push_back(ring);
        }
        if (glyph.id > 0 && !glyph.rings.empty())
            m_glyphs[glyph.id] = glyph;
    }
    return true;
}


bool VectorFont::has(int code) const
{
    return m_glyphs.count(code) != 0;
}


double VectorFont::random(std::uint32_t glyph, std::uint32_t x, std::uint32_t y, std::uint32_t seed)
{
    std::uint32_t v = (glyph*374761393u) ^ (x*668265263u) ^ (y*2246822519u) ^ ((seed + 1)*3266489917u);
    v = (v ^ (v >> 13))*1274126177u;
    v ^= v >> 16;
    return v/4294967295.0;
}


Image VectorFont::render(int code, int w, int h, int scale, const Image &original, const std::vector<Rgb> &palette, double size) const
{
    Image image(w*scale, h*scale, Image::RGBA8888);
    const auto found = m_glyphs.find(code);
    if (found == m_glyphs.end() || w <= 0 || h <= 0)
        return image;
    const Glyph &glyph = found->second;
    const Style &s = m_style;

    // the samples over the game glyph's box in the cell: the large font's box, as wide as the
    // game glyph would be there, at the box's centre, its height taking size times the box's
    const double pixelsPerUnit = size*h/CellHeight;
    const double cx = glyph.ox + (w + CellHeight - h)/2.0, cy = glyph.oy + CellHeight/2.0;
    const double x0 = cx - w/2.0/pixelsPerUnit, y0 = cy - h/2.0/pixelsPerUnit;
    const double perUnit = pixelsPerUnit*scale*Samples;
    const int sw = w*scale*Samples, sh = h*scale*Samples;
    Mask fillMask(sw, sh, perUnit, x0, y0);
    fill(fillMask, glyph.rings);
    Mask outlineMask = fillMask;
    if (s.outline && s.outlineWidth > 0)
        for (const Ring &piece : stroke(glyph.rings, s.outlineWidth, s.join))
            fill(outlineMask, { piece });
    // the lit band is where the glyph moved down and right isn't, the shade where moved up and left isn't
    const bool lit = s.bevel && s.litBand > 0, shade = s.bevel && s.shadeBand > 0;
    Mask litMask(sw, sh, perUnit, x0, y0), shadeMask = litMask;
    if (lit)
        fill(litMask, glyph.rings, s.litBand, s.litBand);
    if (shade)
        fill(shadeMask, glyph.rings, -s.shadeBand, -s.shadeBand);

    // the grain: a grey per cell, 128 neutral, over the editor's whole cell
    const bool grain = s.grain && s.grainAmount > 0;
    const double cell = s.grainOriginal ? 1 : s.grainCell;
    const int nx = int(std::ceil(CellWidth/cell - 1e-9)), ny = int(std::ceil(CellHeight/cell - 1e-9));
    std::vector<double> grey(std::size_t(nx)*ny, 128/255.0);
    if (grain) {
        const std::vector<double> own = s.grainOriginal ? originalGrain(original) : std::vector<double>();
        for (int ly = 0; ly < ny; ly++)
            for (int lx = 0; lx < nx; lx++) {
                double v = 0;
                if (s.grainOriginal) {
                    const int px = lx - glyph.ox, py = ly - glyph.oy;
                    if (px >= 0 && py >= 0 && px < original.width() && py < original.height())
                        v = own[std::size_t(py)*original.width() + px];
                } else {
                    v = random(glyph.id, lx, ly, s.seed)*2 - 1;
                }
                grey[std::size_t(ly)*nx + lx] = int(128 + std::clamp(v, -1.0, 1.0)*127)/255.0;
            }
    }
    auto grainAt = [&](double x, double y) {
        if (s.crisp) {
            const int lx = int(std::floor(x/cell)), ly = int(std::floor(y/cell));
            if (lx < 0 || ly < 0 || lx >= nx || ly >= ny)
                return 128/255.0;
            return grey[std::size_t(ly)*nx + lx];
        }
        // smoothed between the cells' centres, as an image is scaled up
        const double px = x/cell - 0.5, py = y/cell - 0.5;
        const int x0 = int(std::floor(px)), y0 = int(std::floor(py));
        const double fx = px - x0, fy = py - y0;
        auto at = [&](int X, int Y) { return grey[std::size_t(std::clamp(Y, 0, ny - 1))*nx + std::clamp(X, 0, nx - 1)]; };
        return (at(x0, y0)*(1 - fx) + at(x0 + 1, y0)*fx)*(1 - fy) + (at(x0, y0 + 1)*(1 - fx) + at(x0 + 1, y0 + 1)*fx)*fy;
    };
    const Blend mode = blendMode(s.blend);

    // the palette's colour for each of 96 brightness levels: the one nearest in brightness
    std::vector<Color> levels;
    if (s.palette && !palette.empty()) {
        for (int i = 0; i < 96; i++) {
            const double l = (i + 0.5)/96*255;
            Rgb best = palette.front();
            auto lum = [](Rgb c) { return (red(c)*30 + green(c)*59 + blue(c)*11)/100.0; };
            for (Rgb c : palette)
                if (std::abs(lum(c) - l) < std::abs(lum(best) - l))
                    best = c;
            levels.push_back(color(best));
        }
    }

    const Color outlineColor = color(s.outlineColor), litColor = color(s.lit), midColor = color(s.mid), shadeColor = color(s.shade);
    for (int ty = 0; ty < h*scale; ty++) {
        std::uint8_t *out = image.scanLine(ty);
        for (int tx = 0; tx < w*scale; tx++, out += 4) {
            Color sum{ 0, 0, 0 };
            int covered = 0;
            for (int j = ty*Samples; j < (ty + 1)*Samples; j++)
                for (int i = tx*Samples; i < (tx + 1)*Samples; i++) {
                    if (!outlineMask.at(i, j))
                        continue;
                    covered++;
                    Color c;
                    if (!fillMask.at(i, j)) {
                        c = outlineColor;
                    } else {
                        c = midColor;
                        if (lit && !litMask.at(i, j))
                            c = litColor;
                        if (shade && !shadeMask.at(i, j))
                            c = shadeColor;
                        if (grain) {
                            const double g = grainAt(x0 + (i + 0.5)/perUnit, y0 + (j + 0.5)/perUnit);
                            for (double &channel : c)
                                channel = (1 - s.grainAmount)*channel + s.grainAmount*blend(mode, channel, g);
                        }
                    }
                    for (int k = 0; k < 3; k++)
                        sum[k] += c[k];
                }
            if (!covered)
                continue;
            Color c{ sum[0]/covered, sum[1]/covered, sum[2]/covered };
            if (!levels.empty()) {
                const double l = 0.3*c[0] + 0.59*c[1] + 0.11*c[2];
                c = levels[std::clamp(int(l*96), 0, 95)];
            }
            for (int k = 0; k < 3; k++)
                out[k] = std::uint8_t(std::lround(std::clamp(c[k], 0.0, 1.0)*255));
            out[3] = std::uint8_t(std::lround(covered*255.0/(Samples*Samples)));
        }
    }
    return image;
}

} // namespace gfx
