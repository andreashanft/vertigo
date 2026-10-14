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

#include "menu.h"
#include "sfx/soundsystem.h"
#include <QMouseEvent>
#include <cmath>


namespace game {


Menu::Menu() :
    m_rootWidget(NULL),
    m_click(true)
{

}


void Menu::draw()
{
    setupGL();

    if (m_rootWidget)
        m_rootWidget->doDraw();
}


void Menu::setClick(bool enabled)
{
    m_click = enabled;
}


void Menu::setRootWidget(ui::Widget *widget)
{
    m_rootWidget = widget;
}


Point Menu::imagePos(QMouseEvent *event)
{
    const PointD pos = screenToImage(PointD(event->pos().x(), event->pos().y()));
    return Point(int(std::lround(pos.x)), int(std::lround(pos.y)));
}


void Menu::mousePressEvent(QMouseEvent *event)
{
    if (m_rootWidget)
        if (m_rootWidget->doMousePressEvent(imagePos(event), event->button()) && m_click)
            sfx::SoundSystem::get()->sound(sfx::Click)->play();
}


void Menu::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_rootWidget)
        m_rootWidget->doMouseReleaseEvent(imagePos(event), event->button());
}


void Menu::mouseMoveEvent(QMouseEvent *event)
{
    if (m_rootWidget)
        m_rootWidget->doMouseMoveEvent(imagePos(event));
}


} // namespace game
