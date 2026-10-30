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

#ifndef GFX_FONT_H
#define GFX_FONT_H


#include "util/geometry2d.h"
#include "cliprect.h"
#include "colortable.h"
#include "texture.h"
#include "image.h"
#include <memory>
#include <vector>
#include "util/string.hpp"


namespace gfx {


class FontPrivate
{
public:
    FontPrivate();

public:
    void load(const String &filename, const ColorTable &colorTable, bool scale, bool smooth);
    Rect draw(const String &text, const Rect &dstRect, bool alignHCenter = false, bool alignBottom = false, const ClipRect *clipRect = nullptr);
    int height() const { return m_height; }
    int height(int w, const StringList &strings) const;
    int width(const String &text) const;

private:
    struct Symbol
    {
        RectF rect;
        int width;
        int height;
    };

    // the silver fonts' glyphs drawn from the vector font (gfx::VectorFont), made on first
    // use; false without it
    bool prepareVector();

    Texture m_texture;
    std::vector<Symbol> m_symbols;
    int m_height;
    bool m_alpha;
    Rgb m_colorNormal;
    Rgb m_colorHighlight;

    // the silver fonts are drawn from the vector font while the upscaled graphics are on:
    // its glyphs as the file has them, and a texture of them drawn (the game's scaled where the
    // vector font has none) with their places in it
    std::vector<Image> m_images;
    Texture m_vectorTexture;
    std::vector<RectF> m_vectorRects;
    int m_vectorState = 0;  // 0 not tried yet, 1 made, -1 not to be had
    bool m_vectorLarge = false;  // the large font, whose glyphs have places of their own
    double m_vectorSize = 1;  // the vector glyphs' height as a part of the game glyphs'
    double m_vectorGamma = 1;  // the gamma that brightens the vector glyphs' insides
};


class Font
{
public:
    enum Name
    {
        Small,
        Medium,
        Large,
        DialogTop,
        DialogBottom,
        DialogHighlight,
    };

public:
    Font();
    Font(Name name);
    Font(const String &filename, const ColorTable &colorTable, bool scale = false, bool smooth = true);
    Font(const String &filename, Rgb colorNormal, Rgb colorHighlight, bool scale = false, bool smooth = true);

public:
    void load(const String &filename, const ColorTable &colorTable, bool scale = false, bool smooth = true);
    void load(const String &filename, Rgb colorNormal, Rgb colorHighlight, bool scale = false, bool smooth = true);
    Rect draw(const String &text, int x, int y, const ClipRect *clipRect = nullptr);
    Rect draw(const String &text, const Point &dstPos, const ClipRect *clipRect = nullptr);
    Rect draw(const String &text, const Rect &dstRect, const ClipRect *clipRect = nullptr);
    Rect draw(const String &text, const Rect &dstRect, bool alignHCenter, bool alignBottom, const ClipRect *clipRect = nullptr);
    int height() const;
    int width(const String &text) const;

private:
    // shared by the copies
    std::shared_ptr<FontPrivate> d;
};


inline Font::Font() :
    d(new FontPrivate)
{

}


inline Font::Font(const String &filename, const ColorTable &colorTable, bool scale, bool smooth) :
    d(new FontPrivate)
{
    load(filename, colorTable, scale, smooth);
}


inline Font::Font(const String &filename, Rgb colorNormal, Rgb colorHighlight, bool scale, bool smooth) :
    d(new FontPrivate)
{
    load(filename, colorNormal, colorHighlight, scale, smooth);
}


inline void Font::load(const String &filename, const ColorTable &colorTable, bool scale, bool smooth)
{
    d->load(filename, colorTable, scale, smooth);
}


inline void Font::load(const String &filename, Rgb colorNormal, Rgb colorHighlight, bool scale, bool smooth)
{
    d->load(filename, ColorTable{ colorNormal, colorHighlight }, scale, smooth);
}


inline Rect Font::draw(const String &text, int x, int y, const ClipRect *clipRect)
{
    return d->draw(text, Rect(x, y, -1, -1), false, false, clipRect);
}


inline Rect Font::draw(const String &text, const Point &dstPos, const ClipRect *clipRect)
{
    return draw(text, dstPos.x, dstPos.y, clipRect);
}


inline Rect Font::draw(const String &text, const Rect &dstRect, const ClipRect *clipRect)
{
    return d->draw(text, dstRect, false, false, clipRect);
}


inline Rect Font::draw(const String &text, const Rect &dstRect, bool alignHCenter, bool alignBottom, const ClipRect *clipRect)
{
    return d->draw(text, dstRect, alignHCenter, alignBottom, clipRect);
}


inline int Font::height() const
{
    return d->height();
}


inline int Font::width(const String &text) const
{
    return d->width(text);
}


} // namespace gfx


#endif // GFX_FONT_H
