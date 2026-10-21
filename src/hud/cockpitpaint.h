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

#ifndef HUD_COCKPITPAINT_H
#define HUD_COCKPITPAINT_H


#include "gfx/image.h"
#include "util/geometry3d.h"

#include <atomic>
#include <cstdint>
#include <vector>


namespace gfx { class GlbModel; }


namespace hud {


// Paints a 3D cockpit's texture from its kit and the cockpit art of the game data, as
// data/3D-cockpits/compose_texture.py does, without OpenGL (so on a worker thread). The kit
// (data/3D-cockpits/kits, made by bake_maps.py) is the model with its texture atlas, normals and
// the fallback shading, but none of the art:
//  1. the kit seen from the eye at eyeScale times the art's resolution, each part in its own
//     colour: how much of every pixel of the art each part covers;
//  2. per part, a colour field from the art's pixels it covers, continued smoothly beyond them
//     (push-pull), and the art's strong, long edges in it, drawn crisp;
//  3. every part drawn into the atlas: a texel takes its part's colour field where the eye sees
//     it, and blends into the fallback shading, tinted with the part's colour, where the surface
//     turns away from the eye or lies far from the part's pixels;
//  4. the texels between the atlas's islands filled from their colours (pull-push), so filtering
//     doesn't pull in black.
class CockpitPaint
{
public:
    struct Input
    {
        const gfx::GlbModel *kit = nullptr;
        gfx::Image art;                     // the 640x480 cockpit art, RGB888
        std::vector<std::uint8_t> see;      // 640x480, 1 where the art is see-through
        float focal = 245.5f;               // the art's camera: focal length and optical centre
        Vector2D center = Vector2D(320, 240);
        int size = 8192;                    // of the square texture
        int eyeScale = 8;
    };

    // the texture (RGB888), or a null image if the kit has no parts or cancel was set
    static gfx::Image paint(const Input &input, const std::atomic<bool> *cancel = nullptr);

    // the art's see-through area: its colour key (pure black) where it fills at least 40 pixels,
    // smaller spots are black paint; art in ARGB32 with the colour key transparent
    static std::vector<std::uint8_t> seeThrough(const gfx::Image &art);
};


} // namespace hud


#endif // HUD_COCKPITPAINT_H
