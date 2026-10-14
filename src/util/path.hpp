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

#ifndef UTIL_PATH_HPP
#define UTIL_PATH_HPP

#include <filesystem>
#include <string>
#include <vector>

// Paths are UTF-8 strings (util::String can't hold every character a path can), turned into the
// system's own form only when a file is opened: on Windows, a narrow path would be in the ANSI
// code page.
namespace util {

std::filesystem::path toPath(const std::string &path);
std::string fromPath(const std::filesystem::path &path);

// where the game keeps its saves and settings: Application Support/Vertigo/Vertigo on macOS
// (in CFFIXED_USER_HOME if that is set), AppData/Local/Vertigo/Vertigo on Windows and
// .local/share/Vertigo/Vertigo (XDG_DATA_HOME) elsewhere, where Qt kept them
std::string appDataLocation();

// a file or a directory
bool exists(const std::string &path);
// the part after the last '/'
std::string fileName(const std::string &path);
std::string joinPath(const std::string &directory, const std::string &name);
// with the directories leading to it
bool makeDirectory(const std::string &path);
// the names of the files in a directory, sorted
std::vector<std::string> files(const std::string &directory);

// all of a file, from an offset on; false and nothing if it can't be read
bool readFile(const std::string &path, std::vector<char> &data, std::size_t offset = 0);
std::vector<char> readFile(const std::string &path, std::size_t offset = 0);
bool writeFile(const std::string &path, const std::string &data);

// the lines of a text, each with its '\n' (the last one may have none)
std::vector<std::string> lines(const std::vector<char> &data);

} // namespace util

#endif // UTIL_PATH_HPP
