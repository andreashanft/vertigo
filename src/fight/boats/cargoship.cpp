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

#include "cargoship.h"
#include "fight/scenario.h"
#include "fight/module.h"
#include "fight/objects/turret.h"
#include "fight/objects/turretbase.h"
#include "fight/weapons/unitgun.h"
#include "sfx/samplemap.h"
#include "txt/desfile.h"
#include <glm/gtc/constants.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/trigonometric.hpp>
#include <glm/gtx/norm.hpp>
#include <cmath>


namespace fight {


namespace {

// big ships turn and climb slowly; their files copy the bomber's kinematics
const float MaxYawRate = 0.15f;      // rad/s
const float MaxPitchRate = 0.08f;
const float MaxClimbingAngle = 0.09f; // about 5 degrees

const float ScanInterval = 1.0f;     // turrets look for a new target once a second
const float AimTolerance = 0.06f;    // radians
const float MinPitch = -0.1f;        // turrets can't aim into their own hull
const float MaxPitch = 1.4f;

}


CargoShip::CargoShip(Scenario *scenario, txt::DesFile &file, const ObjectInfo &info, float yaw) :
    SimpleBoat(scenario, file, info, yaw)
{
    m_maxAngleVelocity = Vector3D(std::min(m_maxAngleVelocity.x, MaxPitchRate), 0, std::min(m_maxAngleVelocity.z, MaxYawRate));
    m_minClimbingAngle = m_maxClimbingAngle = MaxClimbingAngle;

    // the blocks, each against a side of an earlier one
    for (int i = 0; ; i++)
    {
        file.setSection(QString("CargoBlock%1").arg(i));
        if (!file.contains("name"))
            break;
        const int reference = file.value("ConnectReference").toInt();
        const int side = file.value("ConnectSide").toInt();

        txt::DesFile blockFile(QString("vfx:sobjects/%1.des").arg(file.value("name").toString()));
        blockFile.setSection("cluster");
        Block block;
        block.module = scenario->moduleManager().get(blockFile.value("base").toString());
        blockFile.setSection("size");
        block.scale = blockFile.value("scale").toFloat()/32.0f;
        blockFile.setSection("defense");
        block.shield = blockFile.value("kineticshield0").toInt();

        const BoundingBox own = block.module->box().scale(block.scale);
        block.position = Vector3D(0);
        if (i > 0 && reference >= 0 && reference < i)
        {
            const Block &other = m_blocks[reference];
            const Vector3D otherMin = other.box.minPoint(), otherMax = other.box.maxPoint();
            block.position = other.position;
            switch (side)
            {
            case 1:  block.position.x = otherMin.x - own.maxPoint().x; break; // left
            case 2:  block.position.x = otherMax.x - own.minPoint().x; break; // right
            case 4:  block.position.y = otherMax.y - own.minPoint().y; break; // front
            case 8:  block.position.y = otherMin.y - own.maxPoint().y; break; // back
            case 16: block.position.z = otherMax.z - own.minPoint().z; break; // top
            case 32: block.position.z = otherMin.z - own.maxPoint().z; break; // bottom
            }
        }
        block.box = own.translate(block.position);
        m_blocks.push_back(block);

        // the block's turrets: RelativePosition x/y on the block (model units), z +-1000: on top or under it
        for (int t = 0; ; t++)
        {
            blockFile.setSection(QString("Turret%1").arg(t));
            if (!blockFile.contains("name"))
                break;
            ShipTurret turret;
            const QString name = blockFile.value("name").toString();
            const float x = blockFile.value("RelativePositionX").toFloat();
            const float y = blockFile.value("RelativePositionY").toFloat();
            turret.below = blockFile.value("RelativePositionZ").toFloat() < 0;
            // Alpha turns the turret counter-clockwise from the bow
            const float alpha = blockFile.value("Alpha").toFloat();
            turret.restYaw = turret.below ? alpha : -alpha;
            turret.yaw = turret.restYaw;
            turret.pitch = 0;
            turret.block = i;

            // the point of the hull under (or over) the turret
            const Vector3D modelBox[2] = { block.module->box().minPoint(), block.module->box().maxPoint() };
            const float sign = turret.below ? -1.0f : 1.0f;
            float distance;
            Vector3D normal;
            float z = turret.below ? modelBox[0].z : modelBox[1].z;
            if (block.module->intersect(Vector3D(x, y, sign*10.0f), Vector3D(0, 0, -sign), 0.0f, distance, normal) && distance < 20.0f)
                z = sign*10.0f - sign*distance;
            turret.position = block.position + Vector3D(x, y, z)*block.scale;

            txt::DesFile turretFile(QString("vfx:sobjects/%1.des").arg(name));
            turret.model.reset(new Turret(scenario, turretFile));
            turretFile.setSection("size");
            turret.scale = turretFile.value("scale").toFloat()/32.0f;
            turretFile.setSection("defense");
            turret.shield = turretFile.value("kineticshield0").toInt();
            turretFile.setSection("kinematics");
            turret.turnRate = turretFile.value("MaxAngleVelocityA", 1).toFloat();
            turretFile.setSection("sensor");
            turret.range = turretFile.value("senr", 100).toFloat();

            const UnitGun::Mount mount = UnitGun::readMount(turretFile);
            if (UnitGun::isGun(mount.model))
            {
                std::vector<Vector3D> muzzles;
                for (const Vector3D &muzzle : mount.muzzles)
                    muzzles.push_back(muzzle*turret.scale);
                turret.gun.reset(new UnitGun(scenario, this, mount.model, muzzles, &turretFile));
                if (mount.ammunition >= 0)
                    turret.gun->setAmmunition(mount.ammunition);
            }
            m_turrets.push_back(std::move(turret));
        }
    }

    // the ship's position is the middle of the hull (seen from above), so it turns around it
    updateLocalBox();
    Vector3D middle = 0.5f*(m_localBox.minPoint() + m_localBox.maxPoint());
    middle.z = 0;
    for (Block &block : m_blocks)
    {
        block.position -= middle;
        block.box = block.box.translate(-middle);
    }
    for (ShipTurret &turret : m_turrets)
        turret.position -= middle;

    updateLocalBox();
    const Vector3D dim = m_localBox.dim();
    m_collisionRadius = glm::clamp(0.5f*std::min(dim.x, std::min(dim.y, dim.z)), 1.5f, 30.0f);
    updateBox();
}


CargoShip::~CargoShip()
{
}


void CargoShip::updateLocalBox()
{
    m_localBox = BoundingBox();
    for (const Block &block : m_blocks)
        if (!block.destroyed)
            m_localBox.add(block.box);
    for (const ShipTurret &turret : m_turrets)
        if (!turret.destroyed)
            m_localBox.add(turret.model->box().scale(turret.scale).translate(turret.position));
    if (m_localBox.minPoint().x > m_localBox.maxPoint().x)
        m_localBox = BoundingBox(Vector3D(-1), Vector3D(1));
}


bool CargoShip::update(float elapsedTime)
{
    SimpleBoat::update(elapsedTime);
    for (ShipTurret &turret : m_turrets)
        if (!turret.destroyed)
            updateTurret(turret, elapsedTime);
    return false;
}


bool CargoShip::isTargetValid(const ActiveObject *unit) const
{
    if (!unit)
        return false;
    if (unit == m_scenario->player())
        return !m_scenario->playerDestroyed() && m_scenario->isHostile(iff(), unit->iff());
    return unit->isEnabled() && !const_cast<ActiveObject *>(unit)->eventDestroy()->isCompleted() && m_scenario->isHostile(iff(), unit->iff());
}


// the nearest hostile vessel in range on the turret's side of the hull
ActiveObject* CargoShip::findTarget(const ShipTurret &turret) const
{
    const Vector3D pivot = toWorld(turret.position);
    ActiveObject *nearest = nullptr;
    float nearestDistance = turret.range;
    auto consider = [&](ActiveObject *unit) {
        if (unit == this || !isTargetValid(unit))
            return;
        const float distance = glm::length(unit->center() - pivot);
        if (distance >= nearestDistance)
            return;
        const Vector3D way = toMount(turret, toLocal(unit->center()) - turret.position);
        if (std::atan2(way.z, glm::length(Vector2D(way))) < MinPitch)
            return;
        nearest = unit;
        nearestDistance = distance;
    };
    consider(m_scenario->player());
    for (const auto &record : m_scenario->objectRecords())
        if (ActiveObject *unit = dynamic_cast<ActiveObject *>(record.object))
            if (dynamic_cast<SimpleBoat *>(unit) || dynamic_cast<TurretBase *>(unit))
                consider(unit);
    return nearest;
}


Vector3D CargoShip::turretAim(const ShipTurret &turret) const
{
    const Vector3D aim(std::sin(turret.yaw)*std::cos(turret.pitch), std::cos(turret.yaw)*std::cos(turret.pitch), std::sin(turret.pitch));
    return m_orientationInverted*toMount(turret, aim);
}


void CargoShip::updateTurret(ShipTurret &turret, float elapsedTime)
{
    if (m_scenario->time() >= turret.nextScan || !isTargetValid(turret.target))
    {
        turret.target = findTarget(turret);
        turret.nextScan = m_scenario->time() + ScanInterval;
    }

    // turn towards the target (leading it) or back to rest
    float targetYaw = turret.restYaw, targetPitch = 0;
    Vector3D point;
    if (turret.target)
    {
        point = turret.target->center();
        const float speed = turret.gun ? turret.gun->projectileSpeed() : 0;
        if (speed > 0)
            point += turret.target->velocity()*(glm::length(point - toWorld(turret.position))/speed);
        const Vector3D way = toMount(turret, toLocal(point) - turret.position);
        targetYaw = std::atan2(way.x, way.y);
        targetPitch = glm::clamp(std::atan2(way.z, glm::length(Vector2D(way))), MinPitch, MaxPitch);
    }
    const float step = turret.turnRate*elapsedTime;
    turret.yaw += glm::clamp(std::remainder(targetYaw - turret.yaw, glm::two_pi<float>()), -step, step);
    turret.pitch += glm::clamp(targetPitch - turret.pitch, -step, step);

    if (!turret.gun)
        return;
    const Vector3D pivot = toWorld(turret.position);
    const Vector3D dir = turretAim(turret);
    bool trigger = false;
    if (turret.target)
    {
        const Vector3D way = point - pivot;
        const float distance = glm::length(way);
        trigger = distance < std::min(turret.range, turret.gun->range()) && distance > 0 && glm::dot(dir, way/distance) > std::cos(AimTolerance);
    }
    const Vector3D mountUp = m_orientationInverted*toMount(turret, Vector3D(0, 0, 1));
    Vector3D right = glm::cross(dir, mountUp);
    right = glm::length2(right) > 1e-6f ? glm::normalize(right) : m_orientationInverted*Vector3D(1, 0, 0);
    turret.gun->update(elapsedTime, trigger, pivot, dir, glm::cross(right, dir), right);
}


void CargoShip::draw()
{
    glPushMatrix();
    glTranslatef(m_position.x, m_position.y, m_position.z);
    glMultMatrixf(glm::value_ptr(Matrix(m_orientationInverted)));
    for (const Block &block : m_blocks)
    {
        if (block.destroyed)
            continue;
        glPushMatrix();
        glTranslatef(block.position.x, block.position.y, block.position.z);
        glScalef(block.scale, block.scale, block.scale);
        block.module->draw();
        glPopMatrix();
    }
    for (const ShipTurret &turret : m_turrets)
    {
        if (turret.destroyed)
            continue;
        glPushMatrix();
        glTranslatef(turret.position.x, turret.position.y, turret.position.z);
        if (turret.below)
            glRotatef(180, 0, 1, 0);
        glRotatef(-glm::degrees(turret.yaw), 0, 0, 1);
        glScalef(turret.scale, turret.scale, turret.scale);
        turret.model->draw();
        glPopMatrix();
    }
    glPopMatrix();
}


bool CargoShip::intersect(const Vector3D &start, const Vector3D &dir, float radius, float &distance, Vector3D &normal)
{
    const Vector3D localStart = toLocal(start);
    const Vector3D localDir = m_orientation*dir;
    bool collision = false;
    auto take = [&](float d, const Vector3D &n) {
        if (!collision || d < distance)
        {
            distance = d;
            normal = m_orientationInverted*n;
            collision = true;
        }
    };

    float d;
    Vector3D n;
    for (const Block &block : m_blocks)
        if (!block.destroyed && block.module->intersect((localStart - block.position)/block.scale, localDir, radius/block.scale, d, n))
            take(d*block.scale, n);
    for (const ShipTurret &turret : m_turrets)
    {
        if (turret.destroyed)
            continue;
        // the turret's frame: turned by its yaw (and over, under the hull)
        const float c = std::cos(turret.yaw), s = std::sin(turret.yaw);
        auto toTurret = [&](const Vector3D &v) { const Vector3D m = toMount(turret, v); return Vector3D(c*m.x - s*m.y, s*m.x + c*m.y, m.z); };
        auto fromTurret = [&](const Vector3D &v) { return toMount(turret, Vector3D(c*v.x + s*v.y, -s*v.x + c*v.y, v.z)); };
        if (turret.model->intersect(toTurret(localStart - turret.position)/turret.scale, toTurret(localDir), radius/turret.scale, d, n))
            take(d*turret.scale, fromTurret(n));
    }
    return collision;
}


void CargoShip::damage(int kinetic, int shock, const Vector3D &position, ActiveObject *attacker)
{
    // the part that was hit: a turret near the point, otherwise the nearest block
    const Vector3D local = toLocal(position);
    ShipTurret *hitTurret = nullptr;
    for (ShipTurret &turret : m_turrets)
        if (!turret.destroyed && glm::length(local - turret.position) < 3.0f)
            hitTurret = &turret;
    if (hitTurret)
    {
        hitTurret->shield -= kinetic;
        if (hitTurret->shield < 0)
            destroyTurret(*hitTurret);
    }
    else
    {
        Block *hitBlock = nullptr;
        float nearest = 1e10f;
        for (Block &block : m_blocks)
        {
            if (block.destroyed)
                continue;
            const Vector3D closest = glm::clamp(local, block.box.minPoint(), block.box.maxPoint());
            const float distance = glm::length(local - closest);
            if (distance < nearest)
            {
                nearest = distance;
                hitBlock = &block;
            }
        }
        if (hitBlock)
        {
            hitBlock->shield -= kinetic;
            if (hitBlock->shield < 0)
                destroyBlock(*hitBlock);
        }
    }

    ActiveObject::damage(kinetic, shock, position, attacker);

    bool allDestroyed = true;
    for (const Block &block : m_blocks)
        allDestroyed = allDestroyed && block.destroyed;
    if (allDestroyed)
        destroy();
}


void CargoShip::destroyBlock(Block &block)
{
    block.destroyed = true;
    const Vector3D center = toWorld((block.box.minPoint() + block.box.maxPoint())*0.5f);
    m_scenario->effectManager().addEffect(Effects::Explosion_2, center);
    sfx::SampleMap::playAt(sfx::Sample::Explosion_Big, center.x, center.y, center.z);
    for (int i = 0; i < 4; i++)
    {
        const Vector3D d(qrand()/float(RAND_MAX) - 0.5f, qrand()/float(RAND_MAX) - 0.5f, 0.5f*qrand()/float(RAND_MAX));
        m_scenario->effectManager().addDebris(Effects(int(Effects::Debris_9) + qrand()%5), center, glm::length(d) > 0.01f ? glm::normalize(d) : Vector3D(0, 0, 1));
    }
    // the turrets on it go with it
    for (ShipTurret &turret : m_turrets)
        if (!turret.destroyed && &m_blocks[turret.block] == &block)
            destroyTurret(turret);
    updateLocalBox();
    updateBox();
}


void CargoShip::destroyTurret(ShipTurret &turret)
{
    turret.destroyed = true;
    turret.target = nullptr;
    const Vector3D at = toWorld(turret.position);
    m_scenario->effectManager().addEffect(Effects::Explosion_1, at);
    sfx::SampleMap::playAt(sfx::Sample::Explosion_Small, at.x, at.y, at.z);
}


void CargoShip::destroy()
{
    if (m_eventDestroy.isCompleted())
        return;
    // every block that is still there goes up
    for (const Block &block : m_blocks)
        if (!block.destroyed)
            m_scenario->effectManager().addEffect(Effects::Explosion_2, toWorld((block.box.minPoint() + block.box.maxPoint())*0.5f));
    SimpleBoat::destroy();
}


} // namespace fight
