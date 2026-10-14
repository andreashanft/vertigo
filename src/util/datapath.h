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

#ifndef UTIL_DATAPATH_H
#define UTIL_DATAPATH_H

#include "string.hpp"
#include <string>

namespace util {

// The game data directory. The code names its files in lower case
// ("gfx:img/desktop/gui/cur_norm.img"), the CD has them in upper case
// (GFX/IMG/DESKTOP/GUI/CUR_NORM.IMG), so names are looked up ignoring case;
// not every file system does that itself (Linux, iPadOS). Paths are in UTF-8.
class DataPath
{
public:
    static void set(const std::string &directory);
    static const std::string& directory() { return m_directory; }

    // The path of a data file or directory, given as "gfx:img/x.img" or
    // "gfx/img/x.img" (the parts dat, gfx, sfx, txt and vfx). A name that is
    // missing from the data keeps its case; other names, such as absolute
    // paths, are returned unchanged.
    static std::string resolve(const std::string &name);
    static std::string resolve(const String &name) { return resolve(name.toUtf8()); }
    static std::string resolve(const char *name) { return resolve(std::string(name)); }

    // without "." and "..", doubled and trailing slashes, and with '/' for '\\'
    static std::string cleanPath(const std::string &path);

private:
    static std::string m_directory;
};

} // namespace util

#endif // UTIL_DATAPATH_H
