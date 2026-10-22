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

#include "upscaler.h"
#include "gfx/colortable.h"
#include "gfx/image.h"
#include "gfx/upscaled.h"
#include "txt/desfile.h"
#include "util/datapath.h"
#include "util/path.hpp"
#include "util/process.hpp"
#include "util/sha256.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cctype>
#include <filesystem>
#include <functional>
#include <iostream>
#include <map>
#include <set>
#include <vector>

namespace game {

namespace {

namespace fs = std::filesystem;

// The upscaler's official release (Real-ESRGAN 0.2.5.0, ncnn/Vulkan build of 2022-04-24) and the
// SHA-256 its download must have
struct Download
{
    const char *url;
    std::uint64_t size;
    const char *sha256;
};

#if defined(__APPLE__)
const Download ToolDownload = { "https://github.com/xinntao/Real-ESRGAN/releases/download/v0.2.5.0/realesrgan-ncnn-vulkan-20220424-macos.zip",
                                51817124, "e0ad05580abfeb25f8d8fb55aaf7bedf552c375b5b4d9bd3c8d59764d2cc333a" };
const char *const ToolName = "realesrgan-ncnn-vulkan";
#elif defined(_WIN32)
const Download ToolDownload = { "https://github.com/xinntao/Real-ESRGAN/releases/download/v0.2.5.0/realesrgan-ncnn-vulkan-20220424-windows.zip",
                                45474481, "abc02804e17982a3be33675e4d471e91ea374e65b70167abc09e31acb412802d" };
const char *const ToolName = "realesrgan-ncnn-vulkan.exe";
#else
const Download ToolDownload = { "https://github.com/xinntao/Real-ESRGAN/releases/download/v0.2.5.0/realesrgan-ncnn-vulkan-20220424-ubuntu.zip",
                                46931474, "e5aa6eb131234b87c0c51f82b89390f5e3e642b7b70f2b9bbe95b6a285a40c96" };
const char *const ToolName = "realesrgan-ncnn-vulkan";
#endif

// the general model for the pictures and textures; the one for animation keeps letters whole and
// the cursor's edges crisp, where the general one garbles and smears them
const char *const ModelGeneral = "realesrgan-x4plus";
const char *const ModelSharp = "realesr-animevideov3";

// what the upscaler gets at a time, so that a stopped run keeps what it has done
const std::size_t ChunkImages = 24;
const std::uint64_t ChunkPixels = 3000000;

std::string toolDirectory()
{
    return util::joinPath(util::appDataLocation(), "Real-ESRGAN");
}

std::string toolPath()
{
    return util::joinPath(toolDirectory(), ToolName);
}

std::string lower(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return std::tolower(c); });
    return text;
}

// "gfx/pic/room/abbk1.pcx" for each file under a part of the data, as the loaders take them
std::vector<std::string> dataFiles(const std::string &part)
{
    std::vector<std::string> names;
    const std::string root = util::DataPath::directory();
    std::error_code error;
    for (fs::recursive_directory_iterator it(util::toPath(util::DataPath::resolve(part)), error), end; !error && it != end; it.increment(error)) {
        if (!it->is_regular_file(error))
            continue;
        const std::string path = util::fromPath(it->path());
        if (path.size() > root.size() + 1)
            names.push_back(lower(path.substr(root.size() + 1)));
    }
    std::sort(names.begin(), names.end());
    return names;
}

bool endsWith(const std::string &text, const std::string &end)
{
    return text.size() >= end.size() && text.compare(text.size() - end.size(), end.size(), end) == 0;
}

bool startsWith(const std::string &text, const std::string &start)
{
    return text.compare(0, start.size(), start) == 0;
}

// an image to upscale, decoded again when its turn comes
struct Item
{
    std::uint64_t key;
    bool sharp;
    std::uint64_t pixels;
    std::function<gfx::Image()> decode;
};

// the item's copy, or null if it is missing or of the wrong size
gfx::Image upscaledCopy(const std::string &file, const gfx::Image &original)
{
    const std::vector<char> data = util::readFile(file);
    const gfx::Image copy = gfx::Image::fromData(reinterpret_cast<const std::uint8_t *>(data.data()), data.size());
    const int s = gfx::Upscaled::Scale;
    return copy.width() == original.width()*s && copy.height() == original.height()*s ? copy : gfx::Image();
}

// What gets upscaled, decoded as the game decodes it (each key as the texture made from it has):
// the full-screen pictures, the model and effect textures and the cursor. The fonts and the HUD's
// own art keep their pixels, as does the terrain: its tiles meet their
// neighbours at their edges' pixels, which the upscaler changes, and it trades their light
// patterns for gravel.
std::vector<Item> collect()
{
    std::map<std::uint64_t, Item> items;
    auto add = [&items](bool sharp, std::function<gfx::Image()> decode) {
        const gfx::Image image = decode();
        if (image.isNull() || image.width() < 8 || image.height() < 8)
            return;
        const std::uint64_t key = gfx::Upscaled::key(image);
        if (!items.count(key))
            items[key] = Item{ key, sharp, std::uint64_t(image.width())*image.height(), std::move(decode) };
    };

    // the effects' textures with their transparent colour (gfx::TextureManager)
    std::set<std::string> effects;
    for (const std::string &name : dataFiles("vfx:sobjects")) {
        if (!endsWith(name, ".des"))
            continue;
        for (const std::string &line : util::lines(util::readFile(util::DataPath::resolve(name)))) {
            const std::string text = lower(line);
            const std::size_t at = text.find("imgblockname");
            const std::size_t equals = text.find('=', at);
            if (at == std::string::npos || equals == std::string::npos)
                continue;
            std::string value = text.substr(equals + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t\r\n") + 1);
            effects.insert(value);
        }
    }
    // the models' and effects' textures (MOD_nnn; the terrain's are named after their surface)
    for (const std::string &name : dataFiles("vfx:texture")) {
        if (!startsWith(name, "vfx/texture/mod_") || !endsWith(name, ".imb"))
            continue;
        const std::string base = name.substr(0, name.size() - 4);
        if (!util::exists(util::DataPath::resolve(base + ".s16")))
            continue;
        const String image = String::fromUtf8(name.c_str()), shading = String::fromUtf8((base + ".s16").c_str());
        add(false, [image, shading]() { return gfx::Image::load(image, gfx::ColorTable(shading)); });
        if (effects.count(util::fileName(base)))
            add(false, [image, shading]() {
                gfx::ColorTable table(shading);
                table[0] = gfx::rgba(0, 0, 0, 0);
                return gfx::Image::load(image, table);
            });
    }

    // the pictures, as their screens load them (game/room.cpp, desktop.cpp, notebook.cpp,
    // briefing.cpp, departure.cpp, mainmenu.cpp, hud/hud.cpp); those that are mostly text with the
    // model that keeps letters whole: the sea and area maps (the notebook's and the briefing's),
    // the autopilot's destinations and the credits
    for (const std::string &name : dataFiles("gfx:pic")) {
        const String file = String::fromUtf8(name.c_str());
        if (endsWith(name, ".pcx")) {
            if (startsWith(name, "gfx/pic/room/") || startsWith(name, "gfx/pic/bground/"))
                add(false, [file]() { return gfx::Image::loadPCX(file).toRgb565(); });
            else
                add(startsWith(name, "gfx/pic/apilot/"), [file]() { return gfx::Image::loadPCX(file); });
        } else if (endsWith(name, ".r16")) {
            if (startsWith(name, "gfx/pic/area/"))
                add(true, [file]() { return gfx::Image::load(file, 304, 284, true); });
            else if (startsWith(name, "gfx/pic/notebook/"))
                add(true, [file]() { return gfx::Image::load(file, 304, 284); });
            else if (startsWith(name, "gfx/pic/title/credits"))
                add(true, [file]() { return gfx::Image::load(file, 640, 480); });
        }
    }
    for (const std::string &name : dataFiles("vfx:cockpit"))
        if (endsWith(name, "/cockpit.r16")) {
            const String file = String::fromUtf8(name.c_str());
            add(false, [file]() { return gfx::Image::load(file, 640, 480, true); });
        }

    // the cursor, whose edges the model for animation keeps crisp
    add(true, []() {
        return gfx::Image::loadCursor("gfx:img/desktop/gui/cur_norm.img", gfx::ColorTable("gfx:pal/gui/cursor.pal")).convertToFormat(gfx::Image::RGBA8888);
    });

    std::vector<Item> result;
    for (auto &entry : items)
        result.push_back(std::move(entry.second));
    return result;
}

// as PNG where it has transparency, else as JPEG, a quarter of the size and the difference
// invisible at this quality
bool keep(const gfx::Image &image, const std::string &path)
{
    const gfx::Image rgba = image.convertToFormat(gfx::Image::RGBA8888);
    const std::uint8_t *pixel = rgba.constBits();
    for (std::size_t i = 3; i < std::size_t(rgba.bytesPerLine())*rgba.height(); i += 4)
        if (pixel[i] != 255)
            return rgba.save(path + ".png");
    return rgba.saveJpeg(path + ".jpg", 92);
}


void removeAll(const std::string &path)
{
    std::error_code error;
    fs::remove_all(util::toPath(path), error);
}

} // namespace


Upscaler& Upscaler::get()
{
    static Upscaler upscaler;
    return upscaler;
}


Upscaler::~Upscaler()
{
    stop();
}


bool Upscaler::isSupported()
{
    return util::Process::isSupported() && (hasTool() || ToolDownload.sha256[0] != '\0');
}


bool Upscaler::hasTool()
{
    return util::exists(toolPath()) && util::exists(util::joinPath(toolDirectory(), std::string("models/") + ModelGeneral + ".bin"))
        && util::exists(util::joinPath(toolDirectory(), std::string("models/") + ModelSharp + "-x4.bin"));
}


int Upscaler::downloadMegabytes()
{
    return int((ToolDownload.size + 999999)/1000000);
}


void Upscaler::start()
{
    if (isRunning())
        return;
    if (m_thread.joinable())
        m_thread.join();
    m_stop = false;
    m_permille = 0;
    m_state = State::Running;
    m_thread = std::thread(&Upscaler::run, this);
}


void Upscaler::stop()
{
    m_stop = true;
    if (m_thread.joinable())
        m_thread.join();
    if (m_state == State::Running)
        m_state = State::Idle;
}


std::string Upscaler::error() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_error;
}


bool Upscaler::takeFinished()
{
    return m_finished.exchange(false);
}


void Upscaler::fail(const std::string &error)
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_error = error;
    }
    std::cerr << "Upscaling: " << error << std::endl;
    m_state = State::Failed;
}


void Upscaler::run()
{
    if (!hasTool() && !fetchTool())
        return;
    if (m_stop) {
        m_state = State::Idle;
        return;
    }
    if (upscale()) {
        m_permille = 1000;
        m_finished = true;
        m_state = State::Finished;
    }
}


// waits for a program, false if stopped meanwhile (then it is ended)
static bool finish(util::Process &process, const std::atomic<bool> &stop, const std::function<void()> &progress = {})
{
    while (process.isRunning()) {
        if (stop) {
            process.kill();
            return false;
        }
        if (progress)
            progress();
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    return true;
}


// curl fetches the release (on macOS, Windows and Linux alike), the system unpacks it; it counts
// as the first 5 percent
bool Upscaler::fetchTool()
{
    const std::string directory = toolDirectory();
    const std::string archive = util::joinPath(directory, "download.zip");
    removeAll(directory);
    if (!util::makeDirectory(directory)) {
        fail("Cannot write to " + directory + ".");
        return false;
    }
    util::Process process;
    if (!process.start({ "curl", "-L", "--fail", "--silent", "--show-error", "-o", archive, ToolDownload.url })) {
        fail("Cannot start curl to download the upscaler.");
        return false;
    }
    const bool done = finish(process, m_stop, [this, &archive]() {
        std::error_code error;
        const std::uint64_t size = fs::file_size(util::toPath(archive), error);
        if (!error)
            m_permille = int(std::min<std::uint64_t>(50, 50*size/ToolDownload.size));
    });
    if (!done) {
        removeAll(directory);
        m_state = State::Idle;
        return false;
    }
    if (process.exitCode() != 0) {
        removeAll(directory);
        fail("The upscaler couldn't be downloaded. Is the computer online?");
        return false;
    }
    if (util::Sha256::file(archive) != ToolDownload.sha256) {
        removeAll(directory);
        fail("The downloaded upscaler isn't the expected file (its checksum differs).");
        return false;
    }
#if defined(__APPLE__)
    const std::vector<std::string> unpack = { "ditto", "-x", "-k", archive, directory };
#elif defined(_WIN32)
    const std::vector<std::string> unpack = { "tar", "-xf", archive, "-C", directory };
#else
    const std::vector<std::string> unpack = { "unzip", "-o", "-q", archive, "-d", directory };
#endif
    if (!process.start(unpack) || !finish(process, m_stop) || process.exitCode() != 0 || !util::exists(toolPath())) {
        removeAll(directory);
        if (m_stop)
            m_state = State::Idle;
        else
            fail("The downloaded upscaler couldn't be unpacked.");
        return false;
    }
    std::error_code error;
    fs::remove(util::toPath(archive), error);
    fs::permissions(util::toPath(toolPath()), fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec, fs::perm_options::add, error);
    return true;
}


bool Upscaler::upscale()
{
    const std::string directory = gfx::Upscaled::directory();
    const std::string work = util::joinPath(directory, "work");
    const std::string fingerprint = gfx::Upscaled::fingerprint();
    if (!util::makeDirectory(directory)) {
        fail("Cannot write to " + directory + ".");
        return false;
    }

    const std::vector<Item> items = collect();
    std::vector<const Item *> todo[2];
    std::uint64_t total = 0;
    std::set<std::uint64_t> made;
    for (const std::string &file : util::files(directory)) {
        std::uint64_t key = 0;
        if (gfx::Upscaled::parseName(file, key))
            made.insert(key);
    }
    std::set<std::uint64_t> wanted;
    for (const Item &item : items) {
        wanted.insert(item.key);
        if (!made.count(item.key)) {
            todo[item.sharp].push_back(&item);
            total += item.pixels;
        }
    }
    // what a copy of older game data had and this one doesn't
    for (const std::string &file : util::files(directory)) {
        std::uint64_t key = 0;
        if (gfx::Upscaled::parseName(file, key) && !wanted.count(key))
            removeAll(util::joinPath(directory, file));
    }

    const int start = m_permille;
    std::uint64_t done = 0;
    auto report = [&](std::uint64_t pixels) {
        if (total > 0)
            m_permille = start + int((1000 - start)*std::min<std::uint64_t>(pixels, total)/total);
    };
    util::Process process;
    for (int sharp = 0; sharp < 2; sharp++) {
        const std::vector<const Item *> &list = todo[sharp];
        for (std::size_t first = 0; first < list.size();) {
            // a chunk: its pieces into work/in, upscaled into work/out
            const std::string in = util::joinPath(work, "in"), out = util::joinPath(work, "out");
            removeAll(work);
            if (!util::makeDirectory(in) || !util::makeDirectory(out)) {
                fail("Cannot write to " + work + ".");
                return false;
            }
            std::size_t last = first;
            std::uint64_t pixels = 0;
            std::vector<gfx::Image> originals;
            for (; last < list.size() && last - first < ChunkImages && (pixels < ChunkPixels || last == first); last++) {
                const Item &item = *list[last];
                originals.push_back(item.decode().convertToFormat(gfx::Image::RGBA8888));
                originals.back().save(util::joinPath(in, gfx::Upscaled::keyName(item.key) + ".png"));
                pixels += item.pixels;
            }
            const std::string model = sharp ? ModelSharp : ModelGeneral;
            if (!process.start({ toolPath(), "-i", in, "-o", out, "-n", model, "-s", "4", "-f", "png", "-m", util::joinPath(toolDirectory(), "models") })) {
                fail("Cannot start the upscaler.");
                return false;
            }
            const std::size_t inputs = util::files(in).size();
            const bool finished = finish(process, m_stop, [&]() { report(done + pixels*util::files(out).size()/std::max<std::size_t>(1, inputs)); });
            if (!finished) {
                removeAll(work);
                m_state = State::Idle;
                return false;
            }
            for (std::size_t i = first; i < last; i++) {
                const Item &item = *list[i];
                const gfx::Image copy = upscaledCopy(util::joinPath(out, gfx::Upscaled::keyName(item.key) + ".png"), originals[i - first]);
                if (copy.isNull() || !keep(copy, util::joinPath(directory, gfx::Upscaled::keyName(item.key)))) {
                    removeAll(work);
                    fail(process.exitCode() != 0 ? "The upscaler stopped with an error. Does the computer's graphics support Vulkan?"
                                                 : "An upscaled picture couldn't be kept in " + directory + ".");
                    return false;
                }
            }
            done += pixels;
            report(done);
            first = last;
        }
    }
    removeAll(work);
    if (!util::writeFile(util::joinPath(directory, "source.txt"), fingerprint + "\n")) {
        fail("Cannot write to " + directory + ".");
        return false;
    }
    return true;
}

} // namespace game
