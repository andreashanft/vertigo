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

#include "datetime.hpp"

#include <chrono>
#include <ctime>

namespace util {


namespace {

bool isLeapYear(int year)
{
    return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}


int daysInMonth(int year, int month)
{
    static const int days[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    return month == 2 && isLeapYear(year) ? 29 : days[month - 1];
}


// a number of exactly `digits` digits at `at`
bool number(const String &text, int at, int digits, int &value)
{
    if (at + digits > text.size())
        return false;
    value = 0;
    for (int i = at; i < at + digits; i++) {
        if (text[i] < '0' || text[i] > '9')
            return false;
        value = value*10 + (text[i] - '0');
    }
    return true;
}

} // namespace


DateTime DateTime::now()
{
    const auto now = std::chrono::system_clock::now();
    const std::time_t seconds = std::chrono::system_clock::to_time_t(now);
    std::tm local {};
#ifdef _WIN32
    localtime_s(&local, &seconds);
#else
    localtime_r(&seconds, &local);
#endif
    DateTime time;
    time.year = local.tm_year + 1900;
    time.month = local.tm_mon + 1;
    time.day = local.tm_mday;
    time.hour = local.tm_hour;
    time.minute = local.tm_min;
    time.second = local.tm_sec;
    time.millisecond = int(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000);
    return time;
}


DateTime DateTime::fromIso(const String &text)
{
    // "yyyy-mm-ddThh:mm:ss", and ".zzz" for the milliseconds
    DateTime time;
    const bool milliseconds = text.size() == 23 && text[19] == '.' && number(text, 20, 3, time.millisecond);
    if ((text.size() != 19 && !milliseconds) || !number(text, 0, 4, time.year) || text[4] != '-' || !number(text, 5, 2, time.month)
            || text[7] != '-' || !number(text, 8, 2, time.day) || text[10] != 'T' || !number(text, 11, 2, time.hour) || text[13] != ':'
            || !number(text, 14, 2, time.minute) || text[16] != ':' || !number(text, 17, 2, time.second))
        return DateTime();
    if (time.year == 0 || time.month < 1 || time.month > 12 || time.day < 1 || time.day > daysInMonth(time.year, time.month)
            || time.hour > 23 || time.minute > 59 || time.second > 59)
        return DateTime();
    return time;
}


String DateTime::toIso() const
{
    return String("%1-%2-%3T%4:%5:%6.%7").arg(year, 4, 10, '0').arg(month, 2, 10, '0').arg(day, 2, 10, '0')
            .arg(hour, 2, 10, '0').arg(minute, 2, 10, '0').arg(second, 2, 10, '0').arg(millisecond, 3, 10, '0');
}


} // namespace util
