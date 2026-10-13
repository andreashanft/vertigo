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

#ifndef HUD_MASTERMONITOR_H
#define HUD_MASTERMONITOR_H


#include "util/geometry2d.h"
#include "ui/widget.h"
#include "voxelradar.h"


namespace hud {


class HUD;


class MasterMonitor : public ui::Widget
{
public:
    MasterMonitor(HUD *hud, Rect rect, Rect observationRect, txt::DesFile &file);

public:

protected:
    void draw();

private:
    HUD *m_hud;
    Rect m_rect;
    Rect m_observationRect;
    VoxelRadar m_voxelRadar;    // shown while no target is locked
};


} // namespace hud


#endif // HUD_MASTERMONITOR_H
