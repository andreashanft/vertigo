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

#ifndef UTIL_RANDOM_H
#define UTIL_RANDOM_H

namespace util {

// The game's random numbers, in place of qsrand()/qrand() (gone in Qt 6):
// one sequence for everything that affects the game, so test runs with a
// fixed seed repeat. Like rand(), random() is between 0 and RAND_MAX.
void seedRandom(unsigned int seed);
int random();

} // namespace util

#endif // UTIL_RANDOM_H
