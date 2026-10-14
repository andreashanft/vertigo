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

#ifndef GAME_MISSION_H
#define GAME_MISSION_H


#include "util/string.hpp"
#include <QPoint>


namespace game {


class Mission
{
public:
    Mission(const String &name, int station);

public:
    const String& shortName() const { return m_shortName; }
    const String& name() const { return m_name; }
    const String& scenario() const { return m_scenario; }
    int station() const { return m_station; }
    const QPoint& pos() const { return m_pos; }
    const StringList& textB() const { return m_textB; }
    const StringList& textP() const { return m_textP; }
    const StringList& textS() const { return m_textS; }

private:
    String m_shortName;
    String m_name;
    String m_scenario;
    int m_station;
    QPoint m_pos;
    StringList m_textB;
    StringList m_textP;
    StringList m_textS;
};


} // namespace game


#endif // GAME_MISSION_H
