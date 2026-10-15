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

#include "missionmenu.h"
#include "hud.h"
#include "txt/stringtable.h"

#include "game/input.hpp"
#include "util/ticks.h"

#include <algorithm>


namespace hud {


namespace {

const int BoxWidth = 300;

}


// no parent: the HUD draws the menu itself, on top of everything else
MissionMenu::MissionMenu(HUD *hud, const Point &center) :
    ui::Widget(nullptr),
    m_hud(hud),
    m_center(center),
    m_page(Page::None),
    m_noticeEnd(0),
    m_edgeBL(hud->getImage("hudedgbl"), false),
    m_edgeBR(hud->getImage("hudedgbr"), false),
    m_edgeTL(hud->getImage("hudedgtl"), false),
    m_edgeTR(hud->getImage("hudedgtr"), false)
{
    m_black.createEmpty(BoxWidth, 120, gfx::Texture::Format::RGB, false);
}


void MissionMenu::open(Page page)
{
    m_page = page;
    m_noticeEnd = 0;
    m_highlight = false;
    m_selected = 0;
}


void MissionMenu::select(int step)
{
    int options = 0;
    for (const Line &line : lines())
        if (line.font == &m_hud->fontGreen())
            options++;
    if (m_highlight && options > 0)
        m_selected = (m_selected + step + options) % options;
    m_highlight = true;
}


// the option's digit; without options (no turrets) any key closes the menu
int MissionMenu::key(int option) const
{
    int key = input::Key_0;
    int n = 0;
    for (const Line &line : lines())
        if (line.font == &m_hud->fontGreen() && n++ == option && !line.text.isEmpty() && line.text[0] >= '0' && line.text[0] <= '9')
            key = line.text[0];
    return key;
}


MissionMenu::Action MissionMenu::choose()
{
    if (!m_highlight) {
        select(0);
        return Action::None;
    }
    return press(key(m_selected));
}


int MissionMenu::optionAt(const Point &point) const
{
    const std::vector<Line> lines = this->lines();
    std::vector<Rect> places;
    layout(lines, places);
    int option = 0;
    for (size_t i = 0; i < lines.size(); i++) {
        if (lines[i].font != &m_hud->fontGreen())
            continue;
        if (point.x >= places[i].x && point.x < places[i].x + places[i].width && point.y >= places[i].y && point.y < places[i].y + places[i].height)
            return option;
        option++;
    }
    return -1;
}


MissionMenu::Action MissionMenu::choose(int option)
{
    return press(key(option));
}


MissionMenu::Action MissionMenu::back()
{
    return press(input::Key_Escape);
}


MissionMenu::Action MissionMenu::keyPressEvent(int key)
{
    m_highlight = false;
    return press(key);
}


void MissionMenu::notice(const StringList &lines, float seconds)
{
    m_notice = lines;
    m_noticeEnd = util::ticks() + unsigned(seconds*1000);
}


MissionMenu::Action MissionMenu::press(int key)
{
    const Page page = m_page;
    const Action action = pressOnPage(key);
    if (m_page != page)
        m_selected = 0;
    return action;
}


MissionMenu::Action MissionMenu::pressOnPage(int key)
{
    switch (m_page) {
    case Page::None:
        break;

    case Page::Main:
        if (key == input::Key_0 || key == input::Key_Escape) {
            m_page = Page::None;
            return Action::Continue;
        }
        if (key == input::Key_1)
            m_page = Page::ConfirmAbort;
        if (key == input::Key_2)
            m_page = Page::ConfirmRestart;
        if (key == input::Key_3)
            m_page = Page::ConfirmLoad;
        break;

    case Page::ConfirmAbort:
    case Page::ConfirmRestart:
    case Page::ConfirmLoad:
        if (key == input::Key_1) {
            const Action action = m_page == Page::ConfirmAbort ? Action::Abort :
                                  m_page == Page::ConfirmRestart ? Action::Restart : Action::Load;
            m_page = Page::None;
            return action;
        }
        if (key == input::Key_0 || key == input::Key_Escape)
            m_page = Page::Main;
        break;

    case Page::Turrets:
        if (key == input::Key_0 || key == input::Key_Escape || key == input::Key_O || m_turretCount == 0)
            m_page = Page::None;
        else if (key == input::Key_1)
            m_page = Page::Turret1;
        else if (key == input::Key_2 && m_turretCount > 1)
            m_page = Page::Turret2;
        else if (key == input::Key_3)
        {
            m_turret = -1;
            m_turretOrder = 0;
            m_page = Page::None;
            return Action::TurretOrder;
        }
        break;

    case Page::Turret1:
    case Page::Turret2:
        if (key >= input::Key_1 && key <= input::Key_4)
        {
            m_turret = m_page == Page::Turret1 ? 0 : 1;
            m_turretOrder = key - input::Key_1;
            m_page = Page::None;
            return Action::TurretOrder;
        }
        if (key == input::Key_0 || key == input::Key_Escape)
            m_page = Page::Turrets;
        break;

    case Page::Autopilot:
        if (key == input::Key_0 || key == input::Key_Escape)
            m_page = Page::None;
        else if (key >= input::Key_1 && key - input::Key_1 < std::min(int(m_destinations.size()), 9))
        {
            m_destination = key - input::Key_1;
            m_page = Page::ConfirmAutopilot;
        }
        break;

    case Page::ConfirmAutopilot:
        if (key == input::Key_1)
        {
            m_page = Page::None;
            return Action::Autopilot;
        }
        if (key == input::Key_0 || key == input::Key_Escape)
            m_page = Page::Autopilot;
        break;

    case Page::GameOver:
    case Page::Died:
        if (key == input::Key_1)
            return Action::Abort;
        if (key == input::Key_2)
            return Action::Restart;
        if (key == input::Key_3)
            return Action::Load;
        break;
    }
    return Action::None;
}


std::vector<MissionMenu::Line> MissionMenu::lines() const
{
    gfx::Font *title = &m_hud->fontYellow();
    gfx::Font *red = &m_hud->fontRed();
    gfx::Font *option = &m_hud->fontGreen();
    auto text = [](int n) { return txt::StringTable::get(txt::StringFight(n)); };

    switch (m_page) {
    case Page::None:
        break;
    case Page::Main:
        return { { text(49), title }, { text(50), option }, { text(51), option }, { text(52), option }, { text(62), option } };
    case Page::ConfirmAbort:
        return { { text(53), title }, { text(54), option }, { text(55), option } };
    case Page::ConfirmRestart:
        return { { text(56), title }, { text(54), option }, { text(55), option } };
    case Page::ConfirmLoad:
        return { { text(61), title }, { text(54), option }, { text(55), option } };
    case Page::GameOver:
        return { { text(12).trimmed(), red }, { text(63), red }, { text(64), option }, { text(65), option }, { text(66), option } };
    case Page::Died:
        return { { text(57), red }, { text(58), red }, { text(64), option }, { text(65), option }, { text(66), option } };
    case Page::Turrets:
        if (m_turretCount == 0)
            return { { text(38), title }, { text(39), red } };
        if (m_turretCount == 1)
            return { { text(38), title }, { text(40), option }, { text(42), option }, { text(34), option } };
        return { { text(38), title }, { text(40), option }, { text(41), option }, { text(42), option }, { text(34), option } };
    case Page::Turret1:
    case Page::Turret2:
        return { { text(m_page == Page::Turret1 ? 43 : 44), title }, { text(45), option }, { text(46), option }, { text(47), option }, { text(48), option }, { text(34), option } };
    case Page::Autopilot:
        {
            std::vector<Line> lines = { { text(29), title }, { text(30), title } };
            for (int i = 0; i < std::min(int(m_destinations.size()), 9); i++)
                lines.push_back({ String("%1 - %2").arg(i + 1).arg(m_destinations[i]), option });
            lines.push_back({ text(34), option });
            return lines;
        }
    case Page::ConfirmAutopilot:
        return { { text(35), title }, { m_destinations[m_destination], title }, { text(36), option }, { text(37), option } };
    }

    std::vector<Line> notice;
    if (util::ticks() < m_noticeEnd)
        for (const String &line : m_notice)
            notice.push_back({ line, red });
    return notice;
}


Rect MissionMenu::layout(const std::vector<Line> &lines, std::vector<Rect> &places) const
{
    const int lineHeight = m_hud->fontGreen().height() + 4;
    const int height = int(lines.size())*lineHeight + 8;
    const Rect rect = m_hud->projectCenter(Rect(m_center.x - BoxWidth/2, m_center.y - height/2, BoxWidth, height));

    places.clear();
    int y = rect.y + 6;
    for (size_t i = 0; i < lines.size(); i++) {
        // a gap between the title lines and the options
        if (i > 0 && lines[i].font != lines[i - 1].font)
            y += 4;
        places.push_back(Rect(rect.x, y, rect.width, lineHeight));
        y += lineHeight;
    }
    return rect;
}


void MissionMenu::draw()
{
    const std::vector<Line> lines = this->lines();
    if (lines.empty())
        return;

    std::vector<Rect> places;
    const util::RectangleInclusive<int> rect = layout(lines, places);

    m_black.draw(rect);
    m_edgeBL.draw(rect.left(), rect.bottom() - m_edgeBL.height());
    m_edgeBR.draw(rect.right() - m_edgeBR.width(), rect.bottom() - m_edgeBR.height());
    m_edgeTL.draw(rect.left(), rect.top());
    m_edgeTR.draw(rect.right() - m_edgeTR.width(), rect.top());

    int option = 0;
    for (size_t i = 0; i < lines.size(); i++) {
        gfx::Font *font = lines[i].font;
        if (font == &m_hud->fontGreen() && option++ == m_selected && m_highlight)
            font = &m_hud->fontYellow();
        font->draw(lines[i].text, places[i], true, false);
    }
}


} // namespace hud
