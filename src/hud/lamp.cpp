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

#include "lamp.h"
#include "hud.h"
#include "txt/desfile.h"
#include "util/datapath.h"

#include <QFile>
#include <QImage>


namespace hud {


Lamp::Lamp(HUD *hud, const txt::DesFile &file) :
    m_hud(hud),
    m_rect(file.value("LampX1").toInt(), file.value("LampY1").toInt(), file.value("LampWidth").toInt(), file.value("LampHeight").toInt())
{
    if (!file.contains("LampDataName") || m_rect.width <= 0 || m_rect.height <= 0)
        return;
    // e.g. 001\smon0: VFX/COCKPIT/001/SMON0.D16
    QFile data(util::DataPath::resolve(String("vfx:cockpit/%1.d16").arg(file.value("LampDataName").toString().replace('\\', '/'))));
    const QByteArray bytes = data.open(QIODevice::ReadOnly) ? data.readAll() : QByteArray();
    const int pixels = m_rect.width*m_rect.height;
    if (bytes.size() < 4*pixels)
        return;
    const uchar *p = reinterpret_cast<const uchar *>(bytes.constData());
    for (int frame = 0; frame < 2; frame++)
    {
        QImage image(m_rect.width, m_rect.height, QImage::Format_RGB32);
        for (int i = 0; i < pixels; i++, p += 2)
        {
            const int c = p[0] | (p[1] << 8);
            image.setPixel(i % m_rect.width, i/m_rect.width, qRgb(((c >> 11) & 31)*255/31, ((c >> 5) & 63)*255/63, (c & 31)*255/31));
        }
        m_frames[frame] = gfx::Texture(image, false);
    }
    m_valid = true;
}


void Lamp::draw(bool upper)
{
    if (!m_valid)
        return;
    const Rect rect = m_hud->projectCenter(m_rect);
    m_frames[upper ? 1 : 0].draw(rect.x, rect.y);
}


} // namespace hud
