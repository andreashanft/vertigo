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
#include <map>

namespace gfx {

namespace {

const char *const SettingKey = "Graphics/upscaled";
const char *const SourceFile = "source.txt";
// raised when what the set holds changes, so an older one is brought up to date: the copies it
// lacks are made, those no longer wanted removed (2: without the fonts and the terrain; 3: the
// pictures of text with the model that keeps letters whole)
const char *const SetVersion = "3";

bool g_loaded = false;
bool g_available = false;
std::string g_source;
std::map<std::uint64_t, std::string> g_files;

void load()
{
    if (g_loaded)
        return;
    g_loaded = true;
    g_files.clear();
    g_source.clear();
    const std::string directory = Upscaled::directory();
    const std::vector<char> source = util::readFile(util::joinPath(directory, SourceFile));
    g_source.assign(source.begin(), source.end());
    while (!g_source.empty() && std::isspace(static_cast<unsigned char>(g_source.back())))
        g_source.pop_back();
    for (const std::string &name : util::files(directory)) {
        std::uint64_t key = 0;
        if (Upscaled::parseName(name, key))
            g_files[key] = name;
    }
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
    return Image::fromData(reinterpret_cast<const std::uint8_t *>(data.data()), data.size());
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
