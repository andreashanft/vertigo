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

#ifndef GFX_UPSCALED_H
#define GFX_UPSCALED_H

#include "image.h"

#include <cstdint>
#include <string>
#include <vector>

namespace gfx {

// The upscaled graphics (game::Upscaler makes them): Scale times larger copies of the game's
// pictures, model and effect textures, the depot's art and movies and the cursor, each found by a
// hash of the image the game decodes, so wherever an image becomes a texture (or a movie's
// picture goes into one) its copy takes its place. A texture made from a copy keeps the size of
// the image it stands for. The set lives in "Game Data
// Upscaled" next to the saves (on the iPad in the app's documents, where it can be copied to).
class Upscaled
{
public:
    static const int Scale = 4;

    static std::string directory();
    // the setting "Graphics/upscaled"; the copies are used only with it and a set
    static bool isEnabled();
    static void setEnabled(bool enabled);
    // a set is there (read on first use, again after reload())
    static bool isAvailable();
    // the game data the set was made from, as the set records it (fingerprint())
    static std::string source();
    static void reload();

    // the hash of an image as a texture takes it (its size and RGBA pixels)
    static std::uint64_t key(const Image &image);
    static std::string keyName(std::uint64_t key);
    // a copy's file: the hash in hex and ".jpg", or ".png" (with transparency, from an older
    // Vertigo); a JPEG's transparency, where it has some, is in alphaName()
    static bool parseName(const std::string &name, std::uint64_t &key);
    static std::string alphaName(std::uint64_t key);
    // the copy of the image with this hash, Scale times its size; null without one or when off
    static Image find(std::uint64_t key);
    // the same, kept decoded for images shown again and again (a movie's pictures)
    static Image findRepeated(std::uint64_t key);
    // whether copies are being used
    static bool isActive();

    // a set made by an older Vertigo, to be brought up to date (not used meanwhile)
    static bool isOutdated();

    // a short description of the game data in the data path: the sizes of its graphics files
    static std::string fingerprint();
};

} // namespace gfx

#endif // GFX_UPSCALED_H
