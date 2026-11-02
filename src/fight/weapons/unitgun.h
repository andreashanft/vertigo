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

#ifndef FIGHT_UNITGUN_H
#define FIGHT_UNITGUN_H


#include "gun.h"
#include <vector>
#include "util/string.hpp"


namespace txt { class DesFile; }


namespace fight {


class ActiveObject;


// The gun of an enemy unit: a Gun with its mountings in the unit's coordinates (metres),
// fired in the rhythm of the unit's [VolleyGun] (active and pause phases) while the trigger is pulled.
class UnitGun
{
public:
    struct Mount
    {
        int model = -1;                // [StaticGun] Model, -1 if the file has none
        int ammunition = -1;
        std::vector<Vector3D> muzzles; // model units
    };

public:
    UnitGun(Scenario *scenario, ActiveObject *owner, int model, const std::vector<Vector3D> &muzzles, txt::DesFile *volleyFile = nullptr);

    // [StaticGun] of an object file
    static Mount readMount(txt::DesFile &file);
    // gun models 0-13 exist; 14 is "No Gun"
    static bool isGun(int model) { return model >= 0 && model <= 13; }

public:
    float range() const { return m_gun.range(); }
    float projectileSpeed() const { return m_gun.projectileSpeed(); }
    const String& shortName() const { return m_gun.shortName(); }
    void setAmmunition(int ammunition) { m_gun.setAmmo(ammunition); }
    // how many shots it has fired
    int shots() const { return m_gun.shots(); }
    // most units fire before their gun counts its rearming time down, so on the frame after it is
    // ready again; a tank's fixed gun counts down first and fires on the frame it is ready
    // (docs/ROADMAP.md, appendix K)
    void setCountFirst(bool countFirst) { m_countFirst = countFirst; }
    // the volley starts again at its first phase whenever the trigger is released (boats, whose
    // target has left the aim; docs/ROADMAP.md, appendix J)
    void setRestartOnRelease(bool restart) { m_restartOnRelease = restart; }
    void update(float elapsedTime, bool trigger, const Vector3D &position, const Vector3D &dir, const Vector3D &up, const Vector3D &right);

private:
    struct Phase
    {
        bool active;
        float duration;
    };

    Gun m_gun;
    std::vector<Phase> m_volley; // empty: fire whenever the gun is ready
    int m_phase;
    float m_phaseTime;
    bool m_countFirst = false;
    bool m_restartOnRelease = false;
};


} // namespace fight


#endif // FIGHT_UNITGUN_H
