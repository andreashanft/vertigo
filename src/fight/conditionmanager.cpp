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

#include "conditionmanager.h"
#include "scenario.h"
#include "util/debug.h"


namespace fight {


ConditionManager::ConditionManager(Scenario *scenario) :
    m_scenario(scenario),
    m_condSuccess(scenario),
    m_condFailure(scenario)
{
    m_condFailure.addDependency(&m_condSuccess, false);
}


Condition* ConditionManager::addCondition(int limit)
{
    m_conditions.emplace_back(m_scenario, limit);
    return &m_conditions.back();
}


ConditionRadio* ConditionManager::addCondRadio(const Vector3D &pos, const String &text)
{
    m_condRadio.emplace_back(m_scenario, pos, text);
    return &m_condRadio.back();
}


ConditionSpace* ConditionManager::addCondSpace(int x, int y, int dimx, int dimy, int minz, int maxz)
{
    m_condSpaces.emplace_back(m_scenario, x, y, dimx, dimy, minz, maxz);
    return &m_condSpaces.back();
}


void ConditionManager::testSpace(float x, float y, float height)
{
    for (ConditionSpace &space : m_condSpaces)
        space.test(x, y, height);
}


void ConditionManager::delayComplete(Condition *cond, int delay)
{
    m_delayEntries.emplace_back();
    m_delayEntries.back().cond = cond;
    m_delayEntries.back().completeTime = m_scenario->time() + delay;
}


void ConditionManager::update(float elapsedTime)
{
    m_delayEntries.remove_if([this](const DelayCompleteEntry &delayEntry) {
        if (delayEntry.completeTime <= m_scenario->time())
        {
            delayEntry.cond->complete();
            return true;
        }
        return false;
    });
}


bool ConditionManager::signal(int id)
{
    const auto it = condEntries.find(id);
    if (it == condEntries.end() || it->second.condSignal == nullptr)
        return false;
    it->second.condSignal->complete();
    return true;
}


ConditionManager::ConditionEntry& ConditionManager::addEntry(int id)
{
    return condEntries[id];
}


int ConditionManager::iff(int id) const
{
    const auto it = condEntries.find(id);
    return it != condEntries.end() ? it->second.iff : -1;
}


void ConditionManager::updateObjective(int id, int index, int wccond)
{
    if (wccond == 2 || wccond == 9)
        m_destroyTargets.insert(id);
    const ConditionEntry &entry = condEntries.at(id);
    ConditionEvent *condSuccess = NULL;
    ConditionEvent *condFailure = NULL;
    if (entry.absent)
    {
        // "destroy all of these" stays achievable; the other kinds need any one member, so it just isn't one
        if (wccond != 2 && wccond != 9)
            return;
        m_absentEvents.emplace_back();
        condSuccess = &m_absentEvents.back();
    }
    switch (entry.absent ? 0 : wccond)
    {
    case 0:
        break;
    case 1:
        condSuccess = entry.condTrigger;
        break;
    case 2:
        condSuccess = entry.condSignal;
        break;
    case 3:
        condSuccess = entry.condAttacked;
        break;
    case 4:
        condSuccess = entry.condIdentified;
        break;
    case 5:
        condFailure = entry.condSignal;
        break;
    case 6:
        condSuccess = entry.condParalyzed;
        condFailure = entry.condSignal;
        break;
    case 7:
        condSuccess = entry.condFinished;
        condFailure = entry.condSignal;
        break;
    case 8:
        condSuccess = entry.condBoarded;
        condFailure = entry.condSignal;
        break;
    case 9:
        condSuccess = entry.condSignal;
        condFailure = entry.condSignal;
        break;
    default:
        issue(String("Unhandled objective condition %1 %2").arg(id).arg(wccond));
    }
    if (condSuccess == NULL && condFailure == NULL)
        issue(String("Unachievable objective %1 %2").arg(id).arg(index));

    Condition *cond;
    if (condSuccess != NULL)
    {
        int objective = index*2;
        if (m_condObjectives.find(objective) == m_condObjectives.end())
        {
            cond = &m_condObjectives.emplace(std::piecewise_construct,
                std::forward_as_tuple(objective),
                std::forward_as_tuple(m_scenario, 1)).first->second;
            if (objective < 16)
            {
                cond->addDependency(&m_condSuccess);
                m_condSuccess.setLimit(m_condSuccess.limit() + 1);
            }
        }
        else
        {
            cond = &m_condObjectives.at(objective);
            if (wccond == 2 || wccond == 9)
                cond->setLimit(cond->limit() + 1);
        }
        condSuccess->addDependency(cond);
    }
    if (condFailure != NULL)
    {
        int objective = index*2 + 1;
        if (m_condObjectives.find(objective) == m_condObjectives.end())
        {
            cond = &m_condObjectives.emplace(std::piecewise_construct,
                std::forward_as_tuple(objective),
                std::forward_as_tuple(m_scenario, 1)).first->second;
            if (objective < 16)
                cond->addDependency(&m_condFailure);
        }
        else
        {
            cond = &m_condObjectives.at(objective);
            if (wccond == 9)
                cond->setLimit(cond->limit() + 1);
        }
        condFailure->addDependency(cond);
        if (wccond == 6 || wccond == 8)
        {
            condSuccess->addDependency(cond, false);
            cond->setLimit(2);
        }
    }
}


namespace {

// 0 and 47 both mean "no condition"
bool isCondition(int cond)
{
    return cond != 0 && cond != 47;
}

}


Condition* ConditionManager::addBreakCondition(int cond1, int dep1, int ref1, int op, int cond2, int dep2, int ref2, int del)
{
    const int count = (isCondition(cond1) ? 1 : 0) + (isCondition(cond2) ? 1 : 0);
    if (count == 0)
        return nullptr;

    Condition *condition = addCondition(count == 2 && op == 0 ? 2 : 1);
    condition->setDelay(del);
    if (isCondition(cond1))
        initCondition(cond1, dep1, ref1, condition);
    if (isCondition(cond2))
        initCondition(cond2, dep2, ref2, condition);
    condition->tryComplete();
    return condition;
}


void ConditionManager::buildDependencies()
{
    for (const auto &pair : condEntries)
    {
        const ConditionEntry &entry = pair.second;
        if (entry.condTrigger == nullptr)
        {
            issue(String("Undefined object %1").arg(pair.first));
            continue;
        }

        entry.condTrigger->setDelay(entry.del);
        int count = 0;
        if (isCondition(entry.cond1))
        {
            initCondition(entry.cond1, entry.dep1, entry.ref1, entry.condTrigger);
            count++;
        }
        if (isCondition(entry.cond2))
        {
            initCondition(entry.cond2, entry.dep2, entry.ref2, entry.condTrigger);
            count++;
        }
        if (count == 0)
            continue;

        // op 0 is "and", op 1 is "or"
        entry.condTrigger->setLimit(count == 2 && entry.op == 0 ? 2 : 1);
        if (count == 2 && entry.op != 0 && entry.op != 1)
            issue(String("Unhandled condition operation %1").arg(entry.op));
    }
    for (const auto &pair : condEntries)
        if (pair.second.condTrigger)
            pair.second.condTrigger->tryComplete();

    for (ConditionEvent &event : m_absentEvents)
        event.complete();
}


void ConditionManager::initCondition(int cond, int dep, int ref, Condition *condDepend)
{
    if (cond == 4 || cond == 5 || (cond >= 6 && cond <= 37))
    {
        if (dep != 0)
            issue(String("Unhandled condition dep %1 %2").arg(cond).arg(dep));
        if (ref != -1)
            issue(String("Unexpected reference %1 %2").arg(cond).arg(ref));
    }

    if (cond == 4)
    {
        m_condSuccess.addDependency(condDepend);
        return;
    }
    if (cond == 5)
    {
        m_condFailure.addDependency(condDepend);
        return;
    }
    if (cond >= 6 && cond <= 37)
    {
        // objective n succeeded (even) or failed (odd)
        const int objective = cond - 6;
        const auto it = m_condObjectives.find(objective);
        if (it != m_condObjectives.end())
            it->second.addDependency(condDepend);
        else if (objective % 2 == 0 && m_condObjectives.count(objective + 1))
            // objectives that can only fail (protect, avoid an area) count as achieved until they fail
            m_condObjectives.at(objective + 1).addDependency(condDepend, false);
        else
            // never happens: s01802 and s02006 took theirs over from s01801 and s02005, which have
            // these objectives, and the missions fit it (G4)
            issue(String("No unit has objective %1 (%2)").arg(objective/2).arg(objective % 2 ? "failed" : "achieved"));
        return;
    }

    // the remaining conditions refer to a unit (dep 1) or to every unit of a faction (dep 2);
    // the second code of each pair is the negation
    bool positive;
    switch (cond)
    {
    case 1: case 2: case 3: case 38: case 43: case 45:
        positive = true;
        break;
    case 39: case 40: case 41: case 44: case 46:
        positive = false;
        break;
    default:
        issue(String("Unhandled condition %1").arg(cond));
        return;
    }

    ConditionEvent *event = nullptr;
    if (dep == 1)
        event = unitEvent(cond, ref);
    else if (dep == 2)
        event = factionEvent(cond, ref);
    else
        issue(String("Unhandled condition dep %1 %2").arg(cond).arg(dep));

    if (event)
        event->addDependency(condDepend, positive);
}


ConditionEvent* ConditionManager::unitEvent(int cond, int ref)
{
    const auto it = condEntries.find(ref);
    if (it == condEntries.end())
    {
        issue(String("Condition reference not found %1").arg(ref));
        return nullptr;
    }

    const ConditionEntry &entry = it->second;
    ConditionEvent *event = nullptr;
    switch (cond)
    {
    case 1: case 39: event = entry.condSignal; break;
    case 2: case 40: event = entry.condAttacked; break;
    case 3: case 41: event = entry.condIdentified; break;
    case 38:         event = entry.condParalyzed; break;
    case 43: case 44: event = entry.condFinished; break;
    case 45: case 46: event = entry.condBoarded; break;
    }
    if (event == nullptr)
        issue(String("Undefined object %1").arg(ref));
    return event;
}


// "every unit of the faction destroyed" or "any unit of the faction attacked"
ConditionEvent* ConditionManager::factionEvent(int cond, int iff)
{
    const bool destroyed = cond == 1 || cond == 39;
    if (!destroyed && cond != 2 && cond != 40)
    {
        issue(String("Unhandled faction condition %1").arg(cond));
        return nullptr;
    }

    const auto key = std::make_pair(destroyed ? 1 : 2, iff);
    const auto it = m_factionEvents.find(key);
    if (it != m_factionEvents.end())
        return it->second;

    std::vector<ConditionEvent *> members;
    for (const auto &pair : condEntries)
        if (pair.second.iff == iff)
            members.push_back(destroyed ? pair.second.condSignal : pair.second.condAttacked);

    // a faction without units is never destroyed nor attacked
    if (members.empty())
        issue(String("Empty faction %1").arg(iff));
    Condition *event = addCondition(members.empty() ? -1 : destroyed ? int(members.size()) : 1);
    for (ConditionEvent *member : members)
        member->addDependency(event);

    m_factionEvents[key] = event;
    return event;
}


void ConditionManager::issue(const String &text)
{
    m_issues << text;
    util::Debug() << text.toUtf8().c_str();
}


} // namespace fight
