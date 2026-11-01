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

#ifndef FIGHT_MINE_H
#define FIGHT_MINE_H


#include "simpleobject.h"


namespace fight {


// A mine (docs/ROADMAP.md, appendix Q). A passive one (MINE0.DES) stays where it is laid; an active
// one (MINE1.DES, with a MaxVelocity) creeps towards the player once the player comes near the
// place it was laid and goes back there when the player leaves. Either blows up when its armour
// is gone, by a shot or by running into something, in an explosion of its own kind.
class Mine : public SimpleObject
{
public:
    Mine(Scenario *scenario, txt::DesFile &file, const ObjectInfo &info);

public:
    bool isActive() const { return m_speed > 0; }
    // the sonar never lists a mine, so it can't be locked either (docs/ROADMAP.md, appendix Q)
    bool isContact() const { return false; }
    bool isChasing() const { return m_chasing; }
    // whether running into this unit sets the mine off: boats, tanks, ships, towers, turrets,
    // buildings and trash of another faction (the player has one of its own); mines of the same
    // faction leave their own side's units alone, and mines never touch each other; an active mine
    // also goes up on any ground plan in its way
    bool collidesWith(const ActiveObject *unit) const;
    // the contact: each takes the other's strength, so the mine goes up
    void collide(Object *object, const Vector3D &contact);

    void setPosition(const Vector3D &pos);
    bool update(float elapsedTime);
    void destroy();

private:
    float radius() const;

private:
    float m_speed;          // m/s, 0 for a passive mine
    Vector3D m_home;        // where it was laid
    bool m_chasing = false;
    float m_spin = 0;       // rad/s about the vertical
};


} // namespace fight


#endif // FIGHT_MINE_H
