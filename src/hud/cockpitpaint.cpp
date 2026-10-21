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

#include "cockpitpaint.h"
#include "gfx/glbmodel.h"

#include <glm/geometric.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <map>
#include <thread>


namespace hud {


namespace {

const int ArtWidth = 640;
const int ArtHeight = 480;
// the colour fields reach this far beyond the 640x480 view (pixels)
const int Pad = 64;
const int CanvasWidth = ArtWidth + 2*Pad;
const int CanvasHeight = ArtHeight + 2*Pad;
// confidence per halving of the resolution away from a part's pixels
const float Decay = 0.7f;
// the art's strong, long edges: contrast (sRGB luminance) across them, how clearly they have one
// direction (structure tensor) and length in pixels
const float EdgeContrast = 0.10f;
const float EdgeCoherence = 0.75f;
const int EdgeLength = 10;
// the sharp edges' transition, in pixels of the art
const float EdgeStep = 1.0f/8;
// texels that the fallback's tint is taken from: every TintStep-th in both directions
const int TintStep = 4;
// part, material or overall tint from at least this many texels of an 8192x8192 atlas
const float TintTexels = 2000;


float clamp01(float x)
{
    return std::min(std::max(x, 0.0f), 1.0f);
}

float smoothstep(float a, float b, float x)
{
    const float t = clamp01((x - a)/(b - a));
    return t*t*(3 - 2*t);
}

float toLinear(float c)
{
    return c <= 0.04045f ? c/12.92f : std::pow((c + 0.055f)/1.055f, 2.4f);
}

float toSrgb(float c)
{
    c = clamp01(c);
    return c <= 0.0031308f ? 12.92f*c : 1.055f*std::pow(c, 1/2.4f) - 0.055f;
}

// the luminance the edges are judged by (roughly perceptual)
float edgeLuma(const Vector3D &c)
{
    return std::pow(std::max(glm::dot(c, Vector3D(0.2126f, 0.7152f, 0.0722f)), 0.0f), 1/2.2f);
}

// runs f(0) to f(n - 1) on all processors
void parallel(int n, const std::function<void(int)> &f)
{
    const int threads = std::max(1, std::min(n, int(std::thread::hardware_concurrency())));
    std::atomic<int> next(0);
    std::vector<std::thread> pool;
    for (int t = 0; t < threads; t++)
        pool.emplace_back([&]() {
            for (int i; (i = next++) < n; )
                f(i);
        });
    for (std::thread &thread : pool)
        thread.join();
}


// a float image with c channels, sampled with pixel centres at integer coordinates
struct Grid
{
    int w = 0, h = 0, c = 1;
    std::vector<float> v;

    Grid() = default;
    Grid(int width, int height, int channels) : w(width), h(height), c(channels), v(std::size_t(width)*height*channels, 0.0f) {}
    bool empty() const { return v.empty(); }
    float* at(int x, int y) { return v.data() + (std::size_t(y)*w + x)*c; }
    const float* at(int x, int y) const { return v.data() + (std::size_t(y)*w + x)*c; }
    // the same pixels' edge-padded neighbour
    const float* clamped(int x, int y) const { return at(std::min(std::max(x, 0), w - 1), std::min(std::max(y, 0), h - 1)); }

    Grid crop(int x0, int y0, int width, int height) const
    {
        Grid out(width, height, c);
        for (int y = 0; y < height; y++)
            std::copy(at(x0, y0 + y), at(x0, y0 + y) + std::size_t(width)*c, out.at(0, y));
        return out;
    }

    // Catmull-Rom, clamped at the borders; out gets c values
    void cubic(float x, float y, float *out) const
    {
        const int x0 = int(std::floor(x)), y0 = int(std::floor(y));
        const float fx = x - x0, fy = y - y0;
        auto weights = [](float f, float *w) {
            const float f2 = f*f, f3 = f2*f;
            w[0] = -0.5f*f3 + f2 - 0.5f*f;
            w[1] = 1.5f*f3 - 2.5f*f2 + 1;
            w[2] = -1.5f*f3 + 2*f2 + 0.5f*f;
            w[3] = 0.5f*f3 - 0.5f*f2;
        };
        float wx[4], wy[4];
        weights(fx, wx);
        weights(fy, wy);
        for (int k = 0; k < c; k++)
            out[k] = 0;
        for (int j = 0; j < 4; j++) {
            const int yy = std::min(std::max(y0 + j - 1, 0), h - 1);
            for (int i = 0; i < 4; i++) {
                const int xx = std::min(std::max(x0 + i - 1, 0), w - 1);
                const float weight = wx[i]*wy[j];
                const float *p = at(xx, yy);
                for (int k = 0; k < c; k++)
                    out[k] += weight*p[k];
            }
        }
    }

    // bilinear, clamped at the borders (first channel)
    float linear(float x, float y) const
    {
        x = std::min(std::max(x, 0.0f), float(w - 1));
        y = std::min(std::max(y, 0.0f), float(h - 1));
        const int x0 = std::min(int(x), std::max(w - 2, 0)), y0 = std::min(int(y), std::max(h - 2, 0));
        const int x1 = std::min(x0 + 1, w - 1), y1 = std::min(y0 + 1, h - 1);
        const float fx = x - x0, fy = y - y0;
        return (*at(x0, y0)*(1 - fx) + *at(x1, y0)*fx)*(1 - fy) + (*at(x0, y1)*(1 - fx) + *at(x1, y1)*fx)*fy;
    }
};


// 2x2 sums (odd sizes padded with 0)
Grid down(const Grid &g)
{
    Grid out((g.w + 1)/2, (g.h + 1)/2, g.c);
    for (int y = 0; y < g.h; y++)
        for (int x = 0; x < g.w; x++) {
            const float *p = g.at(x, y);
            float *q = out.at(x/2, y/2);
            for (int k = 0; k < g.c; k++)
                q[k] += p[k];
        }
    return out;
}

// bilinear upsampling by 2 (pixel centres aligned) to width x height
Grid up(const Grid &g, int width, int height)
{
    Grid out(width, height, g.c);
    for (int y = 0; y < height; y++) {
        const float sy = std::min(std::max((y + 0.5f)/2 - 0.5f, 0.0f), float(g.h - 1));
        const int y0 = int(sy), y1 = std::min(y0 + 1, g.h - 1);
        const float fy = sy - y0;
        for (int x = 0; x < width; x++) {
            const float sx = std::min(std::max((x + 0.5f)/2 - 0.5f, 0.0f), float(g.w - 1));
            const int x0 = int(sx), x1 = std::min(x0 + 1, g.w - 1);
            const float fx = sx - x0;
            float *q = out.at(x, y);
            for (int k = 0; k < g.c; k++)
                q[k] = (g.at(x0, y0)[k]*(1 - fx) + g.at(x1, y0)[k]*fx)*(1 - fy) + (g.at(x0, y1)[k]*(1 - fx) + g.at(x1, y1)[k]*fx)*fy;
        }
    }
    return out;
}

// Fills the colours c where the weight w (0..1) is low from the surrounding weighted colours
// (Gortler et al.'s push-pull). col: the filled colours, conf: a confidence that is w at the
// pixels themselves and decays by Decay per halving of the resolution away from them.
void pushPull(const Grid &w, const Grid &c, Grid &col, Grid &conf)
{
    std::vector<Grid> ws, ns;
    ws.emplace_back(w.w, w.h, 1);
    ns.emplace_back(w.w, w.h, c.c);
    for (std::size_t i = 0; i < w.v.size(); i++) {
        const float wi = clamp01(w.v[i]);
        ws[0].v[i] = wi;
        for (int k = 0; k < c.c; k++)
            ns[0].v[i*c.c + k] = wi*c.v[i*c.c + k];
    }
    while (std::min(ws.back().w, ws.back().h) > 2) {
        Grid wd = down(ws.back()), nd = down(ns.back());
        for (std::size_t i = 0; i < wd.v.size(); i++) {
            const float sum = wd.v[i];
            const float f = std::min(sum, 1.0f)/std::max(sum, 1e-12f);
            for (int k = 0; k < nd.c; k++)
                nd.v[i*nd.c + k] *= f;
            wd.v[i] = std::min(sum, 1.0f);
        }
        ws.push_back(std::move(wd));
        ns.push_back(std::move(nd));
    }
    col = ns.back();
    conf = ws.back();
    for (std::size_t i = 0; i < conf.v.size(); i++)
        for (int k = 0; k < col.c; k++)
            col.v[i*col.c + k] /= std::max(conf.v[i], 1e-12f);
    for (int level = int(ws.size()) - 2; level >= 0; level--) {
        const Grid &wl = ws[std::size_t(level)], &nl = ns[std::size_t(level)];
        const Grid upCol = up(col, wl.w, wl.h), upConf = up(conf, wl.w, wl.h);
        col = nl;
        conf = wl;
        for (std::size_t i = 0; i < wl.v.size(); i++) {
            const float rest = 1 - wl.v[i];
            for (int k = 0; k < col.c; k++)
                col.v[i*col.c + k] += rest*upCol.v[i*col.c + k];
            conf.v[i] += rest*Decay*upConf.v[i];
        }
    }
}

// the mean of the 3x3 (r = 1) or 5x5 (r = 2) neighbourhood, edge-padded
Grid box(const Grid &g, int r)
{
    Grid out(g.w, g.h, g.c);
    const float n = float((2*r + 1)*(2*r + 1));
    for (int y = 0; y < g.h; y++)
        for (int x = 0; x < g.w; x++) {
            float *q = out.at(x, y);
            for (int dy = -r; dy <= r; dy++)
                for (int dx = -r; dx <= r; dx++) {
                    const float *p = g.clamped(x + dx, y + dy);
                    for (int k = 0; k < g.c; k++)
                        q[k] += p[k];
                }
            for (int k = 0; k < g.c; k++)
                q[k] /= n;
        }
    return out;
}


// A part's colour field and its sharp edges, in pixels of the canvas (the art with Pad around it)
struct Field
{
    bool valid = false;
    int x0 = 0, y0 = 0;         // of col and conf
    Grid col, conf;
    bool edges = false;
    int ex0 = 0, ey0 = 0;       // of lo, hi and edge
    Grid lo, hi, edge;
};

// The art's strong, long edges in a crop of a colour field (linear): the darker and the brighter
// colour beside every pixel and how much each pixel lies on such an edge
bool edgeDetail(const Grid &rgb, Grid &lo, Grid &hi, Grid &edge)
{
    const int w = rgb.w, h = rgb.h;
    if (w < 3 || h < 3)
        return false;
    Grid luma(w, h, 1);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            const float *p = rgb.at(x, y);
            *luma.at(x, y) = 0.299f*toSrgb(p[0]) + 0.587f*toSrgb(p[1]) + 0.114f*toSrgb(p[2]);
        }
    // a median against the dithering, then the gradient and the structure tensor
    Grid smooth(w, h, 1);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            std::array<float, 9> n;
            for (int k = 0; k < 9; k++)
                n[std::size_t(k)] = *luma.clamped(x + k%3 - 1, y + k/3 - 1);
            std::nth_element(n.begin(), n.begin() + 4, n.end());
            *smooth.at(x, y) = n[4];
        }
    Grid grad(w, h, 2), tensor(w, h, 3);
    auto derivative = [](const Grid &g, int x, int y, int dx, int dy, int size, int i) {
        if (size < 2)
            return 0.0f;
        if (i == 0)
            return *g.at(x + dx, y + dy) - *g.at(x, y);
        if (i == size - 1)
            return *g.at(x, y) - *g.at(x - dx, y - dy);
        return (*g.at(x + dx, y + dy) - *g.at(x - dx, y - dy))/2;
    };
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            const float gx = derivative(smooth, x, y, 1, 0, w, x), gy = derivative(smooth, x, y, 0, 1, h, y);
            grad.at(x, y)[0] = gx;
            grad.at(x, y)[1] = gy;
            float *t = tensor.at(x, y);
            t[0] = gx*gx;
            t[1] = gy*gy;
            t[2] = gx*gy;
        }
    const Grid j = box(tensor, 2);
    // the darkest and brightest neighbour, and edge lines: maxima of the gradient across the edge
    lo = Grid(w, h, 3);
    hi = Grid(w, h, 3);
    std::vector<std::uint8_t> strong(std::size_t(w)*h), ridge(std::size_t(w)*h);
    const int offsets[4][2] = { { 1, 0 }, { 1, 1 }, { 0, 1 }, { -1, 1 } };
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            int darkest = 0, brightest = 0;
            float low = 0, high = 0;
            for (int k = 0; k < 9; k++) {
                const float v = *luma.clamped(x + k%3 - 1, y + k/3 - 1);
                if (k == 0 || v < low) { low = v; darkest = k; }
                if (k == 0 || v > high) { high = v; brightest = k; }
            }
            std::copy_n(rgb.clamped(x + darkest%3 - 1, y + darkest/3 - 1), 3, lo.at(x, y));
            std::copy_n(rgb.clamped(x + brightest%3 - 1, y + brightest/3 - 1), 3, hi.at(x, y));
            const float *g = grad.at(x, y);
            const float magnitude = std::hypot(g[0], g[1]);
            const int direction = ((int(std::nearbyint(std::atan2(g[1], g[0])/(float(M_PI)/4))) % 4) + 4) % 4;
            auto neighbour = [&](int sign) {
                const int nx = x + sign*offsets[direction][0], ny = y + sign*offsets[direction][1];
                if (nx < 0 || ny < 0 || nx >= w || ny >= h)
                    return 0.0f;
                const float *n = grad.at(nx, ny);
                return std::hypot(n[0], n[1]);
            };
            const bool isRidge = magnitude >= neighbour(1) && magnitude >= neighbour(-1);
            const float *t = j.at(x, y);
            const float coherence = std::sqrt((t[0] - t[1])*(t[0] - t[1]) + 4*t[2]*t[2])/std::max(t[0] + t[1], 1e-9f);
            ridge[std::size_t(y)*w + x] = isRidge;
            strong[std::size_t(y)*w + x] = isRidge && high - low > EdgeContrast && coherence > EdgeCoherence;
        }
    // connected lines (also through weaker ridge pixels next to strong ones), at least EdgeLength
    // strong pixels long
    Grid strongGrid(w, h, 1);
    for (std::size_t i = 0; i < strong.size(); i++)
        strongGrid.v[i] = strong[i];
    const Grid near = box(strongGrid, 1);
    std::vector<int> label(std::size_t(w)*h, -1), counts;
    std::vector<int> stack;
    for (int i = 0; i < w*h; i++) {
        auto candidate = [&](int k) { return strong[std::size_t(k)] || (near.v[std::size_t(k)] > 0.2f && ridge[std::size_t(k)]); };
        if (label[std::size_t(i)] >= 0 || !candidate(i))
            continue;
        const int id = int(counts.size());
        counts.push_back(0);
        stack.assign(1, i);
        label[std::size_t(i)] = id;
        while (!stack.empty()) {
            const int k = stack.back();
            stack.pop_back();
            counts.back() += strong[std::size_t(k)];
            const int kx = k%w, ky = k/w;
            const int next[4] = { kx > 0 ? k - 1 : -1, kx < w - 1 ? k + 1 : -1, ky > 0 ? k - w : -1, ky < h - 1 ? k + w : -1 };
            for (int n : next)
                if (n >= 0 && label[std::size_t(n)] < 0 && candidate(n)) {
                    label[std::size_t(n)] = id;
                    stack.push_back(n);
                }
        }
    }
    Grid line(w, h, 1);
    bool any = false;
    for (int i = 0; i < w*h; i++)
        if (strong[std::size_t(i)] && counts[std::size_t(label[std::size_t(i)])] >= EdgeLength) {
            line.v[std::size_t(i)] = 1;
            any = true;
        }
    if (!any)
        return false;
    Grid zone = box(line, 1);
    for (float &v : zone.v)
        v = v > 0 ? 1.0f : 0.0f;
    edge = box(zone, 1);
    return true;
}


// a triangle's texels (texel centres inside it, top-left rule) with their barycentric coordinates
template <typename F>
void rasterize(Vector2D a, Vector2D b, Vector2D c, int width, int height, F &&f)
{
    float area = (b.x - a.x)*(c.y - a.y) - (b.y - a.y)*(c.x - a.x);
    if (std::abs(area) < 1e-12f)
        return;
    const bool flip = area < 0;
    if (flip) {
        std::swap(b, c);
        area = -area;
    }
    const int x0 = std::max(int(std::floor(std::min({ a.x, b.x, c.x }))), 0);
    const int x1 = std::min(int(std::ceil(std::max({ a.x, b.x, c.x }))), width - 1);
    const int y0 = std::max(int(std::floor(std::min({ a.y, b.y, c.y }))), 0);
    const int y1 = std::min(int(std::ceil(std::max({ a.y, b.y, c.y }))), height - 1);
    // edge functions (positive inside); on an edge a texel belongs to the top or left one
    auto edge = [](const Vector2D &p, const Vector2D &q, float x, float y) { return (q.x - p.x)*(y - p.y) - (q.y - p.y)*(x - p.x); };
    auto topLeft = [](const Vector2D &p, const Vector2D &q) { return (q.y == p.y && q.x < p.x) || q.y > p.y; };
    const bool tl0 = topLeft(b, c), tl1 = topLeft(c, a), tl2 = topLeft(a, b);
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
            const float px = x + 0.5f, py = y + 0.5f;
            const float w0 = edge(b, c, px, py), w1 = edge(c, a, px, py), w2 = edge(a, b, px, py);
            if ((w0 > 0 || (w0 == 0 && tl0)) && (w1 > 0 || (w1 == 0 && tl1)) && (w2 > 0 || (w2 == 0 && tl2))) {
                const float l0 = w0/area, l1 = w1/area, l2 = w2/area;
                // (in the caller's vertex order)
                if (flip)
                    f(x, y, l0, l2, l1);
                else
                    f(x, y, l0, l1, l2);
            }
        }
}


struct Part
{
    const gfx::GlbModel::Mesh *mesh = nullptr;
    std::string material;
    int index = 0;      // in the eye's id buffer, from 1
};

} // namespace


std::vector<std::uint8_t> CockpitPaint::seeThrough(const gfx::Image &art)
{
    const gfx::Image rgba = art.convertToFormat(gfx::Image::RGBA8888);
    const int w = rgba.width(), h = rgba.height();
    std::vector<std::uint8_t> key(std::size_t(w)*h), see(std::size_t(w)*h);
    for (int i = 0; i < w*h; i++)
        key[std::size_t(i)] = rgba.constBits()[4*i + 3] == 0;
    std::vector<int> stack, region;
    std::vector<std::uint8_t> visited(key.size());
    for (int i = 0; i < w*h; i++) {
        if (!key[std::size_t(i)] || visited[std::size_t(i)])
            continue;
        stack.assign(1, i);
        region.clear();
        visited[std::size_t(i)] = 1;
        while (!stack.empty()) {
            const int k = stack.back();
            stack.pop_back();
            region.push_back(k);
            const int kx = k%w, ky = k/w;
            const int next[4] = { kx > 0 ? k - 1 : -1, kx < w - 1 ? k + 1 : -1, ky > 0 ? k - w : -1, ky < h - 1 ? k + w : -1 };
            for (int n : next)
                if (n >= 0 && key[std::size_t(n)] && !visited[std::size_t(n)]) {
                    visited[std::size_t(n)] = 1;
                    stack.push_back(n);
                }
        }
        if (region.size() >= 40)
            for (int k : region)
                see[std::size_t(k)] = 1;
    }
    return see;
}


gfx::Image CockpitPaint::paint(const Input &input, const std::atomic<bool> *cancel)
{
    auto cancelled = [cancel]() { return cancel && cancel->load(); };
    const int size = input.size, S = input.eyeScale;
    const float F = input.focal;
    const Vector2D C = input.center;
    if (!input.kit || input.art.width() != ArtWidth || input.art.height() != ArtHeight || int(input.see.size()) != ArtWidth*ArtHeight)
        return gfx::Image();

    // the parts: the kit's textured meshes (the window glass and the HUD plane are blended)
    std::vector<Part> parts;
    int shadeImage = -1;
    for (const gfx::GlbModel::Mesh &mesh : input.kit->meshes())
        if (mesh.image >= 0 && !mesh.blend && mesh.texCoords.size() == mesh.positions.size()) {
            Part part;
            part.mesh = &mesh;
            part.material = mesh.extras["material"].toString();
            part.index = int(parts.size()) + 1;
            parts.push_back(part);
            shadeImage = mesh.image;
        }
    if (parts.empty())
        return gfx::Image();

    // ------------------------------------------------------------------------------------------
    // 1. The parts seen from the eye at S x the art's resolution: their coverage of its pixels
    // ------------------------------------------------------------------------------------------
    const int eyeWidth = ArtWidth*S, eyeHeight = ArtHeight*S;
    std::vector<std::uint16_t> ids(std::size_t(eyeWidth)*eyeHeight, 0);
    std::vector<float> depth(ids.size(), 0.0f);     // 1/distance, 0 = nothing
    const float Near = 0.01f;
    for (const Part &part : parts) {
        const gfx::GlbModel::Mesh &mesh = *part.mesh;
        for (std::size_t t = 0; t + 2 < mesh.indices.size(); t += 3) {
            // clipped at the eye's plane
            std::array<Vector3D, 4> polygon;
            int n = 0;
            for (int k = 0; k < 3; k++) {
                const Vector3D &p = mesh.positions[mesh.indices[t + std::size_t(k)]];
                const Vector3D &q = mesh.positions[mesh.indices[t + std::size_t((k + 1)%3)]];
                const float dp = -p.z - Near, dq = -q.z - Near;
                if (dp >= 0)
                    polygon[std::size_t(n++)] = p;
                if ((dp >= 0) != (dq >= 0) && n < 4)
                    polygon[std::size_t(n++)] = p + (q - p)*(dp/(dp - dq));
            }
            if (n < 3)
                continue;
            std::array<Vector3D, 4> screen;     // x, y in sub-pixels, 1/distance
            for (int k = 0; k < n; k++) {
                const Vector3D &p = polygon[std::size_t(k)];
                screen[std::size_t(k)] = Vector3D((C.x + F*p.x/-p.z)*S, (C.y - F*p.y/-p.z)*S, 1/-p.z);
            }
            for (int k = 1; k + 1 < n; k++) {
                const Vector3D &a = screen[0], &b = screen[std::size_t(k)], &c = screen[std::size_t(k + 1)];
                rasterize(Vector2D(a), Vector2D(b), Vector2D(c), eyeWidth, eyeHeight, [&](int x, int y, float l0, float l1, float l2) {
                    const float inverse = l0*a.z + l1*b.z + l2*c.z;
                    const std::size_t i = std::size_t(y)*eyeWidth + x;
                    if (inverse > depth[i]) {
                        depth[i] = inverse;
                        ids[i] = std::uint16_t(part.index);
                    }
                });
            }
        }
        if (cancelled())
            return gfx::Image();
    }
    depth = std::vector<float>();
    // per part: (art pixel, sub-pixels it covers there)
    std::vector<std::vector<std::pair<int, int>>> coverage(parts.size() + 1);
    {
        std::vector<std::uint16_t> block(std::size_t(S)*S);
        for (int y = 0; y < ArtHeight; y++)
            for (int x = 0; x < ArtWidth; x++) {
                for (int sy = 0; sy < S; sy++)
                    for (int sx = 0; sx < S; sx++)
                        block[std::size_t(sy*S + sx)] = ids[std::size_t(y*S + sy)*eyeWidth + std::size_t(x*S + sx)];
                std::sort(block.begin(), block.end());
                for (std::size_t i = 0; i < block.size(); ) {
                    std::size_t j = i;
                    while (j < block.size() && block[j] == block[i])
                        j++;
                    if (block[i] > 0)
                        coverage[block[i]].emplace_back(y*ArtWidth + x, int(j - i));
                    i = j;
                }
            }
    }
    ids = std::vector<std::uint16_t>();

    // the art in linear colours, and the shading of the surfaces it doesn't show
    Grid art(ArtWidth, ArtHeight, 3);
    for (int y = 0; y < ArtHeight; y++)
        for (int x = 0; x < ArtWidth; x++) {
            const std::uint8_t *p = input.art.constScanLine(y) + 3*x;
            for (int k = 0; k < 3; k++)
                art.at(x, y)[k] = toLinear(p[k]/255.0f);
        }
    const gfx::Image &shadeSource = input.kit->images()[std::size_t(shadeImage)];
    const gfx::Image shadeRgb = shadeSource.format() == gfx::Image::RGB888 ? shadeSource : shadeSource.convertToFormat(gfx::Image::RGB888);
    Grid shade(shadeRgb.width(), shadeRgb.height(), 1);
    {
        float table[256];
        for (int i = 0; i < 256; i++)
            table[i] = toLinear(i/255.0f);
        for (int y = 0; y < shade.h; y++)
            for (int x = 0; x < shade.w; x++)
                *shade.at(x, y) = table[shadeRgb.constScanLine(y)[3*x]];
    }

    // ------------------------------------------------------------------------------------------
    // 2. Per part: the colour field from the pixels it covers, and its sharp edges
    // ------------------------------------------------------------------------------------------
    std::vector<Field> fields(parts.size() + 1);
    parallel(int(parts.size()), [&](int i) {
        if (cancelled())
            return;
        const Part &part = parts[std::size_t(i)];
        const std::vector<std::pair<int, int>> &entries = coverage[std::size_t(part.index)];
        if (entries.empty())
            return;
        Grid cov(ArtWidth, ArtHeight, 1);
        float most = 0;
        int cx0 = ArtWidth, cy0 = ArtHeight, cx1 = -1, cy1 = -1;
        for (const auto &entry : entries) {
            const float v = float(entry.second)/(S*S);
            cov.v[std::size_t(entry.first)] = v;
            most = std::max(most, v);
            const int x = entry.first%ArtWidth, y = entry.first/ArtWidth;
            cx0 = std::min(cx0, x); cx1 = std::max(cx1, x);
            cy0 = std::min(cy0, y); cy1 = std::max(cy1, y);
        }
        if (most < 0.5f)
            return;
        // trust pixels the part covers fully, and less so at its rim (they mix in what's behind it)
        const Grid near = box(cov, 1);
        Grid weight(CanvasWidth, CanvasHeight, 1), colours(CanvasWidth, CanvasHeight, 3);
        float total = 0;
        for (int y = 0; y < ArtHeight; y++)
            for (int x = 0; x < ArtWidth; x++) {
                const std::size_t k = std::size_t(y)*ArtWidth + x;
                const float w = input.see[k] ? 0.0f : smoothstep(0.55f, 0.95f, cov.v[k])*(0.15f + 0.85f*smoothstep(0.6f, 0.95f, near.v[k]));
                *weight.at(x + Pad, y + Pad) = w;
                std::copy_n(art.at(x, y), 3, colours.at(x + Pad, y + Pad));
                total += w;
            }
        if (total < 0.5f)
            return;
        Grid col, conf;
        pushPull(weight, colours, col, conf);
        // kept where the part's texels see it: around the pixels of its vertices in front of the eye
        const gfx::GlbModel::Mesh &mesh = *part.mesh;
        float fx0 = float(cx0 + Pad), fy0 = float(cy0 + Pad), fx1 = float(cx1 + Pad), fy1 = float(cy1 + Pad);
        for (const Vector3D &p : mesh.positions)
            if (-p.z > 0.01f) {
                const float x = C.x + F*p.x/-p.z + Pad - 0.5f, y = C.y - F*p.y/-p.z + Pad - 0.5f;
                fx0 = std::min(fx0, x); fx1 = std::max(fx1, x);
                fy0 = std::min(fy0, y); fy1 = std::max(fy1, y);
            }
        const int x0 = std::max(int(std::floor(fx0)) - 3, 0), y0 = std::max(int(std::floor(fy0)) - 3, 0);
        const int x1 = std::min(int(std::ceil(fx1)) + 4, CanvasWidth), y1 = std::min(int(std::ceil(fy1)) + 4, CanvasHeight);
        Field &field = fields[std::size_t(part.index)];
        field.x0 = x0;
        field.y0 = y0;
        field.col = col.crop(x0, y0, x1 - x0, y1 - y0);
        field.conf = conf.crop(x0, y0, x1 - x0, y1 - y0);
        // the edges around its pixels
        const int ex0 = std::max(cx0 + Pad - 4, 0), ey0 = std::max(cy0 + Pad - 4, 0);
        const int ex1 = std::min(cx1 + Pad + 5, CanvasWidth), ey1 = std::min(cy1 + Pad + 5, CanvasHeight);
        field.edges = edgeDetail(col.crop(ex0, ey0, ex1 - ex0, ey1 - ey0), field.lo, field.hi, field.edge);
        field.ex0 = ex0;
        field.ey0 = ey0;
        field.valid = true;
    });
    coverage = std::vector<std::vector<std::pair<int, int>>>();
    if (cancelled())
        return gfx::Image();

    // ------------------------------------------------------------------------------------------
    // 3. The atlas: every part's texels, from its field where the eye sees them, else the
    //    fallback; the fallback's tint first, from a sample of the texels
    // ------------------------------------------------------------------------------------------
    // a texel: its colour from the field and how much of it to take
    struct Texel { Vector3D rgb; float alpha; float shade; };
    auto texel = [&](const Field &field, const Vector3D &p, const Vector3D &normal, float u, float v, bool sharp) {
        Texel out;
        out.shade = shade.linear(u*shade.w - 0.5f, v*shade.h - 0.5f);
        out.rgb = Vector3D(0);
        out.alpha = 0;
        if (!field.valid)
            return out;
        const float distance = -p.z;
        float facing = -1;
        float px = 0, py = 0;
        if (distance > 0.01f) {
            px = C.x + F*p.x/distance;
            py = C.y - F*p.y/distance;
            facing = normal == Vector3D(0) ? 1.0f : glm::dot(normal, -p/glm::length(p));
        }
        const float x = px + Pad - 0.5f, y = py + Pad - 0.5f;
        const bool inside = x > -1 && x < CanvasWidth && y > -1 && y < CanvasHeight && facing > 0;
        if (!inside)
            return out;
        float rgb[3];
        field.col.cubic(x - field.x0, y - field.y0, rgb);
        out.rgb = glm::max(Vector3D(rgb[0], rgb[1], rgb[2]), Vector3D(0));
        out.alpha = field.conf.linear(x - field.x0, y - field.y0)*smoothstep(0.12f, 0.35f, facing);
        if (sharp && field.edges) {
            const float ex = x - field.ex0, ey = y - field.ey0;
            float e;
            field.edge.cubic(ex, ey, &e);
            if (e > 0.002f && ex > -2 && ey > -2 && ex < field.edge.w + 1 && ey < field.edge.h + 1) {
                // switch from the darker to the brighter colour at the edge's middle, in EdgeStep
                auto at = [&](float dx, float dy, Vector3D &s, Vector3D &l, Vector3D &h) {
                    float c[3];
                    field.col.cubic(x - field.x0 + dx, y - field.y0 + dy, c);
                    s = Vector3D(c[0], c[1], c[2]);
                    field.lo.cubic(ex + dx, ey + dy, c);
                    l = Vector3D(c[0], c[1], c[2]);
                    field.hi.cubic(ex + dx, ey + dy, c);
                    h = Vector3D(c[0], c[1], c[2]);
                    const float yl = edgeLuma(l);
                    return (edgeLuma(s) - yl)/std::max(edgeLuma(h) - yl, 1e-4f);
                };
                Vector3D s, l, h, s2, l2, h2;
                const float t = at(0, 0, s, l, h);
                const float tx = at(EdgeStep, 0, s2, l2, h2), ty = at(0, EdgeStep, s2, l2, h2);
                const float step = std::hypot(tx - t, ty - t);
                const float a = clamp01((t - 0.5f)/std::max(step, 1e-3f) + 0.5f);
                const Vector3D sharpened = l + (h - l)*a;
                out.rgb = glm::max(out.rgb + (sharpened - s)*clamp01(e), Vector3D(0));
            }
        }
        return out;
    };

    // a part's triangles in texels (the atlas's v runs down, like the texture's rows)
    auto forTexels = [&](const Part &part, std::size_t t, int texels, int step,
                         const std::function<void(int, int, const Vector3D &, const Vector3D &, float, float)> &f) {
        const gfx::GlbModel::Mesh &mesh = *part.mesh;
        const std::uint32_t i0 = mesh.indices[t], i1 = mesh.indices[t + 1], i2 = mesh.indices[t + 2];
        const Vector2D a = mesh.texCoords[i0]*float(texels), b = mesh.texCoords[i1]*float(texels), c = mesh.texCoords[i2]*float(texels);
        const bool normals = mesh.normals.size() == mesh.positions.size();
        rasterize(a, b, c, texels, texels, [&](int x, int y, float l0, float l1, float l2) {
            if (step > 1 && (x%step || y%step))
                return;
            const Vector3D p = mesh.positions[i0]*l0 + mesh.positions[i1]*l1 + mesh.positions[i2]*l2;
            Vector3D n(0);
            if (normals) {
                n = mesh.normals[i0]*l0 + mesh.normals[i1]*l1 + mesh.normals[i2]*l2;
                const float length = glm::length(n);
                n = length > 0 ? n/length : Vector3D(0);
            }
            f(x, y, p, n, (x + 0.5f)/texels, (y + 0.5f)/texels);
        });
    };

    // the tints: per part (else per material, else overall) the art's colour per unit of shading
    // where the field is trusted
    struct Sum { Vector3D rgb = Vector3D(0); double shade = 0; int count = 0; };
    std::vector<Sum> partSums(parts.size() + 1);
    std::map<std::string, Sum> materialSums;
    Sum globalSum;
    for (const Part &part : parts) {
        const Field &field = fields[std::size_t(part.index)];
        if (!field.valid)
            continue;
        Sum &sum = partSums[std::size_t(part.index)];
        for (std::size_t t = 0; t + 2 < part.mesh->indices.size(); t += 3)
            forTexels(part, t, size, TintStep, [&](int, int, const Vector3D &p, const Vector3D &n, float u, float v) {
                const Texel x = texel(field, p, n, u, v, false);
                if (x.alpha > 0.9f) {
                    sum.rgb += x.rgb;
                    sum.shade += x.shade;
                    sum.count++;
                }
            });
        Sum &material = materialSums[part.material];
        material.rgb += sum.rgb; material.shade += sum.shade; material.count += sum.count;
        globalSum.rgb += sum.rgb; globalSum.shade += sum.shade; globalSum.count += sum.count;
    }
    const float enough = TintTexels*float(size)*size/(8192.0f*8192.0f)/(TintStep*TintStep);
    auto tintOf = [](const Sum &sum) { return sum.rgb/float(std::max(sum.shade, 1e-5*std::max(sum.count, 1))); };
    const Vector3D globalTint = globalSum.count > 0 ? tintOf(globalSum) : Vector3D(1);
    std::vector<Vector3D> tints(parts.size() + 1, globalTint);
    for (const Part &part : parts) {
        const Sum &sum = partSums[std::size_t(part.index)];
        const auto material = materialSums.find(part.material);
        if (sum.count > enough)
            tints[std::size_t(part.index)] = tintOf(sum);
        else if (material != materialSums.end() && material->second.count > enough)
            tints[std::size_t(part.index)] = tintOf(material->second);
    }
    if (cancelled())
        return gfx::Image();

    // all texels, the triangles in chunks on all processors
    gfx::Image texture(size, size, gfx::Image::RGB888);
    std::vector<std::uint8_t> used(std::size_t(size)*size, 0);
    std::vector<std::pair<const Part *, std::size_t>> chunks;
    const std::size_t Chunk = 3*1024;
    for (const Part &part : parts)
        for (std::size_t t = 0; t + 2 < part.mesh->indices.size(); t += Chunk)
            chunks.emplace_back(&part, t);
    std::uint8_t srgb[4096];
    for (int i = 0; i < 4096; i++)
        srgb[i] = std::uint8_t(std::lround(toSrgb(i/4095.0f)*255));
    auto encode = [&srgb](float c) { return srgb[std::min(std::max(int(c*4095 + 0.5f), 0), 4095)]; };
    parallel(int(chunks.size()), [&](int k) {
        if (cancelled())
            return;
        const Part &part = *chunks[std::size_t(k)].first;
        const Field &field = fields[std::size_t(part.index)];
        const Vector3D tint = tints[std::size_t(part.index)];
        const std::size_t first = chunks[std::size_t(k)].second;
        const std::size_t last = std::min(first + Chunk, part.mesh->indices.size());
        for (std::size_t t = first; t + 2 < last; t += 3)
            forTexels(part, t, size, 1, [&](int x, int y, const Vector3D &p, const Vector3D &n, float u, float v) {
                const Texel s = texel(field, p, n, u, v, true);
                const Vector3D rgb = s.rgb*s.alpha + s.shade*tint*(1 - s.alpha);
                std::uint8_t *out = texture.scanLine(y) + 3*x;
                out[0] = encode(rgb.r);
                out[1] = encode(rgb.g);
                out[2] = encode(rgb.b);
                used[std::size_t(y)*size + x] = 1;
            });
    });
    fields = std::vector<Field>();
    if (cancelled())
        return gfx::Image();

    // ------------------------------------------------------------------------------------------
    // 4. The texels between the islands: pull-push from the islands' colours (levels of colour
    //    averages and how much of them the islands cover)
    // ------------------------------------------------------------------------------------------
    struct Level { int size; std::vector<float> rgba; };
    std::vector<Level> levels;
    {
        // level 1 from the texture
        Level first { (size + 1)/2, {} };
        first.rgba.assign(std::size_t(first.size)*first.size*4, 0.0f);
        for (int y = 0; y < size; y++) {
            const std::uint8_t *row = texture.constScanLine(y);
            for (int x = 0; x < size; x++)
                if (used[std::size_t(y)*size + x]) {
                    float *q = &first.rgba[(std::size_t(y/2)*first.size + x/2)*4];
                    q[0] += row[3*x]; q[1] += row[3*x + 1]; q[2] += row[3*x + 2]; q[3] += 1;
                }
        }
        levels.push_back(std::move(first));
    }
    auto normalize = [](Level &level) {
        for (std::size_t i = 0; i < level.rgba.size(); i += 4) {
            const float w = level.rgba[i + 3];
            if (w > 0) {
                level.rgba[i] /= w; level.rgba[i + 1] /= w; level.rgba[i + 2] /= w;
            }
            level.rgba[i + 3] = std::min(w, 1.0f);
        }
    };
    normalize(levels.back());
    while (levels.back().size > 1) {
        const Level &fine = levels.back();
        Level coarse { (fine.size + 1)/2, {} };
        coarse.rgba.assign(std::size_t(coarse.size)*coarse.size*4, 0.0f);
        for (int y = 0; y < fine.size; y++)
            for (int x = 0; x < fine.size; x++) {
                const float *p = &fine.rgba[(std::size_t(y)*fine.size + x)*4];
                float *q = &coarse.rgba[(std::size_t(y/2)*coarse.size + x/2)*4];
                q[0] += p[0]*p[3]; q[1] += p[1]*p[3]; q[2] += p[2]*p[3]; q[3] += p[3];
            }
        normalize(coarse);
        levels.push_back(std::move(coarse));
    }
    // from the coarsest level down: where a level is not covered, the next one's (bilinear)
    auto upSample = [](const Level &coarse, int x, int y, float *out) {
        const float sx = std::min(std::max((x + 0.5f)/2 - 0.5f, 0.0f), float(coarse.size - 1));
        const float sy = std::min(std::max((y + 0.5f)/2 - 0.5f, 0.0f), float(coarse.size - 1));
        const int x0 = int(sx), y0 = int(sy), x1 = std::min(x0 + 1, coarse.size - 1), y1 = std::min(y0 + 1, coarse.size - 1);
        const float fx = sx - x0, fy = sy - y0;
        for (int k = 0; k < 3; k++) {
            auto at = [&](int xx, int yy) { return coarse.rgba[(std::size_t(yy)*coarse.size + xx)*4 + k]; };
            out[k] = (at(x0, y0)*(1 - fx) + at(x1, y0)*fx)*(1 - fy) + (at(x0, y1)*(1 - fx) + at(x1, y1)*fx)*fy;
        }
    };
    for (int l = int(levels.size()) - 2; l >= 0; l--) {
        Level &fine = levels[std::size_t(l)];
        const Level &coarse = levels[std::size_t(l + 1)];
        for (int y = 0; y < fine.size; y++)
            for (int x = 0; x < fine.size; x++) {
                float *p = &fine.rgba[(std::size_t(y)*fine.size + x)*4];
                if (p[3] >= 1)
                    continue;
                float c[3];
                upSample(coarse, x, y, c);
                for (int k = 0; k < 3; k++)
                    p[k] = p[k]*p[3] + c[k]*(1 - p[3]);
                p[3] = 1;
            }
    }
    for (int y = 0; y < size; y++) {
        std::uint8_t *row = texture.scanLine(y);
        for (int x = 0; x < size; x++)
            if (!used[std::size_t(y)*size + x]) {
                float c[3];
                upSample(levels[0], x, y, c);
                for (int k = 0; k < 3; k++)
                    row[3*x + k] = std::uint8_t(std::min(std::max(c[k] + 0.5f, 0.0f), 255.0f));
            }
    }
    return texture;
}


} // namespace hud
