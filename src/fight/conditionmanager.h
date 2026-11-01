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

#ifndef CONDITIONMANAGER_H
#define CONDITIONMANAGER_H


#include <map>
#include <list>
#include <set>
#include "util/string.hpp"
#include "condition.h"


namespace fight {


class Scenario;


class ConditionManager
{
public:
    ConditionManager(Scenario *scenario);

public:
    struct ConditionEntry
    {
        Condition *condTrigger;
        int cond1;
        int dep1;
        int ref1;
        int op;
        int cond2;
        int dep2;
        int ref2;
        int del;
        ConditionEvent *condSignal;
        ConditionEvent *condAttacked;
        ConditionEvent *condIdentified;
        ConditionEvent *condParalyzed;
        ConditionEvent *condFinished;
        ConditionEvent *condBoarded;
        int iff = -1; // faction of the unit, -1 if it has none
        bool absent = false; // not in the mission at this difficulty: achieved for objectives, no faction
    };

public:
    ConditionEntry& addEntry(int id);
    void updateObjective(int id, int index, int wccond);
    void buildDependencies();

    Condition* addCondition(int limit = 0);
    // a condition for an order slot's "stop when" settings, nullptr if it has none
    Condition* addBreakCondition(int cond1, int dep1, int ref1, int op, int cond2, int dep2, int ref2, int del);
    ConditionRadio* addCondRadio(const Vector3D &pos, const String &text);
    ConditionSpace* addCondSpace(int x, int y, int dimx, int dimy, int minz, int maxz);
    void testSpace(float x, float y, float height);
    void delayComplete(Condition *cond, int delay);
    // completes the "destroyed" event of an entry (for an area or nav point: reached), for testing
    bool signal(int id);
    // the faction of a unit (-1 if it has none), and whether destroying it is an objective
    int iff(int id) const;
    bool isDestroyTarget(int id) const { return m_destroyTargets.count(id) > 0; }
    void fail() { m_condFailure.complete(); }
    // a failure nothing undoes, accomplished or not (three friendly kills)
    void failForced() { m_forcedFailure = true; fail(); }
    void update(float elapsedTime);

    bool autopilot() const { return m_condSuccess.isCompleted() && !m_forcedFailure; }
    bool failed() const { return m_condFailure.isCompleted() || m_forcedFailure; }
    bool failureEnded() const { return m_condFailure.ended(); }

    // problems found while setting up the conditions, e.g. unresolved references
    const StringList& issues() const { return m_issues; }

private:
    void issue(const String &text);

private:
    Scenario *m_scenario;
    std::map<int, ConditionEntry> condEntries;
    std::list<Condition> m_conditions;
    std::list<ConditionRadio> m_condRadio;
    std::list<ConditionSpace> m_condSpaces;

    ConditionSuccess m_condSuccess;
    ConditionFailure m_condFailure;
    bool m_forcedFailure = false;
    StringList m_issues;
    std::map<int, Condition> m_condObjectives;
    std::set<int> m_destroyTargets;
    std::list<ConditionEvent> m_absentEvents; // completed once the dependencies are built

    struct DelayCompleteEntry
    {
        Condition *cond;
        float completeTime;
    };

    std::list<DelayCompleteEntry> m_delayEntries;

private:
    void initCondition(int cond, int dep, int ref, Condition *condDepend);
    ConditionEvent* unitEvent(int cond, int ref);
    ConditionEvent* factionEvent(int cond, int iff);

    std::map<std::pair<int, int>, Condition*> m_factionEvents;
};


}


#endif // CONDITIONMANAGER_H
