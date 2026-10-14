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

#include "soundsystem.h"
#include "samplemap.h"
#include "util/containers.hpp"
#include "openal.hpp"
#include <SDL3/SDL_platform_defines.h>
#if defined(SDL_PLATFORM_IOS)
#include "audiosession.hpp"
#endif
#include <cassert>


namespace sfx {


SoundSystem *SoundSystem::m_singleton = NULL;


struct SoundSystemPrivate
{
    ALCdevice* device;
    ALCcontext* context;
};


SoundSystem::SoundSystem(bool silent) :
    m_silent(silent)
{
    assert(m_singleton == NULL);
    m_singleton = this;

    d = new SoundSystemPrivate;
#if defined(SDL_PLATFORM_IOS)
    setUpAudioSession();
#endif
    d->device = alcOpenDevice(NULL);
    d->context = alcCreateContext(d->device, NULL);
    alcMakeContextCurrent(d->context);

    m_standardSounds.emplace(Click, new Sound("sfx:snd/desktop/click.pcm"));
    m_standardSounds.emplace(Woop, new Sound("sfx:snd/desktop/woop.pcm"));
    util::value(m_standardSounds, Woop)->setVolume(0.1f);
    m_standardSounds.emplace(FrameOpen, new Sound("sfx:snd/desktop/fopen.pcm"));
    util::value(m_standardSounds, FrameOpen)->setVolume(0.1f);
    m_standardSounds.emplace(FrameClose, new Sound("sfx:snd/desktop/fclose.pcm"));
    util::value(m_standardSounds, FrameClose)->setVolume(0.1f);

    m_standardSounds.emplace(NotebookShow, new Sound("sfx:snd/desktop/noteb1.pcm"));
    m_standardSounds.emplace(NotebookHide, new Sound("sfx:snd/desktop/noteb3.pcm"));
    m_standardSounds.emplace(EnComShow, new Sound("sfx:snd/desktop/noteb2.pcm"));
    m_standardSounds.emplace(NotebookBackground, new Sound("sfx:snd/desktop/noteback.pcm"));
    util::value(m_standardSounds, NotebookBackground)->setVolume(0.033f);
}


SoundSystem::~SoundSystem()
{
    // (over a copy, as stopping a sound releases its source)
    const std::map<std::uint32_t, Sound*> sources = m_sources;
    for (const auto &source : sources)
        if (source.second->isInstance())
            source.second->stop();

    util::deleteAll(m_standardSounds);
    SampleMap::reset();

    assert(m_sources.empty());

    alcMakeContextCurrent(NULL);
    alcDestroyContext(d->context);
    alcCloseDevice(d->device);
#if defined(SDL_PLATFORM_IOS)
    tearDownAudioSession();
#endif
    delete d;
    m_singleton = NULL;
}


Sound* SoundSystem::sound(StandardSound snd)
{
    Sound *sound = util::value(m_standardSounds, snd);
    assert(sound);
    return sound;
}


void SoundSystem::releaseUnused()
{
    // stop all Sounds with stopped sources
    // this implicitly frees the sources
    const std::map<std::uint32_t, Sound*> sources = m_sources;
    for (const auto &source : sources) {
        ALint state;
        alGetSourcei(source.first, AL_SOURCE_STATE, &state);
        if (state == AL_STOPPED)
            source.second->stop();
    }
}


std::uint32_t SoundSystem::acquire(Sound *sound)
{
    releaseUnused();
    if (m_sources.size() == 32)
        return 0;

    std::uint32_t source = 0;
    alGenSources(1, &source);
    if (source == 0)
        return 0;
    // attached to the listener unless the sound is given a position
    alSourcei(source, AL_SOURCE_RELATIVE, AL_TRUE);
    alSource3f(source, AL_POSITION, 0, 0, 0);
    m_sources[source] = sound;
    return source;
}


void SoundSystem::release(std::uint32_t source)
{
    alSourceStop(source);
    alDeleteSources(1, &source);
    m_sources.erase(source);
}


void SoundSystem::pauseAll()
{
    releaseUnused();

    const std::map<std::uint32_t, Sound*> sources = m_sources;
    for (const auto &source : sources)
        source.second->pause();
}


void SoundSystem::setListener(float x, float y, float z, float fx, float fy, float fz, float ux, float uy, float uz)
{
    const ALfloat orientation[6] = { fx, fy, fz, ux, uy, uz };
    alListener3f(AL_POSITION, x, y, z);
    alListenerfv(AL_ORIENTATION, orientation);
}


void SoundSystem::setBufferData(std::uint32_t buffer, bool stereo, const std::vector<char> &data, int rate)
{
    // (silence rather than a gain of 0, as the listener's gain left macOS's OpenAL audible; and
    // never a null pointer, also without data)
    const std::vector<char> silence(m_silent ? data.size() : 0, char(0x80));
    const std::vector<char> &samples = m_silent ? silence : data;
    alBufferData(buffer, stereo ? AL_FORMAT_STEREO8 : AL_FORMAT_MONO8, samples.empty() ? "" : samples.data(), ALsizei(samples.size()), rate);
}


void SoundSystem::interrupt(Interruption reason, bool interrupted)
{
    const bool wasInterrupted = m_interruptions != 0;
    // (iOS doesn't always tell when an interruption ends, but back from the background it has)
    m_interruptions = interrupted ? m_interruptions | reason : reason == Background ? 0 : m_interruptions & ~reason;
    if (wasInterrupted == (m_interruptions != 0))
        return;
    // the context stays current, so that sounds can still be loaded meanwhile; made current
    // anew, it plays again after an interruption
    if (m_interruptions != 0) {
        alcSuspendContext(d->context);
        return;
    }
#if defined(SDL_PLATFORM_IOS)
    activateAudioSession();
#endif
    alcMakeContextCurrent(NULL);
    alcMakeContextCurrent(d->context);
    alcProcessContext(d->context);
}


void SoundSystem::resumeAll()
{
    const std::map<std::uint32_t, Sound*> sources = m_sources;
    for (const auto &source : sources)
        source.second->resume();
}


} // namespace sfx
