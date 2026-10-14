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

#ifndef UTIL_DATETIMETEXT_HPP
#define UTIL_DATETIMETEXT_HPP

#include "datetime.hpp"

namespace util {

// short, in the order and with the clock the system's region prefers, e.g. "14.10.26 16:05",
// "10/14/26 4:05 PM" or "26-10-14 16:05"; nothing for an invalid time
String shortDateTime(const DateTime &time);

} // namespace util

#endif // UTIL_DATETIMETEXT_HPP
