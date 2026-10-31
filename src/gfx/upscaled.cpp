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

#include "upscaled.h"
#include "util/datapath.h"
#include "util/path.hpp"
#include "util/settings.hpp"

#include <SDL3/SDL_filesystem.h>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <list>
#include <map>
#include <set>

namespace gfx {

namespace {

const char *const SettingKey = "Graphics/upscaled";
const char *const SourceFile = "source.txt";
// raised when what the set holds changes, so an older one is brought up to date: the copies it
// lacks are made, those no longer wanted removed (2: without the fonts and the terrain; 3: the
// pictures of text with the model that keeps letters whole; 4: the depot's pictures, icons and
// movies)
const char *const SetVersion = "4";
// what findRepeated() keeps decoded at most
const std::size_t RepeatedBytes = std::size_t(256) << 20;

bool g_loaded = false;
bool g_available = false;
std::string g_source;
std::map<std::uint64_t, std::string> g_files;
// the copies with their transparency in alphaName()
std::set<std::uint64_t> g_alpha;
// the copies findRepeated() keeps, the one used last first
std::list<std::pair<std::uint64_t, Image>> g_repeated;
std::size_t g_repeatedBytes = 0;

void load()
{
    if (g_loaded)
        return;
    g_loaded = true;
    g_files.clear();
    g_alpha.clear();
    g_source.clear();
    const std::string directory = Upscaled::directory();
    const std::vector<char> source = util::readFile(util::joinPath(directory, SourceFile));
    g_source.assign(source.begin(), source.end());
    while (!g_source.empty() && std::isspace(static_cast<unsigned char>(g_source.back())))
        g_source.pop_back();
    std::set<std::string> names;
    for (const std::string &name : util::files(directory)) {
        names.insert(name);
        std::uint64_t key = 0;
        if (Upscaled::parseName(name, key))
            g_files[key] = name;
    }
    for (const auto &file : g_files)
        if (names.count(Upscaled::alphaName(file.first)))
            g_alpha.insert(file.first);
    g_available = !g_files.empty() && g_source.compare(0, std::string(SetVersion).size() + 1, std::string(SetVersion) + "-") == 0;
}

} // namespace


std::string Upscaled::directory()
{
#if defined(SDL_PLATFORM_IOS)
    // where the player copies it, next to the game data
    const char *documents = SDL_GetUserFolder(SDL_FOLDER_DOCUMENTS);
    return util::joinPath(util::DataPath::cleanPath(documents ? documents : "."), "Game Data Upscaled");
#else
    return util::joinPath(util::appDataLocation(), "Game Data Upscaled");
#endif
}


bool Upscaled::isEnabled()
{
    return util::Settings::boolValue(SettingKey, false);
}


void Upscaled::setEnabled(bool enabled)
{
    util::Settings::setValue(SettingKey, enabled);
}


bool Upscaled::isAvailable()
{
    load();
    return g_available;
}


std::string Upscaled::source()
{
    load();
    return g_source;
}


bool Upscaled::isOutdated()
{
    load();
    return !g_source.empty() && !g_available;
}


void Upscaled::reload()
{
    g_loaded = false;
    g_repeated.clear();
    g_repeatedBytes = 0;
}


bool Upscaled::isActive()
{
    return isEnabled() && isAvailable();
}


// FNV-1a over the size and the pixels as RGBA
std::uint64_t Upscaled::key(const Image &image)
{
    const Image rgba = image.format() == Image::RGBA8888 ? image : image.convertToFormat(Image::RGBA8888);
    std::uint64_t hash = 14695981039346656037ull;
    auto add = [&hash](const std::uint8_t *data, std::size_t size) {
        for (std::size_t i = 0; i < size; i++) {
            hash ^= data[i];
            hash *= 1099511628211ull;
        }
    };
    const std::int32_t size[2] = { rgba.width(), rgba.height() };
    add(reinterpret_cast<const std::uint8_t *>(size), sizeof(size));
    add(rgba.constBits(), std::size_t(rgba.bytesPerLine())*rgba.height());
    return hash;
}


std::string Upscaled::keyName(std::uint64_t key)
{
    char name[17];
    std::snprintf(name, sizeof(name), "%016llx", static_cast<unsigned long long>(key));
    return name;
}


std::string Upscaled::alphaName(std::uint64_t key)
{
    return keyName(key) + "-alpha.png";
}


bool Upscaled::parseName(const std::string &name, std::uint64_t &key)
{
    if (name.size() != 20 || (name.compare(16, 4, ".png") != 0 && name.compare(16, 4, ".jpg") != 0))
        return false;
    unsigned long long value = 0;
    if (std::sscanf(name.c_str(), "%16llx", &value) != 1)
        return false;
    key = value;
    return true;
}


Image Upscaled::find(std::uint64_t key)
{
    if (!isActive())
        return Image();
    const auto it = g_files.find(key);
    if (it == g_files.end())
        return Image();
    const std::vector<char> data = util::readFile(util::joinPath(directory(), it->second));
    Image copy = Image::fromData(reinterpret_cast<const std::uint8_t *>(data.data()), data.size());
    if (copy.isNull() || !g_alpha.count(key))
        return copy;
    // the transparency, its grey as the red
    const std::vector<char> alphaData = util::readFile(util::joinPath(directory(), alphaName(key)));
    const Image alpha = Image::fromData(reinterpret_cast<const std::uint8_t *>(alphaData.data()), alphaData.size());
    if (alpha.width() != copy.width() || alpha.height() != copy.height() || alpha.format() != Image::RGB888)
        return copy;
    copy = copy.convertToFormat(Image::RGBA8888);
    for (int y = 0; y < copy.height(); y++) {
        std::uint8_t *pixel = copy.scanLine(y);
        const std::uint8_t *grey = alpha.constScanLine(y);
        for (int x = 0; x < copy.width(); x++)
            pixel[4*x + 3] = grey[3*x];
    }
    return copy;
}


Image Upscaled::findRepeated(std::uint64_t key)
{
    for (auto it = g_repeated.begin(); it != g_repeated.end(); ++it)
        if (it->first == key) {
            g_repeated.splice(g_repeated.begin(), g_repeated, it);
            return it->second;
        }
    const Image copy = find(key);
    if (copy.isNull())
        return copy;
    g_repeated.emplace_front(key, copy);
    g_repeatedBytes += std::size_t(copy.bytesPerLine())*copy.height();
    while (g_repeatedBytes > RepeatedBytes && g_repeated.size() > 1) {
        g_repeatedBytes -= std::size_t(g_repeated.back().second.bytesPerLine())*g_repeated.back().second.height();
        g_repeated.pop_back();
    }
    return copy;
}


// the sizes of the files the set is made from, named relative to the data directory in lower case
std::string Upscaled::fingerprint()
{
    namespace fs = std::filesystem;
    const std::string root = util::DataPath::directory();
    std::vector<std::string> entries;
    std::error_code error;
    for (const char *part : { "gfx", "vfx" }) {
        const std::string directory = util::DataPath::resolve(std::string(part) + ":");
        for (fs::recursive_directory_iterator it(util::toPath(directory), error), end; !error && it != end; it.increment(error)) {
            if (!it->is_regular_file(error))
                continue;
            std::string name = util::fromPath(it->path());
            std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return std::tolower(c); });
            const std::string extension = name.size() > 4 ? name.substr(name.size() - 4) : std::string();
            if (extension != ".pcx" && extension != ".r16" && extension != ".imb" && extension != ".s16" && extension != ".fnt"
                && extension != ".img" && extension != ".pal" && extension != ".des")
                continue;
            entries.push_back(name.substr(std::min(name.size(), root.size())) + " " + std::to_string(it->file_size(error)));
        }
    }
    std::sort(entries.begin(), entries.end());
    std::uint64_t hash = 14695981039346656037ull;
    for (const std::string &entry : entries)
        for (char c : entry + "\n") {
            hash ^= static_cast<unsigned char>(c);
            hash *= 1099511628211ull;
        }
    return std::string(SetVersion) + "-" + keyName(hash);
}

} // namespace gfx
