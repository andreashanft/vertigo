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

#ifndef UTIL_DATETIME_HPP
#define UTIL_DATETIME_HPP

#include "string.hpp"
#include <tuple>

namespace util {

// A local date and time, as a saved game records when it was saved
struct DateTime
{
    int year = 0;
    int month = 0;          // 1-12
    int day = 0;            // 1-31
    int hour = 0;
    int minute = 0;
    int second = 0;
    int millisecond = 0;

    // year 0 is never a time read or taken
    bool isValid() const { return year != 0; }

    static DateTime now();
    // "2026-10-14T16:05:09.123" (seconds and the fraction may be missing, a space may stand for
    // the 'T', and a date alone is its midnight); invalid if it isn't one
    static DateTime fromIso(const String &text);
    // with milliseconds, as above
    String toIso() const;

    bool operator<(const DateTime &other) const { return fields() < other.fields(); }
    bool operator>(const DateTime &other) const { return other < *this; }
    bool operator==(const DateTime &other) const { return fields() == other.fields(); }

private:
    std::tuple<int, int, int, int, int, int, int> fields() const { return std::make_tuple(year, month, day, hour, minute, second, millisecond); }
};

} // namespace util

#endif // UTIL_DATETIME_HPP
