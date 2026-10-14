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

#include "slavemonitor.h"
#include "hud.h"
#include "fight/scenario.h"
#include "fight/weapons/gun.h"
#include "fight/weapons/torpedo.h"
#include "txt/desfile.h"
#include "game/boat.h"
#include "game/items.h"


namespace hud {


SlaveMonitor::SlaveMonitor(HUD *hud, const txt::DesFile &file, Page page) :
    ui::Widget(hud->widget()),
    m_hud(hud),
    m_rect(file.value("X1").toInt(), file.value("Y1").toInt(), file.value("Width").toInt(), file.value("Height").toInt()),
    m_page(page),
    m_lamp(hud, file),
    m_tableTopLeft(file.value("WS_TopLeftX").toInt(), file.value("WS_TopLeftY").toInt()),
    m_nameX(file.value("WS_NameX").toInt()),
    m_ammoX(file.value("WS_AmmoX").toInt()),
    m_statusX(file.value("WS_StatusX").toInt()),
    m_spacingY(file.value("WS_SpacingY").toInt())
{
    // e.g. GATO1F: Gator, damage level 1 (intact) to 4, front
    const String prefix = file.value("ArmorImagePrefix").toString();
    static const char sides[4] = { 'f', 'l', 'r', 'b' };
    for (int level = 0; level < 4; level++)
        for (int side = 0; side < 4; side++)
            m_armor[level][side] = gfx::Texture(hud->getImage(String("%1%2%3").arg(prefix).arg(level + 1).arg(sides[side])), true);
}


void SlaveMonitor::draw()
{
    const Rect rect = m_hud->projectCenter(m_rect);
    m_lamp.draw(m_page == Page::Armor);
    switch (m_page)
    {
    case Page::Weapon: drawWeapon(rect); break;
    case Page::Armor: drawArmor(rect); break;
    case Page::Systems: drawSystems(rect); break;
    }
}


gfx::Texture* SlaveMonitor::picture(const String &name)
{
    if (name.isEmpty())
        return nullptr;
    auto it = m_pictures.find(name);
    if (it == m_pictures.end())
        it = m_pictures.emplace(name, gfx::Texture(m_hud->getImage(name), true)).first;
    return it->second.isValid() ? &it->second : nullptr;
}


// three centred lines (the system, its state, and for the magazine the selected torpedo), with
// the picture of the gun or torpedo below
void SlaveMonitor::drawWeapon(const Rect &rect)
{
    const fight::Player *player = m_hud->scenario()->player();
    const game::Boat *boat = m_hud->boat();
    StringList lines;
    String image;
    auto itemName = [](int item) {
        const game::Items::Item *info = game::Items::get(item);
        return info ? info->longname : String("---");
    };
    auto itemImage = [](int item) {
        const game::Items::Item *info = game::Items::get(item);
        return info ? info->imgname : String();
    };
    switch (m_weapon)
    {
    case Weapon::Gun:
        lines << itemName(boat->gun()) << player->playerGun().stateText()
              << (player->playerGun().usesAmmo() ? String::number(player->playerGun().ammo()) : String());
        image = itemImage(boat->gun());
        break;
    case Weapon::Magazine:
        {
            const int selected = player->selectedTorpedo();
            const fight::TorpedoType *torpedo = selected >= 0 ? fight::TorpedoType::get(player->torpedoes()[selected] - 1) : nullptr;
            // GLOBWEAP [TorpedoMagazineStateN]: Ready, Rearming, Empty
            lines << itemName(boat->magazine()) << (!torpedo ? "EMPTY" : player->torpedoReady() ? "READY" : "REARMING");
            if (torpedo)
            {
                lines << torpedo->name;
                image = torpedo->image;
            }
        }
        break;
    case Weapon::Turret1:
    case Weapon::Turret2:
        {
            const int index = m_weapon == Weapon::Turret1 ? 0 : 1;
            const int item = index == 0 ? boat->tur1() : boat->tur2();
            const fight::PlayerTurret *turret = player->turret(index);
            lines << (turret ? itemName(item) : String("TUR%1").arg(index + 1));
            if (turret)
            {
                lines << turret->stateText();
                const game::Items::Item *software = game::Items::get(index == 0 ? boat->tur1soft() : boat->tur2soft());
                if (software)
                    lines << software->longname;
                image = itemImage(item);
            }
            else
                lines << "---";
        }
        break;
    }

    gfx::Font &font = m_hud->fontGreen();
    int y = rect.y + m_tableTopLeft.y;
    for (const String &line : lines)
    {
        font.draw(line.toUpper(), Rect(rect.x, y, rect.width, -1), true, false);
        y += m_spacingY - 1;
    }
    // at the bottom; the 48x48 pictures don't fit below three lines on a 70 pixel monitor,
    // so they shrink to the space left
    if (gfx::Texture *texture = picture(image))
    {
        const int bottom = rect.y + rect.height - 2;
        const float scale = std::min(1.0f, float(bottom - y)/texture->height());
        if (scale > 0)
        {
            const float width = texture->width()*scale;
            const float height = texture->height()*scale;
            texture->draw(RectF(rect.x + (rect.width - width)/2, bottom - height, width, height));
        }
    }
}


void SlaveMonitor::drawArmor(const Rect &rect)
{
    const fight::Player *player = m_hud->scenario()->player();
    gfx::Font &font = m_hud->fontGreen();
    // the pieces make up a 64x64 silhouette: front on top, left and right side by side, back
    // below. It shrinks to fit between the front's percentage and the back's
    const int top = rect.y + 12 + font.height() + 1;
    const int bottom = rect.y + rect.height - 10 - 1;
    const float scale = std::max(0.0f, std::min(1.0f, (bottom - top)/64.0f));
    const float x = rect.x + (rect.width - 64*scale)/2;
    const float y = (top + bottom - 64*scale)/2;
    for (int side = 0; side < 4; side++)
    {
        // level 1 above 75 % of the shield, 4 below 25 %
        const float left = player->shieldMax(side) > 0 ? float(player->shield(side))/player->shieldMax(side) : 0;
        const int level = left > 0.75f ? 0 : left > 0.5f ? 1 : left > 0.25f ? 2 : 3;
        gfx::Texture &texture = m_armor[level][side];
        const int frontHeight = m_armor[level][0].height();
        float offsetX = 0;
        float offsetY = 0;
        switch (side)
        {
        case 0: break;
        case 1: offsetY = frontHeight; break;
        case 2: offsetX = 64 - texture.width(); offsetY = frontHeight; break;
        case 3: offsetY = 64 - texture.height(); break;
        }
        texture.draw(RectF(x + offsetX*scale, y + offsetY*scale, texture.width()*scale, texture.height()*scale));
    }

    // the title, and what is left of each side around the silhouette: front above, back below
    auto percent = [player](int side) {
        return String("%1%").arg(player->shieldMax(side) > 0 ? std::max(0, 100*player->shield(side)/player->shieldMax(side)) : 0);
    };
    font.draw("ARMOR STATE", Rect(rect.x, rect.y + 2, rect.width, -1), true, false);
    font.draw(percent(0), Rect(rect.x, rect.y + 12, rect.width, -1), true, false);
    font.draw(percent(3), Rect(rect.x, rect.y + rect.height - 10, rect.width, -1), true, false);
    font.draw(percent(1), rect.x + 3, rect.y + rect.height/2 - 3);
    font.draw(percent(2), rect.x + rect.width - 3 - font.width(percent(2)), rect.y + rect.height/2 - 3);
}


void SlaveMonitor::drawSystems(const Rect &rect)
{
    const fight::Player *player = m_hud->scenario()->player();
    gfx::Font &font = m_hud->fontGreen();
    int y = rect.y + m_tableTopLeft.y;
    auto row = [&](const String &name, const String &ammo, const String &status) {
        font.draw(name, rect.x + m_nameX, y);
        font.draw(ammo, rect.x + m_ammoX, y);
        font.draw(status, rect.x + m_statusX, y);
        y += m_spacingY;
    };
    auto gunRow = [&](const String &name, const fight::Gun *gun) {
        if (gun)
            row(name, gun->usesAmmo() ? String::number(gun->ammo()) : String("--"), gun->stateName());
        else
            row(name, "--", "---");
    };

    int armor = 0;
    for (int side = 0; side < 4; side++)
        armor += player->shieldMax(side) > 0 ? std::max(0, 100*player->shield(side)/player->shieldMax(side)) : 0;
    row("ARMO", "--", String("%1%").arg(armor/4));
    gunRow("GUN", &player->playerGun());
    // the selected torpedo, how many are left, ready or rearming
    const fight::TorpedoType *torpedo = player->selectedTorpedo() >= 0 ? fight::TorpedoType::get(player->torpedoes()[player->selectedTorpedo()] - 1) : nullptr;
    if (torpedo)
        row(torpedo->shortName, String::number(player->torpedoes().size()), player->torpedoReady() ? "RDY" : "ARM");
    else
        row("TORP", "--", "EMP");
    // turrets: their state (SYN, SCA, LOC, FIR, VOL, STP, ...)
    for (int i = 0; i < 2; i++)
    {
        const fight::PlayerTurret *turret = player->turret(i);
        const String name = String("TUR%1").arg(i + 1);
        if (turret)
            row(name, turret->gun().usesAmmo() ? String::number(turret->gun().ammo()) : String("--"), turret->stateName());
        else
            row(name, "--", "---");
    }
}


} // namespace hud
