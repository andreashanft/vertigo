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

#ifndef GAME_AREA_H
#define GAME_AREA_H


#include "station.h"
#include <map>
#include "util/string.hpp"


namespace game {


class Area
{
public:
    Area();
    Area(const String &name);

public:
    int code() const { return m_code; }
    const String& shortName() const { return m_shortName; }
    String map() const { return m_map; }
    const std::map<int, Station>& stations() const { return m_stations; }

private:
    Area(const Area&) = delete;
    Area& operator=(const Area&) = delete;
    void load(const String &name);

private:
    int m_code;
    String m_shortName;
    String m_name;
    String m_map;
    std::map<int, Station> m_stations;
};


} // namespace game


#endif // GAME_AREA_H
