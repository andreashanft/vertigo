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

#ifndef FIGHT_TORCHCONTACT_H
#define FIGHT_TORCHCONTACT_H


#include "activeobject.h"


namespace fight {


// The torch a Torch torpedo leaves, as a contact for the units' passive sonar (sen 1), which
// hear it like a boat and may take it for their target (docs/ROADMAP.md, appendix S). It follows
// its piece of debris and is gone with it; nothing collides with it, shots and torpedoes put the
// torch itself out.
class TorchContact : public ActiveObject
{
public:
    TorchContact(Scenario *scenario, int decoy, int faction, float noise);

public:
    int decoy() const { return m_decoy; }
    Vector3D center() const { return m_position; }
    bool isContact() const { return false; }
    // takes the torch's place and way; false, and disabled, once it has gone out
    bool follow();

private:
    int m_decoy;
};


} // namespace fight


#endif // FIGHT_TORCHCONTACT_H
