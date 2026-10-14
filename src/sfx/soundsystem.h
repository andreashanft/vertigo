/***************************************************************************
 *  Copyright (C) 2010  Philipp Nordhus                                    *
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

#ifndef SFX_SOUNDSYSTEM_H
#define SFX_SOUNDSYSTEM_H


#include "sound.h"
#include <map>
#include <cassert>
#include <vector>


namespace sfx {


class Sound;
struct SoundSystemPrivate;


enum StandardSound
{
    Click,
    Woop,
    FrameOpen,
    FrameClose,
    NotebookShow,
    NotebookHide,
    NotebookBackground,
    EnComShow,
    EnComHide = NotebookHide,
};


class SoundSystem
{
public:
    // silent: every sound and movie plays as long as ever, but silence (for automated runs)
    explicit SoundSystem(bool silent = false);
    ~SoundSystem();

public:
    Sound* sound(StandardSound);
    std::uint32_t acquire(Sound* sound);
    void release(std::uint32_t source);
    void pauseAll();
    void resumeAll();
    // a buffer's unsigned 8-bit samples, or as many silent ones
    void setBufferData(std::uint32_t buffer, bool stereo, const std::vector<char> &data, int rate);
    // where positional sounds are heard from: position, forward and up
    void setListener(float x, float y, float z, float fx, float fy, float fz, float ux, float uy, float uz);

public:
    static SoundSystem *get() { assert(m_singleton); return m_singleton; }

private:
    SoundSystem(const SoundSystem&) = delete;
    SoundSystem& operator=(const SoundSystem&) = delete;
    void releaseUnused();

private:
    static SoundSystem *m_singleton;
    SoundSystemPrivate *d;
    bool m_silent;
    std::map<StandardSound, Sound*> m_standardSounds;
    std::map<std::uint32_t, Sound*> m_sources;
};


} // namespace sfx


#endif // SFX_SOUNDSYSTEM_H
