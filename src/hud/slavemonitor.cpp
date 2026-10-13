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
    const QString prefix = file.value("ArmorImagePrefix").toString();
    static const char sides[4] = { 'f', 'l', 'r', 'b' };
    for (int level = 0; level < 4; level++)
        for (int side = 0; side < 4; side++)
            m_armor[level][side] = gfx::Texture(hud->getImage(QString("%1%2%3").arg(prefix).arg(level + 1).arg(sides[side])), true);
}


void SlaveMonitor::togglePage()
{
    if (m_detail != Detail::None)
        m_detail = Detail::None;
    else
        m_page = m_page == Page::Armor ? Page::Systems : Page::Armor;
}


void SlaveMonitor::draw()
{
    const Rect rect = m_hud->projectCenter(m_rect);
    m_lamp.draw(m_page == Page::Armor && m_detail == Detail::None);
    if (m_detail != Detail::None)
        drawDetail(rect);
    else if (m_page == Page::Armor)
        drawArmor(rect);
    else
        drawSystems(rect);
}


gfx::Texture* SlaveMonitor::picture(int item)
{
    const game::Items::Item *info = game::Items::get(item);
    if (!info)
        return nullptr;
    auto it = m_pictures.find(item);
    if (it == m_pictures.end())
        it = m_pictures.emplace(item, gfx::Texture(m_hud->getImage(info->imgname), true)).first;
    return it->second.isValid() ? &it->second : nullptr;
}


void SlaveMonitor::drawDetail(const Rect &rect)
{
    const fight::Player *player = m_hud->scenario()->player();
    const game::Boat *boat = m_hud->boat();
    gfx::Font &font = m_hud->fontGreen();
    auto ammo = [](const fight::Gun &gun) { return gun.usesAmmo() ? QString::number(gun.ammo()) : QString("--"); };

    int item = 0;
    QString title;
    QStringList lines;
    float condition = 1;
    switch (m_detail)
    {
    case Detail::Gun:
        item = boat->gun();
        title = "GUN";
        lines << ammo(player->playerGun()) << player->playerGun().stateName();
        condition = boat->defects()[game::Boat::DefectGun];
        break;
    case Detail::Magazine:
        {
            // the magazine, with the selected torpedo, how many are left and whether it is ready
            item = boat->magazine();
            title = "TORP";
            const int selected = player->selectedTorpedo();
            const fight::TorpedoType *torpedo = selected >= 0 ? fight::TorpedoType::get(player->torpedoes()[selected] - 1) : nullptr;
            if (torpedo)
                lines << torpedo->shortName << QString::number(player->torpedoes().size()) << (player->torpedoReady() ? "RDY" : "ARM");
            else
                lines << "--" << "0" << "EMP";
            condition = boat->defects()[game::Boat::DefectToMa];
        }
        break;
    case Detail::Turret1:
    case Detail::Turret2:
        {
            const int index = m_detail == Detail::Turret1 ? 0 : 1;
            item = index == 0 ? boat->tur1() : boat->tur2();
            title = QString("TUR%1").arg(index + 1);
            const fight::PlayerTurret *turret = player->turret(index);
            if (turret)
                lines << turret->softwareName() << ammo(turret->gun()) << turret->stateName();
            condition = boat->defects()[index == 0 ? game::Boat::DefectTur1 : game::Boat::DefectTur2];
        }
        break;
    case Detail::None:
        return;
    }

    // the name on top, the picture on the left, the values beside it
    const game::Items::Item *info = game::Items::get(item);
    font.draw(info ? info->longname : title, Rect(rect.x, rect.y + 3, rect.width, -1), true, false);
    if (gfx::Texture *texture = picture(item))
        texture->draw(rect.x + 4, rect.y + rect.height - 4 - texture->height());
    if (!info)
    {
        font.draw("---", Rect(rect.x, rect.y + rect.height/2, rect.width, -1), true, false);
        return;
    }
    lines << QString("%1%").arg(int(100*condition + 0.5f));
    int y = rect.y + 16;
    for (const QString &line : lines)
    {
        font.draw(line, rect.x + 56, y);
        y += m_spacingY;
    }
}


void SlaveMonitor::drawArmor(const Rect &rect)
{
    const fight::Player *player = m_hud->scenario()->player();
    // the pieces make up a 64x64 silhouette: front on top, left and right side by side, back below
    const int x = rect.x + (rect.width - 64)/2;
    const int y = rect.y + (rect.height - 64)/2;
    for (int side = 0; side < 4; side++)
    {
        // level 1 above 75 % of the shield, 4 below 25 %
        const float left = player->shieldMax(side) > 0 ? float(player->shield(side))/player->shieldMax(side) : 0;
        const int level = left > 0.75f ? 0 : left > 0.5f ? 1 : left > 0.25f ? 2 : 3;
        gfx::Texture &texture = m_armor[level][side];
        const int frontHeight = m_armor[level][0].height();
        switch (side)
        {
        case 0: texture.draw(x, y); break;
        case 1: texture.draw(x, y + frontHeight); break;
        case 2: texture.draw(x + 64 - texture.width(), y + frontHeight); break;
        case 3: texture.draw(x, y + 64 - texture.height()); break;
        }
    }
}


void SlaveMonitor::drawSystems(const Rect &rect)
{
    const fight::Player *player = m_hud->scenario()->player();
    gfx::Font &font = m_hud->fontGreen();
    int y = rect.y + m_tableTopLeft.y;
    auto row = [&](const QString &name, const QString &ammo, const QString &status) {
        font.draw(name, rect.x + m_nameX, y);
        font.draw(ammo, rect.x + m_ammoX, y);
        font.draw(status, rect.x + m_statusX, y);
        y += m_spacingY;
    };
    auto gunRow = [&](const QString &name, const fight::Gun *gun) {
        if (gun)
            row(name, gun->usesAmmo() ? QString::number(gun->ammo()) : QString("--"), gun->stateName());
        else
            row(name, "--", "---");
    };

    int armor = 0;
    for (int side = 0; side < 4; side++)
        armor += player->shieldMax(side) > 0 ? std::max(0, 100*player->shield(side)/player->shieldMax(side)) : 0;
    row("ARMO", "--", QString("%1%").arg(armor/4));
    gunRow("GUN", &player->playerGun());
    // the selected torpedo, how many are left, ready or rearming
    const fight::TorpedoType *torpedo = player->selectedTorpedo() >= 0 ? fight::TorpedoType::get(player->torpedoes()[player->selectedTorpedo()] - 1) : nullptr;
    if (torpedo)
        row(torpedo->shortName, QString::number(player->torpedoes().size()), player->torpedoReady() ? "RDY" : "ARM");
    else
        row("TORP", "--", "EMP");
    // turrets: their state (SYN, SCA, LOC, FIR, VOL, STP, ...)
    for (int i = 0; i < 2; i++)
    {
        const fight::PlayerTurret *turret = player->turret(i);
        const QString name = QString("TUR%1").arg(i + 1);
        if (turret)
            row(name, turret->gun().usesAmmo() ? QString::number(turret->gun().ammo()) : QString("--"), turret->stateName());
        else
            row(name, "--", "---");
    }
}


} // namespace hud
