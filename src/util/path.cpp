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

#include "path.hpp"

#include <algorithm>
#include <fstream>
#include <system_error>

namespace util {


std::filesystem::path toPath(const std::string &path)
{
    return std::filesystem::u8path(path);
}


std::string fromPath(const std::filesystem::path &path)
{
    return path.u8string();
}


bool exists(const std::string &path)
{
    std::error_code error;
    return !path.empty() && std::filesystem::exists(toPath(path), error);
}


std::string fileName(const std::string &path)
{
    const std::size_t slash = path.find_last_of('/');
    return slash == std::string::npos ? path : path.substr(slash + 1);
}


std::string joinPath(const std::string &directory, const std::string &name)
{
    if (directory.empty() || directory.back() == '/')
        return directory + name;
    return directory + '/' + name;
}


bool makeDirectory(const std::string &path)
{
    std::error_code error;
    std::filesystem::create_directories(toPath(path), error);
    return std::filesystem::is_directory(toPath(path), error);
}


std::vector<std::string> files(const std::string &directory)
{
    std::vector<std::string> names;
    std::error_code error;
    for (std::filesystem::directory_iterator it(toPath(directory), error), end; !error && it != end; it.increment(error))
        if (it->is_regular_file(error))
            names.push_back(fromPath(it->path().filename()));
    std::sort(names.begin(), names.end());
    return names;
}


bool readFile(const std::string &path, std::vector<char> &data, std::size_t offset)
{
    data.clear();
    std::ifstream file(toPath(path), std::ios::binary);
    if (!file)
        return false;
    file.seekg(0, std::ios::end);
    const std::streamoff size = file.tellg();
    if (size < 0)
        return false;
    if (std::size_t(size) <= offset)
        return true;
    data.resize(std::size_t(size) - offset);
    file.seekg(std::streamoff(offset));
    file.read(data.data(), std::streamsize(data.size()));
    data.resize(std::size_t(file.gcount()));
    return true;
}


std::vector<char> readFile(const std::string &path, std::size_t offset)
{
    std::vector<char> data;
    readFile(path, data, offset);
    return data;
}


bool writeFile(const std::string &path, const std::string &data)
{
    std::ofstream file(toPath(path), std::ios::binary | std::ios::trunc);
    file.write(data.data(), std::streamsize(data.size()));
    file.close();
    return !file.fail();
}


std::vector<std::string> lines(const std::vector<char> &data)
{
    std::vector<std::string> result;
    std::size_t start = 0;
    while (start < data.size()) {
        const auto newline = std::find(data.begin() + start, data.end(), '\n');
        const std::size_t end = newline == data.end() ? data.size() : std::size_t(newline - data.begin()) + 1;
        result.emplace_back(data.data() + start, end - start);
        start = end;
    }
    return result;
}


} // namespace util
