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

#ifndef GAME_SAVEDGAME_H
#define GAME_SAVEDGAME_H

#include "util/datetime.hpp"
#include "util/string.hpp"

#include <string>

namespace game {

// a saved game in its slot (from 1, as many as there are): a game's autosave, which it keeps
// writing, or one saved by hand in the notebook
struct SavedGame
{
    int slot = 0;
    String name;           // the save's; an autosave's is its game's
    String game;           // the game it belongs to, named at its start
    String station;
    util::DateTime time;   // local
    bool autosave = false;
    std::string file;      // empty for none
};

} // namespace game

#endif // GAME_SAVEDGAME_H
