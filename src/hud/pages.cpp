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

#include "pages.h"
#include "hud.h"
#include "fight/scenario.h"
#include "gfx/font.h"
#include "gfx/image.h"
#include "gfx/pipeline.h"
#include "txt/stringtable.h"
#include "util/datapath.h"

#include <algorithm>
#include <fstream>
#include <sstream>

#ifndef VERTIGO_VERSION
#define VERTIGO_VERSION ""
#endif
#ifndef VERTIGO_COMMIT
#define VERTIGO_COMMIT ""
#endif


namespace hud {


namespace {

// the page's place in the 640x480 art, as the original lays it out: a dimmed box between two
// dotted lines, the title under the top one, the lines below, the footer at the bottom right
const Rect Box(54, 92, 523, 314);
const int TitleY = 113;
const int FirstLineY = 142;
const int LineHeight = 12;
const int Rows = 20;
// the help's four columns and the log's time and text
const int HelpColumns[4] = { 63, 142, 357, 436 };
const int TimeX = 58;
const int TextX = 107;
const int FooterY = 418;
const int FooterRight = 575;
const int HelpPages = 3;
// the briefing's targets and tips: eighteen characters in from the log's left edge, each in its
// half of the rows (a heading, its underline and up to eight lines)
const int BriefingX = TimeX + 18*6;
const int BriefingHalf = 10;

// the help files' columns: key, what it does, key, what it does
const int HelpSplit[3] = { 13, 48, 61 };

// FIGHTEXT: the titles and footers
const int TextTargets = 0, TextTargetsLine = 1, TextTips = 2, TextTipsLine = 3;
const int TextHelpFooter = 20, TextHelpTitle = 21, TextBriefingFooter = 23, TextBriefingTitle = 24;
const int TextRadioFooter = 59, TextRadioTitle = 60;

String fightText(int n)
{
    return txt::StringTable::get(txt::StringFight(n));
}

// the help files are lower case; the original shows them in capitals (the umlauts too, in the
// game's 8-bit text)
String capitals(const String &text)
{
    String result = text.toUpper();
    for (int i = 0; i < result.size(); i++) {
        const unsigned char c = result[i];
        if (c == 0x84) result[i] = char(0x8e);       // ä
        else if (c == 0x94) result[i] = char(0x99);  // ö
        else if (c == 0x81) result[i] = char(0x9a);  // ü
    }
    return result;
}

} // namespace


// no parent: the HUD draws the pages itself, over everything else
Pages::Pages(HUD *hud) :
    ui::Widget(nullptr),
    m_hud(hud)
{
    gfx::Image black(1, 1, gfx::Image::ARGB32);
    black.setPixel(0, 0, gfx::rgba(0, 0, 0, 160));
    m_black = gfx::Texture(black, false);
    gfx::Image dot(1, 1, gfx::Image::ARGB32);
    dot.setPixel(0, 0, gfx::rgba(120, 220, 40, 255));
    m_dot = gfx::Texture(dot, false);
    loadHelp();
}


void Pages::loadHelp()
{
    for (int page = 0; page < HelpPages; page++) {
        std::vector<HelpLine> lines;
        std::ifstream file(util::DataPath::resolve(String("vfx:cockpit/help%1.txt").arg(page)), std::ios::binary);
        std::string raw;
        while (std::getline(file, raw)) {
            while (!raw.empty() && (raw.back() == '\r' || raw.back() == '\n'))
                raw.pop_back();
            const String line = String::fromLatin1(raw);
            HelpLine help;
            int from = 0;
            for (int i = 0; i < 4; i++) {
                const int to = i < 3 ? std::min(HelpSplit[i], line.size()) : line.size();
                help.column[i] = capitals(line.mid(from, std::max(0, to - from)).trimmed());
                from = to;
            }
            lines.push_back(help);
        }
        // the files end in blank lines
        while (!lines.empty() && std::all_of(std::begin(lines.back().column), std::end(lines.back().column), [](const String &s) { return s.isEmpty(); }))
            lines.pop_back();
        m_help.push_back(lines);
    }
}


void Pages::open(Page page)
{
    m_page = page;
    m_helpPage = 0;
    m_briefingPage = 0;
}


void Pages::next()
{
    if (m_page == Page::Help)
        m_helpPage = (m_helpPage + 1) % HelpPages;
    else if (m_page == Page::Briefing)
        m_briefingPage = 1 - m_briefingPage;
}


void Pages::setBriefing(const StringList &text, const StringList &targets, const StringList &tips)
{
    m_briefingText = text;
    m_briefingTargets = targets;
    m_briefingTips = tips;
}


// a point of the 640x480 art, placed as the box is
Point Pages::place(const Rect &box, int x, int y) const
{
    const float k = float(box.width)/Box.width;
    return Point(box.x + int((x - Box.x)*k), box.y + int((y - Box.y)*k));
}


void Pages::draw()
{
    if (m_page == Page::None)
        return;
    const Rect box = m_hud->projectCenter(Box);
    drawFrame(box);
    if (m_page == Page::Help)
        drawHelp(box);
    else if (m_page == Page::Briefing)
        drawBriefing(box);
    else
        drawRadio(box);
}


void Pages::drawFrame(const Rect &box)
{
    m_black.draw(RectF(box.x, box.y, box.width, box.height));
    drawDots(box.x, box.x + box.width, box.y);
    drawDots(box.x, box.x + box.width, box.y + box.height - 1);
}


void Pages::drawDots(int x1, int x2, int y)
{
    for (int x = x1; x < x2; x += 2)
        m_dot.draw(float(x), float(y));
}


void Pages::drawHelp(const Rect &box)
{
    gfx::Font &font = m_hud->fontGreen();
    m_hud->fontYellow().draw(fightText(TextHelpTitle), Rect(box.x, place(box, 0, TitleY).y, box.width, -1), true, false);
    const std::vector<HelpLine> &lines = m_help[std::min<size_t>(m_helpPage, m_help.size() - 1)];
    for (size_t i = 0; i < lines.size() && int(i) < Rows; i++) {
        for (int c = 0; c < 4; c++)
            if (!lines[i].column[c].isEmpty())
                font.draw(lines[i].column[c], place(box, HelpColumns[c], FirstLineY + int(i)*LineHeight));
    }
    // the version, the last commit and its date to the left of the footer, as the original shows
    // its own version and date
    const String commit = VERTIGO_COMMIT;
    const String version = "V" VERTIGO_VERSION + (commit.isEmpty() ? String() : " " + commit);
    drawFooter(box, version.toUpper() + "   " + fightText(TextHelpFooter));
}


void Pages::drawRadio(const Rect &box)
{
    m_hud->fontYellow().draw(fightText(TextRadioTitle), Rect(box.x, place(box, 0, TitleY).y, box.width, -1), true, false);
    // the latest entries that fit, in the order they came
    const std::vector<fight::Scenario::LogEntry> &log = m_hud->scenario()->log();
    const int first = std::max(0, int(log.size()) - Rows);
    for (int i = first; i < int(log.size()); i++) {
        const fight::Scenario::LogEntry &entry = log[i];
        gfx::Font &font = entry.color == fight::Scenario::LogColor::Red ? m_hud->fontRed()
                        : entry.color == fight::Scenario::LogColor::Amber ? m_hud->fontYellow() : m_hud->fontGreen();
        const int y = FirstLineY + (i - first)*LineHeight;
        const int seconds = std::max(0, int(entry.time));
        const String time = String("%1:%2 :").arg(seconds/60, 2, 10, '0').arg(seconds%60, 2, 10, '0');
        font.draw(time, place(box, TimeX, y));
        font.draw(entry.text.toUpper(), place(box, TextX, y));
    }
    drawFooter(box, fightText(TextRadioFooter));
}


// the first page has the text centred line by line, the second the targets in the upper half of the
// rows and the tips in the lower, both headings shown even without lines
void Pages::drawBriefing(const Rect &box)
{
    gfx::Font &font = m_hud->fontGreen();
    m_hud->fontYellow().draw(fightText(TextBriefingTitle), Rect(box.x, place(box, 0, TitleY).y, box.width, -1), true, false);
    if (m_briefingPage == 0) {
        for (int i = 0; i < m_briefingText.count() && i < Rows; i++)
            font.draw(capitals(m_briefingText.at(i)), Rect(box.x, place(box, 0, FirstLineY + i*LineHeight).y, box.width, -1), true, false);
    } else {
        const StringList *lists[2] = { &m_briefingTargets, &m_briefingTips };
        const int headings[2][2] = { { TextTargets, TextTargetsLine }, { TextTips, TextTipsLine } };
        for (int half = 0; half < 2; half++) {
            const int top = half*BriefingHalf;
            font.draw(capitals(fightText(headings[half][0])), place(box, BriefingX, FirstLineY + top*LineHeight));
            font.draw(fightText(headings[half][1]), place(box, BriefingX, FirstLineY + (top + 1)*LineHeight));
            for (int i = 0; i < lists[half]->count() && i < BriefingHalf - 2; i++)
                font.draw(capitals(lists[half]->at(i)), place(box, BriefingX, FirstLineY + (top + 2 + i)*LineHeight));
        }
    }
    drawFooter(box, fightText(TextBriefingFooter));
}


void Pages::drawFooter(const Rect &box, const String &text)
{
    gfx::Font &font = m_hud->fontYellow();
    const Point right = place(box, FooterRight, FooterY);
    font.draw(text, right.x - font.width(text), right.y);
}


} // namespace hud
