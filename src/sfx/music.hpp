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

#ifndef SFX_MUSIC_HPP
#define SFX_MUSIC_HPP

#include "util/core.h"

#include <cstdint>
#include <fstream>
#include <string>

namespace sfx {

// The music of a mission: one of the CD's audio tracks as a WAV file of 16-bit samples, streamed
// from the disk round and round. Silent runs play none.
class Music : util::no_copy
{
public:
    Music();
    ~Music();

public:
    // false if the file isn't there or isn't such a WAV file
    bool play(const std::string &path, float volume);
    void stop();
    void pause(bool paused);
    bool isPlaying() const { return m_source != 0; }
    // keeps the queue filled, once a frame
    void update();

    // the original's "CD Musik" setting
    static bool isEnabled();
    static void setEnabled(bool enabled);

private:
    bool open(const std::string &path);
    bool fill(unsigned int buffer);

private:
    std::ifstream m_file;
    std::uint64_t m_dataStart = 0;
    std::uint64_t m_dataSize = 0;
    std::uint64_t m_position = 0;
    int m_rate = 0;
    int m_channels = 0;
    bool m_paused = false;
    unsigned int m_source = 0;
    unsigned int m_buffers[4] = {};
};

} // namespace sfx

#endif // SFX_MUSIC_HPP
