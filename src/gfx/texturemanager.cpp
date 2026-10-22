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

#include "colortable.h"
#include "image.h"
#include "texturemanager.h"


namespace gfx {


TextureManager::TextureManager()
{

}


Texture TextureManager::getModule(const String &filename, bool colorKey)
{
    String basename = String("vfx:texture/%1").arg(filename);
    const auto it = m_textures.find(basename);
    if (it != m_textures.end())
        return it->second;

    ColorTable colorTable(basename + ".s16");
    if (colorKey)
        colorTable[0] = rgba(0, 0, 0, 0);
    Texture texture = Image::load(basename + ".imb", colorTable);
    // the models' textures (256x256) with mipmaps; the effects' pictures with their transparent
    // colour without, whose black would darken the edges in the smaller levels
    if (!colorKey)
        texture.generateMipmaps();
    m_textures[basename] = texture;
    return texture;
}


} // namespace gfx
