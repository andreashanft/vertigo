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

#include "arrow.h"
#include "gfx/colortable.h"
#include "gfx/image.h"

namespace ui {

Arrow::Arrow(const String &dir, const Point &pos, bool large, std::function<void(int)> &&funcClicked, Widget *parent) :
    m_position(pos),
    m_value(0),
    m_large(large),
    m_funcClicked(std::move(funcClicked)),
    m_label(parent),
    m_button([this]() { m_funcClicked(m_value); }, parent)
{
    const gfx::ColorTable colorTable("gfx:pal/gui/border.pal");

    m_left = dir.endsWith("Left");
    m_top = dir.startsWith("Top");

    char arrowX = 'l';
    char arrowY = 't';
    if (m_left)
        arrowX = 'r';
    if (m_top)
        arrowY = 'b';

    m_label.setTexture(gfx::Image::load(String("gfx:img/desktop/gui/arr%1%2%3.img").arg(arrowY).arg(arrowX).arg(large ? "med" : "sma"), colorTable));
    m_label.setPosition(pos);

    m_button.setFont(m_large ? gfx::Font::Medium : gfx::Font::Small);
    m_button.setOffset(0);
}

void Arrow::hide()
{
    m_label.hide();
    m_button.hide();
}

void Arrow::show()
{
    m_label.show();
    m_button.show();
}

bool Arrow::isVisible()
{
    return m_label.isVisible();
}

void Arrow::setValue(int value)
{
    m_value = value;
}

void Arrow::setText(const String &text)
{
    Point offset(9, 2);
    if (m_large)
        offset = Point(11, 3);
    Point offsetArrow(0, 0);
    const int width = gfx::Font(m_large ? gfx::Font::Medium : gfx::Font::Small).width(text);

    if (m_top) {
        offsetArrow.y = 1 - offset.x;
        offset.y = -offset.y;
    }

    if (m_left) {
        offsetArrow.x = 1 - offset.x;
        offset.x = -1 - width;
    }

    m_button.setText(text);
    m_label.setPosition(m_position + offsetArrow);
    m_button.setPosition(m_position + offsetArrow + offset);
}

} // namespace ui
