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
    DateTime time;
    if (!number(text, 0, 4, time.year) || text.mid(4, 1) != "-" || !number(text, 5, 2, time.month) || text.mid(7, 1) != "-"
            || !number(text, 8, 2, time.day))
        return DateTime();
    // a date alone is its midnight
    int at = 10;
    if (at < text.size()) {
        if ((text.mid(10, 1) != "T" && text.mid(10, 1) != " ") || !number(text, 11, 2, time.hour) || text.mid(13, 1) != ":"
                || !number(text, 14, 2, time.minute))
            return DateTime();
        at = 16;
    }
    if (text.mid(at, 1) == ":") {
        if (!number(text, at + 1, 2, time.second))
            return DateTime();
        at += 3;
        // a fraction of a second, to the millisecond
        if (text.mid(at, 1) == "." || text.mid(at, 1) == ",") {
            int digits = 0;
            double fraction = 0, scale = 0.1;
            for (at++; at < text.size() && text[at] >= '0' && text[at] <= '9'; at++, digits++, scale /= 10)
                fraction += (text[at] - '0')*scale;
            if (digits == 0)
                return DateTime();
            time.millisecond = int(fraction*1000 + 0.5);
            if (time.millisecond == 1000)
                time.millisecond = 999;
        }
    }
    if (at != text.size() || time.year == 0 || time.month < 1 || time.month > 12 || time.day < 1 || time.day > daysInMonth(time.year, time.month)
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
