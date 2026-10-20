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

#include "button.h"
#include "frame.h"
#include "gfx/font.h"
#include "gfx/image.h"
#include "sfx/soundsystem.h"
#include <SDL3/SDL_platform_defines.h>
#include <algorithm>

namespace ui {

Frame::Frame(std::function<void()> &&funcClose, ui::Widget *parent) :
    Label(parent),
    m_funcClose(std::move(funcClose)),
    m_lblTitle(this),
    m_btnClose([this]() { closeFrame(); }, this)
{
}

void Frame::setupFrame(const Size &size, const String &title, bool closable)
{
    const gfx::ColorTable colorTable("gfx:pal/gui/border.pal");

    m_lblTitle.setFont(gfx::Font::Medium);
    m_lblTitle.setText(title);
    m_lblTitle.setAlignment(ui::Label::AlignHCenter);
    m_lblTitle.setPosition(0, 1);
    m_lblTitle.setWidth(size.width);

    gfx::Image left = gfx::Image::load("gfx:img/desktop/gui/borl.img", colorTable);
    gfx::Image right = gfx::Image::load("gfx:img/desktop/gui/borr.img", colorTable);
    gfx::Image bottom = gfx::Image::load("gfx:img/desktop/gui/borb1.img", colorTable);

    gfx::Texture texture;
    texture.createEmpty(size, gfx::Texture::RGB);

    {
        std::vector<gfx::Image> images;
        const int total = size.width - 30;
        static const int id[] = { 1,2,3,3,4,5,2,4,3,1 };
        int sum = 0;
        for (int i = 0; i < 10; i++) {
            if (i == 3 && total < 270)
                continue;

            gfx::Image image = getBorder(colorTable, id[i]);
            if (sum + image.width() > total)
                break;
            sum += image.width();
            images.push_back(image);
        }

        int beg = 13;
        updateBorder(texture, colorTable, beg, 9);
        beg += 2 + (total - sum) / 2;

        for (const gfx::Image &image : images) {
            texture.update(beg, 0, image);
            beg += image.width();
        }

        updateBorder(texture, colorTable, beg - 1, 9);
    }

    texture.update(                0,                  0, left.copy(0, 0, left.width(), size.height));
    texture.update(size.width -  7,                  0, right.copy(0, 0, right.width(), size.height));
    texture.update(                0, size.height -  7, bottom.copy(0, 0, size.width, bottom.height()));
    texture.update(                0,                  0, gfx::Image::load("gfx:img/desktop/gui/edgetl.img", colorTable));
    texture.update(size.width - 12,                  0, gfx::Image::load("gfx:img/desktop/gui/edgetr.img", colorTable));
    texture.update(size.width - 12, size.height - 17, gfx::Image::load("gfx:img/desktop/gui/edgebr.img", colorTable));
    texture.update(                0, size.height - 17, gfx::Image::load("gfx:img/desktop/gui/edgebl.img", colorTable));

    texture.update((size.width - 170) / 2, 0, gfx::Image::load("gfx:img/desktop/gui/bortw.img", colorTable));

    const gfx::Image exit = gfx::Image::load("gfx:img/desktop/gui/gdexitu.img", colorTable);
    m_btnClose.setVisible(closable);
    m_btnClose.setTexture(exit);
    m_btnClose.setSize(exit.width(), exit.height());
    m_btnClose.setPressedTexture(gfx::Image::load("gfx:img/desktop/gui/gdexitd.img", colorTable));

    setTexture(texture);
    setSize(size);

    sfx::SoundSystem::get()->sound(sfx::FrameOpen)->play();
}

int Frame::updateBorder(gfx::Texture texture, const gfx::ColorTable &colorTable, int x, int id)
{
    const gfx::Image image = gfx::Image::load(String("gfx:img/desktop/gui/bort%1.img").arg(id), colorTable);
    if (texture.width() - (x + image.width()) < 0) {
        const int width = texture.width() - x;
        texture.update(x, 0, image.copy(0, 0, width, image.height()));
        return width;
    } else {
        texture.update(x, 0, image);
        return image.width();
    }
}

void Frame::closeFrame()
{
    sfx::SoundSystem::get()->sound(sfx::FrameClose)->play();
    m_funcClose();
}

bool Frame::mousePressEvent(const Point &pos, input::MouseButton button)
{
    const Rect frame = mapToGlobal(Rect(Point(0), size()));
#if defined(SDL_PLATFORM_IOS)
    if (!frame.contains(pos)) {
        if (!m_btnClose.isVisible())
            return false;
        closeFrame();
        return true;
    }
#endif
    // the content is where the room's or the depot's picture is
    const Rect content = mapToGlobal(Rect(9, 15, int(width()) - 18, int(height()) - 24));
    const Rect close = m_btnClose.mapToGlobal(Rect(Point(0), m_btnClose.size()));
    if (button == input::LeftButton && frame.contains(pos) && !content.contains(pos) && !(m_btnClose.isVisible() && close.contains(pos))) {
        m_dragging = true;
        m_dragOffset = pos - frame.pos();
    }
    // (left to the others, as dragging doesn't click)
    return false;
}

void Frame::mouseReleaseEvent(const Point &pos, input::MouseButton button)
{
    if (button == input::LeftButton)
        m_dragging = false;
}

void Frame::mouseMoveEvent(const Point &pos)
{
    if (!m_dragging)
        return;

    // all of the window stays on the screen
    const Point origin = mapToGlobal(Rect(Point(0), size())).pos() - rect().pos();
    const Point to = pos - m_dragOffset;
    setPosition(std::clamp(to.x, 0, std::max(0, 640 - int(width()))) - origin.x, std::clamp(to.y, 0, std::max(0, 480 - int(height()))) - origin.y);
}

gfx::Image Frame::getBorder(const gfx::ColorTable &colorTable, int id)
{
    return gfx::Image::load(String("gfx:img/desktop/gui/bort%1.img").arg(id), colorTable);
}

} // namespace ui
