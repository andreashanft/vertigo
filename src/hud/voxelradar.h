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


// The terrain around the boat as a small 3D view inside the master monitor (cockpit
// [VoxelRadar0]), as in the original: a square patch turning with the boat, seen from behind and
// above, shaded by height with the cockpit's shading table (VFX/TEXTURE/VOX_nnn.S16), low ground
// dark and peaks bright, and with nothing around it, so the monitor shows through. Buildings are
// orange blocks; the boat is an orange line up from the ground and the sonar contacts lines in
// red (hostile), green (friendly) or blue (neutral).
//
// How the original uses its settings is still to be compared: here Projection is the angle the
// view looks down at and the patch reaches WorldMaxY cells to each side of the boat; WorldMinY
// and FlightY aren't used yet.
class VoxelRadar
{
public:
    // reads [VoxelRadar0] of the cockpit file; isValid() is false without one
    explicit VoxelRadar(txt::DesFile &file);

public:
    bool isValid() const { return m_valid; }
    // relative to the master monitor
    const Rect& rect() const { return m_rect; }
    // keeps the view above a line of the monitor (from the top of rect())
    void limitHeight(int height);

    void draw(fight::Scenario *scenario, const RectF &window);

private:
    void render(fight::Scenario *scenario);
    QRgb shade(int fog, int light) const;

private:
    bool m_valid = false;
    Rect m_rect;
    float m_elevation = 0.52f;  // radians the view looks down
    float m_extent = 20;        // cells from the boat to each side of the patch
    std::vector<QRgb> m_table;  // FogRows x Entries, fog row 0 is the farthest
    QImage m_image;
    gfx::Texture m_texture;
};


} // namespace hud


#endif // HUD_VOXELRADAR_H
