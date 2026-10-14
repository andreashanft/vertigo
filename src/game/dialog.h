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

#ifndef GAME_DIALOG_H
#define GAME_DIALOG_H


#include "ui/widget.h"
#include <map>
#include <set>
#include <vector>

#include <functional>
#include "util/string.hpp"

namespace game {


class Dialog : public ui::Widget
{
private:
    struct Option
    {
        int text;
        int next;
        int message;
        std::set<int> onlyIf;
        std::set<int> notIf;
    };

    struct Entry
    {
        int text;
        std::vector<Option> options;
    };

    struct Message
    {
        enum Type
        {
            None,
            AddTask,
            RemoveTask,
            ChangeChapter,
            AddDialog,
            RemoveDialog,
            AddCredit,
            EnableStation,
            DisableStation,
            AddMissionStation,
            AddMissionArea,
            ReplaceApproachMovie,
            Boat,
            GameOver
        };

        Message() : type(None), value(0) {}

        Type type;
        int value;
        String name;
    };

    struct Precondition
    {
        enum Type
        {
            None,
            NumSmallTalks,
            Mission,
        };

        Precondition() : type(None), amount(0) {}

        Type type;
        int amount;
        String name;
    };

    enum Return
    {
        RemoveDialog = -100001,
    };

public:
    Dialog(int chapter, int id, ui::Widget *parent,
       std::function<void(int)> funcRemove,
       std::function<void(int)> funcAddMessage,
       std::function<void(int)> funcAddTask,
       std::function<void(int)> funcRemoveTask,
       std::function<void(int)> funcChangeChapter,
       std::function<void(int)> funcAddDialog,
       std::function<void(int)> funcRemoveDialog,
       std::function<void(int)> funcAddCredit,
       std::function<void(int)> funcEnableStation,
       std::function<void(int)> funcDisableStation,
       std::function<void(String, int)> funcAddMission,
       std::function<void(int, String)> funcReplaceApproachMovie,
       std::function<void(int)> funcUpgradeBoat,
       std::function<void()> funcGameOver
    );

    std::function<void()> funcClose;

public:
    int id() const { return m_id; }
    const String& name() const { return m_name; }
    bool isFemale() const { return m_female; }
    int room() const { return m_room; }
    int person() const { return m_person; }
    bool matches(int area, int station, int room) const;
    bool matchesEnCom(int area, int station, bool room) const;
    bool matchesDirect(int area, int station) const;
    bool isSmallTalk() const { return m_isSmallTalk; }
    bool remove() const { return m_remove; }

private:
    void draw();
    int drawOption(int y, const Option *option);
    void mouseMoveEvent(const Point &pos);
    bool mousePressEvent(const Point &pos, input::MouseButton button);
    void select();
    void loadTree(const String &filename);
    void loadStrings(const String &filename);
    bool testPreconditions() const;

private:
    int m_id;
    std::map<int, Entry> m_entries;
    std::map<int, String> m_strings;
    int m_current;
    int m_optionIndex;
    Point m_mousePos;
    Rect m_rect;
    const Option *m_option;
    String m_name;
    bool m_female;
    int m_area;
    int m_station;
    int m_room;
    int m_person;
    bool m_finished;
    bool m_remove;
    // the messages of an id, the last one read first
    std::multimap<int, Message> m_messages;
    int m_changeChapter;
    bool m_gameOver;
    bool m_isSmallTalk;
    std::vector<Precondition> m_preconditions;
    std::function<void(int)> m_funcRemove;
    std::function<void(int)> m_funcAddMessage;
    std::function<void(int)> m_funcAddTask;
    std::function<void(int)> m_funcRemoveTask;
    std::function<void(int)> m_funcChangeChapter;
    std::function<void(int)> m_funcAddDialog;
    std::function<void(int)> m_funcRemoveDialog;
    std::function<void(int)> m_funcAddCredit;
    std::function<void(int)> m_funcEnableStation;
    std::function<void(int)> m_funcDisableStation;
    std::function<void(String, int)> m_funcAddMission;
    std::function<void(int, String)> m_funcReplaceApproachMovie;
    std::function<void(int)> m_funcUpgradeBoat;
    std::function<void()> m_funcGameOver;
};


} // namespace game


#endif // GAME_DIALOG_H
