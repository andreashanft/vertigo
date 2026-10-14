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

#include "datapath.h"
#include "path.hpp"

#include <unordered_map>

namespace util {

namespace {

// lower-case relative path -> relative path as on disk, for every file and
// directory in the data directory (about 4,300 entries)
std::unordered_map<std::string, std::string> s_entries;

const char *const s_parts[] = { "dat", "gfx", "sfx", "txt", "vfx" };

// the data's names are ASCII
std::string toLower(std::string name)
{
    for (char &c : name)
        if (c >= 'A' && c <= 'Z')
            c = char(c - 'A' + 'a');
    return name;
}

} // namespace

std::string DataPath::m_directory;

void DataPath::set(const std::string &directory)
{
    m_directory = directory.empty() ? std::string() : cleanPath(directory);
    s_entries.clear();
    if (m_directory.empty())
        return;

    const std::filesystem::path root = toPath(m_directory);
    std::error_code error;
    for (std::filesystem::recursive_directory_iterator it(root, error), end; !error && it != end; it.increment(error)) {
        const std::string relative = it->path().lexically_relative(root).generic_u8string();
        s_entries.emplace(toLower(relative), relative);
    }
}

std::string DataPath::resolve(const std::string &name)
{
    std::string relative;
    for (const char *part : s_parts) {
        const std::string prefix(part);
        if (name.compare(0, prefix.size(), prefix) == 0 && name.size() > prefix.size() && (name[prefix.size()] == ':' || name[prefix.size()] == '/')) {
            relative = prefix + '/' + name.substr(prefix.size() + 1);
            break;
        }
    }
    if (relative.empty())
        return name;

    relative = cleanPath(relative);
    const auto it = s_entries.find(toLower(relative));
    return m_directory + '/' + (it != s_entries.end() ? it->second : relative);
}

std::string DataPath::cleanPath(const std::string &path)
{
    std::string normal = path;
    for (char &c : normal)
        if (c == '\\')
            c = '/';
    const bool absolute = !normal.empty() && normal[0] == '/';
    std::vector<std::string> parts;
    std::size_t start = 0;
    while (start <= normal.size()) {
        std::size_t end = normal.find('/', start);
        if (end == std::string::npos)
            end = normal.size();
        const std::string part = normal.substr(start, end - start);
        if (part == "..") {
            if (!parts.empty() && parts.back() != "..")
                parts.pop_back();
            else if (!absolute)
                parts.push_back(part);
        } else if (!part.empty() && part != ".") {
            parts.push_back(part);
        }
        start = end + 1;
    }
    std::string result = absolute ? "/" : "";
    for (std::size_t i = 0; i < parts.size(); i++)
        result += (i > 0 ? "/" : "") + parts[i];
    return result.empty() ? "." : result;
}

} // namespace util
