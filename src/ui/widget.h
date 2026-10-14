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

#ifndef UI_WIDGET_H
#define UI_WIDGET_H

#include "util/deferreddeletable.h"
#include "util/geometry2d.h"

#include "game/input.hpp"
#include <vector>


namespace ui {


class Widget : public util::DeferredDeletable
{
public:
    Widget(Widget *parent = NULL, bool visible = true);
    ~Widget();

public:
    void setEnabled(bool);
    void enable();
    void disable();
    void setVisible(bool);
    void hide();
    void show();

public:
    void setParentWidget(Widget *parent);
    bool isEnabled() const { return m_enabled; }
    bool isVisible() const { return m_visible; }
    void setPosition(int x, int y);
    void setPosition(const Point &pos);
    void setSize(int w, int h);
    void setSize(const Size &size);
    void setWidth(int w);
    void setHeight(int h);
    const Rect& rect() const { return m_rect; }
    Size size() const { return m_rect.size(); }
    float width() const { return m_rect.width; }
    float height() const { return m_rect.height; }
    Rect mapToGlobal(Rect rect) const;

    void doDraw();
    bool doMousePressEvent(const Point &pos, input::MouseButton button);
    void doMouseReleaseEvent(const Point &pos, input::MouseButton button);
    void doMouseMoveEvent(const Point &pos);

private:
    virtual void draw() = 0;
    virtual bool mousePressEvent(const Point &pos, input::MouseButton button) { return false; }
    virtual void mouseReleaseEvent(const Point &pos, input::MouseButton button) {}
    virtual void mouseMoveEvent(const Point &pos) {}

private:
    Widget *m_parent;
    bool m_enabled;
    bool m_visible;
    std::vector<Widget*> m_children;
    Rect m_rect;
};


inline void Widget::setSize(int w, int h)
{
    setSize(Size(w, h));
}


} // namespace ui


#endif // UI_WIDGET_H
