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

#ifndef FIGHT_BOMBER_H
#define FIGHT_BOMBER_H


#include "simpleboat.h"
#include <vector>


namespace fight {


class MountedTurret;


// A bomber (typ 2050): a boat with its fighting and orders, which carries up to 8 gun turrets of its
// own ([Turret0] to [Turret7] of its file, docs/ROADMAP.md, appendix J). It hands every order to its
// turrets and runs them after its own move; asleep, it lets them rest; paralysed, it paralyses them.
// Its destruction differs from a boat's (appendix R): the hull isn't kicked about, half the turrets
// blow up with the first blast and the rest with the fifth, other debris goes, and after the last blast
// the hull stays until no turret is left, then blows up at both ends and is gone.
class Bomber : public SimpleBoat
{
public:
    Bomber(Scenario *scenario, txt::DesFile &file, const ObjectInfo &info, float yaw, const String &desName);
    ~Bomber();

public:
    // the turrets as units of their own; the scenario owns them
    const std::vector<MountedTurret *>& turrets() const { return m_turrets; }

    bool update(float elapsedTime);
    void orderStarted(const OrderSlot *slot);
    void sleep();

protected:
    bool startTumbling() override;
    bool wreckKicks() const { return false; }
    void wreckStepped(int step);
    void wreckLastDebris();
    void wreckEnd();

private:
    std::vector<MountedTurret *> m_turrets;
    bool m_bioBomber;           // the bio bomber leaves no wreck bodies
    bool m_waitingForTurrets = false;
};


} // namespace fight


#endif // FIGHT_BOMBER_H
