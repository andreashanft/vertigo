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

#include "gamedata.h"

#include "txt/stringtable.h"
#include "util/cdimage.hpp"
#include "util/datapath.h"
#include "util/path.hpp"

#include <SDL3/SDL_platform_defines.h>
#if defined(SDL_PLATFORM_IOS)
#include <CoreFoundation/CoreFoundation.h>
#endif

#include <algorithm>
#include <unordered_map>

namespace game {

namespace {

const char *const s_parts[] = { "dat", "gfx", "sfx", "txt", "vfx" };

// the data's names are ASCII
std::string toLower(std::string name)
{
    for (char &c : name)
        if (c >= 'A' && c <= 'Z')
            c = char(c - 'A' + 'a');
    return name;
}


std::string toUpper(std::string name)
{
    for (char &c : name)
        if (c >= 'a' && c <= 'z')
            c = char(c - 'a' + 'A');
    return name;
}


// where the CD has DAT, GFX, SFX, TXT and VFX: "SF/", or "" at its root; false if it hasn't
bool dataRoot(const std::vector<util::CdImage::File> &files, std::string &root)
{
    // the directories with files, at any depth: "sf/", "sf/gfx/", …
    std::unordered_map<std::string, std::string> directories;
    for (const util::CdImage::File &file : files)
        for (std::size_t slash = file.path.find('/'); slash != std::string::npos; slash = file.path.find('/', slash + 1))
            directories.emplace(toLower(file.path.substr(0, slash + 1)), file.path.substr(0, slash + 1));
    std::vector<std::string> candidates = { std::string() };
    for (const auto &directory : directories)
        if (directory.first.find('/') + 1 == directory.first.size())
            candidates.push_back(directory.second);
    std::sort(candidates.begin(), candidates.end());
    for (const std::string &candidate : candidates) {
        bool all = true;
        for (const char *part : s_parts)
            all = all && directories.count(toLower(candidate) + part + "/");
        if (all) {
            root = candidate;
            return true;
        }
    }
    return false;
}


// "GFX/…" and the like, in any case
bool inParts(const std::string &name)
{
    for (const char *part : s_parts)
        if (toLower(name.substr(0, 4)) == std::string(part) + "/")
            return true;
    return false;
}


std::string megabytes(std::uintmax_t bytes)
{
    return std::to_string((bytes + 999999) / 1000000) + " MB";
}

} // namespace


std::string GameData::missing()
{
    static const char *const files[] = { "txt:deeptext.des", "txt:fightext.des", "dat:story/ch1.des", "vfx:scenario",
                                         "gfx:pal/gui/cursor.pal", "gfx:img/desktop/gui/cur_norm.img", "gfx:mvi/film/d02.mvi",
                                         "gfx:pic/title/title.pcx", "sfx:snd/bground/b02.pcl" };
    for (const char *file : files)
        if (!util::exists(util::DataPath::resolve(file)))
            return std::string(file).replace(3, 1, "/");
    return txt::StringTable::load() ? std::string() : std::string("the text of txt/deeptext.des");
}


std::string GameData::findImage(const std::string &directory)
{
    for (const std::string &name : util::files(directory)) {
        const std::string extension = toLower(name.substr(std::min(name.size(), name.rfind('.'))));
        util::CdImage image;
        std::string root;
        if ((extension == ".gog" || extension == ".iso" || extension == ".bin") && image.open(util::joinPath(directory, name)) && dataRoot(image.files(), root))
            return util::joinPath(directory, name);
    }
    return std::string();
}


bool GameData::import(const std::string &imagePath, const std::string &target, std::string &error)
{
    util::CdImage image;
    std::string root;
    if (!image.open(imagePath) || !dataRoot(image.files(), root)) {
        error = imagePath + " isn't a CD image of the game.";
        return false;
    }

    // the CD's files, in its order (to read the image from start to end), then those installed;
    // all in upper case, as the CD has them (and DataPath finds them in any case)
    struct Copy
    {
        std::string name;
        const util::CdImage::File *file;
        std::string installed;
        std::uintmax_t size;
    };
    std::vector<Copy> copies;
    std::unordered_map<std::string, std::size_t> index;
    for (const util::CdImage::File &file : image.files()) {
        const std::string name = toUpper(file.path.substr(std::min(file.path.size(), root.size())));
        if (file.path.compare(0, root.size(), root) == 0 && inParts(name)) {
            index[name] = copies.size();
            copies.push_back({ name, &file, std::string(), file.size });
        }
    }
    const std::filesystem::path installation = util::toPath(imagePath).parent_path();
    std::error_code ec;
    for (std::filesystem::recursive_directory_iterator it(installation, ec), end; !ec && it != end; it.increment(ec)) {
        const std::string name = toUpper(it->path().lexically_relative(installation).generic_u8string());
        if (!inParts(name + "/")) {
            it.disable_recursion_pending();
            continue;
        }
        if (!it->is_regular_file(ec))
            continue;
        const Copy copy = { name, nullptr, util::fromPath(it->path()), it->file_size(ec) };
        const auto found = index.find(name);
        if (found != index.end()) {
            copies[found->second] = copy;
        } else {
            index[name] = copies.size();
            copies.push_back(copy);
        }
    }
    if (ec) {
        error = "Cannot read " + util::fromPath(installation) + ": " + ec.message();
        return false;
    }

    std::uintmax_t total = 0;
    for (const Copy &copy : copies)
        total += copy.size;
    const std::filesystem::path targetPath = util::toPath(target);
    const std::filesystem::path parent = targetPath.parent_path();
    util::makeDirectory(util::fromPath(parent));
    const std::filesystem::space_info space = std::filesystem::space(parent, ec);
    if (!ec && space.available < total + 20000000) {
        error = "The game data takes " + megabytes(total) + ", but only " + megabytes(space.available) + " are free.";
        return false;
    }

    // into a hidden directory first, so that an import cut short leaves no data that looks
    // complete
    const std::filesystem::path part = parent / util::toPath("." + util::fileName(target) + ".part");
    std::filesystem::remove_all(part, ec);
    for (const Copy &copy : copies) {
        const std::filesystem::path path = part / util::toPath(copy.name);
        std::filesystem::create_directories(path.parent_path(), ec);
        const bool copied = copy.file ? image.copy(*copy.file, util::fromPath(path))
                                      : std::filesystem::copy_file(util::toPath(copy.installed), path, std::filesystem::copy_options::overwrite_existing, ec);
        if (!copied) {
            error = "Cannot copy " + (copy.file ? copy.file->path + " from the CD image" : copy.installed) + " to " + util::fromPath(path) + ".";
            std::filesystem::remove_all(part, ec);
            return false;
        }
    }
    std::filesystem::rename(part, targetPath, ec);
    if (ec) {
        error = "Cannot rename " + util::fromPath(part) + " to " + target + ": " + ec.message();
        std::filesystem::remove_all(part, ec);
        return false;
    }

#if defined(SDL_PLATFORM_IOS)
    // not in the iPad's backups: it can be made again from the game
    if (CFURLRef url = CFURLCreateFromFileSystemRepresentation(nullptr, reinterpret_cast<const UInt8 *>(target.c_str()), CFIndex(target.size()), true)) {
        CFURLSetResourcePropertyForKey(url, kCFURLIsExcludedFromBackupKey, kCFBooleanTrue, nullptr);
        CFRelease(url);
    }
#endif
    return true;
}

} // namespace game
