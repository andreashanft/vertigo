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

#include "datetimetext.hpp"

#include <SDL3/SDL_time.h>
#ifdef __APPLE__
// (in a file of its own: MacTypes.h has a Point and a Rect, too)
#include <CoreFoundation/CoreFoundation.h>
#endif

namespace util {


namespace {

#ifdef __APPLE__
// in the short formats of the system's region, as Qt took them (SDL looks at LANG, which apps
// started from Finder don't have)
String appleShortDateTime(const DateTime &time)
{
    CFLocaleRef locale = CFLocaleCopyCurrent();
    CFCalendarRef calendar = CFCalendarCreateWithIdentifier(nullptr, kCFGregorianCalendar);
    CFAbsoluteTime at = 0;
    CFCalendarComposeAbsoluteTime(calendar, &at, "yMdHms", time.year, time.month, time.day, time.hour, time.minute, time.second);
    auto format = [&](CFDateFormatterStyle date, CFDateFormatterStyle clock) {
        CFDateFormatterRef formatter = CFDateFormatterCreate(nullptr, locale, date, clock);
        CFStringRef text = CFDateFormatterCreateStringWithAbsoluteTime(nullptr, formatter, at);
        char buffer[128] = "";
        CFStringGetCString(text, buffer, sizeof(buffer), kCFStringEncodingUTF8);
        CFRelease(text);
        CFRelease(formatter);
        return String::fromUtf8(buffer);
    };
    const String result = format(kCFDateFormatterShortStyle, kCFDateFormatterNoStyle) + " " + format(kCFDateFormatterNoStyle, kCFDateFormatterShortStyle);
    CFRelease(calendar);
    CFRelease(locale);
    return result;
}
#endif


} // namespace


String shortDateTime(const DateTime &time)
{
    if (!time.isValid())
        return String();
#ifdef __APPLE__
    return appleShortDateTime(time);
#endif
    SDL_DateFormat dateFormat = SDL_DATE_FORMAT_YYYYMMDD;
    SDL_TimeFormat timeFormat = SDL_TIME_FORMAT_24HR;
    SDL_GetDateTimeLocalePreferences(&dateFormat, &timeFormat);
    const int year = time.year % 100;
    String date;
    switch (dateFormat) {
    case SDL_DATE_FORMAT_DDMMYYYY:
        date = String("%1.%2.%3").arg(time.day, 2, 10, '0').arg(time.month, 2, 10, '0').arg(year, 2, 10, '0');
        break;
    case SDL_DATE_FORMAT_MMDDYYYY:
        date = String("%1/%2/%3").arg(time.month).arg(time.day).arg(year, 2, 10, '0');
        break;
    default:
        date = String("%1-%2-%3").arg(year, 2, 10, '0').arg(time.month, 2, 10, '0').arg(time.day, 2, 10, '0');
        break;
    }
    if (timeFormat == SDL_TIME_FORMAT_12HR)
        return date + String(" %1:%2 %3").arg((time.hour + 11) % 12 + 1).arg(time.minute, 2, 10, '0').arg(time.hour < 12 ? "AM" : "PM");
    return date + String(" %1:%2").arg(time.hour, 2, 10, '0').arg(time.minute, 2, 10, '0');
}


} // namespace util
