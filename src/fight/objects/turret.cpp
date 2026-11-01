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

#include "turret.h"
#include "fight/scenario.h"
#include "gfx/pipeline.h"
#include <glm/trigonometric.hpp>


namespace fight {


Turret::Turret(Scenario *scenario, txt::DesFile &file) :
    Object(scenario, file),
    m_armLeft(nullptr),
    m_armRight(nullptr)
{
    file.setSection("cluster");
    m_body = scenario->moduleManager().get(file.value("body").toString());
    m_box = m_body->box();

    if (file.contains("arml") && file.value("arml").toString() != "no")
    {
        m_armLeft = scenario->moduleManager().get(file.value("arml").toString());
        m_armLeftPosition.x = file.value("ArmLOffsetX").toFloat();
        m_armLeftPosition.y = file.value("ArmLOffsetY").toFloat();
        m_armLeftPosition.z = file.value("ArmLOffsetZ").toFloat();
        m_box.add(m_armLeft->box().translate(m_armLeftPosition));
    }

    if (file.contains("armr") && file.value("armr").toString() != "no")
    {
        m_armRight = scenario->moduleManager().get(file.value("armr").toString());
        m_armRightPosition.x = file.value("ArmROffsetX").toFloat();
        m_armRightPosition.y = file.value("ArmROffsetY").toFloat();
        m_armRightPosition.z = file.value("ArmROffsetZ").toFloat();
        m_box.add(m_armRight->box().translate(m_armRightPosition));
    }
}


void Turret::draw()
{
    gfx::Pipeline::translate(m_position);
    //gfx::Pipeline::scale(m_scale);
    m_body->draw();

    if (m_armLeft != nullptr)
    {
        gfx::Pipeline::pushMatrix();
        gfx::Pipeline::translate(m_armLeftPosition);
        gfx::Pipeline::rotate(glm::degrees(m_armPitch), Vector3D(1, 0, 0));
        m_armLeft->draw();
        gfx::Pipeline::popMatrix();
    }

    if (m_armRight != nullptr)
    {
        gfx::Pipeline::pushMatrix();
        gfx::Pipeline::translate(m_armRightPosition);
        gfx::Pipeline::rotate(glm::degrees(m_armPitch), Vector3D(1, 0, 0));
        m_armRight->draw();
        gfx::Pipeline::popMatrix();
    }
}


bool Turret::intersect(const Vector3D &start, const Vector3D &dir, float radius, float &distance, Vector3D &normal)
{
    bool collision = m_body->intersect(start - m_position, dir, radius, distance, normal);

    float d;
    Vector3D norm;
    if (m_armLeft != nullptr && m_armLeft->intersect(start - m_position - m_armLeftPosition, dir, radius, d, norm))
    {
        if (!collision || distance > d)
        {
            distance = d;
            normal = norm;
            collision = true;
        }
    }
    if (m_armRight != nullptr && m_armRight->intersect(start - m_position - m_armRightPosition, dir, radius, d, norm))
    {
        if (!collision || distance > d)
        {
            distance = d;
            normal = norm;
            collision = true;
        }
    }

    return collision;
}


} // namespace fight
