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

#include "hud.h"
#include <QKeyEvent>
#include <QMouseEvent>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions>
#include <QSettings>
#include <algorithm>
#include <cmath>
#include <iostream>
#include "util/ticks.h"
#include "fight/scenario.h"
#include "fight/objects/navpoint.h"
#include "fight/objects/activeobject.h"
#include "fight/boats/playerturret.h"
#include "game/boat.h"
#include "game/window.h"
#include "gfx/image.h"
#include "gfx/pipeline.h"

#include <glm/gtx/transform.hpp>

#include "crosshair.h"
#include "heading.h"
#include "beta.h"
#include "meter.h"
#include "digiblock.h"
#include "radiomessage.h"
#include "energybar.h"
#include "shield.h"
#include "activesonar.h"
#include "master.h"
#include "mastermonitor.h"
#include "sonarmonitor.h"
#include "autopilot.h"
#include "horizon.h"
#include "debugoverlay.h"
#include "missionmenu.h"
#include "pause.h"
#include "alert.h"
#include "slavemonitor.h"
#include "symbols.h"
#include "cockpit3d.h"
#include "sfx/soundsystem.h"
#include "txt/stringtable.h"


namespace hud {


namespace {

bool g_settingsRead = false;
HUD::CockpitMode g_cockpitMode = HUD::CockpitMode::Art;
float g_headMotion = 1;
bool g_lookFixed = false;
Vector2D g_look;
float g_fixedStep = 0;

// mouse look: degrees per pixel dragged, how far the head turns (further, the eye would be in the
// headrest), and the neck it turns about (below and behind the eye, so the eye moves as well)
const float LookSpeed = glm::radians(0.18f);
const Vector2D LookMin(glm::radians(-105.0f), glm::radians(-55.0f));
const Vector2D LookMax(glm::radians(105.0f), glm::radians(65.0f));
const Vector3D Neck(0, -0.10f, 0.08f);
// after the drag the head turns back to the front, about two thirds of the way in this time
const float LookReturn = 0.18f;

void readSettings()
{
    if (g_settingsRead)
        return;
    g_settingsRead = true;
    QSettings settings;
    g_cockpitMode = settings.value("cockpit/mode").toString() == "3d" ? HUD::CockpitMode::Model : HUD::CockpitMode::Art;
    g_headMotion = settings.value("cockpit/shake", 1.0f).toFloat();
}

// The pilot's head turned by `rotation` (boat camera frame): how points at infinity move in the
// view, as a homography of its normalized device coordinates. Applied to clip coordinates, it
// keeps HUD symbols in the window on what they mark in the world.
Matrix viewTurn(const Matrix &projection, const glm::mat3 &rotation)
{
    // camera directions to homogeneous x, y, w of the view
    glm::mat3 k;
    for (int c = 0; c < 3; c++)
    {
        k[c][0] = projection[c][0];
        k[c][1] = projection[c][1];
        k[c][2] = projection[c][3];
    }
    const glm::mat3 h = k*glm::transpose(rotation)*glm::inverse(k);
    Matrix m(1);
    for (int c = 0; c < 2; c++)
    {
        m[c][0] = h[c][0];
        m[c][1] = h[c][1];
        m[c][3] = h[c][2];
    }
    m[3][0] = h[2][0];
    m[3][1] = h[2][1];
    m[3][3] = h[2][2];
    return m;
}

}


HUD::CockpitMode HUD::cockpitMode()
{
    readSettings();
    return g_cockpitMode;
}


void HUD::setCockpitMode(CockpitMode mode, bool save)
{
    readSettings();
    g_cockpitMode = mode;
    if (save)
        QSettings().setValue("cockpit/mode", mode == CockpitMode::Model ? "3d" : "2d");
}


float HUD::headMotion()
{
    readSettings();
    return g_headMotion;
}


void HUD::setFixedStep(float seconds)
{
    g_fixedStep = seconds;
}


void HUD::setLook(float yaw, float pitch)
{
    g_lookFixed = true;
    g_look = glm::clamp(Vector2D(glm::radians(yaw), glm::radians(pitch)), LookMin, LookMax);
}


void HUD::setHeadMotion(float strength, bool save)
{
    readSettings();
    g_headMotion = strength;
    if (save)
        QSettings().setValue("cockpit/shake", strength);
}


HUD::HUD() :
    m_scenario(nullptr),
    m_debugOverlay(nullptr),
    m_debugOverlayVisible(false),
    m_menu(nullptr),
    m_paused(false),
    m_frozen(false),
    m_successShown(false),
    m_failureShown(false),
    m_deathTime(-1),
    m_focal(245.5f),
    m_looking(false),
    m_colorTable("gfx:pal/gui/border.pal"),
    m_fontGreen("gfx:fnt/cpit1ahc.fnt", m_colorTable, true, false),
    m_fontRed("gfx:fnt/cpit1bhc.fnt", m_colorTable, true, false),
    m_fontYellow("gfx:fnt/cpit1chc.fnt", m_colorTable, true, false)
{

}


HUD::~HUD()
{
}


bool HUD::showsModel() const
{
    return cockpitMode() == CockpitMode::Model && m_model && m_model->isValid();
}


Rect HUD::readRect(const txt::DesFile &file)
{
    return Rect(file.value("X1").toInt(), file.value("Y1").toInt(), file.value("Width").toInt(), file.value("Height").toInt());
}


gfx::Image HUD::getImage(const String &name)
{
    return gfx::Image::load(String("gfx:img/cockpit/%1.img").arg(name), m_colorTable);
}


void HUD::load(game::Boat *boat)
{
    m_boat = boat;
    for (const auto &child : m_children)
        child->setParentWidget(nullptr);
    m_children.clear();

    m_wide = false;
    m_cockpit.setTexture(gfx::Image::load(String("vfx:cockpit/%1/cockpit.r16").arg(boat->cockpit()), 640, 480, true));

    txt::DesFile file(String("vfx:cockpit/%1.des").arg(boat->cockpit()));

    file.setSection("front");
    m_center = glm::ivec2(file.value("VFX_StartX").toInt() + file.value("VFX_FlightX").toInt(), file.value("VFX_StartY").toInt() + file.value("VFX_FlightY").toInt());

    // the camera the cockpit art was rendered with (horizontal view angle over 640 pixels)
    file.setSection("camera");
    const float viewAngle = file.contains("ViewAngle") ? file.value("ViewAngle").toFloat() : 105.0f;
    m_focal = 320/std::tan(glm::radians(viewAngle)/2);
    const QString model = Cockpit3D::find(boat->cockpit());
    m_model = model.isEmpty() ? nullptr : Cockpit3D::get(model);
    if (m_model)
        m_model->setCamera(m_focal, m_center);
    else if (cockpitMode() == CockpitMode::Model)
        std::cerr << "No 3D cockpit cockpit_" << boat->cockpit() << ".glb in "
                  << Cockpit3D::directory().toUtf8().constData() << ", showing the 2D cockpit" << std::endl;
    m_rectGL = Rect();

    m_integerScale = true;

    file.setSection("hudshield");
    Shield *shield = new Shield(this, readRect(file));
    m_children.emplace_back(shield);

    file.setSection("hudactivesonarphase");
    ActiveSonar *activeSonar = new ActiveSonar(this, readRect(file));
    m_children.emplace_back(activeSonar);

    file.setSection("hudautopilot");
    Autopilot *autopilot = new Autopilot(this, readRect(file));
    m_children.emplace_back(autopilot);

    file.setSection("radiomessage");
    RadioMessage *radioMessage = new RadioMessage(this, Rect(file.value("X").toInt(), file.value("Y").toInt(), 640 - file.value("X").toInt()*2, 12));
    m_children.emplace_back(radioMessage);

    // status monitors: with two, the first shows a weapon and the second the armor (I switches it
    // to the systems and back); a single one shows a weapon (I steps through all three pages)
    m_slaveMonitors.clear();
    for (int i = 0; i < 2; i++)
    {
        file.setSection(String("slavemonitor%1").arg(i));
        if (!file.contains("X1"))
            continue;
        SlaveMonitor *monitor = new SlaveMonitor(this, file, i == 0 ? SlaveMonitor::Page::Weapon : SlaveMonitor::Page::Armor);
        m_children.emplace_back(monitor);
        m_slaveMonitors.push_back(monitor);
    }

    file.setSection("hudalertblock");
    Alert *alert = new Alert(this, readRect(file));
    m_children.emplace_back(alert);

    file.setSection("hudthreatblock");
    Threat *threat = new Threat(this, readRect(file));
    m_children.emplace_back(threat);

    file.setSection("hudsymbolblock1");
    Symbols *symbols = new Symbols(this, readRect(file));
    m_children.emplace_back(symbols);

    file.setSection("hudpauseblock");
    Pause *pause = new Pause(this, readRect(file));
    m_children.emplace_back(pause);

    m_menu = new MissionMenu(this, m_center);
    m_children.emplace_back(m_menu);

    m_integerScale = true;

    file.setSection("hudheading");
    Heading *heading = new Heading(this, readRect(file));
    m_children.emplace_back(heading);

    file.setSection("hudbeta");
    Beta *beta = new Beta(this, readRect(file));
    m_children.emplace_back(beta);

    file.setSection("hudmeter");
    Meter *meter = new Meter(this, readRect(file), glm::ivec2(file.value("BarX1").toInt(), file.value("BarY1").toInt()), file.value("BarHeight").toInt(), false);
    m_children.emplace_back(meter);

    file.setSection("hudkmeter");
    Meter *kmeter = new Meter(this, readRect(file), glm::ivec2(file.value("BarX1").toInt(), file.value("BarY1").toInt()), file.value("BarHeight").toInt(), true);
    m_children.emplace_back(kmeter);

    file.setSection("huddigiblock1");
    DigiBlock *digiBlock = new DigiBlock(this, readRect(file));
    m_children.emplace_back(digiBlock);

    file.setSection("hudenergybar");
    EnergyBar *energyBar = new EnergyBar(this, readRect(file));
    m_children.emplace_back(energyBar);

    file.setSection("mastermonitor");
    MasterMonitor *masterMonitor = new MasterMonitor(this, readRect(file), Rect(file.value("ObservationX1").toInt(), file.value("ObservationY1").toInt(), file.value("ObservationWidth").toInt(), file.value("ObservationHeight").toInt()), file);
    m_children.emplace_back(masterMonitor);

    file.setSection("roundmonitor0");
    m_sonar1 = new SonarMonitor(this, file, 360);
    m_children.emplace_back(m_sonar1);

    file.setSection("roundmonitor1");
    m_sonar2 = new SonarMonitor(this, file, 180);
    m_children.emplace_back(m_sonar2);

    m_integerScale = false;

    file.setSection("masterhud");
    Master *master = new Master(this, readRect(file));
    m_children.emplace_back(master);

    Crosshair *crosshair = new Crosshair(this, m_center, &master->clipRect());
    m_children.emplace_back(crosshair);

    file.setSection("hudsynthetichorizon");
    Horizon *horizon = new Horizon(this, m_center, file);
    m_children.emplace_back(horizon);

    m_debugOverlay = new DebugOverlay(this);
    m_children.emplace_back(m_debugOverlay);
    setDebugOverlay(m_debugOverlayVisible);
}


void HUD::setDebugOverlay(bool visible)
{
    m_debugOverlayVisible = visible;
    if (!m_debugOverlay)
        return;
    if (visible)
        m_debugOverlay->show();
    else
        m_debugOverlay->hide();
}


void HUD::start(fight::Scenario *scenario)
{
    hideCursor();
    m_rectGL = Rect();
    m_scenario = scenario;
    m_scenario->setBoat(m_boat);
    m_lastTicks = util::ticks();
    m_frames = 0;
    m_navPoint = -1;
    m_paused = false;
    m_frozen = false;
    m_successShown = false;
    m_failureShown = false;
    m_deathTime = -1;
    m_menu->open(MissionMenu::Page::None);
    m_scenario->headMotion().setStrength(headMotion());
}


void HUD::step(float elapsedTime)
{
    if (!m_scenario)
        return;

    setFrozen(m_paused || m_menu->pauses());
    if (m_frozen)
        elapsedTime = 0;
    while (elapsedTime > 0.0f) {
        const float step = std::min(elapsedTime, 0.02f);
        m_scenario->update(step);
        elapsedTime -= step;
    }

    updateMissionState();
}


void HUD::setFrozen(bool frozen)
{
    if (frozen == m_frozen)
        return;
    m_frozen = frozen;
    if (frozen)
        sfx::SoundSystem::get()->pauseAll();
    else
        sfx::SoundSystem::get()->resumeAll();
}


void HUD::updateMissionState()
{
    auto text = [](int n) { return txt::StringTable::get(txt::StringFight(n)).trimmed(); };
    const fight::ConditionManager &conditions = m_scenario->conditionManager();
    const MissionMenu::Page page = m_menu->page();

    if (m_scenario->playerDestroyed())
    {
        if (m_deathTime < 0)
        {
            m_deathTime = m_scenario->time();
            m_menu->notice({ text(13) }, 3.0f);
        }
        else if (m_scenario->time() >= m_deathTime + 3.0f && page != MissionMenu::Page::Died)
        {
            m_menu->open(MissionMenu::Page::Died);
            m_eventDied();
        }
        return;
    }

    if (conditions.failed() && !m_failureShown)
    {
        m_failureShown = true;
        m_menu->notice({ text(12) }, 10.0f);
    }
    if (conditions.failureEnded() && page != MissionMenu::Page::GameOver)
    {
        m_menu->open(MissionMenu::Page::GameOver);
        m_eventFailure();
    }
    if (conditions.autopilot() && !m_successShown)
    {
        m_successShown = true;
        m_menu->notice({ text(11) }, 4.0f);
    }
}


void HUD::leave(util::event<> &event)
{
    // paused sounds would never play again
    setFrozen(false);
    m_paused = false;
    m_scenario = nullptr;
    event();
}


void HUD::setRect(const Rect &rect)
{
    if (m_rectGL.width == rect.width && m_rectGL.height == rect.height)
        return;

    m_rectGL = rect;
    Renderer::setRect(rect);

    const RectD ortho = rectOrtho();
    if (m_wide)
        m_rectHUD = RectF(rect.x, rect.y - ortho.y/ortho.height*rect.height, rect.width, rect.height*480/ortho.height);
    else
        m_rectHUD = RectF(rect.x - ortho.x/ortho.width*rect.width, rect.y - ortho.y/ortho.height*rect.height, rect.width*640/ortho.width, rect.height*480/ortho.height);

    if (m_scenario)
    {
        Renderer::setRect(Rect(glm::round(m_rectHUD.x), glm::round(m_rectHUD.y), glm::round(m_rectHUD.width), glm::round(m_rectHUD.height)));
        m_centerNDC = Vector2D(m_projectionMatrix * Vector4D(m_center, 0, 1));
        // the 3D cockpit needs the world in the perspective of the cockpit art
        m_scenario->setRect(m_rectHUD, m_centerNDC, showsModel() ? glm::degrees(2*std::atan(240/m_focal)) : 60.0f);
    }

    int w = 640;
    int h = 480;
    while (w + 640 <= rect.width && h + 480 <= rect.height)
    {
        w += 640;
        h += 480;
    }
    m_integerScaleProjectionMatrix = glm::ortho(0.0f, m_rectHUD.width*640/w, m_rectHUD.height*480/h, 0.0f);
    m_integerScaleProjectionMatrixInverted = glm::inverse(m_integerScaleProjectionMatrix);
    m_noScaleProjectionMatrix = glm::ortho(0.0f, m_rectHUD.width, m_rectHUD.height, 0.0f);
    m_noScaleProjectionMatrixInverted = glm::inverse(m_noScaleProjectionMatrix);
}


Point HUD::project(const Point &point)
{
    return glm::ivec2(glm::round(hudProjectionMatrixInverted() * (m_projectionMatrix * Vector4D(point, 0, 1))));
}


Rect HUD::projectCenter(const Rect &rect)
{
    return Rect(project(rect.center()) - glm::ivec2(rect.width/2, rect.height/2), rect.size());
}


void HUD::draw()
{
    if (!m_scenario)
    {
        setupGL(true);
        m_cockpit.doDraw();
        return;
    }

    const int ticks = util::ticks();
    // the explicit physics integration is unstable for large steps (e.g. after the window was hidden)
    const float elapsed = g_fixedStep > 0 ? g_fixedStep : std::min((ticks - m_lastTicks)/1000.0f, 0.25f);
    m_frames++;
    step(elapsed);
    updateLook(g_fixedStep > 0 ? g_fixedStep : (ticks - m_lastTicks)/1000.0f);
    m_lastTicks = ticks;
    // a handler of the events in step() may have ended the mission
    if (!m_scenario)
    {
        setupGL(true);
        m_cockpit.doDraw();
        return;
    }

    // the GL viewport is in device pixels, the rects in logical pixels
    const double pixelRatio = window() ? window()->devicePixelRatio() : 1.0;
    if (showsModel() && drawModel(pixelRatio))
        return;
    if (rect().x != m_rectGL.x || rect().y != m_rectGL.y)
        glViewport(rect().x*pixelRatio, rect().y*pixelRatio, rect().width*pixelRatio, rect().height*pixelRatio);

    m_scenario->draw();

    setupGL(false);
    m_cockpit.doDraw();

    m_integerScale = true;
    gfx::Pipeline::setProjection(m_integerScaleProjectionMatrix);
    m_integerScaleWidget.doDraw();

    m_integerScale = false;
    gfx::Pipeline::setProjection(m_noScaleProjectionMatrix);
    m_noScaleWidget.doDraw();

    m_integerScale = true;
    gfx::Pipeline::setProjection(m_integerScaleProjectionMatrix);
    m_menu->doDraw();
    m_integerScale = false;

    if (rect().x != m_rectGL.x || rect().y != m_rectGL.y)
        glViewport(m_rectGL.x*pixelRatio, m_rectGL.y*pixelRatio, m_rectGL.width*pixelRatio, m_rectGL.height*pixelRatio);
}


// The 3D cockpit: the instruments go into a texture that is projected onto the model, the world
// and the model are drawn in the whole window as seen from the pilot's head, and the HUD in the
// window and the menu are drawn on top. False if the model can't be shown.
bool HUD::drawModel(double pixelRatio)
{
    QOpenGLFunctions *gl = QOpenGLContext::currentContext() ? QOpenGLContext::currentContext()->functions() : nullptr;
    if (!gl || !QOpenGLFramebufferObject::hasOpenGLFramebufferObjects())
    {
        m_model.reset();
        m_rectGL = Rect();
        return false;
    }

    // the instruments (everything in the art's pixel scale) with premultiplied alpha
    const QSize size(qRound(m_rectHUD.width*pixelRatio), qRound(m_rectHUD.height*pixelRatio));
    if (!m_instruments || m_instruments->size() != size)
        m_instruments.reset(new QOpenGLFramebufferObject(size, QOpenGLFramebufferObject::Depth));
    m_instruments->bind();
    glViewport(0, 0, size.width(), size.height());
    setupGL(false);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    gl->glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    m_integerScale = true;
    gfx::Pipeline::setProjection(m_integerScaleProjectionMatrix);
    m_integerScaleWidget.doDraw();
    m_integerScale = false;
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    m_instruments->release();

    // the whole window shows the world and the model; the 640x480 view is a part of it
    glViewport(m_rectGL.x*pixelRatio, m_rectGL.y*pixelRatio, m_rectGL.width*pixelRatio, m_rectGL.height*pixelRatio);
    const Matrix toWindow = glm::translate(Vector3D((2*(m_rectHUD.x - m_rectGL.x) + m_rectHUD.width)/m_rectGL.width - 1,
                                                    1 - (2*(m_rectHUD.y - m_rectGL.y) + m_rectHUD.height)/m_rectGL.height, 0))
                          * glm::scale(Vector3D(m_rectHUD.width/m_rectGL.width, m_rectHUD.height/m_rectGL.height, 1));
    const Matrix head = lookTransform()*m_scenario->headMotion().transform();
    const Matrix headInverse = glm::inverse(head);
    m_scenario->draw(toWindow*m_scenario->projectionMatrix(), headInverse*m_scenario->cameraMatrix());

    // the model in its own depth range, from 2 cm in front of the eye
    glClear(GL_DEPTH_BUFFER_BIT);
    const Matrix modelProjection = toWindow*glm::translate(Vector3D(m_centerNDC, 0))
                                 * glm::perspective(2*std::atan(240/m_focal), m_rectHUD.width/m_rectHUD.height, 0.02f, 20.0f);
    if (!m_model->draw(modelProjection, headInverse))
    {
        // the 2D cockpit from now on, with its view angle
        m_rectGL = Rect();
        return false;
    }
    m_model->drawDecal(modelProjection, headInverse, m_instruments->texture());

    // the HUD in the window is collimated: it turns with the head but doesn't shift; the menu stays put
    glViewport(rect().x*pixelRatio, rect().y*pixelRatio, rect().width*pixelRatio, rect().height*pixelRatio);
    setupGL(false);
    gfx::Pipeline::setProjection(viewTurn(m_scenario->projectionMatrix(), glm::mat3(head))*m_noScaleProjectionMatrix);
    m_noScaleWidget.doDraw();

    m_integerScale = true;
    gfx::Pipeline::setProjection(m_integerScaleProjectionMatrix);
    m_menu->doDraw();
    m_integerScale = false;

    glViewport(m_rectGL.x*pixelRatio, m_rectGL.y*pixelRatio, m_rectGL.width*pixelRatio, m_rectGL.height*pixelRatio);
    return true;
}


void HUD::keyPressEvent(QKeyEvent *e)
{
    if (!m_scenario)
        return;

    // the turret menu only takes digits, Esc and O; everything else still steers the boat
    const bool turretMenu = m_menu->page() != MissionMenu::Page::None && !m_menu->pauses();
    const bool menuKey = (e->key() >= Qt::Key_0 && e->key() <= Qt::Key_9) || e->key() == Qt::Key_Escape || e->key() == Qt::Key_O;
    if (m_menu->page() != MissionMenu::Page::None && (!turretMenu || menuKey))
    {
        switch (m_menu->keyPressEvent(e->key()))
        {
        case MissionMenu::Action::None:
        case MissionMenu::Action::Continue:
            break;
        case MissionMenu::Action::Abort:
            leave(m_eventAbort);
            break;
        case MissionMenu::Action::Restart:
            leave(m_eventRestart);
            break;
        case MissionMenu::Action::Load:
            leave(m_eventLoad);
            break;
        case MissionMenu::Action::TurretOrder:
            {
                const fight::PlayerTurret::Order orders[4] = { fight::PlayerTurret::Order::Attack, fight::PlayerTurret::Order::Sync,
                                                               fight::PlayerTurret::Order::Search, fight::PlayerTurret::Order::Stop };
                for (int i = 0; i < 2; i++)
                    if ((m_menu->turret() == i || m_menu->turret() < 0) && m_scenario->player()->turret(i))
                        m_scenario->player()->turret(i)->setOrder(orders[m_menu->turretOrder()]);
            }
            break;
        }
        return;
    }

    if (e->key() == Qt::Key_Escape)
    {
        m_menu->open(MissionMenu::Page::Main);
        return;
    }
    if (e->key() == Qt::Key_O)
    {
        m_menu->setTurrets((m_scenario->player()->turret(0) ? 1 : 0) + (m_scenario->player()->turret(1) ? 1 : 0));
        m_menu->open(MissionMenu::Page::Turrets);
        return;
    }
    // Alt+V switches between the 2D and the 3D cockpit
    if (e->key() == Qt::Key_V && (e->modifiers() & ~Qt::KeypadModifier) == Qt::AltModifier)
    {
        setCockpitMode(cockpitMode() == CockpitMode::Model ? CockpitMode::Art : CockpitMode::Model, true);
        if (cockpitMode() == CockpitMode::Model && !showsModel())
            std::cerr << "No 3D cockpit for cockpit " << m_boat->cockpit() << " in "
                      << Cockpit3D::directory().toUtf8().constData() << std::endl;
        m_menu->notice({ showsModel() ? "COCKPIT 3D" : "COCKPIT 2D" }, 1.5f);
        // the view angle changes with it
        m_rectGL = Rect();
        return;
    }
    // Ctrl+D, Cmd+D on macOS
    if (e->key() == Qt::Key_D && (e->modifiers() & Qt::ControlModifier))
    {
        setDebugOverlay(!m_debugOverlayVisible);
        return;
    }
    // debug: Ctrl+K (Cmd+K on macOS) skips the mission as if it had been accomplished
    if (e->key() == Qt::Key_K && (e->modifiers() & Qt::ControlModifier))
    {
        leave(m_eventSuccess);
        return;
    }
    if (e->key() == Qt::Key_P)
    {
        m_paused = !m_paused;
        return;
    }
    if (m_paused || m_scenario->playerDestroyed())
        return;
    if (e->key() == Qt::Key_N)
    {
        if ((e->modifiers() & ~Qt::KeypadModifier) == Qt::AltModifier)
        {
            if (m_navPoint >= 0)
                m_scenario->target().lockNavPoint(m_scenario->navPoints()[m_navPoint]);
        }
        else
        {
            m_navPoint++;
            while (m_navPoint < m_scenario->navPoints().size() && !m_scenario->navPoints()[m_navPoint]->isEnabled())
                m_navPoint++;
            if (m_navPoint >= m_scenario->navPoints().size())
                m_navPoint = -1;
        }
    }
    if (e->key() == Qt::Key_R)
    {
        if ((e->modifiers() & ~Qt::KeypadModifier) == Qt::AltModifier)
        {
            int scale = m_sonar1->scale();
            if (scale == 90)
                scale = 360;
            else
                scale /= 2;
            m_sonar1->setScale(scale);
            m_sonar2->setScale(scale/2);
        }
        else
            m_scenario->sonar().toggle();
    }
    if (e->key() == Qt::Key_L)
        m_scenario->target().lockReticle();
    if (e->key() == Qt::Key_U)
        m_scenario->target().lockNearestEnemy();
    if (e->key() == Qt::Key_I && !m_slaveMonitors.empty())
    {
        SlaveMonitor *monitor = m_slaveMonitors.back();
        switch (monitor->page())
        {
        case SlaveMonitor::Page::Weapon: monitor->setPage(SlaveMonitor::Page::Armor); break;
        case SlaveMonitor::Page::Armor: monitor->setPage(SlaveMonitor::Page::Systems); break;
        case SlaveMonitor::Page::Systems:
            monitor->setPage(m_slaveMonitors.size() > 1 ? SlaveMonitor::Page::Armor : SlaveMonitor::Page::Weapon);
            break;
        }
    }
    // F1-F4: the gun, the magazine or a turret on the weapon page
    if (e->key() >= Qt::Key_F1 && e->key() <= Qt::Key_F4 && !(e->modifiers() & Qt::AltModifier) && !m_slaveMonitors.empty())
    {
        SlaveMonitor *monitor = m_slaveMonitors.front();
        monitor->setPage(SlaveMonitor::Page::Weapon);
        monitor->showWeapon(SlaveMonitor::Weapon(e->key() - Qt::Key_F1));
    }
    if (e->key() == Qt::Key_A)
    {
        if ((e->modifiers() & ~Qt::KeypadModifier) == Qt::AltModifier)
        {
            if (m_scenario->conditionManager().autopilot())
            {
                leave(m_eventSuccess);
                return;
            }
            m_menu->notice({ txt::StringTable::get(txt::Fight31), txt::StringTable::get(txt::Fight32) }, 3.0f);
        }
    }

    m_scenario->keyPressEvent(e);
}


void HUD::updateLook(float elapsedTime)
{
    if (g_lookFixed)
        m_look = g_look;
    else if (!m_looking || !showsModel())
        m_look *= std::exp(-std::max(0.0f, elapsedTime)/LookReturn);
}


Matrix HUD::lookTransform() const
{
    return glm::translate(Neck)*glm::rotate(m_look.x, Vector3D(0, 1, 0))*glm::rotate(m_look.y, Vector3D(1, 0, 0))*glm::translate(-Neck);
}


void HUD::mousePressEvent(QMouseEvent *e)
{
    if (!m_scenario || !showsModel() || g_lookFixed || m_looking)
        return;
    m_looking = true;
    m_lookStart = Point(e->pos().x(), e->pos().y());
    m_lookFrom = m_look;
}


void HUD::mouseMoveEvent(QMouseEvent *e)
{
    if (!m_looking)
        return;
    // drag right to look right, up to look up
    const Point drag = Point(e->pos().x(), e->pos().y()) - m_lookStart;
    m_look = glm::clamp(m_lookFrom - LookSpeed*Vector2D(drag), LookMin, LookMax);
}


void HUD::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->buttons() == Qt::NoButton)
        m_looking = false;
}


void HUD::keyReleaseEvent(QKeyEvent *e)
{
    if (!m_scenario)
        return;

    m_scenario->keyReleaseEvent(e);
}


} // namespace hud
