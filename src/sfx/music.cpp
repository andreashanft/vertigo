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

#include "music.hpp"
#include "soundsystem.h"
#include "util/path.hpp"
#include "util/settings.hpp"

#include "openal.hpp"

#include <algorithm>
#include <cstring>
#include <vector>

namespace sfx {

namespace {

// a buffer's bytes: about 0.37 s of CD audio, so that the four of them outlast a slow frame
const std::size_t BufferSize = 65536;

std::uint32_t le32(const char *bytes)
{
    const unsigned char *b = reinterpret_cast<const unsigned char *>(bytes);
    return std::uint32_t(b[0]) | std::uint32_t(b[1]) << 8 | std::uint32_t(b[2]) << 16 | std::uint32_t(b[3]) << 24;
}

} // namespace


Music::Music()
{
}


Music::~Music()
{
    stop();
}


bool Music::isEnabled()
{
    return util::Settings::boolValue("Sound/cdMusic", true);
}


void Music::setEnabled(bool enabled)
{
    util::Settings::setValue("Sound/cdMusic", enabled);
}


bool Music::play(const std::string &path, float volume)
{
    stop();
    if (SoundSystem::get()->isSilent() || !open(path))
        return false;

    alGenSources(1, &m_source);
    alGenBuffers(4, m_buffers);
    alSourcei(m_source, AL_SOURCE_RELATIVE, AL_TRUE);
    alSource3f(m_source, AL_POSITION, 0, 0, 0);
    alSourcef(m_source, AL_GAIN, volume);
    for (unsigned int buffer : m_buffers)
        if (fill(buffer))
            alSourceQueueBuffers(m_source, 1, &buffer);
    m_paused = false;
    alSourcePlay(m_source);
    return true;
}


void Music::stop()
{
    if (m_source != 0) {
        alSourceStop(m_source);
        alSourcei(m_source, AL_BUFFER, 0);
        alDeleteSources(1, &m_source);
        alDeleteBuffers(4, m_buffers);
        m_source = 0;
    }
    m_file.close();
}


void Music::pause(bool paused)
{
    if (m_source == 0 || paused == m_paused)
        return;
    m_paused = paused;
    if (paused)
        alSourcePause(m_source);
    else
        alSourcePlay(m_source);
}


void Music::update()
{
    if (m_source == 0 || m_paused)
        return;
    ALint processed = 0;
    alGetSourcei(m_source, AL_BUFFERS_PROCESSED, &processed);
    while (processed-- > 0) {
        ALuint buffer;
        alSourceUnqueueBuffers(m_source, 1, &buffer);
        if (fill(buffer))
            alSourceQueueBuffers(m_source, 1, &buffer);
    }
    // after a frame too slow for the queue the source has stopped
    ALint state;
    alGetSourcei(m_source, AL_SOURCE_STATE, &state);
    if (state != AL_PLAYING)
        alSourcePlay(m_source);
}


bool Music::open(const std::string &path)
{
    m_file.close();
    m_file.clear();
    m_file.open(util::toPath(path), std::ios::binary);
    char header[12];
    if (!m_file.read(header, 12) || std::memcmp(header, "RIFF", 4) != 0 || std::memcmp(header + 8, "WAVE", 4) != 0)
        return false;
    // the chunks: "fmt " with the format, "data" with the samples
    bool format = false;
    for (char chunk[8]; m_file.read(chunk, 8); ) {
        const std::uint32_t size = le32(chunk + 4);
        if (std::memcmp(chunk, "fmt ", 4) == 0 && size >= 16) {
            char fmt[16];
            m_file.read(fmt, 16);
            m_channels = fmt[2];
            m_rate = int(le32(fmt + 4));
            // PCM, mono or stereo, 16 bits
            format = fmt[0] == 1 && fmt[1] == 0 && (m_channels == 1 || m_channels == 2) && fmt[14] == 16 && m_rate > 0;
            m_file.seekg(size - 16 + size % 2, std::ios::cur);
        } else if (std::memcmp(chunk, "data", 4) == 0) {
            if (!format)
                return false;
            m_dataStart = std::uint64_t(m_file.tellg());
            m_dataSize = size / (2 * m_channels) * (2 * m_channels);
            m_position = 0;
            return m_dataSize > 0;
        } else {
            m_file.seekg(size + size % 2, std::ios::cur);
        }
    }
    return false;
}


// the next samples, from the start again at the end
bool Music::fill(unsigned int buffer)
{
    std::vector<char> data(BufferSize / (2 * m_channels) * (2 * m_channels));
    std::size_t filled = 0;
    while (filled < data.size()) {
        if (m_position >= m_dataSize)
            m_position = 0;
        const std::size_t bytes = std::size_t(std::min<std::uint64_t>(data.size() - filled, m_dataSize - m_position));
        m_file.clear();
        m_file.seekg(std::streamoff(m_dataStart + m_position));
        if (!m_file.read(data.data() + filled, std::streamsize(bytes)))
            return false;
        filled += bytes;
        m_position += bytes;
    }
    // (the samples are little-endian, as OpenAL wants them on every system the game runs on)
    alBufferData(buffer, m_channels == 2 ? AL_FORMAT_STEREO16 : AL_FORMAT_MONO16, data.data(), ALsizei(data.size()), m_rate);
    return true;
}

} // namespace sfx
