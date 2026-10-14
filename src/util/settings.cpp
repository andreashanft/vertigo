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

#include "settings.hpp"
#include "path.hpp"
#include "string.hpp"

#include <charconv>
#include <iostream>
#include <map>

namespace util {


namespace {

std::map<std::string, std::string> s_values;
bool s_loaded = false;


// '\\', '\n' and '\r' escaped, so that a value stays on its line
std::string escape(const std::string &text)
{
    std::string out;
    for (const char c : text) {
        if (c == '\\')
            out += "\\\\";
        else if (c == '\n')
            out += "\\n";
        else if (c == '\r')
            out += "\\r";
        else
            out += c;
    }
    return out;
}


std::string unescape(const std::string &text)
{
    std::string out;
    for (std::size_t i = 0; i < text.size(); i++) {
        if (text[i] != '\\' || i + 1 == text.size()) {
            out += text[i];
            continue;
        }
        const char c = text[++i];
        out += c == 'n' ? '\n' : c == 'r' ? '\r' : c;
    }
    return out;
}


void load()
{
    if (s_loaded)
        return;
    s_loaded = true;
    for (std::string line : lines(readFile(Settings::fileName()))) {
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
            line.pop_back();
        const std::size_t equals = line.find('=');
        if (line.empty() || line[0] == '#' || equals == std::string::npos)
            continue;
        s_values[line.substr(0, equals)] = unescape(line.substr(equals + 1));
    }
}


void save()
{
    std::string text = "# Vertigo's settings\n";
    for (const auto &value : s_values)
        text += value.first + "=" + escape(value.second) + "\n";
    if (!makeDirectory(appDataLocation()) || !writeFile(Settings::fileName(), text))
        std::cerr << "Cannot write " << Settings::fileName() << std::endl;
}

} // namespace


std::string Settings::fileName()
{
    return appDataLocation() + "/settings.ini";
}


bool Settings::contains(const std::string &key)
{
    load();
    return s_values.count(key) != 0;
}


std::string Settings::value(const std::string &key, const std::string &defaultValue)
{
    load();
    const auto it = s_values.find(key);
    return it != s_values.end() ? it->second : defaultValue;
}


bool Settings::boolValue(const std::string &key, bool defaultValue)
{
    const std::string text = value(key);
    return text.empty() ? defaultValue : text == "true" || text == "1";
}


int Settings::intValue(const std::string &key, int defaultValue)
{
    bool ok = false;
    const int number = String::fromUtf8(value(key)).toInt(&ok);
    return ok ? number : defaultValue;
}


float Settings::floatValue(const std::string &key, float defaultValue)
{
    bool ok = false;
    const float number = String::fromUtf8(value(key)).toFloat(&ok);
    return ok ? number : defaultValue;
}


void Settings::setValue(const std::string &key, const std::string &value)
{
    load();
    if (s_values.count(key) && s_values[key] == value)
        return;
    s_values[key] = value;
    save();
}


void Settings::setValue(const std::string &key, bool value)
{
    setValue(key, std::string(value ? "true" : "false"));
}


void Settings::setValue(const std::string &key, int value)
{
    setValue(key, String::number(value).toUtf8());
}


void Settings::setValue(const std::string &key, float value)
{
    // the shortest text that reads back as the same float
    char text[32];
    const std::to_chars_result result = std::to_chars(text, text + sizeof(text), value);
    setValue(key, std::string(text, result.ptr));
}


} // namespace util
