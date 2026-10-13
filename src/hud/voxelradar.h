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

#ifndef HUD_VOXELRADAR_H
#define HUD_VOXELRADAR_H


#include "gfx/texture.h"
#include "util/geometry2d.h"
#include <QImage>
#include <vector>


namespace txt { class DesFile; }
namespace fight { class Scenario; }


namespace hud {


// The terrain ahead of the boat as a small perspective view inside the master monitor
// (cockpit [VoxelRadar0]), shaded and fogged with the cockpit's shading table
// (VFX/TEXTURE/VOX_nnn.S16), with the boat and the sonar contacts marked.
//
// How the original uses the settings is still to be compared: here the view looks from
// -WorldMinY cells behind the boat to WorldMaxY cells ahead of it, Projection is half the
// horizontal view angle in degrees and FlightY the row of the horizon.
class VoxelRadar
{
public:
    // reads [VoxelRadar0] of the cockpit file; isValid() is false without one
    explicit VoxelRadar(txt::DesFile &file);

public:
    bool isValid() const { return m_valid; }
    // relative to the master monitor
    const Rect& rect() const { return m_rect; }

    void draw(fight::Scenario *scenario, const RectF &window);

private:
    void render(fight::Scenario *scenario);
    QRgb shade(int fog, int light) const;
    void mark(int x, int y, QRgb color);

private:
    bool m_valid = false;
    Rect m_rect;
    float m_behind = 5;     // cells
    float m_ahead = 20;     // cells
    float m_focal = 104;    // pixels
    int m_horizon = 16;     // row
    std::vector<QRgb> m_table;  // FogRows x Entries, fog row 0 is the farthest
    QImage m_image;
    gfx::Texture m_texture;
};


} // namespace hud


#endif // HUD_VOXELRADAR_H
