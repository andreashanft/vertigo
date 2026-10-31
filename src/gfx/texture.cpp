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

#include "texture.h"
#include "pipeline.h"
#include "gfx/gl.hpp"
#include "upscaled.h"
#include <vector>


namespace gfx {


inline static std::uint32_t nextPowerOfTwo(std::uint32_t v)
{
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    ++v;
    return v;
}


TexturePrivate::TexturePrivate() :
    m_texture(0),
    m_width(0),
    m_height(0),
    m_textureWidth(0),
    m_textureHeight(0)
{

}


TexturePrivate::~TexturePrivate()
{
    clear();
}


void TexturePrivate::clear()
{
    if (m_texture > 0) {
        glDeleteTextures(1, &m_texture);
        m_texture = 0;
        m_width = 0;
        m_height = 0;
        m_textureWidth = 0;
        m_textureHeight = 0;
    }
}


// OpenGL 1.2
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

void TexturePrivate::setFilter(bool smooth)
{
    bind();
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, smooth ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}


void TexturePrivate::fromImage(const Image &image, bool smooth)
{
    Texture::Format format = Texture::RGB;
    if (image.format() != Image::RGB888)
        format = Texture::RGBA;

    // an upscaled copy of the image takes its place, smoothed and with mipmaps, as the window
    // shows it at any size
    if (Upscaled::isActive()) {
        const Image copy = Upscaled::find(Upscaled::key(image));
        if (copy.width() == image.width()*Upscaled::Scale && copy.height() == image.height()*Upscaled::Scale) {
            createEmptyScaled(image.width(), image.height(), format, Upscaled::Scale);
            updateScaled(0, 0, copy);
            generateMipmaps();
            return;
        }
    }

    createEmpty(image.width(), image.height(), format, smooth);
    update(0, 0, image);
}


void TexturePrivate::createEmpty(int w, int h, Texture::Format format, bool smooth)
{
    clear();
    m_scale = 1;
    m_mipmaps = false;

    glGenTextures(1, &m_texture);
    bind();
    setFilter(smooth);

    m_format = format;
    m_width = w;
    m_height = h;
    m_textureWidth = nextPowerOfTwo(m_width);
    m_textureHeight = nextPowerOfTwo(m_height);

    // always RGBA, which OpenGL ES can update from RGBA images; without alpha of its own the
    // texture is opaque, also in the padding up to the power of two
    std::vector<std::uint8_t> fill(4 * std::size_t(m_textureWidth) * m_textureHeight, 0);
    if (format == Texture::RGB)
        for (std::size_t i = 3; i < fill.size(); i += 4)
            fill[i] = 255;
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_textureWidth, m_textureHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, fill.data());
}


void TexturePrivate::createEmptyScaled(int w, int h, Texture::Format format, int scale)
{
    createEmpty(w*scale, h*scale, format, true);
    m_scale = scale;
    m_width = w;
    m_height = h;
    m_textureWidth /= scale;
    m_textureHeight /= scale;
}


void TexturePrivate::createEmptyUpscaled(int w, int h, Texture::Format format)
{
    if (!Upscaled::isActive()) {
        createEmpty(w, h, format, true);
        return;
    }
    createEmptyScaled(w, h, format, Upscaled::Scale);
    generateMipmaps();
}


void TexturePrivate::updateScaled(int x, int y, const Image &image)
{
    const int scale = m_scale;
    m_scale = 1;
    update(x*scale, y*scale, image);
    m_scale = scale;
}


void TexturePrivate::update(int x, int y, const Image &source)
{
    if (m_scale > 1) {
        // its copy, kept for the next time (a movie shows its pictures again)
        if (m_scale == Upscaled::Scale && Upscaled::isActive()) {
            const Image copy = Upscaled::findRepeated(Upscaled::key(source));
            if (copy.width() == source.width()*m_scale && copy.height() == source.height()*m_scale) {
                updateScaled(x, y, copy);
                return;
            }
        }
        updateScaled(x, y, source.scaled(source.width()*m_scale, source.height()*m_scale));
        return;
    }
    Image image = source.convertToFormat(Image::RGBA8888);
    // an alpha texture only takes the alpha (the colour is the pipeline's)
    if (m_format == Texture::Alpha) {
        for (int line = 0; line < image.height(); line++) {
            std::uint8_t *pixel = image.scanLine(line);
            for (int i = 0; i < image.width(); i++, pixel += 4)
                pixel[0] = pixel[1] = pixel[2] = 255;
        }
    }

    bind();
    glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, image.width(), image.height(), GL_RGBA, GL_UNSIGNED_BYTE, image.constBits());
    // the smaller levels follow (a movie drawn into a picture)
    if (m_mipmaps)
        glGenerateMipmap(GL_TEXTURE_2D);
}


// OpenGL 1.4
#ifndef GL_LINEAR_MIPMAP_LINEAR
#define GL_LINEAR_MIPMAP_LINEAR 0x2703
#endif

void TexturePrivate::generateMipmaps()
{
    if (!isValid())
        return;
    bind();
    glGenerateMipmap(GL_TEXTURE_2D);
    m_mipmaps = true;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    gl::setAnisotropy();
}


void TexturePrivate::bind()
{
    glBindTexture(GL_TEXTURE_2D, m_texture);
}


RectF TexturePrivate::draw(const RectF &dstRect, const RectF &srcRect, const ClipRect *clipRect)
{
    RectF dst = dstRect;
    RectF src(srcRect.x/m_textureWidth, srcRect.y/m_textureHeight, srcRect.width/m_textureWidth, srcRect.height/m_textureHeight);

    if (clipRect == nullptr || clipRect->clip(dst, src))
    {
        // without an image in the colour alone, as OpenGL 1 drew it
        bind();
        Pipeline::drawQuad(dst, src, isValid());
    }

    return dstRect;
}


} // namespace gfx
