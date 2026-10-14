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

#ifndef SFX_SOUND_H
#define SFX_SOUND_H


#include "util/string.hpp"


namespace sfx {


class Sound
{
public:
    Sound();
    Sound(Sound&& o);
    Sound(const String &file);
    ~Sound();

public:
    void stop();
    void play();
    void playLoop();
    void playInstance();
    void pause();
    void resume();
    void load(const String &file, int rate = 0);
    void load(const String &leftFile, const String &rightFile);
    void setVolume(float volume);
    void setRandomPitch(float randomPitch);
    void setPitch(float pitch);
    // makes the sound come from a point in the world (sounds are attached to the listener otherwise)
    void setPosition(float x, float y, float z);
    bool hasSource() const { return m_source != 0; }
    bool isInstance() const { return m_instance; }

private:
    friend class SampleMap;
    Sound(const Sound &o);
    QByteArray loadFile(const String &filename);
    bool acquire();

private:
    quint32 m_source;
    quint32 m_buffer;
    float m_volume;
    float m_randomPitch;
    bool m_instance;
};


} // namespace sfx


#endif // SFX_SOUND_H
