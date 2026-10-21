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

#include "commandline.hpp"
#include "path.hpp"

#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

namespace util {


namespace {

const std::size_t HelpWidth = 79;


// text broken into lines of at most `width` characters, each but the first indented
std::string wrap(const std::string &text, std::size_t indent, std::size_t width)
{
    std::string out, line;
    std::size_t start = 0;
    auto flush = [&]() {
        out += (out.empty() ? "" : "\n" + std::string(indent, ' ')) + line;
        line.clear();
    };
    while (start < text.size()) {
        std::size_t end = text.find_first_of(" \n", start);
        if (end == std::string::npos)
            end = text.size();
        const std::string word = text.substr(start, end - start);
        if (!line.empty() && line.size() + 1 + word.size() > width)
            flush();
        line += (line.empty() ? "" : " ") + word;
        if (end < text.size() && text[end] == '\n')
            flush();
        start = end + 1;
    }
    if (!line.empty())
        flush();
    return out;
}

} // namespace


CommandLine::CommandLine(const std::string &description) :
    m_description(description)
{
    addOption("help", "Show this help.");
}


void CommandLine::addOption(const std::string &name, const std::string &description, const std::string &valueName)
{
    m_options.push_back({ name, description, valueName });
}


void CommandLine::addArgument(const std::string &name, const std::string &description, const std::string &syntax)
{
    m_argumentHelp.emplace_back(syntax.empty() ? name : syntax, description);
}


const CommandLine::Option* CommandLine::find(const std::string &name) const
{
    for (const Option &option : m_options)
        if (option.name == name)
            return &option;
    return nullptr;
}


bool CommandLine::parse(const std::vector<std::string> &arguments)
{
    m_values.clear();
    m_arguments.clear();
    m_error.clear();
    m_program = arguments.empty() ? std::string("vertigo") : fileName(arguments[0]);
    bool options = true;
    for (std::size_t i = 1; i < arguments.size(); i++) {
        const std::string &argument = arguments[i];
        if (!options || argument.size() < 2 || argument[0] != '-') {
            m_arguments.push_back(argument);
            continue;
        }
        if (argument == "--") {
            options = false;
            continue;
        }
        if (argument == "-h") {
            m_values["help"].push_back(std::string());
            continue;
        }
        const std::size_t equals = argument.find('=');
        const std::string name = argument.substr(argument[1] == '-' ? 2 : 1, equals == std::string::npos ? std::string::npos : equals - (argument[1] == '-' ? 2 : 1));
        const Option *option = argument[1] == '-' ? find(name) : nullptr;
        if (!option) {
            m_error = "Unknown option '" + argument.substr(0, equals) + "'.";
            return false;
        }
        if (option->valueName.empty()) {
            if (equals != std::string::npos) {
                m_error = "Unexpected value after '--" + name + "'.";
                return false;
            }
            m_values[name].push_back(std::string());
        } else if (equals != std::string::npos) {
            m_values[name].push_back(argument.substr(equals + 1));
        } else if (i + 1 < arguments.size()) {
            m_values[name].push_back(arguments[++i]);
        } else {
            m_error = "Missing value after '--" + name + "'.";
            return false;
        }
    }
    return true;
}


bool CommandLine::isSet(const std::string &name) const
{
    return m_values.count(name) != 0;
}


std::string CommandLine::value(const std::string &name) const
{
    const auto it = m_values.find(name);
    return it != m_values.end() ? it->second.back() : std::string();
}


std::vector<std::string> CommandLine::values(const std::string &name) const
{
    const auto it = m_values.find(name);
    return it != m_values.end() ? it->second : std::vector<std::string>();
}


std::string CommandLine::help() const
{
    std::string usage = "Usage: " + m_program + " [options]";
    for (const auto &argument : m_argumentHelp)
        usage += " " + argument.first;
    std::string text = usage + "\n" + m_description + "\n\nOptions:\n";

    std::vector<std::pair<std::string, std::string>> rows;
    for (const Option &option : m_options)
        rows.emplace_back((option.name == "help" ? "-h, --help" : "--" + option.name) + (option.valueName.empty() ? "" : " <" + option.valueName + ">"), option.description);
    std::size_t column = 0;
    for (const auto &row : rows)
        column = std::max(column, row.first.size());
    column = std::min<std::size_t>(column, 30) + 4;
    auto table = [&](const std::vector<std::pair<std::string, std::string>> &entries) {
        for (const auto &entry : entries) {
            std::string line = "  " + entry.first;
            if (line.size() + 2 > column) {
                text += line + "\n";
                line.clear();
            }
            line.resize(column, ' ');
            text += line + wrap(entry.second, column, HelpWidth - column) + "\n";
        }
    };
    table(rows);
    if (!m_argumentHelp.empty()) {
        text += "\nArguments:\n";
        std::vector<std::pair<std::string, std::string>> arguments;
        for (const auto &argument : m_argumentHelp)
            arguments.emplace_back(argument.first, argument.second);
        table(arguments);
    }
    return text;
}


std::vector<std::string> CommandLine::arguments(int argc, char *argv[])
{
    std::vector<std::string> result;
#ifdef _WIN32
    int count = 0;
    if (wchar_t **wide = CommandLineToArgvW(GetCommandLineW(), &count)) {
        for (int i = 0; i < count; i++)
            result.push_back(std::filesystem::path(wide[i]).u8string());
        LocalFree(wide);
        return result;
    }
#endif
    for (int i = 0; i < argc; i++)
        result.push_back(argv[i]);
    return result;
}


} // namespace util
