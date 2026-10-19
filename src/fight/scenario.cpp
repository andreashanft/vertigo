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

#include "scenario.h"
#include "module.h"
#include "orders.h"
#include "enginesounds.h"
#include "weapons/torpedo.h"
#include "sfx/soundsystem.h"
#include "surface/element.h"
#include "surface/betaspline.h"
#include "effects/billboard.h"
#include "effects/trash.h"
#include "game/boat.h"
#include "sfx/samplemap.h"
#include "util/debug.h"
#include "txt/parse.hpp"
#include "txt/stringtable.h"

#include "objects/building.h"
#include "objects/mine.h"
#include "objects/turretbase.h"
#include "objects/navpoint.h"
#include "objects/placeholder.h"
#include "boats/cargoship.h"
#include "objects/simpleobject.h"

#include "boats/simpleboat.h"
#include "boats/tank.h"
#include "util/random.h"
#include "gfx/pipeline.h"

#include "gfx/gl.hpp"
#include "util/settings.hpp"
#include <chrono>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/transform.hpp>
#include <algorithm>
#include <cmath>


namespace fight {


static int s_difficultyOverride = -1;
static int s_randomSeed = -1;


int Scenario::difficulty()
{
    if (s_difficultyOverride >= 0)
        return s_difficultyOverride;
    return glm::clamp(util::Settings::intValue("Game/difficulty", 1), 0, 2);
}


void Scenario::setDifficulty(int difficulty)
{
    util::Settings::setValue("Game/difficulty", glm::clamp(difficulty, 0, 2));
}


void Scenario::setRandomSeed(int seed)
{
    s_randomSeed = seed;
}


void Scenario::overrideDifficulty(int difficulty)
{
    s_difficultyOverride = glm::clamp(difficulty, 0, 2);
}


namespace {

// how hard a current pushes a boat (m/s²) for spd 0 and spd 2, from the original's measured
// drifts (docs/ROADMAP.md, appendix D): the boat's own drag across and the cap of twice its top
// speed along its axis set the speeds reached, about 19 m/s across and 41 m/s along for spd 2
const float CurrentPushSlow = 3.1f;
const float CurrentPushFast = 9.0f;

// the player and active buildings face one of four ways, card 0, 2, 4 or 6; any other card leaves
// them facing as card 0 does (cargo ships take all eight)
float quarterHeading(int card)
{
    return card >= 0 && card <= 6 && card % 2 == 0 ? card*glm::quarter_pi<float>() : 0.0f;
}

}


Scenario::Scenario(const String &name) :
    m_name(name),
    m_playerId(-1),
    m_moduleManager(m_textureManager),
    m_effectManager(this),
    m_conditionManager(this),
    m_sonar(this),
    m_target(this),
    m_yaw(0),
    m_pitch(0),
    m_velocityTarget(0),
    m_time(0),
    m_left(0.0f),
    m_right(0.0f),
    m_up(0.0f),
    m_down(0.0f),
    m_forwards(0.0f),
    m_backwards(0.0f)
{
    util::seedRandom(name.right(4).toInt());

    m_file.load(String("vfx:scenario/%1.des").arg(name));

    // the music: Track 1 is the CD's second track, its first audio track after the data (0: none)
    m_file.setSection("cd");
    m_cdTrack = m_file.value("track", 0).toInt();

    m_file.setSection("surface");
    m_depth = m_file.value("depth").toInt();
    m_surface.load(m_file.value("name").toString().toLower(), m_file.value("maxheightscale").toInt(), m_file.value("patchcomb").toInt());

    // dtyp -> object file, as the original maps them (docs/ROADMAP.md, appendix A)
    std::map<int, String> types = {
        // scouts
        { 1, "anscout2" }, { 8, "bioscout" }, { 9, "honglong" }, {10, "russcout" }, {11, "prscout1" },
        {12, "atscout" }, {13, "prscout2" }, {14, "moghboot" }, {15, "indrboot" }, {39, "anscout1" },
        {67, "entrobot" }, {88, "anscout3" }, {89, "anscout4" },
        // bombers
        {16, "anbombr1" }, {40, "anbombr2" }, {41, "atbomber" }, {42, "prbomber" }, {43, "biobombr" },
        {44, "russbomb" },
        // tanks
        { 2, "tank0" }, { 3, "tank1" }, { 4, "tank2" }, {65, "tank3" }, {66, "tank4" },
        // gun towers and the torpedo tower
        { 5, "guntow0" }, { 6, "guntow1" }, { 7, "guntow2" }, {90, "guntow3" }, {36, "tortow0" },
        // mines
        {27, "mine0" }, {28, "mine1" },
        // buildings
        {45, "build0" }, {46, "build1" }, {47, "build2" }, {48, "build3" }, {49, "build4" },
        {50, "build5" }, {51, "build6" }, {52, "build7" },
        // cargo ships
        {91, "bship10" },
    };
    for (int i = 0; i < 10; i++)
    {
        types[68 + i] = String("cship%1").arg(i);
        types[78 + i] = String("bship%1").arg(i);
    }

    float initialDir = 0.0f;
    std::set<String> placeholderSections;
    std::vector<std::pair<ActiveObject *, std::vector<OrderSlot>>> pendingOrders;
    std::vector<ActiveObject *> absentUnits; // only at a higher difficulty
    const int difficulty = Scenario::difficulty();
    for (const String &section : m_file.sections().withPrefix("movable")) {
        m_file.setSection(section);

        const int id = m_file.value("id").toInt();
        ConditionManager::ConditionEntry &entry = m_conditionManager.addEntry(id);
        entry.cond1 = m_file.value("bccond1").toInt();
        entry.dep1 = m_file.value("bcdep1").toInt();
        entry.ref1 = m_file.value("bcref1").toInt();
        entry.op = m_file.value("bcop").toInt();
        entry.cond2 = m_file.value("bccond2").toInt();
        entry.dep2 = m_file.value("bcdep2").toInt();
        entry.ref2 = m_file.value("bcref2").toInt();
        entry.del = m_file.value("bcdel").toInt();
        entry.condTrigger = NULL;
        entry.condSignal = NULL;
        entry.condAttacked = NULL;
        entry.condIdentified = NULL;
        entry.condParalyzed = NULL;
        entry.condFinished = NULL;
        entry.condBoarded = NULL;

        Object *object = NULL;
        std::vector<Mine *> moreMines; // a mine entry's mines above the first
        const int type = m_file.value("typ").toInt();
        const int dType = m_file.value("dtyp").toInt();
        String desName;

        txt::DesFile objectDes;
        bool placeholder = false;
        const bool usesObjectFile = type == TypeBoat || type == TypeBomber || type == TypeTank || type == TypeTower ||
                                    type == TypeTorpedoTower || type == TypeMine || type == TypeActiveBuilding ||
                                    type == TypeCargoShip;
        if (usesObjectFile)
        {
            const auto it = types.find(dType);
            if (it != types.end())
                desName = it->second;
            else if (type == TypeBoat)
                desName = "anscout4"; // the original's fallback for scouts
            else
                placeholder = true;
            if (!desName.isEmpty())
                objectDes.load(String("vfx:sobjects/%1.des").arg(desName));
        }

        ActiveObject::ObjectInfo info;
        if (m_file.contains("iff"))
            info.iff = m_file.value("iff").toInt();
        if (m_file.contains("inf"))
            info.name = m_file.valueText("inf");
        if (m_file.contains("carg"))
            info.cargo = m_file.valueText("carg");
        if (m_file.contains("sen"))
            info.sensor = m_file.value("sen").toInt();
        if (m_file.contains("senr"))
            info.sensorRange = m_file.value("senr").toInt();
        // sle 0 keeps the unit running anywhere on the map; 1 (or no key) lets it sleep out of the
        // player's sonar range (docs/ROADMAP.md, appendix J)
        info.sleeps = !m_file.contains("sle") || m_file.value("sle").toInt() == 1;
        // ai (0 to 3 on some boats, bombers and tanks) does nothing in the original; taken here so
        // the mission check doesn't list it
        m_file.value("ai");

        switch (placeholder ? 0 : type) {
        case TypeBoat:
            {
                object = new SimpleBoat(this, objectDes, info, m_file.value("card").toInt()*glm::quarter_pi<float>());
                Vector3D pos = getPosition();
                pos.z += 20.0f;
                object->setPosition(pos);
            }
            break;

        case TypeBomber:
            {
                object = new SimpleBoat(this, objectDes, info, m_file.value("card").toInt()*glm::quarter_pi<float>());
                Vector3D pos = getPosition();
                pos.z += 20.0f;
                object->setPosition(pos);
            }
            break;

        case TypeTank:
            {
                Tank *tank = new Tank(this, objectDes, info, m_file.value("card").toInt()*glm::quarter_pi<float>());
                tank->setGroundWreck(ActiveObject::GroundWreck::Tank);
                object = tank;
                object->setPosition(getPosition());
            }
            break;

        case TypeTower:
        case TypeTorpedoTower:
            {
                TurretBase *tower = new TurretBase(this, objectDes, info);
                tower->setGroundWreck(ActiveObject::GroundWreck::Tower);
                tower->setGroundCell(m_file.value("px").toInt(), m_file.value("py").toInt());
                object = tower;
                // once it is there, a tower flattens its cell to the height of its corner and
                // darkens the ground, a gun tower more than a torpedo tower (docs/ROADMAP.md,
                // appendix N)
                m_groundMarks.push_back({ object, m_file.value("px").toInt(), m_file.value("py").toInt(), type == TypeTower ? 0.25f : 0.35f });
                object->setPosition(getPosition());
            }
            break;

        case TypeBuilding:
            {
                if (m_file.value("pz").toInt() != 0)
                    loadIssue("unhandled: ground plan with pz != 0");
                objectDes.load(String("vfx:sobjects/gp%1x%1_%2.des").arg(m_file.value("siz").toInt() + 1).arg(m_file.value("buityp").toInt()));
                object = new Building(this, objectDes, m_file.value("siz").toInt(), m_file.value("card", 0).toInt(), m_file.value("px").toInt(), m_file.value("py").toInt(), m_file.value("refx").toInt(), m_file.value("refy").toInt());
            }
            break;

        case TypeMine:
            {
                // num mines above each other, dis height units apart, the lowest at hei: the entries
                // with more than one mine sit in neighbouring cells (16 m), too close for rows. To be
                // compared with the original
                object = new Mine(this, objectDes, info);
                object->setPosition(getPosition());
                const int count = m_file.value("num", 1).toInt();
                const float distance = m_file.value("dis").toInt()*m_surface.scale().z;
                for (int i = 1; i < count; i++)
                {
                    Mine *mine = new Mine(this, objectDes, info);
                    mine->setPosition(getPosition() + Vector3D(0, 0, i*distance));
                    moreMines.push_back(mine);
                }
            }
            break;

        case TypePlayer:
            m_position = getPosition();
            m_yaw = quarterHeading(m_file.value("card").toInt());
            m_height = m_position.z - m_surface.heightAt(m_position.x, m_position.y);
            entry.condTrigger = m_conditionManager.addCondition(0);
            entry.condSignal = &m_eventPlayerDestroyed;
            m_playerId = id;

            {
                /*for (int i = 0; i < 27; i++)
                    m_effectManager.addEffect((Effects)((int)Effects::Explosion_0 + i), m_position + Vector3D(i*5, 0, 0))->setPermanent(true);
                for (int i = 0; i < 9; i++)
                    m_effectManager.addEffect((Effects)((int)Effects::Shoot_0 + i), m_position + Vector3D(i*5, -10, 0))->setPermanent(true);
                for (int i = 0; i < 23; i++)
                    m_effectManager.addEffect((Effects)((int)Effects::Debris_0 + i), m_position + Vector3D(i*5, -20, 0))->setPermanent(true);
                for (int i = 0; i < 5; i++)
                    m_effectManager.addEffect((Effects)((int)Effects::Trash_0 + i), m_position + Vector3D(i*5, -30, 0))->setPermanent(true);
                for (int i = 0; i < 3; i++)
                    m_effectManager.addEffect((Effects)((int)Effects::Bubble_0 + i), m_position + Vector3D(i*5, -40, 0))->setPermanent(true);*/
            }
            break;

        case TypeCrawler:
            {
                objectDes.load("vfx:sobjects/gvehicle.des");
                desName = "gvehicle";
                Tank *crawler = new Tank(this, objectDes, info, m_file.value("card").toInt()*glm::quarter_pi<float>());
                crawler->setGroundWreck(ActiveObject::GroundWreck::Crawler);
                object = crawler;
                object->setPosition(getPosition());
            }
            break;

        case TypeNavPoint:
            {
                object = new NavPoint(this, dType);
                object->setPosition(getPosition() + Vector3D(-0.5f*m_surface.scale().x, -0.5f*m_surface.scale().y, 10));
                m_navPoints.push_back(static_cast<NavPoint*>(object));
            }
            break;

        case TypeActiveBuilding:
            {
                SimpleObject *building = new SimpleObject(this, objectDes, info, 16);
                building->setGroundWreck(ActiveObject::GroundWreck::Building);
                building->setGroundCell(m_file.value("px").toInt(), m_file.value("py").toInt());
                m_groundMarks.push_back({ building, m_file.value("px").toInt(), m_file.value("py").toInt(), 0.35f });
                building->setYaw(quarterHeading(m_file.value("card", 0).toInt()));
                building->setPosition(getPosition());
                object = building;
            }
            break;

        case TypeTrash:
            {
                Vector3D pos = getPosition();
                entry.condTrigger = m_conditionManager.addCondition(0);
                entry.condSignal = m_conditionManager.addCondition(9);
                for (int i = 0; i < 9; i++)
                {
                    Trash *trash = new Trash(this, m_effectManager.getBillboard(Trash::trashCollection[i]), pos);
                    m_objects.emplace_back(trash);
                    trash->disable();
                    trash->condEnable()->setLimit(1);
                    entry.condTrigger->addDependency(trash->condEnable());
                    trash->eventDestroy()->addDependency(static_cast<Condition *>(entry.condSignal));
                }
            }
            break;

        case TypeSpace:
            {
                if (m_file.value("pz").toInt() != 0)
                    loadIssue("unhandled: space with pz != 0");
                ConditionSpace *space = m_conditionManager.addCondSpace(m_file.value("px").toInt()*m_surface.scale().x,
                                                                        m_file.value("py").toInt()*m_surface.scale().y,
                                                                        m_file.value("dimx").toInt()*m_surface.scale().x,
                                                                        m_file.value("dimy").toInt()*m_surface.scale().y,
                                                                        m_file.value("minz").toInt()*m_surface.scale().z,
                                                                        m_file.value("maxz").toInt()*m_surface.scale().z);
                entry.condTrigger = space->condEnable();
                entry.condSignal = space;
            }
            break;

        case TypeCargoShip:
            {
                CargoShip *ship = new CargoShip(this, objectDes, info, m_file.value("card").toInt()*glm::quarter_pi<float>());
                ship->setPicture(CargoShip::pictureFor(desName));
                object = ship;
                // high enough for the hull to clear the ground
                Vector3D pos = getPosition();
                pos.z += std::max(20.0f, ship->draught() + 5.0f);
                object->setPosition(pos);
            }
            break;

        default:
            {
                loadIssue(String("placeholder: typ %1 dtyp %2").arg(type).arg(dType));
                placeholderSections.insert(section);
                object = new Placeholder(this, objectDes, info, 4.0f);
                object->setPosition(getPosition());
            }
        }

        if (object != NULL)
        {
            m_objects.emplace_back(object);
            m_records.push_back({ id, type, desName, object });
            if (CargoShip *ship = dynamic_cast<CargoShip *>(object))
                for (CargoSegment *segment : ship->createSegments())
                    m_objects.emplace_back(segment);
            object->disable();
            entry.condTrigger = object->condEnable();
            ActiveObject *activeObject = dynamic_cast<ActiveObject *>(object);
            if (activeObject)
            {
                activeObject->arm(m_file.contains("gun") ? m_file.value("gun").toInt() : -1, objectDes);
                activeObject->setEngine(m_file.value("eng", 0).toInt());
                // armour and energy defence in percent (the AD Augmented Mod's changelog calls them PD
                // and ED and changes them with these keys)
                activeObject->setDefence(m_file.value("inte", 100).toInt(), m_file.value("eint", 100).toInt());
                // torpedoes for units with tubes: boats, bombers and torpedo towers
                const int torpedoes = m_file.value("ntor", 0).toInt();
                const int torpedoType = m_file.value("tor", -1).toInt();
                if ((type == TypeBoat || type == TypeBomber || type == TypeTorpedoTower) && torpedoes > 0)
                    activeObject->setTorpedoes(torpedoType, torpedoes);
                entry.iff = m_file.contains("iff") ? m_file.value("iff").toInt() : -1;
                if (m_file.value("diff", 0).toInt() > difficulty)
                {
                    absentUnits.push_back(activeObject);
                    entry.absent = true;
                    entry.iff = -1;
                }
                entry.condSignal = activeObject->eventDestroy();
                entry.condAttacked = activeObject->eventAttack();
                entry.condIdentified = activeObject->eventIdentify();
                entry.condParalyzed = activeObject->eventParalyze();
                entry.condFinished = activeObject->eventFinish();
                entry.condBoarded = activeObject->eventBoard();
            }
            // "destroyed" for a nav point means it has been reached
            NavPoint *navPoint = dynamic_cast<NavPoint *>(object);
            if (navPoint)
                entry.condSignal = navPoint->eventReached();
        }

        // the mines of an entry come together: they appear at once, the entry is attacked when one of
        // them is and destroyed when all of them are
        if (!moreMines.empty())
        {
            Condition *enable = m_conditionManager.addCondition(0);
            Condition *destroyed = m_conditionManager.addCondition(int(moreMines.size()) + 1);
            Condition *attacked = m_conditionManager.addCondition(1);
            moreMines.insert(moreMines.begin(), static_cast<Mine *>(object));
            for (Mine *mine : moreMines)
            {
                if (mine != object)
                {
                    m_objects.emplace_back(mine);
                    m_records.push_back({ id, type, desName, mine });
                    mine->disable();
                    mine->arm(m_file.contains("gun") ? m_file.value("gun").toInt() : -1, objectDes);
                    if (entry.absent)
                        absentUnits.push_back(mine);
                }
                mine->condEnable()->setLimit(1);
                enable->addDependency(mine->condEnable());
                mine->eventDestroy()->addDependency(destroyed);
                mine->eventAttack()->addDependency(attacked);
            }
            entry.condTrigger = enable;
            entry.condSignal = destroyed;
            entry.condAttacked = attacked;
        }

        // order orderSlots; all of them are read, so the load report only lists what is really ignored
        std::vector<OrderSlot> orderSlots;
        bool anySlot = false;
        for (int n = 0; n < 8; n++)
        {
            auto value = [this, n](const char *name, int defaultValue) {
                const String key = String("o%1%2").arg(n).arg(name);
                return m_file.contains(key) ? m_file.value(key).toInt() : defaultValue;
            };
            OrderSlot slot;
            slot.type = value("typ", OrderSlot::None);
            slot.dur = value("dur", 0);
            slot.refx = value("refx", 0);
            slot.refy = value("refy", 0);
            slot.vel = value("vel", 100);
            slot.hei = value("hei", 0);
            slot.rad = value("rad", 0);
            slot.mih = value("mih", 0);
            slot.mah = value("mah", 0);
            slot.esc = value("esc", -1);
            slot.offset = Vector3D(value("xof", 0), value("yof", 0), value("zof", 0));
            slot.ene = value("ene", -1);
            slot.ran = value("ran", 150);
            slot.car = value("car", 0);
            slot.con = value("con", 0);
            slot.cond1 = value("bccond1", 0);
            slot.dep1 = value("bcdep1", 0);
            slot.ref1 = value("bcref1", -1);
            slot.op = value("bcop", 0);
            slot.cond2 = value("bccond2", 0);
            slot.dep2 = value("bcdep2", 0);
            slot.ref2 = value("bcref2", -1);
            slot.del = value("bcdel", 0);
            // empty slots stay, the orders skip them when they go round
            anySlot = anySlot || slot.type != OrderSlot::None;
            orderSlots.push_back(slot);
        }
        if (anySlot)
        {
            ActiveObject *unit = dynamic_cast<ActiveObject *>(object);
            if (unit)
            {
                pendingOrders.emplace_back(unit, orderSlots);
                const bool moves = dynamic_cast<SimpleBoat *>(unit) != nullptr;
                for (const OrderSlot &slot : orderSlots)
                {
                    if (slot.type == OrderSlot::None)
                        continue;
                    const bool forAll = slot.type == OrderSlot::Wait || slot.type == OrderSlot::CardinalPatrol ||
                                        slot.type == OrderSlot::CardinalAttack || slot.type == OrderSlot::Explode;
                    if (!OrderSlot::isImplemented(slot.type))
                        loadIssue(String("order not implemented: %1").arg(OrderSlot::name(slot.type)));
                    else if (!moves && !forAll)
                        loadIssue(String("order not run: %1 for typ %2").arg(OrderSlot::name(slot.type)).arg(type));
                }
            }
            else if (object)
                loadIssue(String("orders not run: typ %1").arg(type));
        }

        // Reading Objectives
        for (int i = 0; i < 16; i++)
        {
            int wccond = m_file.value(String("wccond%1").arg(i)).toInt();
            if (wccond == 0)
                continue;
            m_conditionManager.updateObjective(id, i, wccond);
        }
    }

    // currents: each cell covers a 2x2 block of map cells; their strength isn't in the data (docs/ROADMAP.md, appendix D)
    for (const String &section : m_file.sections().withPrefix("current")) {
        m_file.setSection(section);
        m_file.value("id");
        if (m_file.value("typ").toInt() != 3072 || m_file.value("pz").toInt() != 0)
            loadIssue(String("unhandled: current typ %1 pz %2").arg(m_file.value("typ").toInt()).arg(m_file.value("pz").toInt()));

        const int card = m_file.value("card").toInt();
        const float push = m_file.value("spd").toInt() == 0 ? CurrentPushSlow : CurrentPushFast;
        Vector3D direction;
        if (card == 8)
            direction = Vector3D(0, 0, 1);
        else if (card == 9)
            direction = Vector3D(0, 0, -1);
        else
            direction = Vector3D(glm::sin(card*glm::quarter_pi<float>()), glm::cos(card*glm::quarter_pi<float>()), 0);

        const int x = m_file.value("px").toInt();
        const int y = m_file.value("py").toInt();
        m_currentCells.push_back({ x, y, direction*push });
        m_currentGrid[std::make_pair(x/2, y/2)] = direction*push;
    }

    for (const String &section : m_file.sections().withPrefix("radio")) {
        m_file.setSection(section);

        const int id = m_file.value("id").toInt();
        ConditionManager::ConditionEntry &entry = m_conditionManager.addEntry(id);
        entry.cond1 = m_file.value("bccond1").toInt();
        entry.dep1 = m_file.value("bcdep1").toInt();
        entry.ref1 = m_file.value("bcref1").toInt();
        entry.op = m_file.value("bcop").toInt();
        entry.cond2 = m_file.value("bccond2").toInt();
        entry.dep2 = m_file.value("bcdep2").toInt();
        entry.ref2 = m_file.value("bcref2").toInt();
        entry.del = m_file.value("bcdel").toInt();

        const int type = m_file.value("typ").toInt();
        if (type != 5120)
            loadIssue(String("unhandled: radio typ %1").arg(type));

        ConditionRadio *radio = m_conditionManager.addCondRadio(getPosition(), m_file.valueText("rtxt"));
        entry.condTrigger = radio;
        entry.condSignal = nullptr;
        entry.condAttacked = nullptr;
        entry.condIdentified = nullptr;
        entry.condParalyzed = nullptr;
        entry.condFinished = nullptr;
        entry.condBoarded = nullptr;
    }

    m_conditionManager.buildDependencies();
    for (ActiveObject *unit : absentUnits)
        unit->leaveMission();

    for (auto &pending : pendingOrders)
    {
        // an empty slot never runs, so its break condition (some name objectives no unit has) is never set up
        for (OrderSlot &slot : pending.second)
            if (slot.type != OrderSlot::None)
                slot.breakCondition = m_conditionManager.addBreakCondition(slot.cond1, slot.dep1, slot.ref1, slot.op, slot.cond2, slot.dep2, slot.ref2, slot.del);
        pending.first->setOrders(new Orders(this, pending.first, pending.second));
    }

    std::sort(m_navPoints.begin(), m_navPoints.end(), [](NavPoint *a, NavPoint *b) { return a->num() < b->num(); });

    m_file.setSection("AttitudeMatrix");
    for (int i = 0; i < 9; i++)
        for (int j = 0; j < 9; j++)
            m_attitudeMatrix[i][j] = m_file.value(String("X%1Y%2").arg(j).arg(i)).toInt();

    m_file.setSection("End");
    m_endType = m_file.value("Type").toInt();

    // report what the loader ignored, with numbers in names collapsed to '#'
    for (const String &section : m_file.sections()) {
        if (placeholderSections.count(section))
            continue;
        const String sectionName = txt::replaceNumbers(section, "#");
        if (!m_file.wasSectionRead(section)) {
            loadIssue(String("unread section: %1").arg(sectionName));
            continue;
        }
        for (const String &key : m_file.unreadKeys(section))
            loadIssue(String("unread key: %1/%2").arg(sectionName, txt::replaceNumbers(key, "#")));
    }



    // without a seed of its own, the milliseconds of the current minute
    const long long milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    util::seedRandom(s_randomSeed >= 0 ? uint(s_randomSeed) : uint(milliseconds % 60000));

    sfx::SampleMap::load();
}


Scenario::~Scenario()
{
}


void Scenario::setBoat(game::Boat *boat)
{
    //if (boat->gun() == 3081)
    //    boat->buy(3083, "GUN"); // TODO: remove
    m_player.reset(new Player(this, boat, m_yaw));
    m_player->setPosition(m_position);

    m_buzzers = boat->buzzers().size();

    m_sonar.init(boat->sensor());
    m_sonar.activate();
}


void Scenario::setRect(const RectF &rect, const Vector2D &center, float fovY)
{
    m_projectionMatrix = glm::translate(Vector3D(center, 0)) * glm::perspective(glm::radians(fovY), float(rect.width / rect.height), 0.1f, 500.0f);
    m_projectionMatrixInverted = glm::inverse(m_projectionMatrix);
}


void Scenario::update(float elapsedTime)
{
    markGround();
    m_time += elapsedTime;

    // pushed forward the stick lowers the nose, as the original's joystick (and the Up key)
    m_player->setTurnVelocity(Vector3D(glm::clamp(m_up - m_down - m_stick.y, -1.0f, 1.0f), glm::clamp(m_left - m_right - m_stick.x, -1.0f, 1.0f), 0));
    m_player->update(elapsedTime);
    m_position = m_player->position();

    Vector3D dir = m_player->dir();
    m_yaw = glm::atan(-dir.x, -dir.y);
    m_pitch = dir.z < -1.0f ? -glm::half_pi<float>() : dir.z > 1.0f ? glm::half_pi<float>() : glm::asin(dir.z);
    m_height = m_position.z - m_surface.heightAt(m_position.x, m_position.y);

    if (playerDestroyed())
    {
        updateDeathView(elapsedTime);
    }
    else
    {
        m_cameraMatrixInverted = Matrix(Vector4D(m_player->right(), 0), Vector4D(m_player->up(), 0), Vector4D(-m_player->dir(), 0), Vector4D(m_position, 1));
        m_cameraMatrix = glm::inverse(m_cameraMatrixInverted);
        m_headMotion.update(elapsedTime, m_player->velocity(), glm::mat3(m_cameraMatrixInverted));
    }

    m_conditionManager.update(elapsedTime);
    if (m_failTime < 0 && m_conditionManager.failed())
        m_failTime = m_time;

    m_conditionManager.testSpace(m_position.x, m_position.y, m_height);

    // attackers that are gone or asleep leave the player's list
    m_playerAttackers.erase(std::remove_if(m_playerAttackers.begin(), m_playerAttackers.end(), [](ActiveObject *unit) {
        return !unit->isEnabled() || unit->isAsleep() || unit->isDying();
    }), m_playerAttackers.end());

    // sleeping units (docs/ROADMAP.md, appendix J): beyond the player's sonar range plus 3 cells a unit
    // with sle stops altogether, and starts its order slot again when the player comes back
    const float sleepMargin = 3.0f*m_surface.scale().x;
    for (const auto &object : m_objects)
    {
        if (!object->isEnabled())
            continue;
        if (ActiveObject *unit = dynamic_cast<ActiveObject *>(object.get()))
        {
            if (unit->sleeps())
            {
                const float distance = glm::length(Vector2D(unit->position()) - Vector2D(m_position));
                // the player's sonar range plus 3 cells, whatever the unit's noise (docs/ROADMAP.md, appendix J)
                const float radius = m_sonar.passiveRange() + sleepMargin;
                if (unit->isAsleep())
                {
                    if (distance > radius)
                        continue;
                    unit->wake();
                }
                else if (distance > radius && unit->canSleep())
                {
                    unit->sleep();
                    continue;
                }
            }
        }
        if (ActiveObject *unit = dynamic_cast<ActiveObject *>(object.get()))
            unit->rechargeEnergy(elapsedTime);
        object->update(elapsedTime);
    }

    m_effectManager.update(elapsedTime);
    updateTorchContacts();

    for (const auto &torpedo : m_torpedoes)
        torpedo->update(elapsedTime);
    m_torpedoes.remove_if([this](const std::unique_ptr<Torpedo> &torpedo) {
        if (!torpedo->isDone())
            return false;
        m_collisionManager.removeObject(torpedo.get());
        return true;
    });

    m_sonar.update(elapsedTime);

    // positional sounds are heard from the player
    sfx::SoundSystem::get()->setListener(m_position.x, m_position.y, m_position.z,
                                         m_player->dir().x, m_player->dir().y, m_player->dir().z,
                                         m_player->up().x, m_player->up().y, m_player->up().z);
    if (!m_engineSounds)
        m_engineSounds.reset(new EngineSounds(this));
    m_engineSounds->update();
}


void Scenario::log(const String &text, LogColor color)
{
    m_log.push_back({ m_time, text, color });
}


void Scenario::logEvent(const String &name, const ConditionEvent *event, txt::StringFight what)
{
    if (!name.isEmpty() && event->isUsed())
        log(name + " " + txt::StringTable::get(what), LogColor::Amber);
}


bool Scenario::engagePlayer(ActiveObject *unit)
{
    if (std::find(m_playerAttackers.begin(), m_playerAttackers.end(), unit) != m_playerAttackers.end())
        return true;
    static const int limits[] = { 4, 6, 8 };
    if (int(m_playerAttackers.size()) >= limits[difficulty()])
        return false;
    m_playerAttackers.push_back(unit);
    return true;
}


void Scenario::disengagePlayer(ActiveObject *unit)
{
    m_playerAttackers.erase(std::remove(m_playerAttackers.begin(), m_playerAttackers.end(), unit), m_playerAttackers.end());
}


void Scenario::explosionAt(const Vector3D &position, float strength)
{
    if (!m_player || playerDestroyed())
        return;
    // felt up to 80 m away, pushing the boat away from the blast
    const Vector3D away = m_position - position;
    const float distance = glm::length(away);
    const float felt = strength*std::max(0.0f, 1.0f - distance/80.0f);
    if (felt <= 0)
        return;
    m_headMotion.shake(felt);
    if (distance > 1e-3f)
        m_headMotion.jolt(away/distance*felt*4.0f);
}


void Scenario::draw()
{
    draw(m_projectionMatrix, m_cameraMatrix);
}


void Scenario::destroyPlayer()
{
    if (playerDestroyed())
        return;
    m_eventPlayerDestroyed.complete();
    // the camera starts in the hull and moves out to its place beside it
    m_deathCamera = m_player->position();
    m_deathYaw = -glm::half_pi<float>();
    m_deathPitch = glm::quarter_pi<float>();
    m_deathDistance = 3.0f;
}


// The death view (docs/ROADMAP.md, appendix R): the camera circles the hull at 45° a second, 45°
// above it and three hull radii away, in the hull's own frame, moving a tenth of a second's worth
// towards that place every frame and always looking at the hull. Once the hull is gone it comes
// down level and closes in to half a radius.
void Scenario::updateDeathView(float elapsedTime)
{
    m_deathYaw += glm::quarter_pi<float>()*elapsedTime;
    if (m_player->wreckGone())
    {
        m_deathPitch -= m_deathPitch*std::min(1.0f, 1.5f*elapsedTime);
        if (m_deathDistance > 0.5f)
            m_deathDistance -= 1.25f*elapsedTime;
    }
    const float d = m_deathDistance*m_player->hullRadius();
    const Vector3D offset(-d*std::cos(m_deathPitch)*std::sin(m_deathYaw), d*std::cos(m_deathPitch)*std::cos(m_deathYaw), d*std::sin(m_deathPitch));
    const Vector3D target = m_player->position();
    const Vector3D place = target + glm::transpose(m_player->orientation())*offset;
    m_deathCamera += (place - m_deathCamera)*std::min(1.0f, 10.0f*elapsedTime);

    Vector3D forward = target - m_deathCamera;
    forward = glm::length(forward) > 1e-3f ? glm::normalize(forward) : m_player->dir();
    Vector3D right = glm::cross(forward, Vector3D(0, 0, 1));
    right = glm::length(right) > 1e-3f ? glm::normalize(right) : m_player->right();
    const Vector3D up = glm::cross(right, forward);
    m_cameraMatrixInverted = Matrix(Vector4D(right, 0), Vector4D(up, 0), Vector4D(-forward, 0), Vector4D(m_deathCamera, 1));
    m_cameraMatrix = glm::inverse(m_cameraMatrixInverted);
}


void Scenario::draw(const Matrix &projection, const Matrix &camera)
{
    glEnable(GL_CULL_FACE);
    glFrontFace(GL_CW);

    glClearColor(0.0, 0.0, 0.15, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);

    gfx::Pipeline::setProjection(projection);
    gfx::Pipeline::setModelView(camera);

    // the nearest energy shots light the ground and the objects around them,
    // so they are set before the terrain is drawn
    gfx::Pipeline::PointLight shots[gfx::Pipeline::MaxPointLights];
    const int shotLights = m_effectManager.pointLights(m_position, shots, gfx::Pipeline::MaxPointLights);
    for (int i = 0; i < shotLights; i++)
        shots[i].position = Vector3D(camera*Vector4D(shots[i].position, 1));
    gfx::Pipeline::setPointLights(shots, shotLights);

    // the water's fog from the map's shading tables, on the terrain and everything else
    gfx::Pipeline::setFog(true);
    gfx::Pipeline::setFog(m_surface.fog());

    const float brightness = this->brightness();
    m_surface.draw(m_position, m_player->dir(), brightness);

    // the models take their rows of the shading tables by the terrain's rule, from the same
    // light at the eye, without any light of their own; their tables hold the palette itself
    // in the last row
    gfx::Pipeline::setLighting(false);
    gfx::Pipeline::setTableShading(m_surface.shading(brightness));
    gfx::Pipeline::setTableShading(true);
    for (const auto &object : m_objects)
        if (object->isEnabled())
            object->draw();
    for (const auto &torpedo : m_torpedoes)
        torpedo->draw();
    // the death view sees the player's hull until it is gone
    if (playerDestroyed() && !m_player->wreckGone())
        m_player->draw();
    gfx::Pipeline::setTableShading(false);

    // the sprites glow by themselves and fade with the distance
    gfx::Pipeline::setPointLights(nullptr, 0);
    m_effectManager.draw();
}

static float pitch = 1.0f;
void Scenario::keyPressEvent(input::KeyEvent *e)
{
    if (e->key() == input::Key_Left || e->key() == input::Key_4)
        m_left = 1.0f;
    if (e->key() == input::Key_Right || e->key() == input::Key_6)
        m_right = 1.0f;
    if (e->key() == input::Key_Up || e->key() == input::Key_8)
        m_up = 1.0f;
    if (e->key() == input::Key_Down || e->key() == input::Key_5)
        m_down = 1.0f;
    if (e->key() == input::Key_Q)
        m_player->setBoostKey(true);
    if (e->key() == input::Key_A)
    {
        m_forwards = m_backwards + 1;
        if (m_player->engineRunning())
        {
            m_player->setVelocityTarget(m_player->maxVelocity());
            m_player->setFullThrottle(true);
        }
    }
    if (e->key() == input::Key_Z)
    {
        m_backwards = m_forwards + 1;
        if (m_player->engineRunning())
        {
            m_player->setVelocityTarget(m_player->minVelocity());
            m_player->setFullThrottle(true);
        }
    }
    if (e->key() == input::Key_S)
    {
        m_velocityTarget += 2.778f;
        if (m_velocityTarget > m_player->maxVelocity())
            m_velocityTarget = m_player->maxVelocity();
        if (m_forwards == 0 && m_backwards == 0 && m_player->engineRunning())
            m_player->setVelocityTarget(m_velocityTarget);
    }
    if (e->key() == input::Key_X)
    {
        m_velocityTarget -= 2.778f;
        if (m_velocityTarget < m_player->minVelocity())
            m_velocityTarget = m_player->minVelocity();
        if (m_forwards == 0 && m_backwards == 0 && m_player->engineRunning())
            m_player->setVelocityTarget(m_velocityTarget);
    }
    if (e->key() == input::Key_C)
    {
        m_velocityTarget = 0;
        if (m_forwards == 0 && m_backwards == 0 && m_player->engineRunning())
            m_player->setVelocityTarget(m_velocityTarget);
    }
    if (e->key() == input::Key_QuoteLeft)
    {
        m_player->engineToggle();
        if (m_player->engineRunning())
        {
            if (m_forwards == 0 && m_backwards == 0)
                m_player->setVelocityTarget(m_velocityTarget);
            else
            {
                if (m_backwards > 0)
                    m_player->setVelocityTarget(m_player->minVelocity());
                if (m_forwards > m_backwards)
                    m_player->setVelocityTarget(m_player->maxVelocity());
                m_player->setFullThrottle(true);
            }
        }
        else
        {
            m_player->setVelocityTarget(0);
            m_player->setFullThrottle(false);
        }
    }
    if (e->key() == input::Key_Space)
        m_player->fire();
    // turret orders: F5-F8 for turret 1, F9-F12 for turret 2 (attack my target, sync, search and destroy, stop)
    if (e->key() >= input::Key_F5 && e->key() <= input::Key_F12)
    {
        const int index = e->key() >= input::Key_F9 ? 1 : 0;
        const PlayerTurret::Order orders[4] = { PlayerTurret::Order::Attack, PlayerTurret::Order::Sync, PlayerTurret::Order::Search, PlayerTurret::Order::Stop };
        if (PlayerTurret *turret = m_player->turret(index))
            turret->setOrder(orders[(e->key() - input::Key_F5) % 4]);
    }
    if (e->key() == input::Key_Tab)
        m_player->fireTorpedo();
    if (e->key() == input::Key_Return || e->key() == input::Key_Enter)
        m_player->selectNextTorpedo();
    if (e->key() == input::Key_Backspace || e->key() == input::Key_D)
        m_player->dropBuzzer();
}


void Scenario::keyReleaseEvent(input::KeyEvent *e)
{
    if (e->key() == input::Key_Left || e->key() == input::Key_4)
        m_left = 0.0f;
    if (e->key() == input::Key_Right || e->key() == input::Key_6)
        m_right = 0.0f;
    if (e->key() == input::Key_Up || e->key() == input::Key_8)
        m_up = 0.0f;
    if (e->key() == input::Key_Down || e->key() == input::Key_5)
        m_down = 0.0f;
    if (e->key() == input::Key_Q)
        m_player->setBoostKey(false);
    if (e->key() == input::Key_A)
    {
        m_forwards = 0;
        if (m_backwards > 0 && m_player->engineRunning())
            m_player->setVelocityTarget(m_player->minVelocity());
        else if (m_player->engineRunning())
        {
            m_player->setVelocityTarget(m_velocityTarget);
            m_player->setFullThrottle(false);
        }
    }
    if (e->key() == input::Key_Z)
    {
        m_backwards = 0;
        if (m_forwards > 0 && m_player->engineRunning())
            m_player->setVelocityTarget(m_player->maxVelocity());
        else if (m_player->engineRunning())
        {
            m_player->setVelocityTarget(m_velocityTarget);
            m_player->setFullThrottle(false);
        }
    }
    if (e->key() == input::Key_Space)
        m_player->fireStop();
}


StringList Scenario::loadReport() const
{
    StringList report = m_loadIssues;
    for (const String &issue : m_conditionManager.issues())
        report << "condition: " + issue;
    return report;
}


void Scenario::loadIssue(const String &text)
{
    m_loadIssues << text;
}


// Friendly fire: hits on units that aren't hostile to the player (faction 8), aren't destroy objectives or mines
// get escalating warnings (texts 67-70, at most one every 3 s); each friendly kill is reported (71),
// and the third fails the mission (89).
void Scenario::playerHit(ActiveObject *unit, bool destroyed)
{
    int id = -1;
    for (const auto &record : m_records)
        if (record.object == unit)
            id = record.id;
    if (id < 0 || m_conditionManager.iff(id) < 0 || isHostile(PlayerFaction, m_conditionManager.iff(id)) || m_conditionManager.isDestroyTarget(id) || dynamic_cast<Mine *>(unit))
        return;

    auto say = [this, unit](int text) {
        m_conditionManager.addCondRadio(unit->position(), txt::StringTable::get(txt::StringFight(text)))->complete();
    };
    if (destroyed)
    {
        m_friendlyKills++;
        say(71);
        if (m_friendlyKills >= 3)
        {
            say(89);
            m_conditionManager.fail();
        }
    }
    else if (m_time - m_lastFriendlyWarning >= 3.0f)
    {
        say(67 + std::min(m_friendlyWarnings, 3));
        m_friendlyWarnings++;
        m_lastFriendlyWarning = m_time;
    }
}


Torpedo* Scenario::launchTorpedo(int type, const Vector3D &position, const Vector3D &direction, float speed, ActiveObject *owner, ActiveObject *target)
{
    if (!TorpedoType::get(type))
        return nullptr;
    m_torpedoes.emplace_back(new Torpedo(this, type, position, direction, speed, owner, target));
    m_torpedoesLaunched++;
    // turrets and guns can shoot it down
    m_collisionManager.addObject(m_torpedoes.back().get());
    sfx::SampleMap::playAt(sfx::Sample::Torpedo_Launch, position.x, position.y, position.z);
    return m_torpedoes.back().get();
}


int Scenario::threatsTo(const ActiveObject *unit) const
{
    return std::count_if(m_torpedoes.begin(), m_torpedoes.end(), [unit](const std::unique_ptr<Torpedo> &torpedo) {
        return !torpedo->isDone() && torpedo->target() == unit;
    });
}


// a torch lit since the last frame becomes a contact; the others follow their torches
void Scenario::updateTorchContacts()
{
    for (const auto &torch : m_torchContacts)
        torch->follow();
    std::vector<Particles::DecoyInfo> decoys;
    m_effectManager.particles().decoys(decoys);
    int last = m_lastTorchContact;
    for (const Particles::DecoyInfo &decoy : decoys)
        if (decoy.kind == Particles::Decoy::Torch && decoy.id > m_lastTorchContact)
        {
            m_torchContacts.push_back(std::make_unique<TorchContact>(this, decoy.id, decoy.faction, decoy.noise));
            last = std::max(last, decoy.id);
        }
    m_lastTorchContact = last;
}


// Within the type's sonar range: the Tiger and Man Eater Sharks and the Flash Sharks (sen 3) take the
// nearest buzzer, whoever dropped it; the Thresher and Bull Sharks (sen 4) the nearest buzzer
// of a hostile faction, or else what is loudest by its noise over the distance squared, the
// target or a hostile torch, if that is more than 3 over the range squared; the others (sen 99)
// never turn away
int Scenario::lure(const Torpedo *torpedo) const
{
    const int sensor = torpedo->type().sensor;
    if (sensor != 3 && sensor != 4)
        return 0;
    const float range2 = torpedo->type().sensorRange*torpedo->type().sensorRange;
    const Vector3D &position = torpedo->position();
    std::vector<Particles::DecoyInfo> decoys;
    m_effectManager.particles().decoys(decoys);

    int buzzer = 0;
    float nearest = range2;
    for (const Particles::DecoyInfo &decoy : decoys)
    {
        const float distance2 = glm::distance2(decoy.position, position);
        if (decoy.kind == Particles::Decoy::Buzzer && distance2 < nearest && (sensor == 3 || isHostile(torpedo->iff(), decoy.faction)))
        {
            nearest = distance2;
            buzzer = decoy.id;
        }
    }
    if (buzzer || sensor == 3)
        return buzzer;

    float loudest = 3/range2;
    if (const ActiveObject *target = torpedo->target())
    {
        const float distance2 = std::max(glm::distance2(target->center(), position), 1e-3f);
        if (distance2 < range2)
            loudest = std::max(loudest, target->noise()/distance2);
    }
    int torch = 0;
    for (const Particles::DecoyInfo &decoy : decoys)
    {
        const float distance2 = std::max(glm::distance2(decoy.position, position), 1e-3f);
        if (decoy.kind == Particles::Decoy::Torch && distance2 < range2 && isHostile(torpedo->iff(), decoy.faction) && decoy.noise/distance2 > loudest)
        {
            loudest = decoy.noise/distance2;
            torch = decoy.id;
        }
    }
    return torch;
}


float Scenario::brightness() const
{
    const float fadeIn = 1.5f, fadeOut = 2.0f;
    float brightness = glm::clamp(m_time/fadeIn, 0.0f, 1.0f);
    if (m_failTime >= 0)
        brightness *= glm::clamp(1.0f - (m_time - m_failTime)/fadeOut, 0.0f, 1.0f);
    return brightness*m_dimming;
}


float Scenario::radiation() const
{
    return glm::clamp(1.0f - depth()/100.0f, 0.0f, 1.0f);
}


// (docs/ROADMAP.md, appendix D) Each refresh draws the monitor with a chance of 1 - 0.7 times the
// radiation, from 1 a hundred metres down to 0.3 at the surface, and leaves it blank otherwise
// until the next refresh
bool Scenario::monitorBlanked(Monitor monitor) const
{
    const float radiation = this->radiation();
    if (radiation <= 0)
        return false;
    static const float refresh[5] = { 0.1f, 0.2f, 0.18f, 0.21f, 0.23f };
    const int index = int(monitor);
    const float tick = std::floor(m_time/refresh[index]);
    // a hash of the refresh, so the drawing takes nothing from the game's random numbers
    const float noise = glm::fract(std::sin(tick*12.9898f + index*78.233f)*43758.5453f);
    return noise >= 1.0f - 0.7f*radiation;
}


void Scenario::holdThrottle(float velocity)
{
    m_velocityTarget = velocity;
    if (m_player->engineRunning())
    {
        m_player->setVelocityTarget(velocity);
        m_player->setFullThrottle(false);
    }
}


Vector3D Scenario::current(const Vector3D &position) const
{
    if (m_currentGrid.empty())
        return Vector3D(0);

    // the block the position is in, with no fade at its edges: a boat half a cell outside doesn't move
    const Vector3D &scale = m_surface.scale();
    const auto it = m_currentGrid.find(std::make_pair(int(glm::floor(position.x/scale.x/2)), int(glm::floor(position.y/scale.y/2))));
    return it != m_currentGrid.end() ? it->second : Vector3D(0);
}


void Scenario::markGround()
{
    m_groundMarks.erase(std::remove_if(m_groundMarks.begin(), m_groundMarks.end(), [this](const GroundMark &mark) {
        if (!mark.object->isEnabled())
            return false;
        const Vector3D position = mark.object->position();
        const float before = m_surface.heightAt(position.x, position.y);
        m_surface.setHeight(mark.x, mark.y, mark.x, mark.y);
        m_surface.darken(mark.x, mark.y, mark.shade);
        m_surface.invalidate(mark.x, mark.y);
        // it keeps its height over the flattened ground
        mark.object->setPosition(position + Vector3D(0, 0, m_surface.heightAt(position.x, position.y) - before));
        return true;
    }), m_groundMarks.end());
}


Vector3D Scenario::getPosition()
{
    Vector3D pos;
    pos.x = m_file.value("px").toInt() + 0.5f;
    pos.y = m_file.value("py").toInt() + 0.5f;
    pos.z = m_file.value("pz").toInt() + m_file.value("hei").toInt();

    pos *= m_surface.scale();
    pos.z += m_surface.heightAt(pos.x, pos.y);

    return pos;
}


} // namespace fight
