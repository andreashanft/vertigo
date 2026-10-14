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
#include <vector>

namespace gfx {

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
            qFatal("Unknown font type");
        }

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

Rect FontPrivate::draw(const String &text, const Rect &dstRect, bool alignHCenter, bool alignBottom, const ClipRect *clipRect)
{
    m_texture.bind();

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
            RectF src = symbol.rect;
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
