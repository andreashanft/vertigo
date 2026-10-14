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

#include "widget.h"
#include "gfx/pipeline.h"
#include <algorithm>


namespace ui {


Widget::Widget(Widget *parent, bool visible) :
    m_parent(NULL),
    m_enabled(true),
    m_visible(visible)
{
    setSize(640, 480);
    setParentWidget(parent);
}


Widget::~Widget()
{
    // each one leaves the list as it goes
    const std::vector<Widget*> children = m_children;
    for (Widget *child : children)
        delete child;
    setParentWidget(NULL);
}


void Widget::setParentWidget(Widget *parent)
{
    if (m_parent) {
        std::vector<Widget*> &siblings = m_parent->m_children;
        siblings.erase(std::remove(siblings.begin(), siblings.end(), this), siblings.end());
    }

    m_parent = parent;

    if (m_parent)
        m_parent->m_children.push_back(this);
}


void Widget::setEnabled(bool enabled)
{
    m_enabled = enabled;
}


void Widget::enable()
{
    m_enabled = true;
}


void Widget::disable()
{
    m_enabled = false;
}


void Widget::setVisible(bool visible)
{
    m_visible = visible;
}


void Widget::hide()
{
    m_visible = false;
}


void Widget::show()
{
    m_visible = true;
}


void Widget::setPosition(const Point &pos)
{
    m_rect.setPos(pos);
}


void Widget::setPosition(int x, int y)
{
    m_rect.setPos(Point(x, y));
}


void Widget::setSize(const Size &size)
{
    m_rect.setSize(size);
}


void Widget::setWidth(int w)
{
    m_rect.width = w;
}


void Widget::setHeight(int h)
{
    m_rect.height = h;
}


Rect Widget::mapToGlobal(Rect rect) const
{
    rect.setPos(rect.pos() + m_rect.pos());
    if (!m_parent)
        return rect;

    return m_parent->mapToGlobal(rect);
}


void Widget::doDraw()
{
    if (m_visible) {
        gfx::Pipeline::pushMatrix();
        gfx::Pipeline::translate(m_rect.x, m_rect.y, 0.0f);

        draw();
        // (over a copy, as the children can change meanwhile)
        const std::vector<Widget*> children = m_children;
        for (Widget *widget : children)
            widget->doDraw();

        gfx::Pipeline::popMatrix();
    }
}


bool Widget::doMousePressEvent(const Point &pos, Qt::MouseButton button)
{
    if (m_enabled && m_visible) {
        if (mousePressEvent(pos, button))
            return true;

        const std::vector<Widget*> children = m_children;
        for (Widget *widget : children) {
            if (widget->doMousePressEvent(pos, button))
                return true;
        }
    }

    return false;
}


void Widget::doMouseReleaseEvent(const Point &pos, Qt::MouseButton button)
{
    if (m_enabled && m_visible) {
        mouseReleaseEvent(pos, button);
        const std::vector<Widget*> children = m_children;
        for (Widget *widget : children)
            widget->doMouseReleaseEvent(pos, button);
    }
}


void Widget::doMouseMoveEvent(const Point &pos)
{
    if (m_enabled && m_visible) {
        mouseMoveEvent(pos);
        const std::vector<Widget*> children = m_children;
        for (Widget *widget : children)
            widget->doMouseMoveEvent(pos);
    }
}


} // namespace ui
