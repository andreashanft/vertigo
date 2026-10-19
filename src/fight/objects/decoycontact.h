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

#ifndef FIGHT_DECOYCONTACT_H
#define FIGHT_DECOYCONTACT_H


#include "activeobject.h"
#include "fight/effects/particles.h"


namespace fight {


// A buzzer or the Torch's torch as a contact for the units' sensors, which may take it for their
// target (docs/ROADMAP.md, appendices J and S). It follows its piece of debris and is gone with it;
// nothing collides with it, shots and torpedoes hit the debris itself.
class DecoyContact : public ActiveObject
{
public:
    DecoyContact(Scenario *scenario, const Particles::DecoyInfo &decoy);

public:
    int decoy() const { return m_decoy; }
    bool isBuzzer() const { return m_kind == Particles::Decoy::Buzzer; }
    bool isTorch() const { return m_kind == Particles::Decoy::Torch; }
    Vector3D center() const { return m_position; }
    bool isContact() const { return false; }
    // takes the decoy's place and way; false, and disabled, once it is gone
    bool follow();

private:
    int m_decoy;
    Particles::Decoy m_kind;
};


} // namespace fight


#endif // FIGHT_DECOYCONTACT_H
