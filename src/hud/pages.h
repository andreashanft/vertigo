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

#ifndef HUD_PAGES_H
#define HUD_PAGES_H


#include "util/geometry2d.h"
#include "ui/widget.h"
#include "gfx/texture.h"
#include "util/string.hpp"
#include <vector>


namespace gfx { class Font; }


namespace hud {


class HUD;


// The two pages the original lays over the view (docs/ROADMAP.md, appendix E): the key help
// (H, its pages from VFX/COCKPIT/HELP0-2.TXT, H turns them, Esc closes) and the log of the radio
// messages and the units' events (M; Esc or M closes). The mission goes on underneath.
class Pages : public ui::Widget
{
public:
    enum class Page { None, Help, Radio };

public:
    Pages(HUD *hud);

public:
    Page page() const { return m_page; }
    void open(Page page);
    void close() { m_page = Page::None; }
    // the help's next page
    void next();

protected:
    void draw();

private:
    struct HelpLine
    {
        String column[4];   // key, what it does, a second key and what it does
    };
    void loadHelp();
    Point place(const Rect &box, int x, int y) const;
    void drawFrame(const Rect &box);
    void drawDots(int x1, int x2, int y);
    void drawHelp(const Rect &box);
    void drawRadio(const Rect &box);
    // right-aligned at the box's bottom right
    void drawFooter(const Rect &box, const String &text);

private:
    HUD *m_hud;
    Page m_page = Page::None;
    int m_helpPage = 0;
    std::vector<std::vector<HelpLine>> m_help;
    gfx::Texture m_black;
    gfx::Texture m_dot;
};


} // namespace hud


#endif // HUD_PAGES_H
