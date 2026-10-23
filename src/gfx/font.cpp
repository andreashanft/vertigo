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

#include "fontmanager.h"
#include "image.h"
#include "pipeline.h"
#include "upscaled.h"
#include "vectorfont.h"
#include "util/error.h"
#include "util/path.hpp"

#include <SDL3/SDL_filesystem.h>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <vector>

namespace gfx {

namespace {

// the vector font that comes with Vertigo (data/font): font/vertigo-mono.json next to the program
// (in the app's resources on iOS), or in share/vertigo/font when installed; null without it
const VectorFont* vectorFont()
{
    static const std::unique_ptr<VectorFont> font = []() -> std::unique_ptr<VectorFont> {
        const char *base = SDL_GetBasePath();
        if (!base)
            return nullptr;
        for (const char *candidate : { "font/vertigo-mono.json", "../share/vertigo/font/vertigo-mono.json" }) {
            std::ifstream file(util::toPath(util::joinPath(base, candidate)), std::ios::binary);
            if (!file)
                continue;
            std::stringstream text;
            text << file.rdbuf();
            auto font = std::make_unique<VectorFont>();
            if (font->parse(text.str()))
                return font;
            std::cerr << "The vector font " << candidate << " can't be read" << std::endl;
        }
        return nullptr;
    }();
    return font.get();
}

} // namespace

Font::Font(Name name)
{
    *this = FontManager::get()->font(name);
}

FontPrivate::FontPrivate() :
    m_height(0)
{

}

void FontPrivate::load(const String &filename, const ColorTable &colorTable, bool scale, bool smooth)
{
    util::File file(String(filename).replace(":", "/").toUtf8().data());

    int scaleFactor = scale ? 2 : 1;
    const float res = 256 * scaleFactor;

    std::uint8_t type;
    std::uint32_t numEntries;

    file >> type;
    file.skipBytes(5);
    file >> numEntries;
    file.skipBytes(8);

    if (colorTable.size() == 2) {
        m_texture.createEmpty(res, res, Texture::Alpha, smooth);
        m_colorNormal = colorTable[0];
        m_colorHighlight = colorTable[1];
        m_alpha = true;
    } else {
        m_texture.createEmpty(res, res, Texture::RGBA, smooth);
        m_alpha = false;
    }

    // the large silver font has a vector version
    std::string name = String(filename).toUtf8();
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return std::tolower(c); });
    const bool vector = !m_alpha && name.size() >= 11 && name.compare(name.size() - 11, 11, "dplarge.fnt") == 0;

    int maxHeight = 0;
    int x = 1;
    int y = 1;
    for (unsigned int i = 0; i < numEntries; i++) {
        Image image;
        switch (type) {
        case 2:
            image = Image::load(file, Image::PaletteRLE, colorTable);
            break;

        case 4:
        case 18:
            image = Image::load(file, Image::Bitmap, colorTable);
            break;

        case 20:
            image = Image::load(file, Image::RGB565, colorTable);
            break;

        default:
            util::fatal("Unknown font type");
        }

        if (vector)
            m_images.push_back(image);
        image = image.scaled(image.width() * scaleFactor, image.height() * scaleFactor);

        if (x + image.width() + 1 >= res) {
            x = 1;
            y += maxHeight + 1;
        }

        m_texture.update(x, y, image);

        Symbol symbol;
        symbol.rect.x = x / res;
        symbol.rect.width = image.width() / res;
        symbol.rect.y = y / res;
        symbol.rect.height = image.height() / res;
        symbol.width = image.width() / scaleFactor;
        symbol.height = image.height() / scaleFactor;
        m_symbols.push_back(symbol);

        maxHeight = std::max(maxHeight, image.height());

        x += image.width() + 1;
    }

    m_height = maxHeight / scaleFactor;
}

bool FontPrivate::prepareVector()
{
    if (m_vectorState != 0)
        return m_vectorState > 0;
    m_vectorState = -1;
    const VectorFont *font = vectorFont();
    if (font == nullptr || m_images.empty())
        return false;

    // the game's palette for the style that snaps to it: the colours of the glyphs the vector font
    // redraws, those that occur at least eight times
    std::map<Rgb, int> counts;
    for (std::size_t i = 0; i < m_images.size(); i++) {
        if (!font->has(int(i) + 32))
            continue;
        const Image &image = m_images[i];
        for (int y = 0; y < image.height(); y++)
            for (int x = 0; x < image.width(); x++) {
                const Rgb c = image.pixel(x, y);
                if (alpha(c) > 0)
                    counts[c | 0xff000000]++;
            }
    }
    std::vector<Rgb> palette;
    for (const auto &[c, n] : counts)
        if (n >= 8)
            palette.push_back(c);

    // the vector glyph of each game glyph: its own, or that of a glyph that looks the same (the
    // font's small letters are its capitals)
    auto same = [](const Image &a, const Image &b) {
        if (a.width() != b.width() || a.height() != b.height())
            return false;
        for (int y = 0; y < a.height(); y++)
            for (int x = 0; x < a.width(); x++)
                if (a.pixel(x, y) != b.pixel(x, y))
                    return false;
        return true;
    };
    std::vector<int> drawnAs(m_images.size(), 0);
    for (std::size_t i = 0; i < m_images.size(); i++) {
        if (font->has(int(i) + 32)) {
            drawnAs[i] = int(i) + 32;
            continue;
        }
        for (std::size_t j = 0; j < m_images.size() && !drawnAs[i]; j++)
            if (font->has(int(j) + 32) && same(m_images[i], m_images[j]))
                drawnAs[i] = int(j) + 32;
    }

    // the glyphs in rows, a game pixel apart so the smaller mipmap levels keep them apart
    const int scale = VectorFont::Scale, pad = scale;
    for (const int res : { 2048, 4096 }) {
        std::vector<Point> places;
        int x = pad, y = pad, rowHeight = 0;
        for (const Image &image : m_images) {
            const int w = image.width()*scale, h = image.height()*scale;
            if (x + w + pad > res) {
                x = pad;
                y += rowHeight + pad;
                rowHeight = 0;
            }
            places.push_back(Point(x, y));
            x += w + pad;
            rowHeight = std::max(rowHeight, h);
        }
        if (y + rowHeight + pad > res)
            continue;

        m_vectorTexture.createEmpty(res, res, Texture::RGBA, true);
        for (std::size_t i = 0; i < m_images.size(); i++) {
            const Image &image = m_images[i];
            m_vectorTexture.update(places[i].x, places[i].y, drawnAs[i]
                ? font->render(drawnAs[i], image.width(), image.height(), scale, image, palette)
                : image.scaled(image.width()*scale, image.height()*scale));
            RectF rect;
            rect.x = places[i].x / float(res);
            rect.y = places[i].y / float(res);
            rect.width = image.width()*scale / float(res);
            rect.height = image.height()*scale / float(res);
            m_vectorRects.push_back(rect);
        }
        m_vectorTexture.generateMipmaps();
        m_vectorState = 1;
        return true;
    }
    return false;
}

Rect FontPrivate::draw(const String &text, const Rect &dstRect, bool alignHCenter, bool alignBottom, const ClipRect *clipRect)
{
    // with the upscaled graphics the large silver font is drawn from its vector version
    const bool vector = !m_images.empty() && Upscaled::isActive() && prepareVector();
    (vector ? m_vectorTexture : m_texture).bind();

    Rect drawRect(dstRect.pos(), Size(0, m_height));

    if (alignHCenter) {
        const int totalW = width(text);
        if (dstRect.width > totalW)
            drawRect.x += (dstRect.width - totalW) / 2;
    }

    StringList strings = text.split(" ");
    if (alignBottom) {
        const int totalH = height(dstRect.width, strings);
        if (dstRect.height > totalH)
            drawRect.y += (dstRect.height - totalH);
    }

    Point pos = drawRect.pos();

    // the glyphs are drawn together, as long as their colour stays the same
    std::vector<Pipeline::Vertex> quads;
    auto flush = [&quads]() {
        Pipeline::drawQuads(quads.data(), int(quads.size()));
        quads.clear();
    };
    auto setColor = [&](Rgb rgb) {
        const Vector4D color(red(rgb) / 255.0f, green(rgb) / 255.0f, blue(rgb) / 255.0f, alpha(rgb) / 255.0f);
        if (color != Pipeline::color()) {
            flush();
            Pipeline::setColor(color);
        }
    };

    bool highlight = false;
    int lineWidth = 0;
    int current = 0;
    for (String word : strings) {
        if (++current < strings.size())
            word += " ";
        const int wordWidth = width(word);
        if (dstRect.width > 0 && lineWidth + wordWidth > dstRect.width) {
            pos.x -= lineWidth;
            pos.y += m_height + 1;
            drawRect.width = std::max(drawRect.width, lineWidth);
            drawRect.height += m_height + 1;
            lineWidth = 0;
        }

        const std::string latin1 = word.toLatin1();
        for (int i = 0; i < int(latin1.length()); i++) {
            std::uint8_t c = latin1[i];
            if (c == '~') {
                i++;
                c = word[i];
                switch (c) {
                case '0':
                    highlight = false;
                    break;

                case '1':
                    highlight = true;
                    break;
                }
                continue;
            }

            c -= 32;

            // characters without a glyph are drawn as '?'
            if (c >= m_symbols.size())
                c = '?' - 32;
            const Symbol &symbol = m_symbols[c];

            if (m_alpha)
                setColor(highlight ? m_colorHighlight : m_colorNormal);

            RectF dst = RectF(pos, SizeF(symbol.width, symbol.height));
            RectF src = vector ? m_vectorRects[c] : symbol.rect;
            if (clipRect == nullptr || clipRect->clip(dst, src))
            {
                quads.push_back({ Vector3D(dst.left(), dst.top(), 0), Vector2D(src.left(), src.top()) });
                quads.push_back({ Vector3D(dst.left(), dst.bottom(), 0), Vector2D(src.left(), src.bottom()) });
                quads.push_back({ Vector3D(dst.right(), dst.bottom(), 0), Vector2D(src.right(), src.bottom()) });
                quads.push_back({ Vector3D(dst.right(), dst.top(), 0), Vector2D(src.right(), src.top()) });
            }

            pos.x += symbol.width;
            lineWidth += symbol.width;
        }
    }
    drawRect.width = std::max(drawRect.width, lineWidth);

    flush();
    Pipeline::setColor(Vector4D(1));

    return drawRect;
}

int FontPrivate::height(int w, const StringList &strings) const
{
    int totalHeight = m_height;
    int lineWidth = 0.0f;
    for (String word : strings) {
        word += " ";
        const int wordWidth = width(word);
        if (w > 0 && lineWidth + wordWidth > w) {
            totalHeight += m_height + 1;
            lineWidth = 0;
        }

        lineWidth += wordWidth;
    }

    return totalHeight;
}

int FontPrivate::width(const String &text) const
{
    int totalW = 0;
    const std::string latin1 = text.toLatin1();
    for (int i = 0; i < int(latin1.size()); i++) {
        std::uint8_t c = latin1[i];
        if (c == '~') {
            i++;
            continue;
        }
        c -= 32;
        if (c >= m_symbols.size())
            c = '?' - 32;
        const Symbol &symbol = m_symbols[c];
        totalW += symbol.width;
    }

    return totalW;
}

} // namespace gfx
