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
#include "gfx/framebuffer.hpp"
#include "gfx/gl.hpp"
#include <cmath>
#include "util/datapath.h"
#include "util/settings.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
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
#include "pages.h"
#include "pause.h"
#include "alert.h"
#include "slavemonitor.h"
#include "symbols.h"
#include "cockpit3d.h"
#include "touchcontrols.h"
#include "sfx/soundsystem.h"

#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_keyboard.h>
#include "txt/stringtable.h"


namespace hud {


namespace {

bool g_settingsRead = false;
HUD::CockpitMode g_cockpitMode = HUD::CockpitMode::Art;
float g_headMotion = 1;
bool g_lookFixed = false;
Vector2D g_look;
float g_fixedStep = 0;
// the touch controls are shown once a finger touched the screen in a mission, until a key or a
// game controller is used (on the iPad from the start without either)
bool g_touchControls = false;
bool g_touchControlsChecked = false;
// the keys held by touch buttons, after those of the game controller's buttons
const int TouchButtons = 100;

// mouse look: degrees per pixel dragged, how far the head turns (further, the eye would be in the
// headrest), and the neck it turns about (below and behind the eye, so the eye moves as well)
const float LookSpeed = glm::radians(0.18f);
const Vector2D LookMin(glm::radians(-105.0f), glm::radians(-55.0f));
const Vector2D LookMax(glm::radians(105.0f), glm::radians(65.0f));
const Vector3D Neck(0, -0.10f, 0.08f);
// after the drag the head turns back to the front, about two thirds of the way in this time
const float LookReturn = 0.18f;
// the head follows the right stick of a game controller, two thirds of the way in this time
const float LookFollow = 0.08f;
// the d-pad changes the speed again and again while held: after this long, then at this interval
const float RepeatDelay = 0.4f;
const float RepeatInterval = 0.1f;

void readSettings()
{
    if (g_settingsRead)
        return;
    g_settingsRead = true;
    g_cockpitMode = util::Settings::value("cockpit/mode") == "3d" ? HUD::CockpitMode::Model : HUD::CockpitMode::Art;
    g_headMotion = util::Settings::floatValue("cockpit/shake", 1.0f);
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
        util::Settings::setValue("cockpit/mode", mode == CockpitMode::Model ? "3d" : "2d");
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
        util::Settings::setValue("cockpit/shake", strength);
}


HUD::HUD() :
    m_scenario(nullptr),
    m_debugOverlay(nullptr),
    m_debugOverlayVisible(false),
    m_menu(nullptr),
    m_pages(nullptr),
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
    TouchControls::Handlers handlers;
    handlers.press = [this](const input::KeyEvent &key, int button) { pressKey(key, TouchButtons + button); };
    handlers.release = [this](const input::KeyEvent &, int button) { releaseKey(TouchButtons + button); };
    handlers.stick = [this](const Vector2D &stick) { m_touchStick = stick; updateStick(); };
    handlers.lookStart = [this](const PointD &position) { startLook(position); };
    handlers.lookMove = [this](const PointD &position) { moveLook(position); };
    handlers.lookEnd = [this]() { m_looking = false; };
    m_touch.reset(new TouchControls(m_fontGreen, m_fontYellow, handlers));

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
    const std::string model = Cockpit3D::find(boat->cockpit());
    m_model = model.empty() ? nullptr : Cockpit3D::get(model);
    if (m_model)
        m_model->setCamera(m_focal, m_center);
    else if (cockpitMode() == CockpitMode::Model)
        std::cerr << "No 3D cockpit cockpit_" << boat->cockpit() << ".glb in "
                  << Cockpit3D::directory() << ", showing the 2D cockpit" << std::endl;
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

    // status monitors: with two, the first shows a weapon and the second the armor; I switches
    // between the armor and the system state (a single one starts on a weapon; F1-F4 bring it back)
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
    m_pages = new Pages(this);
    m_children.emplace_back(m_pages);

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
    m_menu->setScreen(readRect(file));
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
    // (not the time the mission took to load)
    m_frameRate = FrameRate();
    m_frameBegin = std::chrono::steady_clock::time_point();
    m_countFrames = 0;
    m_countLongest = 0;
    m_countWork = 0;
    m_navPoint = -1;
    m_paused = false;
    m_frozen = false;
    m_successShown = false;
    m_failureShown = false;
    m_deathTime = -1;
    m_menu->open(MissionMenu::Page::None);
    m_scenario->headMotion().setStrength(headMotion());
    // the CD's audio tracks, as the game data import copies them out of the CD image, at half
    // the volume (CD audio is mastered louder than the game's sounds)
    if (m_scenario->cdTrack() > 0 && sfx::Music::isEnabled()) {
        char name[32];
        std::snprintf(name, sizeof(name), "sfx:cd/track%02d.wav", m_scenario->cdTrack());
        m_music.play(util::DataPath::resolve(name), 0.5f);
    }
#if defined(SDL_PLATFORM_IOS)
    // on the iPad without a keyboard and a game controller the touch controls are there at once
    if (!g_touchControlsChecked)
        g_touchControls = !SDL_HasKeyboard() && !SDL_HasGamepad();
#endif
    g_touchControlsChecked = true;
    // a finger on the screen comes back to the controls of the new mission when it is lifted
    m_touch->reset();
    m_touchStick = Vector2D(0);
    for (auto it = m_heldKeys.begin(); it != m_heldKeys.end(); )
        it = it->first >= TouchButtons ? m_heldKeys.erase(it) : std::next(it);
    if (m_repeat >= TouchButtons)
        m_repeat = -1;
    updateStick();
}


void HUD::step(float elapsedTime)
{
    if (!m_scenario)
        return;

    // (also in a pause)
    m_touch->update(elapsedTime);
    setFrozen(m_paused || m_menu->pauses());
    m_music.update();
    if (m_frozen)
        elapsedTime = 0;
    if (m_repeat >= 0 && elapsedTime > 0)
    {
        m_repeatWait -= elapsedTime;
        while (m_repeatWait <= 0 && m_repeat >= 0)
        {
            m_repeatWait += RepeatInterval;
            input::KeyEvent key = m_heldKeys[m_repeat];
            press(&key);
        }
    }
    while (elapsedTime > 0.0f) {
        const float step = std::min(elapsedTime, 0.02f);
        m_scenario->update(step);
        elapsedTime -= step;
    }

    updateMissionState();
}


void HUD::pause()
{
    if (!m_scenario)
        return;
    m_paused = true;
    setFrozen(true);
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
    m_music.pause(frozen);
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


void HUD::setDestinations(const StringList &names)
{
    m_menu->setDestinations(names);
}


void HUD::setBriefing(const StringList &text, const StringList &targets, const StringList &tips)
{
    m_pages->setBriefing(text, targets, tips);
}


int HUD::destination() const
{
    return m_menu->destinations().isEmpty() ? -1 : m_menu->destination();
}


void HUD::engageAutopilot()
{
    // with one destination the original went there at once
    if (m_menu->destinations().size() <= 1)
        leave(m_eventSuccess);
    else
        m_menu->open(MissionMenu::Page::Autopilot);
}


void HUD::leave(util::event<> &event)
{
    // paused sounds would never play again
    setFrozen(false);
    m_music.stop();
    m_paused = false;
    m_scenario = nullptr;
    event();
}


void HUD::setRect(const Rect &rect)
{
    m_touch->setSize(rect.width, rect.height);
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
    const auto begin = std::chrono::steady_clock::now();
    drawFrame();
    countFrame(begin, std::chrono::steady_clock::now());
}


bool HUD::touchControls()
{
    return g_touchControls;
}


void HUD::countFrame(std::chrono::steady_clock::time_point begin, std::chrono::steady_clock::time_point end)
{
    // with a fixed step the screenshots repeat exactly, which the times wouldn't
    if (!m_scenario || g_fixedStep > 0)
        return;
    using Milliseconds = std::chrono::duration<double, std::milli>;
    if (m_frameBegin == std::chrono::steady_clock::time_point())
    {
        m_countBegin = begin;
    }
    else
    {
        m_countFrames++;
        m_countLongest = std::max(m_countLongest, Milliseconds(begin - m_frameBegin).count());
        m_countWork += Milliseconds(end - begin).count();
    }
    m_frameBegin = begin;
    const double counted = Milliseconds(begin - m_countBegin).count();
    if (counted >= 1000 && m_countFrames > 0)
    {
        m_frameRate = { float(m_countFrames*1000/counted), float(counted/m_countFrames), float(m_countLongest), float(m_countWork/m_countFrames) };
        m_countBegin = begin;
        m_countFrames = 0;
        m_countLongest = 0;
        m_countWork = 0;
    }
}


void HUD::drawFrame()
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
    {
        drawTouchControls(pixelRatio);
        return;
    }
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
    m_pages->doDraw();
    m_integerScale = false;

    if (rect().x != m_rectGL.x || rect().y != m_rectGL.y)
        glViewport(m_rectGL.x*pixelRatio, m_rectGL.y*pixelRatio, m_rectGL.width*pixelRatio, m_rectGL.height*pixelRatio);
    drawTouchControls(pixelRatio);
}


// The 3D cockpit: the instruments go into a texture that is projected onto the model, the world
// and the model are drawn in the whole window as seen from the pilot's head, and the HUD in the
// window and the menu are drawn on top. False if the model can't be shown.
bool HUD::drawModel(double pixelRatio)
{
    // the instruments (everything in the art's pixel scale) with premultiplied alpha
    const int width = int(std::lround(m_rectHUD.width*pixelRatio)), height = int(std::lround(m_rectHUD.height*pixelRatio));
    if (!m_instruments || m_instruments->width() != width || m_instruments->height() != height)
        m_instruments.reset(new gfx::Framebuffer(width, height));
    if (!m_instruments->isValid())
    {
        m_model.reset();
        m_rectGL = Rect();
        return false;
    }
    m_instruments->bind();
    glViewport(0, 0, width, height);
    setupGL(false);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
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
    m_pages->doDraw();
    m_integerScale = false;

    glViewport(m_rectGL.x*pixelRatio, m_rectGL.y*pixelRatio, m_rectGL.width*pixelRatio, m_rectGL.height*pixelRatio);
    return true;
}


// the keyboard: the touch controls go
void HUD::keyPressEvent(input::KeyEvent *e)
{
    if (e->key() != input::Key_None)
        g_touchControls = false;
    press(e);
}


void HUD::press(input::KeyEvent *e)
{
    if (!m_scenario)
        return;

    // the turret and autopilot menus only take digits, Esc and O; everything else still steers the boat
    const bool turretMenu = m_menu->page() != MissionMenu::Page::None && !m_menu->pauses();
    const bool menuKey = (e->key() >= input::Key_0 && e->key() <= input::Key_9) || e->key() == input::Key_Escape || e->key() == input::Key_O;
    if (m_menu->page() != MissionMenu::Page::None && (!turretMenu || menuKey))
    {
        menuAction(int(m_menu->keyPressEvent(e->key())));
        return;
    }

    // the help (H turns its pages), the radio log and the briefing (B or Enter turns its pages)
    // lie over the view until Esc (or M) closes them; the boat is still steered meanwhile
    if (m_pages->page() != Pages::Page::None)
    {
        if (e->key() == input::Key_Escape || (e->key() == input::Key_M && m_pages->page() == Pages::Page::Radio))
        {
            m_pages->close();
            return;
        }
        if ((e->key() == input::Key_H && m_pages->page() == Pages::Page::Help) ||
            ((e->key() == input::Key_B || e->key() == input::Key_Return || e->key() == input::Key_Enter) && m_pages->page() == Pages::Page::Briefing))
        {
            m_pages->next();
            return;
        }
    }
    if (e->key() == input::Key_Escape)
    {
        m_menu->open(MissionMenu::Page::Main);
        return;
    }
    if (e->key() == input::Key_H && !(e->modifiers() & ~input::KeypadModifier))
    {
        m_pages->open(Pages::Page::Help);
        return;
    }
    if (e->key() == input::Key_M && !(e->modifiers() & ~input::KeypadModifier))
    {
        m_pages->open(Pages::Page::Radio);
        return;
    }
    if (e->key() == input::Key_B && !(e->modifiers() & ~input::KeypadModifier))
    {
        m_pages->open(Pages::Page::Briefing);
        return;
    }
    // Alt+I sets the Fixer's repair priority (appendix O)
    if (e->key() == input::Key_I && (e->modifiers() & ~input::KeypadModifier) == input::AltModifier)
    {
        m_scenario->player()->cycleFixer();
        return;
    }
    if (e->key() == input::Key_O)
    {
        m_menu->setTurrets((m_scenario->player()->turret(0) ? 1 : 0) + (m_scenario->player()->turret(1) ? 1 : 0));
        m_menu->open(MissionMenu::Page::Turrets);
        return;
    }
    // Alt+V switches between the 2D and the 3D cockpit
    if (e->key() == input::Key_V && (e->modifiers() & ~input::KeypadModifier) == input::AltModifier)
    {
        setCockpitMode(cockpitMode() == CockpitMode::Model ? CockpitMode::Art : CockpitMode::Model, true);
        if (cockpitMode() == CockpitMode::Model && !showsModel())
            std::cerr << "No 3D cockpit for cockpit " << m_boat->cockpit() << " in "
                      << Cockpit3D::directory() << std::endl;
        m_menu->notice({ showsModel() ? "COCKPIT 3D" : "COCKPIT 2D" }, 1.5f);
        // the view angle changes with it
        m_rectGL = Rect();
        return;
    }
    // Ctrl+D, Cmd+D on macOS
    if (e->key() == input::Key_D && (e->modifiers() & input::ControlModifier))
    {
        setDebugOverlay(!m_debugOverlayVisible);
        return;
    }
    // debug: Ctrl+K (Cmd+K on macOS) skips the mission as if it had been accomplished
    if (e->key() == input::Key_K && (e->modifiers() & input::ControlModifier))
    {
        engageAutopilot();
        return;
    }
    if (e->key() == input::Key_P)
    {
        m_paused = !m_paused;
        return;
    }
    if (m_paused || m_scenario->playerDestroyed())
        return;
    if (e->key() == input::Key_N)
    {
        if ((e->modifiers() & ~input::KeypadModifier) == input::AltModifier)
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
    if (e->key() == input::Key_R)
    {
        if ((e->modifiers() & ~input::KeypadModifier) == input::AltModifier)
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
    if (e->key() == input::Key_L)
        m_scenario->target().lockReticle();
    if (e->key() == input::Key_U)
        m_scenario->target().lockNearestEnemy();
    if (e->key() == input::Key_I && !m_slaveMonitors.empty())
    {
        SlaveMonitor *monitor = m_slaveMonitors.back();
        switch (monitor->page())
        {
        case SlaveMonitor::Page::Weapon: monitor->setPage(SlaveMonitor::Page::Armor); break;
        case SlaveMonitor::Page::Armor: monitor->setPage(SlaveMonitor::Page::Systems); break;
        case SlaveMonitor::Page::Systems: monitor->setPage(SlaveMonitor::Page::Armor); break;
        }
    }
    // F1-F4: the gun, the magazine or a turret on the weapon page
    if (e->key() >= input::Key_F1 && e->key() <= input::Key_F4 && !(e->modifiers() & input::AltModifier) && !m_slaveMonitors.empty())
    {
        SlaveMonitor *monitor = m_slaveMonitors.front();
        monitor->setPage(SlaveMonitor::Page::Weapon);
        monitor->showWeapon(SlaveMonitor::Weapon(e->key() - input::Key_F1));
    }
    if (e->key() == input::Key_A)
    {
        if ((e->modifiers() & ~input::KeypadModifier) == input::AltModifier)
        {
            if (m_scenario->conditionManager().autopilot())
            {
                engageAutopilot();
                return;
            }
            m_menu->notice({ txt::StringTable::get(txt::Fight31), txt::StringTable::get(txt::Fight32) }, 3.0f);
        }
    }

    m_scenario->keyPressEvent(e);
}


void HUD::menuAction(int action)
{
    switch (MissionMenu::Action(action))
    {
    case MissionMenu::Action::None:
        break;
    // also after a pause (P, or the app interrupted on the iPad)
    case MissionMenu::Action::Continue:
        m_paused = false;
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
    case MissionMenu::Action::Autopilot:
        leave(m_eventSuccess);
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
}


// The keys a game controller's buttons stand for in a mission, after the original's joystick
// buttons where it had them (fire, lock the target in the reticle, fire a torpedo, the next
// torpedo); LB held gives the second set
input::KeyEvent HUD::buttonKey(input::Button button)
{
    using namespace input;
    const bool second = m_shoulder;
    switch (button)
    {
    case Button_RightTrigger: return KeyEvent(Key_Space);
    case Button_LeftTrigger: return KeyEvent(Key_Tab);
    case Button_RightShoulder: return KeyEvent(Key_L);
    case Button_South: return second ? KeyEvent(Key_A, AltModifier) : KeyEvent(Key_U);
    case Button_East: return second ? KeyEvent(Key_QuoteLeft) : KeyEvent(Key_D);
    case Button_West: return second ? KeyEvent(Key_D, ControlModifier) : KeyEvent(Key_Return);
    case Button_North: return second ? KeyEvent(Key_N, AltModifier) : KeyEvent(Key_N);
    case Button_Up: return second ? KeyEvent(Key(Key_F1 + m_weaponPage)) : KeyEvent(Key_S);
    case Button_Down: return second ? KeyEvent(Key_I) : KeyEvent(Key_X);
    case Button_Left: return second ? KeyEvent(Key_Q) : KeyEvent(Key_C);
    case Button_Right: return second ? KeyEvent(Key_R, AltModifier) : KeyEvent(Key_R);
    case Button_LeftStick: return KeyEvent(Key_A);
    case Button_RightStick: return KeyEvent(Key_Z);
    case Button_Start: return second ? KeyEvent(Key_P) : KeyEvent(Key_Escape);
    case Button_Back: return second ? KeyEvent(Key_V, AltModifier) : KeyEvent(Key_O);
    default: return KeyEvent();
    }
}


void HUD::buttonPressEvent(input::Button button)
{
    g_touchControls = false;
    if (!m_scenario || button == input::Button_None)
        return;
    if (button == input::Button_LeftShoulder)
    {
        m_shoulder = true;
        return;
    }

    // in the menus the d-pad picks an option, A takes it and B goes back; the turret menu leaves
    // the other buttons to the boat
    if (m_menu->page() != MissionMenu::Page::None)
    {
        const bool turretMenu = !m_menu->pauses();
        switch (button)
        {
        case input::Button_Up:
        case input::Button_Down:
            m_menu->select(button == input::Button_Up ? -1 : 1);
            return;
        case input::Button_South:
            menuAction(int(m_menu->choose()));
            return;
        case input::Button_East:
            menuAction(int(m_menu->back()));
            return;
        case input::Button_Start:
            if (!turretMenu)
            {
                menuAction(int(m_menu->back()));
                return;
            }
            break;
        case input::Button_Back:
            if (turretMenu)
            {
                input::KeyEvent key(input::Key_O);
                press(&key);
                return;
            }
            break;
        default:
            break;
        }
        if (!turretMenu)
            return;
    }

    const bool menu = m_menu->page() != MissionMenu::Page::None;
    const input::KeyEvent key = buttonKey(button);
    pressKey(key, button);
    if (!m_scenario)
        return;
    // a menu opened by a button shows its highlight at once
    if (!menu && m_menu->page() != MissionMenu::Page::None)
        m_menu->select(0);
    if (key.key() >= input::Key_F1 && key.key() <= input::Key_F4)
        m_weaponPage = (m_weaponPage + 1) % 4;
}


void HUD::buttonReleaseEvent(input::Button button)
{
    if (button == input::Button_None)
        return;
    if (button == input::Button_LeftShoulder)
        m_shoulder = false;
    releaseKey(button);
}


void HUD::stickEvent(input::Stick stick, const Vector2D &position)
{
    if (position != Vector2D(0))
        g_touchControls = false;
    if (stick == input::RightStick)
        m_lookStick = position;
    else
    {
        m_padStick = position;
        updateStick();
    }
}


// A key a game controller's button or a touch button stands for: held until the button is let
// go; the speed keys repeat meanwhile
void HUD::pressKey(input::KeyEvent key, int source)
{
    m_heldKeys[source] = key;
    if (key.key() == input::Key_None)
        return;
    press(&key);
    if (m_scenario && (key.key() == input::Key_S || key.key() == input::Key_X))
    {
        m_repeat = source;
        m_repeatWait = RepeatDelay;
    }
}


void HUD::releaseKey(int source)
{
    if (source == m_repeat)
        m_repeat = -1;
    const auto held = m_heldKeys.find(source);
    if (held == m_heldKeys.end())
        return;
    input::KeyEvent key = held->second;
    m_heldKeys.erase(held);
    if (key.key() != input::Key_None)
        release(&key);
}


// both sticks steer, the controller's and the one on the touch screen
void HUD::updateStick()
{
    if (m_scenario)
        m_scenario->setStick(glm::clamp(m_padStick + m_touchStick, Vector2D(-1), Vector2D(1)));
}


void HUD::touchPressEvent(input::TouchEvent *e)
{
    if (!m_scenario)
        return;
    g_touchControls = true;
    // (also without drawing, in headless runs)
    if (window())
        m_touch->setSize(window()->width(), window()->height());
    // a tap on an option of a menu takes it
    if (m_menu->page() != MissionMenu::Page::None)
    {
        const int option = menuOption(e->position());
        if (option >= 0)
        {
            menuAction(int(m_menu->choose(option)));
            return;
        }
    }
    m_touch->press(e->id(), e->position());
}


void HUD::touchMoveEvent(input::TouchEvent *e)
{
    if (m_scenario)
        m_touch->move(e->id(), e->position());
}


void HUD::touchReleaseEvent(input::TouchEvent *e)
{
    if (m_scenario)
        m_touch->release(e->id());
}


// the option of the menu at a point of the window, or -1
int HUD::menuOption(const PointD &position)
{
    // the menu is drawn in the HUD's rect, scaled by whole numbers
    const Vector4D ndc(2*(position.x - rect().x)/rect().width - 1, 1 - 2*(position.y - rect().y)/rect().height, 0, 1);
    const Vector4D point = m_integerScaleProjectionMatrixInverted*ndc;
    m_integerScale = true;
    const int option = m_menu->optionAt(Point(int(std::floor(point.x)), int(std::floor(point.y))));
    m_integerScale = false;
    return option;
}


void HUD::drawTouchControls(double pixelRatio)
{
    if (!g_touchControls || !m_scenario)
        return;
    glViewport(m_rectGL.x*pixelRatio, m_rectGL.y*pixelRatio, m_rectGL.width*pixelRatio, m_rectGL.height*pixelRatio);
    setupGL(false);
    gfx::Pipeline::setProjection(glm::ortho(0.0f, m_touch->width(), m_touch->height(), 0.0f));
    m_touch->draw();
}


void HUD::updateLook(float elapsedTime)
{
    if (g_lookFixed)
        m_look = g_look;
    else if (m_lookStick != Vector2D(0) && showsModel())
    {
        // the right stick turns the head as far as it is pushed, and the head follows it smoothly
        const Vector2D to(m_lookStick.x > 0 ? m_lookStick.x*LookMin.x : -m_lookStick.x*LookMax.x,
                          m_lookStick.y > 0 ? m_lookStick.y*LookMin.y : -m_lookStick.y*LookMax.y);
        m_look += (to - m_look)*(1 - std::exp(-std::max(0.0f, elapsedTime)/LookFollow));
    }
    else if (!m_looking || !showsModel())
        m_look *= std::exp(-std::max(0.0f, elapsedTime)/LookReturn);
}


Matrix HUD::lookTransform() const
{
    return glm::translate(Neck)*glm::rotate(m_look.x, Vector3D(0, 1, 0))*glm::rotate(m_look.y, Vector3D(1, 0, 0))*glm::translate(-Neck);
}


// (fingers come as touch events, see touchPressEvent())
void HUD::mousePressEvent(input::MouseEvent *e)
{
    if (!e->isFromTouch())
        startLook(e->position());
}


void HUD::mouseMoveEvent(input::MouseEvent *e)
{
    if (!e->isFromTouch())
        moveLook(e->position());
}


void HUD::mouseReleaseEvent(input::MouseEvent *e)
{
    if (!e->isFromTouch() && e->buttons() == input::NoButton)
        m_looking = false;
}


void HUD::startLook(const PointD &position)
{
    if (!m_scenario || !showsModel() || g_lookFixed || m_looking)
        return;
    m_looking = true;
    m_lookStart = position;
    m_lookFrom = m_look;
}


void HUD::moveLook(const PointD &position)
{
    if (!m_looking)
        return;
    // drag right to look right, up to look up
    const PointD drag = position - m_lookStart;
    m_look = glm::clamp(m_lookFrom - LookSpeed*Vector2D(drag), LookMin, LookMax);
}


void HUD::keyReleaseEvent(input::KeyEvent *e)
{
    release(e);
}


void HUD::release(input::KeyEvent *e)
{
    if (!m_scenario)
        return;

    m_scenario->keyReleaseEvent(e);
}


} // namespace hud
