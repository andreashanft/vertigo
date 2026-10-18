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

#ifndef FIGHT_GUN_H
#define FIGHT_GUN_H


#include "util/string.hpp"
#include <vector>
#include "util/geometry3d.h"
#include "sfx/samplemap.h"


namespace fight {


class Scenario;
class ActiveObject;
enum class Effects;


class Gun
{
public:
    enum State { StateReady, StateRearming, StateFiring, StateJammed, StateEmpty, StateEnergyDelay };
    struct GunState
    {
        String longName;
        String shortName;
    };
    static GunState* GunStates;

public:
    // owner: the unit that fires, never hit by its own shots
    Gun(Scenario *scenario, int model, ActiveObject *owner = nullptr);

public:
    int model() const { return m_model; }
    float energyDemand() const { return m_energyDemand; }
    float range() const { return m_range; }
    float projectileSpeed() const { return m_projectileSpeed; }
    sfx::Sample sample() const { return m_sample; }

    float defect() const { return m_defect; }
    int ammo() const { return m_ammo; }
    // the rounds left as the displays count them: one round fires all the barrels in turn
    int rounds() const { return m_muzzles.empty() ? m_ammo : int((m_ammo + m_muzzles.size() - 1)/m_muzzles.size()); }
    int shots() const { return m_shots; }  // fired so far
    bool usesAmmo() const { return m_energyDemand == 0.0f; }
    const String& shortName() const { return m_shortName; }
    const String& longName() const { return m_longName; }
    String stateName() const { return GunStates ? GunStates[m_state].shortName : String(); }
    String stateText() const { return GunStates ? GunStates[m_state].longName : String(); }
    State state() const { return m_state; }

    void setDefect(float defect) { m_defect = defect; }
    void setAmmo(int ammo) { m_ammo = ammo; }
    void addMuzzle(const Vector3D &muzzle);
    void addMuzzles(const Vector3D &muzzle, int model);
    // the barrels a gun model fires in turn (appendix K): 1, 2, 1, 2, 4, 1 for the energy guns,
    // 2, 2, 2, 3, 4, 2, 2, 2 for the others
    static int barrels(int model);

public:
    bool update(float elapsedTime);
    void fire(const Vector3D &pos, const Vector3D &dir, const Vector3D &up, const Vector3D &right);

protected:
    Scenario *m_scenario;
    ActiveObject *m_owner;

    int m_model;
    String m_longName;
    String m_shortName;
    String m_imageName;

    float m_rearmingTime;
    float m_energyDemand;
    float m_defectDamping;
    float m_defectDelay;
    float m_range;
    float m_projectileSpeed;

    float m_defect;
    int m_ammo;
    State m_state;
    float m_stateTime;

    Effects m_projectile;
    std::vector<Vector3D> m_muzzles;
    size_t m_nextMuzzle = 0;  // the barrel the next pull fires from
    int m_shots = 0;
    sfx::Sample m_sample;
};


} // namespace fight


#endif // FIGHT_GUN_H
