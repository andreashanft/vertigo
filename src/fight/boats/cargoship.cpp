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
#include "fight/effects/billboard.h"
#include "fight/effects/effectmanager.h"
#include "fight/objects/turret.h"
#include "fight/objects/mountedturret.h"
#include "fight/weapons/unitgun.h"
#include "sfx/samplemap.h"
#include "txt/stringtable.h"
#include "txt/desfile.h"
#include "util/random.h"
#include "gfx/pipeline.h"
#include <glm/gtc/constants.hpp>
#include <glm/trigonometric.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/transform.hpp>
#include <algorithm>
#include <cmath>
#include <map>


namespace fight {


namespace {

// big ships turn and climb slowly; their files copy the bomber's kinematics
const float MaxYawRate = 0.15f;      // rad/s
const float MaxPitchRate = 0.08f;
const float MaxClimbingAngle = 0.09f; // about 5 degrees


// a block's own shield: a small explosion where it is hit when 60 % and 80 % of it are gone,
// and then it is whole again
const float HitHard = 0.4f;
const float HitHarder = 0.2f;

float uniform()
{
    return util::random()/float(RAND_MAX);
}

float between(float min, float max)
{
    return min + (max - min)*uniform();
}

}


CargoShip::CargoShip(Scenario *scenario, txt::DesFile &file, const ObjectInfo &info, float yaw) :
    SimpleBoat(scenario, file, info, yaw)
{
    m_maxAngleVelocity = Vector3D(std::min(m_maxAngleVelocity.x, MaxPitchRate), 0, std::min(m_maxAngleVelocity.z, MaxYawRate));
    m_minClimbingAngle = m_maxClimbingAngle = MaxClimbingAngle;

    // the blocks, each against a side of an earlier one
    for (int i = 0; ; i++)
    {
        file.setSection(String("CargoBlock%1").arg(i));
        if (!file.contains("name"))
            break;
        const int reference = file.value("ConnectReference").toInt();
        const int side = file.value("ConnectSide").toInt();

        txt::DesFile blockFile(String("vfx:sobjects/%1.des").arg(file.value("name").toString()));
        blockFile.setSection("cluster");
        Block block;
        block.module = scenario->moduleManager().get(blockFile.value("base").toString());
        blockFile.setSection("size");
        block.scale = blockFile.value("scale").toFloat()/32.0f;
        blockFile.setSection("defense");
        block.shield = block.shieldMax = std::max(1, blockFile.value("KineticShield0").toInt());

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
            blockFile.setSection(String("Turret%1").arg(t));
            if (!blockFile.contains("name"))
                break;
            const String name = blockFile.value("name").toString();
            const float x = blockFile.value("RelativePositionX").toFloat();
            const float y = blockFile.value("RelativePositionY").toFloat();
            const bool below = blockFile.value("RelativePositionZ").toFloat() < 0;
            // Alpha turns the turret counter-clockwise from the bow
            const float alpha = blockFile.value("Alpha").toFloat();

            // the point of the hull under (or over) the turret
            const Vector3D modelBox[2] = { block.module->box().minPoint(), block.module->box().maxPoint() };
            const float sign = below ? -1.0f : 1.0f;
            float distance;
            Vector3D normal;
            float z = below ? modelBox[0].z : modelBox[1].z;
            if (block.module->intersect(Vector3D(x, y, sign*10.0f), Vector3D(0, 0, -sign), 0.0f, distance, normal) && distance < 20.0f)
                z = sign*10.0f - sign*distance;
            txt::DesFile turretFile(String("vfx:sobjects/%1.des").arg(name));
            ShipTurret turret;
            turret.unit = new MountedTurret(scenario, this, turretFile, info, block.position + Vector3D(x, y, z)*block.scale, below, alpha);
            turret.block = i;
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
        turret.unit->setMountPosition(turret.unit->mountPosition() - middle);

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
    if (m_localBox.minPoint().x > m_localBox.maxPoint().x)
        m_localBox = BoundingBox(Vector3D(-1), Vector3D(1));
}


bool CargoShip::update(float elapsedTime)
{
    if (m_paralysed)
        freeze();
    if (m_exploding)
    {
        // it brakes to a stop; the blocks start in their order, the first at once, then one
        // every 0.5 to 1.5 s
        hold();
        if (m_nextBlock < int(m_blocks.size()) && m_scenario->time() >= m_nextExplosion)
        {
            Block &block = m_blocks[m_nextBlock++];
            block.start = m_scenario->time();
            m_nextExplosion = m_scenario->time() + between(0.5f, 1.5f);
        }
    }
    SimpleBoat::update(elapsedTime);
    for (ShipTurret &turret : m_turrets)
    {
        if (!turret.unit->isPresent())
            continue;
        if (!turret.unit->isEnabled())
            turret.unit->enable();
        turret.unit->think(elapsedTime);
    }
    for (int i = 0; i < int(m_blocks.size()); i++)
        updateBlock(i, elapsedTime);
    updateSegments();
    if (m_exploding && std::all_of(m_blocks.begin(), m_blocks.end(), [](const Block &block) { return block.destroyed; }))
        disable();
    return false;
}


String CargoShip::pictureFor(const String &desName)
{
    // from the comments of the ship files; cship* are container freighters
    static const std::map<String, String> pictures = {
        { "bship0", "shpseaco" }, { "bship1", "shptrito" }, { "bship2", "shppolar" }, { "bship3", "shpinder" },
        { "bship4", "shpbiont" }, { "bship5", "shpbfm" }, { "bship6", "shpbfm" }, { "bship7", "shpbiont" },
        { "bship8", "shptrito" }, { "bship9", "shppolar" }, { "bship10", "shpinder" },
    };
    const String name = desName.toLower();
    const auto it = pictures.find(name);
    if (it != pictures.end())
        return it->second;
    return name.startsWith("cship") ? "shpcontf" : String();
}


std::vector<CargoSegment *> CargoShip::createSegments()
{
    std::vector<CargoSegment *> segments;
    for (int i = 0; i < int(m_blocks.size()); i++)
    {
        m_blocks[i].segment = new CargoSegment(m_scenario, this, i, m_info);
        m_blocks[i].segment->disable();
        segments.push_back(m_blocks[i].segment);
    }
    return segments;
}


// the segments follow their blocks and are there as long as the blocks are
void CargoShip::updateSegments()
{
    for (const Block &block : m_blocks)
    {
        if (!block.segment)
            continue;
        block.segment->setEnabled(isEnabled() && !block.destroyed);
        const Matrix motion = blockMotion(block);
        block.segment->place(block.box.transform(glm::translate(m_position)*Matrix(m_orientationInverted)*motion), m_velocity, m_kineticShield, m_kineticShieldMax);
    }
}


void CargoShip::drawBlock(int index)
{
    const Block &block = m_blocks[index];
    gfx::Pipeline::pushMatrix();
    gfx::Pipeline::translate(m_position);
    gfx::Pipeline::multiply(Matrix(m_orientationInverted));
    gfx::Pipeline::multiply(blockTransform(block));
    block.module->draw();
    gfx::Pipeline::popMatrix();
}


CargoSegment::CargoSegment(Scenario *scenario, CargoShip *ship, int block, const ObjectInfo &info) :
    ActiveObject(scenario),
    m_ship(ship),
    m_block(block)
{
    m_info = info;
    m_noise = 0;
    m_kineticShield = m_kineticShieldMax = 1;
}


void CargoSegment::place(const BoundingBox &box, const Vector3D &velocity, int shield, int shieldMax)
{
    m_box = box;
    m_position = center();
    m_velocity = velocity;
    m_kineticShield = shield;
    m_kineticShieldMax = shieldMax;
}


float CargoSegment::noise() const
{
    return m_ship->noise();
}


String CargoSegment::observationImage() const
{
    return m_ship->observationImage();
}


void CargoSegment::drawPreview()
{
    m_ship->drawBlock(m_block);
}


// identifying a block identifies the ship's cargo
void CargoSegment::identify()
{
    if (!m_eventIdentify.isCompleted())
        m_eventIdentify.complete();
    if (!m_ship->isIdentified())
        m_ship->identify();
}


std::vector<MountedTurret *> CargoShip::turretUnits() const
{
    std::vector<MountedTurret *> units;
    for (const ShipTurret &turret : m_turrets)
        units.push_back(turret.unit);
    return units;
}


bool CargoShip::startTumbling()
{
    m_paralysed = true;
    setOrders(nullptr);
    m_velocity = Vector3D(0);
    freeze();
    return true;
}


void CargoShip::orderStarted(const OrderSlot *slot)
{
    for (ShipTurret &turret : m_turrets)
        turret.unit->setOrder(slot);
}


void CargoShip::sleep()
{
    SimpleBoat::sleep();
    for (ShipTurret &turret : m_turrets)
        turret.unit->reset();
}


void CargoShip::draw()
{
    gfx::Pipeline::pushMatrix();
    gfx::Pipeline::translate(m_position);
    gfx::Pipeline::multiply(Matrix(m_orientationInverted));
    for (const Block &block : m_blocks)
    {
        if (block.destroyed)
            continue;
        gfx::Pipeline::pushMatrix();
        gfx::Pipeline::multiply(blockTransform(block));
        block.module->draw();
        gfx::Pipeline::popMatrix();
    }
    gfx::Pipeline::popMatrix();
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
    {
        if (block.destroyed)
            continue;
        const Matrix transform = blockTransform(block);
        const Matrix inverse = glm::inverse(transform);
        if (block.module->intersect(Vector3D(inverse*Vector4D(localStart, 1)), glm::normalize(Vector3D(inverse*Vector4D(localDir, 0))), radius/block.scale, d, n))
            take(d*block.scale, glm::normalize(Vector3D(transform*Vector4D(n, 0))));
    }
    return collision;
}


// hits count for the whole ship (ActiveObject::damage); the block hit, the nearest to where the
// shot struck, takes them on its own shield as well, for its small explosions. Once the ship is
// blowing up they do nothing
void CargoShip::damage(int kinetic, int shock, const Vector3D &position, ActiveObject *attacker)
{
    if (m_exploding)
        return;
    const Vector3D local = toLocal(position);
    Block *hit = nullptr;
    float nearest = 0;
    for (Block &block : m_blocks)
    {
        const float distance = glm::length(local - glm::clamp(local, block.box.minPoint(), block.box.maxPoint()));
        if (!block.destroyed && (!hit || distance < nearest))
        {
            hit = &block;
            nearest = distance;
        }
    }
    if (hit && kinetic > 0)
    {
        hit->shield -= kinetic;
        auto explosion = [this, &position]() {
            // a little out of the hull, towards the player
            const Vector3D way = m_scenario->player() ? m_scenario->player()->position() - position : Vector3D(0);
            const Vector3D at = position + (glm::length(way) > 1.0f ? glm::normalize(way) : Vector3D(0));
            m_scenario->effectManager().addEffect(Effects::Explosion_1, at);
            sfx::SampleMap::playAt(sfx::Sample::Explosion_Big, at.x, at.y, at.z);
        };
        if (!hit->hitHard && hit->shield < HitHard*hit->shieldMax)
        {
            explosion();
            hit->hitHard = true;
        }
        if (hit->shield < HitHarder*hit->shieldMax)
        {
            explosion();
            hit->shield = hit->shieldMax;
            hit->hitHard = false;
        }
    }
    ActiveObject::damage(kinetic, shock, position, attacker);
}


// The destruction: the ship keeps its turrets firing and stays a target; it counts as destroyed
// when its first block is gone
void CargoShip::destroy()
{
    if (m_exploding || m_eventDestroy.isCompleted())
        return;
    m_exploding = true;
    setOrders(nullptr);
    m_trigger = false;
    m_nextBlock = 0;
    m_nextExplosion = m_scenario->time();
}


Vector3D CargoShip::blockCenter(const Block &block) const
{
    return toWorld(0.5f*(block.box.minPoint() + block.box.maxPoint()) + block.shake + Vector3D(0, 0, -block.sink));
}


float CargoShip::blockRadius(const Block &block) const
{
    const Vector3D dim = block.box.dim();
    return 0.5f*std::max(dim.x, std::max(dim.y, dim.z));
}


Matrix CargoShip::blockTransform(const Block &block) const
{
    return blockMotion(block)*glm::translate(block.position)*glm::scale(Vector3D(block.scale));
}


// how the block has moved from its place: shaken, sunk and tilted about its middle
Matrix CargoShip::blockMotion(const Block &block) const
{
    const Vector3D middle = 0.5f*(block.box.minPoint() + block.box.maxPoint());
    return glm::translate(middle + block.shake + Vector3D(0, 0, -block.sink))*Matrix(block.tilt)*glm::translate(-middle);
}


// A block's 2.7 s, from its start (docs/ROADMAP.md, appendix A): it shakes by up to 5 cm, sinks
// faster and faster and turns slowly, with a boat's bursts and debris; its turrets go, half at
// the start and half at 1.6 s, and at the end so does the block
void CargoShip::updateBlock(int index, float elapsedTime)
{
    Block &block = m_blocks[index];
    if (block.start < 0 || block.destroyed)
        return;
    const float t = m_scenario->time() - block.start;
    if (block.step == 0)
    {
        block.sinkAcceleration = between(10.0f, 40.0f)/32.0f;
        block.spin = Vector2D(between(-0.07f, 0.07f), between(-0.07f, 0.07f));
    }
    else
    {
        if (t < 2.7f)
            block.sinkSpeed += block.sinkAcceleration*elapsedTime;
        block.sink += block.sinkSpeed*elapsedTime;
        block.shake = Vector3D(between(-0.05f, 0.05f), between(-0.05f, 0.05f), 0);
        block.tilt = glm::mat3(glm::rotate(block.spin.x*elapsedTime, Vector3D(1, 0, 0))*glm::rotate(block.spin.y*elapsedTime, Vector3D(0, 1, 0)))*block.tilt;
    }

    std::vector<ShipTurret *> turrets;
    for (ShipTurret &turret : m_turrets)
        if (turret.block == index)
            turrets.push_back(&turret);
    const int half = int(turrets.size() + 1)/2;
    const float fifth = 0.2f*glm::pi<float>();

    // the bursts as a boat's (appendix R) about the block's middle and within its size
    const Vector3D centre = blockCenter(block);
    const float r = blockRadius(block);
    auto debris = [&](int index, float minElevation, float maxElevation) { ActiveObject::wreckDebris(index, minElevation, maxElevation, centre, r); };
    while (block.step < 7 && t >= WreckTimes[block.step])
    {
        const int step = block.step++;
        wreckBursts(step, centre, r);
        switch (step)
        {
        case 0:
            if (util::random()%3 != 0)
                sfx::SampleMap::playAt(sfx::Sample::Sample18, centre.x, centre.y, centre.z);
            for (int i = 0; i < half; i++)
                turrets[i]->unit->blowUpTurret();
            break;
        case 2:
            for (int index : { 2, 2, 1 })
                debris(index, fifth, 2*fifth);
            break;
        case 4:
            for (int i = half; i < int(turrets.size()); i++)
                turrets[i]->unit->blowUpTurret();
            break;
        case 5:
            {
                for (int index : { 2, 1, 2, 1 })
                    debris(index, 0, fifth);
                // one body: 14 or 15 two times in five, 16 once
                debris(14 + util::random()%5%3, 0, glm::half_pi<float>());
                // and a big one on its side towards the player
                const Vector3D way = m_scenario->player() ? m_scenario->player()->position() - centre : Vector3D(0);
                const Vector3D at = centre + (glm::length(way) > 1.0f ? glm::normalize(way)*r : Vector3D(0));
                Billboard *billboard = m_scenario->effectManager().getBillboard(Effects::Explosion_2);
                m_scenario->effectManager().addEffect(Effects::Explosion_2, at, 0, 0.35f*r/std::max(billboard->sizeScale(), 1e-3f));
                sfx::SampleMap::playAt(sfx::Sample::Explosion_Big, at.x, at.y, at.z);
                m_scenario->explosionAt(at, 1.0f);
            }
            break;
        case 6:
            block.destroyed = true;
            if (block.segment)
                block.segment->remove();
            updateLocalBox();
            updateBox();
            countDestroyed();
            break;
        }
    }
}


} // namespace fight
