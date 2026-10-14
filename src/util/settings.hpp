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

#ifndef UTIL_SETTINGS_HPP
#define UTIL_SETTINGS_HPP

#include <map>
#include <string>

namespace util {

// The game's settings: "key=value" lines in settings.ini next to the saves (appDataLocation()),
// keys like "cockpit/mode", values in UTF-8. Each change is written at once.
class Settings
{
public:
    static bool contains(const std::string &key);
    static std::string value(const std::string &key, const std::string &defaultValue = std::string());
    static bool boolValue(const std::string &key, bool defaultValue);
    static int intValue(const std::string &key, int defaultValue);
    static float floatValue(const std::string &key, float defaultValue);

    static void setValue(const std::string &key, const std::string &value);
    static void setValue(const std::string &key, const char *value) { setValue(key, std::string(value)); }
    static void setValue(const std::string &key, bool value);
    static void setValue(const std::string &key, int value);
    static void setValue(const std::string &key, float value);

    // whether the file is there yet
    static bool exists();
    // takes these settings and writes them, as a first file
    static void import(const std::map<std::string, std::string> &values);

    static std::string fileName();
};

} // namespace util

#endif // UTIL_SETTINGS_HPP
