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

#include "area.h"
#include "desktop.h"
#include "txt/desfile.h"

#include "txt/parse.hpp"


namespace game {


Area::Area()
{

}


Area::Area(const String &name)
{
    load(name);
}


void Area::load(const String &name)
{
    m_shortName = name;
    txt::DesFile file(String("dat:world/%1.des").arg(name));
    file.setSection("Area");
    m_name = file.value("Name").toString();
    m_code = file.value("Code").toInt();

    file.setSection("Map");
    m_map = file.value("Name").toString();

    for (const String &section : file.sections()) {
        String number;
        if (txt::parseNumbered(section, "station", number)) {
            file.setSection(section);
            const int index = number.toUInt();
            const String station = file.value("Name").toString();
            m_stations[index] = Station(index, station);
        }
    }
}


} // namespace game
