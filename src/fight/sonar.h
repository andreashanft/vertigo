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

#ifndef SONAR_H
#define SONAR_H


#include <vector>


namespace fight {


class Scenario;
class ActiveObject;


class Sonar
{
public:
    Sonar(Scenario *scenario);

public:
    struct SonarEntry
    {
        ActiveObject *object;
        float distance;
        bool isFriend;      // without friend-or-foe detection every contact counts as a friend
        bool isNeutral;     // neither friend nor hostile, told apart (the round radars skip it)
        bool isPassive;
        bool isTorpedo;     // a torpedo on its way (never passive, never neutral)
    };

    std::vector<SonarEntry>::const_iterator begin() const { return m_sonarEntries.cbegin(); }
    std::vector<SonarEntry>::const_iterator end() const { return m_sonarEntries.cend(); }

public:
    void init(int sensor);
    void activate();
    void deactivate();
    void toggle();
    bool isActive() const { return m_active; }
    bool isActivating() const { return m_activatingDelay > 0; }
    bool detectArmor() const { return m_detectArmor; }
    bool detectWeapons() const { return m_detectWeapons; }
    bool detectActivity() const { return m_detectActivity; }
    // what the radars and the HUD's frames show, for the mission: Alt+T leaves out the torpedoes
    // of the player's side, Alt+S everything but the hostile contacts (as the sonar tells them) and
    // hostile torpedoes; nothing left out can be locked
    void toggleFriendlyTorpedoes() { m_friendlyTorpedoes = !m_friendlyTorpedoes; }
    void toggleOnlyEnemies() { m_onlyEnemies = !m_onlyEnemies; }
    bool isShown(const SonarEntry &entry) const;

    // distance at which an object is detected passively / actively
    float passiveRange(const ActiveObject *object) const;
    // the player's own passive range, without a unit's noise
    float passiveRange() const { return m_passiveRange; }
    float activeRange(const ActiveObject *object) const;
    // the torpedo contacts' colour blinks between two reds, 0.15 s each: whether the bright one is on
    bool torpedoesBright() const;

public:
    void update(float elapsedTime);

private:
    Scenario *m_scenario;
    bool m_friendlyTorpedoes = true;
    bool m_onlyEnemies = false;
    float m_delay;
    float m_passiveRange;
    float m_passiveRangeMult;
    float m_activeRange;
    float m_activeRangeMult;
    bool m_iff;
    bool m_detectArmor;
    bool m_detectWeapons;
    bool m_detectActivity;

    bool m_active;
    float m_activatingDelay;
    std::vector<SonarEntry> m_sonarEntries;
};


}


#endif // SONAR_H
