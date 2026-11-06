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
#include "game/input.hpp"
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

// The controller page: a controller drawn in the art's pixels, in the middle of the box, and each
// control's job beside it with a line to it, the regular set in green and the one with LB held in
// yellow below it. The controller is drawn in a design space of 170 by 120 and shrunk to fit.
const float PadScale = 0.76f;
const Point PadOrigin(251, 185);
// the picture holds the controller and the lines, over the part of the box they take
const Rect PadPicture(54, 136, 523, 194);
// the label columns: the control's name (an arrow for the d-pad), then its job, up to 25 characters
const int PadLeftX = 60;
const int PadRightX = 388;
const int PadTextX = 34;
const int PadCharWidth = 6;
const gfx::Rgb PadGreen = gfx::rgb(66, 214, 58);
const gfx::Rgb PadLine = gfx::rgb(66, 130, 58);
const gfx::Rgb PadYellow = gfx::rgb(239, 214, 0);

struct PadControl
{
    const char *name;               // empty for the d-pad, which has an arrow, and the face buttons
    Vector2D at;                    // where its line ends, in the design space
    txt::StringVertigo job;
    int second;                     // its job with LB held, -1 for none
    int arrow = -1;                 // the d-pad's: 0 up, 1 right, 2 down, 3 left
    int face = -1;                  // a face button's place: 0 top, 1 left, 2 right, 3 bottom
};

// The face buttons work by their place (top, left, right, bottom); their names are the letters
// printed there on the controller connected last (input::FaceLabels; a PlayStation controller's
// symbols are drawn)
const char *faceName(int place)
{
    static const char *const names[2][4] = { { "Y", "X", "B", "A" }, { "X", "Y", "A", "B" } };
    const input::FaceLabels labels = input::faceLabels();
    return labels == input::FaceLabels::PlayStation ? "" : names[labels == input::FaceLabels::Nintendo ? 1 : 0][place];
}

// down each side in the order of the rows, so that the lines rarely cross
const PadControl PadLeft[] = {
    { "LT", Vector2D(35, 3), txt::Pad_Torpedo, txt::Pad_AttackingTorpedo },
    { "LB", Vector2D(35, 13), txt::Pad_Shift, -1 },
    { "BACK", Vector2D(70, 36), txt::Pad_Turrets, txt::Pad_Autopilot },
    { "", Vector2D(42, 40), txt::Pad_Faster, txt::Pad_WeaponStatus, 0 },
    { "", Vector2D(50, 48), txt::Pad_Sonar, txt::Pad_SonarZoom, 1 },
    { "", Vector2D(42, 56), txt::Pad_Slower, txt::Pad_Armor, 2 },
    { "", Vector2D(34, 48), txt::Pad_Stop, txt::Pad_Engine, 3 },
    { "LS", Vector2D(62, 74), txt::Pad_Steer, -1 },
    { "L3", Vector2D(62, 74), txt::Pad_Help, txt::Pad_Radio },
};
const PadControl PadRight[] = {
    { "RT", Vector2D(135, 3), txt::Pad_Gun, txt::Pad_Attacker },
    { "RB", Vector2D(135, 13), txt::Pad_Lock, txt::Pad_LockEnemy },
    { "START", Vector2D(100, 36), txt::Pad_Menu, txt::Pad_Pause },
    { "", Vector2D(128, 38), txt::Pad_NextTorpedo, -1, -1, 0 },
    { "", Vector2D(118, 48), txt::Pad_NavPoint, txt::Pad_LockNavPoint, -1, 1 },
    { "", Vector2D(138, 48), txt::Pad_Ahead, txt::Pad_Booster, -1, 2 },
    { "", Vector2D(128, 58), txt::Pad_Astern, txt::Pad_Buzzer, -1, 3 },
    { "RS", Vector2D(108, 74), txt::Pad_Look, txt::Pad_Zoom },
    { "R3", Vector2D(108, 74), txt::Pad_View, -1 },
};

std::vector<PadControl> padRight()
{
    return std::vector<PadControl>(std::begin(PadRight), std::end(PadRight));
}

// a point of the design space in the art
Point padPoint(const Vector2D &at)
{
    return PadOrigin + Point(glm::round(at*PadScale));
}

// lines and circles in the art's pixels, into the picture
struct PadCanvas
{
    gfx::Image image;

    void pixel(int x, int y, gfx::Rgb colour)
    {
        x -= PadPicture.x;
        y -= PadPicture.y;
        if (x >= 0 && y >= 0 && x < image.width() && y < image.height())
            image.setPixel(x, y, colour);
    }
    void line(Point a, const Point &b, gfx::Rgb colour)
    {
        const int dx = std::abs(b.x - a.x), dy = -std::abs(b.y - a.y);
        const int sx = a.x < b.x ? 1 : -1, sy = a.y < b.y ? 1 : -1;
        int error = dx + dy;
        for (;;) {
            pixel(a.x, a.y, colour);
            if (a == b)
                break;
            const int e2 = 2*error;
            if (e2 >= dy) { error += dy; a.x += sx; }
            if (e2 <= dx) { error += dx; a.y += sy; }
        }
    }
    void shape(const std::vector<Vector2D> &points, gfx::Rgb colour)
    {
        for (std::size_t i = 0; i < points.size(); i++)
            line(padPoint(points[i]), padPoint(points[(i + 1) % points.size()]), colour);
    }
    void box(const Vector2D &a, const Vector2D &b, gfx::Rgb colour)
    {
        shape({ a, Vector2D(b.x, a.y), b, Vector2D(a.x, b.y) }, colour);
    }
    // a small solid triangle pointing up, right, down or left, 7 pixels across, from its corner
    void arrow(const Point &at, int direction, gfx::Rgb colour)
    {
        for (int i = 0; i < 4; i++)
            for (int j = 3 - i; j <= 3 + i; j++) {
                const Point p = direction == 0 ? Point(j, i) : direction == 1 ? Point(3 - i, j) : direction == 2 ? Point(j, 3 - i) : Point(i, j);
                pixel(at.x + p.x, at.y + p.y, colour);
            }
    }
    // a PlayStation face button's symbol, 7 pixels across, from its corner: a triangle (top), a
    // square (left), a circle (right) or a cross (bottom)
    void symbol(const Point &at, int face, gfx::Rgb colour)
    {
        if (face == 0) {
            line(at + Point(3, 0), at + Point(0, 6), colour);
            line(at + Point(3, 0), at + Point(6, 6), colour);
            line(at + Point(0, 6), at + Point(6, 6), colour);
        } else if (face == 1) {
            line(at, at + Point(6, 0), colour);
            line(at + Point(6, 0), at + Point(6, 6), colour);
            line(at + Point(6, 6), at + Point(0, 6), colour);
            line(at + Point(0, 6), at, colour);
        } else if (face == 2) {
            for (const Point &p : { Point(2, 0), Point(3, 0), Point(4, 0), Point(1, 1), Point(5, 1), Point(0, 2), Point(6, 2), Point(0, 3),
                                    Point(6, 3), Point(0, 4), Point(6, 4), Point(1, 5), Point(5, 5), Point(2, 6), Point(3, 6), Point(4, 6) })
                pixel(at.x + p.x, at.y + p.y, colour);
        } else {
            line(at, at + Point(6, 6), colour);
            line(at + Point(6, 0), at + Point(0, 6), colour);
        }
    }
    void circle(const Vector2D &centre, float radius, gfx::Rgb colour)
    {
        const Point c = padPoint(centre);
        const int r = std::max(1, int(std::lround(radius*PadScale)));
        int x = r, y = 0, error = 1 - r;
        while (x >= y) {
            for (const Point &p : { Point(x, y), Point(y, x), Point(-y, x), Point(-x, y), Point(-x, -y), Point(-y, -x), Point(y, -x), Point(x, -y) })
                pixel(c.x + p.x, c.y + p.y, colour);
            y++;
            if (error < 0)
                error += 2*y + 1;
            else {
                x--;
                error += 2*(y - x) + 1;
            }
        }
    }
};

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


void Pages::openController()
{
    open(Page::Help);
    m_helpPage = int(m_help.size());
}


void Pages::next()
{
    if (m_page == Page::Help)
        m_helpPage = (m_helpPage + 1) % (int(m_help.size()) + 1);
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
    if (m_helpPage >= int(m_help.size()))
        drawController(box);
    else {
        const std::vector<HelpLine> &lines = m_help[m_helpPage];
        for (size_t i = 0; i < lines.size() && int(i) < Rows; i++) {
            for (int c = 0; c < 4; c++)
                if (!lines[i].column[c].isEmpty())
                    font.draw(lines[i].column[c], place(box, HelpColumns[c], FirstLineY + int(i)*LineHeight));
        }
    }
    // the version, the last commit and its date to the left of the footer, as the original shows
    // its own version and date
    const String commit = VERTIGO_COMMIT;
    const String version = "V" VERTIGO_VERSION + (commit.isEmpty() ? String() : " " + commit);
    drawFooter(box, version.toUpper() + "   " + fightText(TextHelpFooter));
}


// The game controllers' buttons (HUD::buttonKey), with or without one connected
void Pages::drawController(const Rect &box)
{
    gfx::Font &green = m_hud->fontGreen();
    gfx::Font &yellow = m_hud->fontYellow();
    auto row = [](int i) { return FirstLineY + i*LineHeight; };
    // (made again when another kind of controller has been connected since)
    const int faces = int(input::faceLabels());
    if (!m_controller.isValid() || faces != m_controllerFaces) {
        m_controllerFaces = faces;
        PadCanvas canvas{ gfx::Image(PadPicture.width, PadPicture.height, gfx::Image::ARGB32) };
        canvas.shape({ { 14, 22 }, { 30, 19 }, { 60, 21 }, { 110, 21 }, { 140, 19 }, { 156, 22 }, { 163, 32 }, { 166, 52 },
                       { 165, 77 }, { 160, 102 }, { 152, 116 }, { 140, 120 }, { 128, 114 }, { 118, 98 }, { 106, 88 },
                       { 64, 88 }, { 52, 98 }, { 42, 114 }, { 30, 120 }, { 18, 116 }, { 10, 102 }, { 5, 77 }, { 4, 52 },
                       { 7, 32 } }, PadGreen);
        canvas.box({ 24, 0 }, { 46, 7 }, PadGreen);
        canvas.box({ 124, 0 }, { 146, 7 }, PadGreen);
        canvas.box({ 20, 10 }, { 50, 17 }, PadYellow);
        canvas.box({ 120, 10 }, { 150, 17 }, PadGreen);
        canvas.shape({ { 38, 38 }, { 46, 38 }, { 46, 44 }, { 52, 44 }, { 52, 52 }, { 46, 52 }, { 46, 58 }, { 38, 58 },
                       { 38, 52 }, { 32, 52 }, { 32, 44 }, { 38, 44 } }, PadGreen);
        for (const Vector2D &button : { Vector2D(128, 38), Vector2D(118, 48), Vector2D(138, 48), Vector2D(128, 58) })
            canvas.circle(button, 5, PadGreen);
        canvas.box({ 66, 34 }, { 74, 38 }, PadGreen);
        canvas.box({ 96, 34 }, { 104, 38 }, PadGreen);
        for (const Vector2D &stick : { Vector2D(62, 74), Vector2D(108, 74) }) {
            canvas.circle(stick, 11, PadGreen);
            canvas.circle(stick, 5, PadGreen);
        }
        // the lines: from the end of a job on the left, to the start of the names on the right
        int i = 0;
        for (const PadControl &control : PadLeft) {
            if (control.arrow >= 0)
                canvas.arrow(Point(PadLeftX + 1, row(i) + (control.arrow % 2 == 0 ? 2 : 0)), control.arrow, PadGreen);
            const int length = txt::StringTable::get(control.job).size();
            const gfx::Rgb colour = control.job == txt::Pad_Shift ? PadYellow : PadLine;
            canvas.line(Point(PadLeftX + PadTextX + length*PadCharWidth + 3, row(i) + 3), padPoint(control.at), colour);
            i += control.second >= 0 ? 2 : 1;
        }
        i = 0;
        for (const PadControl &control : padRight()) {
            if (control.face >= 0 && input::faceLabels() == input::FaceLabels::PlayStation)
                canvas.symbol(Point(PadRightX + 1, row(i)), control.face, PadGreen);
            canvas.line(padPoint(control.at), Point(PadRightX - 4, row(i) + 3), PadLine);
            i += control.second >= 0 ? 2 : 1;
        }
        m_controller = gfx::Texture(canvas.image, false);
    }
    const Point topLeft = place(box, PadPicture.x, PadPicture.y);
    const Point bottomRight = place(box, PadPicture.x + PadPicture.width, PadPicture.y + PadPicture.height);
    m_controller.draw(RectF(topLeft.x, topLeft.y, bottomRight.x - topLeft.x, bottomRight.y - topLeft.y));

    for (int side = 0; side < 2; side++) {
        const int x = side == 0 ? PadLeftX : PadRightX;
        int i = 0;
        for (const PadControl &control : side == 0 ? std::vector<PadControl>(std::begin(PadLeft), std::end(PadLeft)) : padRight()) {
            gfx::Font &font = control.job == txt::Pad_Shift ? yellow : green;
            font.draw(control.face >= 0 ? faceName(control.face) : control.name, place(box, x, row(i)));
            font.draw(txt::StringTable::get(control.job), place(box, x + PadTextX, row(i)));
            if (control.second >= 0)
                yellow.draw(txt::StringTable::get(txt::StringVertigo(control.second)), place(box, x + PadTextX, row(++i)));
            i++;
        }
    }
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
