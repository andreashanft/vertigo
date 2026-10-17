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

#include "mastermonitor.h"
#include "hud.h"
#include "missionmenu.h"
#include "fight/scenario.h"
#include "fight/objects/activeobject.h"
#include "fight/objects/navpoint.h"
#include "fight/weapons/torpedo.h"
#include "fight/weapons/unitgun.h"
#include "fight/orders.h"
#include "txt/stringtable.h"
#include "gfx/pipeline.h"
#include "gfx/gl.hpp"
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>


namespace hud {


MasterMonitor::MasterMonitor(HUD *hud, Rect rect, Rect observationRect, txt::DesFile &file) :
    ui::Widget(hud->widget()),
    m_hud(hud),
    m_rect(rect),
    m_observationRect(observationRect),
    m_lamp(hud, file),
    m_voxelRadar(file)
{
    // down to the cargo line, which the original draws over the patch's near edge
    m_voxelRadar.limitHeight(rect.height - 8 - m_voxelRadar.rect().y);
}


void MasterMonitor::draw()
{
    if (m_hud->scenario()->monitorBlanked(1))
        return;
    // a menu in place of the target and the voxel radar
    if (m_hud->menu().onScreen())
    {
        m_lamp.draw(m_hud->scenario()->target().isLocked());
        m_hud->menu().drawOnScreen();
        return;
    }

    Point pos;
    bool isFriend = true;
    String txtName = txt::StringTable::get(txt::Target_NoTargetLocked);
    String txtAction = "";
    String txtCargo = "-------";
    String txtDistance = "---";
    String txtHeight = "---";
    String txtVelocity = "---";
    String txtNoise = "---";
    String txtRange = "----";
    String txtGun = "----";
    String txtTorpedo = "----";

    fight::Target &target = m_hud->scenario()->target();
    fight::ActiveObject *object = target.locked();
    if (object != nullptr)
    {
        isFriend = m_hud->scenario()->isFriend(fight::Scenario::PlayerFaction, object->iff());
        txtName = object->name();
        // what it does: its orders, or paralyzed
        if (m_hud->scenario()->sonar().detectActivity())
        {
            if (object->eventParalyze()->isCompleted() && object->shockShield() < 0)
                txtAction = txt::StringTable::get(txt::Fight81);
            else if (object->orders())
                txtAction = txt::StringTable::get(object->orders()->activity());
        }
        if (object->isIdentified())
            txtCargo = object->cargo();
        else
            txtCargo = txt::StringTable::get(txt::Target_NotIdentified);

        int distance = static_cast<int>(glm::round(target.distance()));
        txtDistance = String("%1M").arg(distance);
        int height = static_cast<int>(glm::round(object->center().z - m_hud->scenario()->surface().heightAt(object->center().x, object->center().y)));
        txtHeight = String("%1M").arg(height);

        txtVelocity = String("%1KMH").arg(static_cast<int>(glm::round(3.6f*glm::length(object->velocity()))));
        txtNoise = String("%1").arg(object->noise());

        // the target's sensor range: with any sonar, once the contact is active
        if (!target.isPassive() && object->sensor() > 0)
            txtRange = String("%1M").arg(object->sensorRange());
        if (m_hud->scenario()->sonar().detectWeapons() && !target.isPassive())
        {
            // the short names of its gun and torpedoes (GLOBWEAP)
            if (object->gun())
                txtGun = object->gun()->shortName();
            if (const fight::TorpedoType *torpedo = object->hasTorpedoes() ? fight::TorpedoType::get(object->torpedoType()) : nullptr)
                txtTorpedo = torpedo->shortName;
        }
    }

    fight::NavPoint *navPoint = target.lockedNavPoint();
    if (navPoint != nullptr)
    {
        txtName = String("NAV %1").arg(static_cast<char>('A' + navPoint->num()));
        txtDistance = String("%1M").arg(static_cast<int>(glm::round(target.distance())));
        txtHeight = "10M";
        txtVelocity = "0KMH";
        txtNoise = "0";
    }

    m_lamp.draw(target.isLocked());
    // the voxel radar below, the target's picture above it
    if (m_voxelRadar.isValid())
    {
        const Rect &radar = m_voxelRadar.rect();
        const Point topLeft = m_hud->project(m_rect.pos() + radar.pos());
        const Point bottomRight = m_hud->project(m_rect.pos() + radar.pos() + Point(radar.width, radar.height));
        m_voxelRadar.draw(m_hud->scenario(), RectF(topLeft.x, topLeft.y, bottomRight.x - topLeft.x, bottomRight.y - topLeft.y));
    }

    fight::ActiveObject *pictured = target.locked();
    const String picture = pictured ? pictured->observationImage() : String();
    if (!picture.isEmpty())
    {
        // big ships: a picture of the ship (SHP*.IMG, the size of the observation area)
        auto it = m_pictures.find(picture);
        if (it == m_pictures.end())
            it = m_pictures.emplace(picture, gfx::Texture(m_hud->getImage(picture), true)).first;
        const Point observeCenter = m_hud->project(m_rect.pos() + m_observationRect.center());
        it->second.draw(observeCenter.x - it->second.width()/2, observeCenter.y - it->second.height()/2);
    }
    else if (target.isLocked())
    {
        Point observePos = m_hud->project(m_rect.pos() + m_observationRect.pos());
        Point observeCenter = m_hud->project(m_rect.pos() + m_observationRect.center());
        Matrix m(1);
        m = glm::translate(m, Vector3D(observeCenter, -0.5f));
        m = glm::scale(m, Vector3D(observeCenter.x - observePos.x, observePos.y - observeCenter.y, 0.5f));
        m *= m_hud->scenario()->cameraMatrix();
        m = glm::translate(m, m_hud->scenario()->position());

        fight::Object *obj = target.object();
        const fight::BoundingBox& box = obj->box();
        m = glm::scale(m, Vector3D(2/glm::length(box.dim())));
        m = glm::translate(m, -0.5f*(box.maxPoint() + box.minPoint()));

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glFrontFace(GL_CW);
        gfx::Pipeline::setModelView(m);
        obj->drawPreview();
        gfx::Pipeline::setModelView(Matrix(1));
        glDisable(GL_CULL_FACE);
        glFrontFace(GL_CCW);
        glDisable(GL_DEPTH_TEST);
    }

    pos = m_hud->project(m_rect.pos() + Point(m_rect.width/2, 4));
    (isFriend ? m_hud->fontGreen() : m_hud->fontRed()).draw(txtName, Rect(pos.x - 100, pos.y, 200, -1), true, false);

    pos.y += 12;
    m_hud->fontGreen().draw(txtAction, Rect(pos.x - 100, pos.y, 200, -1), true, false);

    pos = m_hud->project(m_rect.pos() + Point(m_rect.width/2, m_rect.height - 12));
    m_hud->fontGreen().draw(String("%1%2").arg(txt::StringTable::get(txt::Target_Cargo)).arg(txtCargo), Rect(pos.x - 100, pos.y, 200, -1), true, false);

    pos = m_hud->project(m_rect.pos() + Point(0, 32));
    m_hud->fontGreen().draw(String("D:%1").arg(txtDistance), pos);
    
    pos.y += 12;
    m_hud->fontGreen().draw(String("H:%1").arg(txtHeight), pos);

    pos.y += 12;
    m_hud->fontGreen().draw(String("V:%1").arg(txtVelocity), pos);

    pos.y += 12;
    m_hud->fontGreen().draw(String("N:%1").arg(txtNoise), pos);

    pos = m_hud->project(m_rect.pos() + Point(m_rect.width, 32));
    pos.x -= 36;
    m_hud->fontGreen().draw(String("R:%1").arg(txtRange), pos);

    pos.y += 12;
    m_hud->fontGreen().draw(String("G:%1").arg(txtGun), pos);

    pos.y += 12;
    m_hud->fontGreen().draw(String("T:%1").arg(txtTorpedo), pos);
}


} // namespace hud
