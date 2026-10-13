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

#include <QGLContext>
#include <QSettings>
#include <QTime>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/transform.hpp>
#include <algorithm>


namespace fight {


static int s_difficultyOverride = -1;
static int s_randomSeed = -1;


int Scenario::difficulty()
{
    if (s_difficultyOverride >= 0)
        return s_difficultyOverride;
    return glm::clamp(QSettings().value("Game/difficulty", 1).toInt(), 0, 2);
}


void Scenario::setDifficulty(int difficulty)
{
    QSettings().setValue("Game/difficulty", glm::clamp(difficulty, 0, 2));
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

// current speeds in m/s for spd 0 and spd 2; estimates, to be measured in the original
const float CurrentSpeedSlow = 5.0f;
const float CurrentSpeedFast = 12.0f;

}


Scenario::Scenario(const QString &name) :
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
    qsrand(name.right(4).toInt());

    m_file.load(QString("vfx:scenario/%1.des").arg(name));

    m_file.setSection("surface");
    m_depth = m_file.value("depth").toInt();
    m_surface.load(m_file.value("name").toString().toLower(), m_file.value("maxheightscale").toInt(), m_file.value("patchcomb").toInt());

    // dtyp -> object file, decoded from the original FIGHT.EXE (docs/ROADMAP.md, appendix A)
    std::map<int, QString> types = {
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
        types[68 + i] = QString("cship%1").arg(i);
        types[78 + i] = QString("bship%1").arg(i);
    }

    float initialDir = 0.0f;
    QSet<QString> placeholderSections;
    std::vector<std::pair<ActiveObject *, std::vector<OrderSlot>>> pendingOrders;
    std::vector<ActiveObject *> absentUnits; // only at a higher difficulty
    const int difficulty = Scenario::difficulty();
    foreach (const QString &section, m_file.sections().filter(QRegExp("^movable\\d*"))) {
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
        const int type = m_file.value("typ").toInt();
        const int dType = m_file.value("dtyp").toInt();
        QString desName;

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
                objectDes.load(QString("vfx:sobjects/%1.des").arg(desName));
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
                object = new Tank(this, objectDes, info, m_file.value("card").toInt()*glm::quarter_pi<float>());
                object->setPosition(getPosition());
            }
            break;

        case TypeTower:
        case TypeTorpedoTower:
            {
                object = new TurretBase(this, objectDes, info);
                object->setPosition(getPosition());
            }
            break;

        case TypeBuilding:
            {
                objectDes.load(QString("vfx:sobjects/gp%1x%1_%2.des").arg(m_file.value("siz").toInt() + 1).arg(m_file.value("buityp").toInt()));
                object = new Building(this, objectDes, m_file.value("siz").toInt(), m_file.value("card", 0).toInt() * 45.0f, m_file.value("px").toInt(), m_file.value("py").toInt(), m_file.value("refx").toInt(), m_file.value("refy").toInt());
            }
            break;

        case TypeMine:
            {
                // TODO: dis num
                object = new Mine(this, objectDes, info);
                object->setPosition(getPosition());
            }
            break;

        case TypePlayer:
            m_position = getPosition();
            m_yaw = m_file.value("card").toInt()*glm::quarter_pi<float>();
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
                object = new Tank(this, objectDes, info, m_file.value("card").toInt()*glm::quarter_pi<float>());
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
                object = new SimpleObject(this, objectDes, info, 16);
                object->setPosition(getPosition());
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
                loadIssue(QString("placeholder: typ %1 dtyp %2").arg(type).arg(dType));
                placeholderSections << section;
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

        // order orderSlots; all of them are read, so the load report only lists what is really ignored
        std::vector<OrderSlot> orderSlots;
        bool slotsEnded = false;
        for (int n = 0; n < 8; n++)
        {
            auto value = [this, n](const char *name, int defaultValue) {
                const QString key = QString("o%1%2").arg(n).arg(name);
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
            if (slot.type == OrderSlot::None)
                slotsEnded = true;
            if (!slotsEnded)
                orderSlots.push_back(slot);
        }
        if (!orderSlots.empty())
        {
            ActiveObject *unit = dynamic_cast<ActiveObject *>(object);
            if (unit)
            {
                pendingOrders.emplace_back(unit, orderSlots);
                const bool moves = dynamic_cast<SimpleBoat *>(unit) != nullptr;
                for (const OrderSlot &slot : orderSlots)
                {
                    const bool forAll = slot.type == OrderSlot::Wait || slot.type == OrderSlot::CardinalPatrol ||
                                        slot.type == OrderSlot::CardinalAttack || slot.type == OrderSlot::Explode;
                    if (!OrderSlot::isImplemented(slot.type))
                        loadIssue(QString("order not implemented: %1").arg(OrderSlot::name(slot.type)));
                    else if (!moves && !forAll)
                        loadIssue(QString("order not run: %1 for typ %2").arg(OrderSlot::name(slot.type)).arg(type));
                }
            }
            else if (object)
                loadIssue(QString("orders not run: typ %1").arg(type));
        }

        // Reading Objectives
        for (int i = 0; i < 16; i++)
        {
            int wccond = m_file.value(QString("wccond%1").arg(i)).toInt();
            if (wccond == 0)
                continue;
            m_conditionManager.updateObjective(id, i, wccond);
        }
    }

    // currents: each cell covers a 2x2 block of map cells; speeds aren't in the data (docs/ROADMAP.md, appendix D)
    foreach (const QString &section, m_file.sections().filter(QRegExp("^current\\d*", Qt::CaseInsensitive))) {
        m_file.setSection(section);
        m_file.value("id");
        if (m_file.value("typ").toInt() != 3072 || m_file.value("pz").toInt() != 0)
            loadIssue(QString("unhandled: current typ %1 pz %2").arg(m_file.value("typ").toInt()).arg(m_file.value("pz").toInt()));

        const int card = m_file.value("card").toInt();
        const float speed = m_file.value("spd").toInt() == 0 ? CurrentSpeedSlow : CurrentSpeedFast;
        Vector3D direction;
        if (card == 8)
            direction = Vector3D(0, 0, -1);
        else if (card == 9)
            direction = Vector3D(0, 0, 1);
        else
            direction = Vector3D(glm::sin(card*glm::quarter_pi<float>()), glm::cos(card*glm::quarter_pi<float>()), 0);

        const int x = m_file.value("px").toInt();
        const int y = m_file.value("py").toInt();
        m_currentCells.push_back({ x, y, direction*speed });
        m_currentGrid[std::make_pair(x/2, y/2)] = direction*speed;
    }

    foreach (const QString &section, m_file.sections().filter(QRegExp("^radio\\d*"))) {
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
            loadIssue(QString("unhandled: radio typ %1").arg(type));

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
        for (OrderSlot &slot : pending.second)
            slot.breakCondition = m_conditionManager.addBreakCondition(slot.cond1, slot.dep1, slot.ref1, slot.op, slot.cond2, slot.dep2, slot.ref2, slot.del);
        pending.first->setOrders(new Orders(this, pending.first, pending.second));
    }

    std::sort(m_navPoints.begin(), m_navPoints.end(), [](NavPoint *a, NavPoint *b) { return a->num() < b->num(); });

    m_file.setSection("AttitudeMatrix");
    for (int i = 0; i < 9; i++)
        for (int j = 0; j < 9; j++)
            m_attitudeMatrix[i][j] = m_file.value(QString("X%1Y%2").arg(j).arg(i)).toInt();

    m_file.setSection("End");
    m_endType = m_file.value("Type").toInt();

    // report what the loader ignored, with numbers in names collapsed to '#'
    QRegExp digits("\\d+");
    foreach (const QString &section, m_file.sections()) {
        if (placeholderSections.contains(section))
            continue;
        const QString sectionName = QString(section).replace(digits, "#");
        if (!m_file.wasSectionRead(section)) {
            loadIssue(QString("unread section: %1").arg(sectionName));
            continue;
        }
        foreach (const QString &key, m_file.unreadKeys(section))
            loadIssue(QString("unread key: %1/%2").arg(sectionName, QString(key).replace(digits, "#")));
    }



    qsrand(s_randomSeed >= 0 ? uint(s_randomSeed) : uint(QTime::currentTime().second() * 1000 + QTime::currentTime().msec()));

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
    m_time += elapsedTime;

    m_player->setTurnVelocity(Vector3D(m_up - m_down, m_left - m_right, 0));
    m_player->update(elapsedTime);
    m_position = m_player->position();

    Vector3D dir = m_player->dir();
    m_yaw = glm::atan(-dir.x, -dir.y);
    m_pitch = dir.z < -1.0f ? -glm::half_pi<float>() : dir.z > 1.0f ? glm::half_pi<float>() : glm::asin(dir.z);
    m_height = m_position.z - m_surface.heightAt(m_position.x, m_position.y);

    m_cameraMatrixInverted = Matrix(Vector4D(m_player->right(), 0), Vector4D(m_player->up(), 0), Vector4D(-m_player->dir(), 0), Vector4D(m_position, 1));
    m_cameraMatrix = glm::inverse(m_cameraMatrixInverted);
    m_headMotion.update(elapsedTime, m_player->velocity(), glm::mat3(m_cameraMatrixInverted));

    m_conditionManager.update(elapsedTime);

    m_conditionManager.testSpace(m_position.x, m_position.y, m_height);

    for (const auto &object : m_objects)
        if (object->isEnabled())
            object->update(elapsedTime);

    m_effectManager.update(elapsedTime);

    for (const auto &torpedo : m_torpedoes)
        torpedo->update(elapsedTime);
    m_torpedoes.remove_if([this](const std::unique_ptr<Torpedo> &torpedo) {
        if (!torpedo->isDone())
            return false;
        m_collisionManager.removeObject(torpedo.get());
        return true;
    });
    m_decoys.erase(std::remove_if(m_decoys.begin(), m_decoys.end(), [this](const Decoy &d) { return d.until < m_time; }), m_decoys.end());

    m_sonar.update(elapsedTime);

    // positional sounds are heard from the player
    sfx::SoundSystem::get()->setListener(m_position.x, m_position.y, m_position.z,
                                         m_player->dir().x, m_player->dir().y, m_player->dir().z,
                                         m_player->up().x, m_player->up().y, m_player->up().z);
    if (!m_engineSounds)
        m_engineSounds.reset(new EngineSounds(this));
    m_engineSounds->update();
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


void Scenario::draw(const Matrix &projection, const Matrix &camera)
{
    glEnable(GL_CULL_FACE);
    glFrontFace(GL_CW);
    glEnable(GL_NORMALIZE);

    glClearColor(0.0, 0.0, 0.15, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);

    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(glm::value_ptr(projection));

    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(glm::value_ptr(camera));

    GLfloat global_ambient[] = { 0.5f, 0.5f, 1.0f, 1.0f };
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, global_ambient);

    glEnable(GL_LIGHTING);

    GLfloat light_ambient[] = { 4.0, 4.0, 4.0, 1.0 };
    GLfloat light_diffuse[] = { 1.0, 1.0, 1.0, 1.0 };
    glLightfv(GL_LIGHT0, GL_AMBIENT, light_ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse);
    
    glLightfv(GL_LIGHT0, GL_POSITION, glm::value_ptr(Vector4D(m_player->position() - m_player->dir()*5.0f, 1)));
    glLightfv(GL_LIGHT0, GL_SPOT_DIRECTION, glm::value_ptr(m_player->dir()));

    glLightf(GL_LIGHT0, GL_SPOT_CUTOFF, 90.0);
    glLightf(GL_LIGHT0, GL_CONSTANT_ATTENUATION, 0.1);
    glLightf(GL_LIGHT0, GL_QUADRATIC_ATTENUATION, 0.0002);
    glLightf(GL_LIGHT0, GL_SPOT_EXPONENT, 4.0);

    glEnable(GL_LIGHT0);

    if (m_lightSources.size() > 0)
    {
        Object *object = m_lightSources[0];

        glLightfv(GL_LIGHT1, GL_AMBIENT, light_ambient);
        glLightfv(GL_LIGHT1, GL_DIFFUSE, light_diffuse);
        
        glLightfv(GL_LIGHT1, GL_POSITION, glm::value_ptr(object->position()));
        glLightfv(GL_LIGHT1, GL_SPOT_DIRECTION, glm::value_ptr(Vector3D(glm::cos(0.005f*m_time), glm::sin(0.005f*m_time), 0.0f)));

        glLightf(GL_LIGHT1, GL_SPOT_CUTOFF, 90.0);
        glLightf(GL_LIGHT1, GL_CONSTANT_ATTENUATION, 0.1);
        glLightf(GL_LIGHT1, GL_QUADRATIC_ATTENUATION, 0.0002);
        glLightf(GL_LIGHT1, GL_SPOT_EXPONENT, 4.0);

        glEnable(GL_LIGHT1);
    }

    glEnable(GL_FOG);
    GLfloat fogColor[4] = {0.0, 0.0, 0.15, 1.0};
    glFogi(GL_FOG_MODE, GL_LINEAR);
    glFogfv(GL_FOG_COLOR, fogColor);
    glHint(GL_FOG_HINT, GL_FASTEST);
    glFogf(GL_FOG_START, 100.0);
    glFogf(GL_FOG_END, 200.0);

    m_surface.draw(m_position, m_player->dir());

    for (const auto &object : m_objects)
        if (object->isEnabled())
            object->draw();
    for (const auto &torpedo : m_torpedoes)
        torpedo->draw();

    m_effectManager.draw();
}

static float pitch = 1.0f;
void Scenario::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Left || e->key() == Qt::Key_4)
        m_left = 1.0f;
    if (e->key() == Qt::Key_Right || e->key() == Qt::Key_6)
        m_right = 1.0f;
    if (e->key() == Qt::Key_Up || e->key() == Qt::Key_8)
        m_up = 1.0f;
    if (e->key() == Qt::Key_Down || e->key() == Qt::Key_5)
        m_down = 1.0f;
    if (e->key() == Qt::Key_Q)
        m_player->setDebugBoost(100);
    if (e->key() == Qt::Key_W)
        m_player->setDebugBoost(-100);
    if (e->key() == Qt::Key_A)
    {
        m_forwards = m_backwards + 1;
        if (m_player->engineRunning())
        {
            m_player->setVelocityTarget(m_player->maxVelocity());
            m_player->setFullThrottle(true);
        }
    }
    if (e->key() == Qt::Key_Z)
    {
        m_backwards = m_forwards + 1;
        if (m_player->engineRunning())
        {
            m_player->setVelocityTarget(m_player->minVelocity());
            m_player->setFullThrottle(true);
        }
    }
    if (e->key() == Qt::Key_S)
    {
        m_velocityTarget += 2.778f;
        if (m_velocityTarget > m_player->maxVelocity())
            m_velocityTarget = m_player->maxVelocity();
        if (m_forwards == 0 && m_backwards == 0 && m_player->engineRunning())
            m_player->setVelocityTarget(m_velocityTarget);
    }
    if (e->key() == Qt::Key_X)
    {
        m_velocityTarget -= 2.778f;
        if (m_velocityTarget < m_player->minVelocity())
            m_velocityTarget = m_player->minVelocity();
        if (m_forwards == 0 && m_backwards == 0 && m_player->engineRunning())
            m_player->setVelocityTarget(m_velocityTarget);
    }
    if (e->key() == Qt::Key_C)
    {
        m_velocityTarget = 0;
        if (m_forwards == 0 && m_backwards == 0 && m_player->engineRunning())
            m_player->setVelocityTarget(m_velocityTarget);
    }
    if (e->key() == Qt::Key_QuoteLeft)
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
    if (e->key() == Qt::Key_Space)
        m_player->fire();
    // turret orders: F5-F8 for turret 1, F9-F12 for turret 2 (attack my target, sync, search and destroy, stop)
    if (e->key() >= Qt::Key_F5 && e->key() <= Qt::Key_F12)
    {
        const int index = e->key() >= Qt::Key_F9 ? 1 : 0;
        const PlayerTurret::Order orders[4] = { PlayerTurret::Order::Attack, PlayerTurret::Order::Sync, PlayerTurret::Order::Search, PlayerTurret::Order::Stop };
        if (PlayerTurret *turret = m_player->turret(index))
            turret->setOrder(orders[(e->key() - Qt::Key_F5) % 4]);
    }
    if (e->key() == Qt::Key_Tab)
        m_player->fireTorpedo();
    if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter)
        m_player->selectNextTorpedo();
    if (e->key() == Qt::Key_Backspace || e->key() == Qt::Key_D)
        m_player->dropBuzzer();
}


void Scenario::keyReleaseEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Left || e->key() == Qt::Key_4)
        m_left = 0.0f;
    if (e->key() == Qt::Key_Right || e->key() == Qt::Key_6)
        m_right = 0.0f;
    if (e->key() == Qt::Key_Up || e->key() == Qt::Key_8)
        m_up = 0.0f;
    if (e->key() == Qt::Key_Down || e->key() == Qt::Key_5)
        m_down = 0.0f;
    if (e->key() == Qt::Key_Q)
        m_player->setDebugBoost(0);
    if (e->key() == Qt::Key_W)
        m_player->setDebugBoost(0);
    if (e->key() == Qt::Key_A)
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
    if (e->key() == Qt::Key_Z)
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
    if (e->key() == Qt::Key_Space)
        m_player->fireStop();
}


QStringList Scenario::loadReport() const
{
    QStringList report = m_loadIssues;
    foreach (const QString &issue, m_conditionManager.issues())
        report << "condition: " + issue;
    return report;
}


void Scenario::loadIssue(const QString &text)
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


void Scenario::addDecoy(const Vector3D &position, float seconds)
{
    m_decoys.push_back({ position, m_time + seconds });
}


bool Scenario::decoyNear(const Vector3D &position, float range, const Torpedo *self, Vector3D &decoy) const
{
    float nearest = range;
    bool found = false;
    auto consider = [&](const Vector3D &candidate) {
        const float distance = glm::length(candidate - position);
        if (distance < nearest)
        {
            nearest = distance;
            decoy = candidate;
            found = true;
        }
    };
    for (const Decoy &d : m_decoys)
        consider(d.position);
    for (const auto &torpedo : m_torpedoes)
        if (torpedo.get() != self && !torpedo->isDone() && torpedo->type().isDecoy())
            consider(torpedo->position());
    return found;
}


Vector3D Scenario::current(const Vector3D &position) const
{
    if (m_currentGrid.empty())
        return Vector3D(0);

    // bilinear between the centres of the 2x2 blocks, so the current fades in and out at the edges
    const Vector3D &scale = m_surface.scale();
    const float u = position.x/scale.x/2 - 0.5f;
    const float v = position.y/scale.y/2 - 0.5f;
    const int i = int(glm::floor(u));
    const int j = int(glm::floor(v));
    const float fu = u - i;
    const float fv = v - j;
    auto at = [this](int x, int y) {
        const auto it = m_currentGrid.find(std::make_pair(x, y));
        return it != m_currentGrid.end() ? it->second : Vector3D(0);
    };
    return glm::mix(glm::mix(at(i, j), at(i + 1, j), fu), glm::mix(at(i, j + 1), at(i + 1, j + 1), fu), fv);
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
