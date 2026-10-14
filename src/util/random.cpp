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

#include "random.h"

#include <cstdlib>
#include <random>

namespace util {

namespace {

// the same numbers on every platform with the same RAND_MAX, unlike the
// standard library's distributions
std::mt19937 s_engine;

} // namespace

void seedRandom(unsigned int seed)
{
    s_engine.seed(seed);
}

int random()
{
    return int(s_engine() % (unsigned(RAND_MAX) + 1u));
}

} // namespace util
