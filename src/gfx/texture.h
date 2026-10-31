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

#ifndef GFX_TEXTURE_H
#define GFX_TEXTURE_H


#include "cliprect.h"
#include "image.h"
#include "util/geometry2d.h"
#include <cstdint>
#include <memory>


namespace gfx {



class TexturePrivate;


class Texture
{
public:
    enum Format { RGB, RGBA, Alpha };

public:
    Texture();
    Texture(const Image &image, bool smooth = true);

public:
    bool isValid() const;
    int width() const;
    int height() const;
    Size size() const;
    unsigned int id() const;
    void bind();
    void fromImage(const Image &image, bool smooth = true);
    void createEmpty(int w, int h, Texture::Format format, bool smooth = true);
    void createEmpty(const Size &size, Texture::Format format, bool smooth = true);
    // for images put in with update() that have upscaled copies (a movie's pictures, icons in a
    // row): while the copies are used it holds them, smoothed and with mipmaps
    void createEmptyUpscaled(int w, int h, Texture::Format format);
    // (on a texture made from an upscaled copy or for them, at the position the image stands for:
    // the image's copy takes its place, or the image is scaled up)
    void update(int x, int y, const Image &image);
    // trilinear with mipmaps and filtered anisotropically, for the world's textures seen far off
    // and at an angle; the image fills the texture (sides powers of two), else the padding
    // bleeds into the smaller levels
    void generateMipmaps();
    RectF draw(const ClipRect *clipRect = nullptr);
    RectF draw(float x, float y, const ClipRect *clipRect = nullptr);
    RectF draw(const PointF &dstPos, const ClipRect *clipRect = nullptr);
    RectF draw(const RectF &dstRect, const ClipRect *clipRect = nullptr);
    RectF draw(float x, float y, const RectF &srcRect, const ClipRect *clipRect = nullptr);
    RectF draw(const PointF &dstPos, const RectF &srcRect, const ClipRect *clipRect = nullptr);
    RectF draw(const RectF &dstRect, const RectF &srcRect, const ClipRect *clipRect = nullptr);

private:
    // shared by the copies
    std::shared_ptr<TexturePrivate> d;
};


class TexturePrivate
{
public:
    TexturePrivate();
    TexturePrivate(const TexturePrivate&);
    ~TexturePrivate();

public:
    bool isValid() const { return m_texture != 0; }
    int width() const { return m_width; }
    int height() const { return m_height; }
    unsigned int id() const { return m_texture; }
    void bind();
    void setFilter(bool smooth);
    void fromImage(const Image &image, bool smooth);
    void createEmpty(int w, int h, Texture::Format format, bool smooth);
    void createEmptyUpscaled(int w, int h, Texture::Format format);
    void update(int x, int y, const Image &image);
    // an empty texture of a size that holds images scale times as large (an upscaled copy), and
    // such an image at a position of the size it stands for
    void createEmptyScaled(int w, int h, Texture::Format format, int scale);
    void updateScaled(int x, int y, const Image &image);
    void generateMipmaps();
    RectF draw(const RectF &dstRect, const RectF &srcRect, const ClipRect *clipRect);

private:
    void clear();

private:
    unsigned int m_texture;
    Texture::Format m_format;
    std::uint32_t m_width;
    std::uint32_t m_height;
    std::uint32_t m_textureWidth;
    std::uint32_t m_textureHeight;
    // the texels per pixel of the size the texture stands for (an upscaled copy's)
    int m_scale = 1;
    bool m_mipmaps = false;
};


inline Texture::Texture() :
    d(new TexturePrivate)
{

}


inline Texture::Texture(const Image &image, bool smooth) :
    d(new TexturePrivate)
{
    d->fromImage(image, smooth);
}


inline bool Texture::isValid() const
{
    return d->isValid();
}


inline int Texture::width() const
{
    return d->width();
}


inline int Texture::height() const
{
    return d->height();
}


inline Size Texture::size() const
{
    return Size(width(), height());
}


inline unsigned int Texture::id() const
{
    return d->id();
}


inline void Texture::bind()
{
    d->bind();
}


inline void Texture::fromImage(const Image &image, bool smooth)
{
    d->fromImage(image, smooth);
}


inline void Texture::createEmpty(int w, int h, Texture::Format format, bool smooth)
{
    d->createEmpty(w, h, format, smooth);
}


inline void Texture::createEmpty(const Size &size, Texture::Format format, bool smooth)
{
    d->createEmpty(size.width, size.height, format, smooth);
}


inline void Texture::createEmptyUpscaled(int w, int h, Texture::Format format)
{
    d->createEmptyUpscaled(w, h, format);
}


inline void Texture::update(int x, int y, const Image &image)
{
    d->update(x, y, image);
}


inline void Texture::generateMipmaps()
{
    d->generateMipmaps();
}



inline RectF Texture::draw(const ClipRect *clipRect)
{
    return draw(RectF(0, 0, width(), height()), clipRect);
}


inline RectF Texture::draw(float x, float y, const ClipRect *clipRect)
{
    return draw(RectF(x, y, width(), height()), clipRect);
}


inline RectF Texture::draw(const PointF &dstPos, const ClipRect *clipRect)
{
    return draw(RectF(dstPos.x, dstPos.y, width(), height()), clipRect);
}


inline RectF Texture::draw(const RectF &dstRect, const ClipRect *clipRect)
{
    return draw(dstRect, RectF(0, 0, width(), height()), clipRect);
}


inline RectF Texture::draw(float x, float y, const RectF &srcRect, const ClipRect *clipRect)
{
    return draw(RectF(x, y, srcRect.width, srcRect.height), srcRect, clipRect);
}


inline RectF Texture::draw(const PointF &dstPos, const RectF &srcRect, const ClipRect *clipRect)
{
    return draw(RectF(dstPos.x, dstPos.y, srcRect.width, srcRect.height), srcRect, clipRect);
}


inline RectF Texture::draw(const RectF &dstRect, const RectF &srcRect, const ClipRect *clipRect)
{
    return d->draw(dstRect, srcRect, clipRect);
}


} // namespace gfx


#endif // GFX_TEXTURE_H
