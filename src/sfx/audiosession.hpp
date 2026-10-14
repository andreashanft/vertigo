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

#ifndef SFX_AUDIOSESSION_HPP
#define SFX_AUDIOSESSION_HPP

namespace sfx {

// iOS's audio session (the iPad only): the game's sound mixes with other apps' (the player's
// music, as missions have none of their own yet) and is silent with the silent switch. While a
// call, Siri or an alarm interrupts it, the sound system rests.
void setUpAudioSession();
void tearDownAudioSession();
// after an interruption or the background
void activateAudioSession();

} // namespace sfx

#endif // SFX_AUDIOSESSION_HPP
