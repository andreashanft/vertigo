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

#ifndef FIGHT_PLACEHOLDER_H
#define FIGHT_PLACEHOLDER_H


#include "activeobject.h"


namespace fight {


// Stand-in for units that can't be built yet (cargo ships, unknown types).
// It keeps the unit's conditions, shields and sonar noise working, and is
// drawn and hit as a plain box.
class Placeholder : public ActiveObject
{
public:
    Placeholder(Scenario *scenario, txt::DesFile &file, const ObjectInfo &info, float size);

public:
    void draw();
    bool intersect(const Vector3D &start, const Vector3D &dir, float radius, float &distance, Vector3D &normal);

private:
    float m_halfSize;
};


} // namespace fight


#endif // FIGHT_PLACEHOLDER_H
