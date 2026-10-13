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

#ifndef FIGHT_ENGINESOUNDS_H
#define FIGHT_ENGINESOUNDS_H


#include <map>


namespace sfx { class Sound; }


namespace fight {


class Object;
class Scenario;


// The engines of the nearest moving units, as looping sounds from where the units are.
class EngineSounds
{
public:
    EngineSounds(Scenario *scenario);
    ~EngineSounds();

public:
    void update();

private:
    Scenario *m_scenario;
    std::map<const Object *, sfx::Sound *> m_playing;
};


} // namespace fight


#endif // FIGHT_ENGINESOUNDS_H
