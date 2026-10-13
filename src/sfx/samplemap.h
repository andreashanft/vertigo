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

#ifndef SFX_SAMPLEMAP_H
#define SFX_SAMPLEMAP_H


#include "sound.h"
#include <vector>


namespace sfx {


enum class Sample
{
    Sample0,
    Explosion_Big,
    Hit_Metal,
    Shoot_Vendetta,
    Engine_Torpedo,
    Sample5,
    Threat,
    LockOn,
    Error,
    TorpedoLockOn,
    Alert,
    Engine,
    Shoot_MaximumImpact,
    Shoot_PlasmaGun,
    Torpedo_Launch,
    Explosion_Ground,
    Explosion_Small,
    Explosion_EMS,
    Sample18,
    Sample19,
    Sample20,
    Engine_Turret,
    Radio,
    Engine_Enemy1,
    Engine_Enemy2,
    Shoot_DoomMortar,
    Shoot_PlasmaDeath,
    Boat_Collision,
    Sample28,
    Sample29,
    Sample30,
    Engine_Startup,
    Engine_Shutdown,
    Sample33,
    Engine_Biont,
    Engine_BigShip,
    Sample36,
    Sample37,
    Sample38,
    Sample39,
    Sample40,
    Sample41,
    Sample42,
    Sample43,
    Sample44,
    Sample45,
    Sample46,
    Sample47,
    Sample48,
    Sample49,
    Sample50,
    Sample51,
    Sample52,
    Sample53,
    Sample54,
    Sample55,
    Sample56,
    Sample57,
    Sample58,
    Sample59,
    Sample60,
    Sample61,
    Sample62,
    Sample63,
    SonarActivated,
    SonarDeactivated,
    Sample66,
    Sample67,
    TargetIdentified,
    TargetDestroyed,
    Sample70,
    TargetLost,
    Sample72,
    Sample73,
    Sample74,
    Sample75,
    Sample76,
    Sample77,
    MissionAccomplished,
    Autopilot,
    NavPoint,
    Sample81,
    Sample82,
    MissionFailed,
};


class SampleMap
{
public:
    static void load();
    static void reset();
    static Sound& get(Sample sample) { return m_samples[static_cast<int>(sample)]; }
    // plays a copy of the sample at a point in the world
    static void playAt(Sample sample, float x, float y, float z);
    // a looping copy at a point, for engines; stop() deletes it; nullptr if no source is free
    static Sound* loopAt(Sample sample, float x, float y, float z);

private:
    static std::vector<Sound> m_samples;
};


} // namespace sfx


#endif // SFX_SAMPLEMAP_H
