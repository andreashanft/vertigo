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

#ifndef UTIL_COMMANDLINE_HPP
#define UTIL_COMMANDLINE_HPP

#include <map>
#include <string>
#include <vector>

namespace util {

// The options and arguments of the command line: "--name", "--name value" or "--name=value"
// (options with a value can be given more than once), -h or --help, "--" before arguments that
// start with '-'. Texts are in UTF-8.
class CommandLine
{
public:
    explicit CommandLine(const std::string &description);

public:
    // without a value name a flag
    void addOption(const std::string &name, const std::string &description, const std::string &valueName = std::string());
    void addArgument(const std::string &name, const std::string &description, const std::string &syntax);

    // false with error() on an unknown option or a missing value
    bool parse(const std::vector<std::string> &arguments);
    const std::string& error() const { return m_error; }
    bool helpRequested() const { return isSet("help"); }
    std::string help() const;

    bool isSet(const std::string &name) const;
    // the last value given, or nothing
    std::string value(const std::string &name) const;
    std::vector<std::string> values(const std::string &name) const;
    const std::vector<std::string>& arguments() const { return m_arguments; }

    // argv in UTF-8 (on Windows from the wide command line, as argv is in the ANSI code page)
    static std::vector<std::string> arguments(int argc, char *argv[]);

private:
    struct Option
    {
        std::string name;
        std::string description;
        std::string valueName;
    };

    const Option* find(const std::string &name) const;

private:
    std::string m_program;
    std::string m_description;
    std::vector<Option> m_options;
    std::vector<std::pair<std::string, std::string>> m_argumentHelp;   // syntax, description
    std::map<std::string, std::vector<std::string>> m_values;           // a flag has an empty value
    std::vector<std::string> m_arguments;
    std::string m_error;
};

} // namespace util

#endif // UTIL_COMMANDLINE_HPP
