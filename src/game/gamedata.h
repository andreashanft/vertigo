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

#ifndef GAME_GAMEDATA_H
#define GAME_GAMEDATA_H

#include <string>

namespace game {

// The game data in util::DataPath's directory: whether it is complete, and building it from a CD
// image of the game (GOG's game.gog, or an .iso of the CD) with the installed files next to it
class GameData
{
public:
    // The first of the files the game reads at the start that the data directory lacks
    // ("gfx/mvi/film/d02.mvi"; GOG's installation lacks the CD's part), or "the text of
    // txt/deeptext.des" if that can't be read. Empty if the data is there, and then the text is
    // loaded (txt::StringTable).
    static std::string missing();

    // a CD image of the game in the directory ("…/game.gog"), empty if there is none
    static std::string findImage(const std::string &directory);

    // Copies DAT, GFX, SFX, TXT and VFX from the CD's SF directory into the target directory,
    // with those of an installation in the image's directory (GOG's) on top. The target, which
    // mustn't exist yet, only appears once the copy is complete; false with the reason in error
    // if it couldn't be made.
    static bool import(const std::string &image, const std::string &target, std::string &error);
};

} // namespace game

#endif // GAME_GAMEDATA_H
