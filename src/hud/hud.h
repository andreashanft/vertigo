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

#ifndef HUD_HUD_H
#define HUD_HUD_H


#include "game/renderer.h"
#include "util/geometry2d.h"
#include "util/event.hpp"
#include "ui/label.h"
#include "gfx/colortable.h"
#include "gfx/image.h"
#include "sfx/music.hpp"

#include <chrono>
#include <map>
#include <memory>
#include "util/string.hpp"


namespace gfx { class Framebuffer; }


namespace fight { class Scenario; class ActiveObject; }
namespace game { class Boat; }
namespace txt { class DesFile; }


namespace hud {


class SonarMonitor;
class MissionMenu;
class Pages;
class SlaveMonitor;
class Cockpit3D;
class TouchControls;


class HUD : public game::Renderer
{
public:
    // the cockpit: the original 2D art, or the 3D model built from it by data/3D-cockpits
    enum class CockpitMode { Art, Model };

public:
    HUD();
    ~HUD();

public:
    // saved in the setting "cockpit/mode" ("2d" or "3d"); save = false changes it for this run only
    static CockpitMode cockpitMode();
    static void setCockpitMode(CockpitMode mode, bool save);
    // how strongly the pilot's head moves in the 3D cockpit, 0 to turn it off (setting "cockpit/shake")
    static float headMotion();
    static void setHeadMotion(float strength, bool save);
    // turns the pilot's head in the 3D cockpit for good (degrees, left and up positive), e.g. for
    // screenshots; the mouse can't turn it then
    static void setLook(float yaw, float pitch);
    // advances the mission by this many seconds per frame instead of by the real time, so test
    // runs with a window repeat exactly (0: real time)
    static void setFixedStep(float seconds);
    // frames drawn of the current mission
    int frames() const { return m_frames; }
    // the last second's frames, for the debug overlay: frames a second, the average and the
    // longest time from one to the next and the time the game took for one (updating and drawing,
    // without waiting for the screen), in milliseconds; 0 until a second has passed, and with a
    // fixed step
    struct FrameRate { float fps = 0, frame = 0, longest = 0, work = 0; };
    const FrameRate& frameRate() const { return m_frameRate; }
    // the touch controls are shown
    static bool touchControls();

public:
    // the player leaves the mission: accomplished (autopilot), abort the game, restart the mission, load the last save
    util::event<>& eventSuccess() { return m_eventSuccess; }
    util::event<>& eventAbort() { return m_eventAbort; }
    util::event<>& eventRestart() { return m_eventRestart; }
    util::event<>& eventLoad() { return m_eventLoad; }
    // where the autopilot can take the boat once the mission is accomplished, chosen in its menu
    // (none: it leaves at once, as in test mode and training); after eventSuccess the one chosen
    void setDestinations(const StringList &names);
    // the mission's briefing texts for the briefing page (B): the text, the targets and the tips
    void setBriefing(const StringList &text, const StringList &targets, const StringList &tips);
    int destination() const;
    // the mission is over and the game over menu is shown
    util::event<>& eventFailure() { return m_eventFailure; }
    util::event<>& eventDied() { return m_eventDied; }

    void setDebugOverlay(bool visible);
    // the 3D cockpit is selected and there is a model for the boat's cockpit
    bool showsModel() const;

    game::Boat* boat() const { return m_boat; }
    fight::Scenario* scenario() const { return m_scenario; }
    int navPoint() const { return m_navPoint; }
    bool isPaused() const { return m_paused; }
    void pause();

    bool wide() const { return m_wide; }
    const RectF& rectHUD() const { return m_rectHUD; }
    const Point& center() const { return m_center; }
    ui::Widget* widget() { return m_integerScale ? &m_integerScaleWidget : &m_noScaleWidget; }
    gfx::Image getImage(const String &name);
    gfx::Font& fontGreen() { return m_fontGreen; }
    MissionMenu& menu() { return *m_menu; }
    gfx::Font& fontRed() { return m_fontRed; }
    gfx::Font& fontYellow() { return m_fontYellow; }

    const Matrix& hudProjectionMatrix() const { return m_integerScale ? m_integerScaleProjectionMatrix : m_noScaleProjectionMatrix; }
    const Matrix& hudProjectionMatrixInverted() const { return m_integerScale ? m_integerScaleProjectionMatrixInverted : m_noScaleProjectionMatrixInverted; }
    Point project(const Point &point);
    Rect projectCenter(const Rect &rect);

    void load(game::Boat *boat);
    void start(fight::Scenario *scenario);

    void setRect(const Rect &rect);
    void draw();
    void drawFrame();
    // advances the mission without drawing (draw() calls it with the time since the last frame)
    void step(float elapsedTime);

protected:
    void keyPressEvent(input::KeyEvent *);
    void keyReleaseEvent(input::KeyEvent *);
    // mouse look in the 3D cockpit: dragging with a mouse button turns the pilot's head
    void mousePressEvent(input::MouseEvent *);
    void mouseReleaseEvent(input::MouseEvent *);
    void mouseMoveEvent(input::MouseEvent *);
    // a game controller: the buttons stand for keys (a second set with LB held), the d-pad and A
    // and B work the menus; the left stick steers, the right one turns the head in the 3D cockpit
    void buttonPressEvent(input::Button button);
    void buttonReleaseEvent(input::Button button);
    void stickEvent(input::Stick stick, const Vector2D &position);
    // a touch screen: the touch controls, taps on the menus' options
    void touchPressEvent(input::TouchEvent *);
    void touchMoveEvent(input::TouchEvent *);
    void touchReleaseEvent(input::TouchEvent *);

private:
    Rect readRect(const txt::DesFile &file);
    void setFrozen(bool frozen);
    void updateMissionState();
    void leave(util::event<> &event);
    // leaves with success, through the destination menu if there are destinations
    void engageAutopilot();
    bool drawModel(double pixelRatio);
    void drawDeathView(double pixelRatio);
    void updateLook(float elapsedTime);
    void menuAction(int action);
    input::KeyEvent buttonKey(input::Button button);
    // keys from any source; the keyboard's also hide the touch controls
    void press(input::KeyEvent *);
    void release(input::KeyEvent *);
    // keys held by a controller's button or a touch button (source: the button)
    void pressKey(input::KeyEvent key, int source);
    void releaseKey(int source);
    void updateStick();
    void releaseTouch();
    void startLook(const PointD &position);
    void moveLook(const PointD &position);
    // a point of the window in the 640x480 view the menu and the instruments are drawn in
    Point viewPoint(const PointD &position);
    int menuOption(const PointD &position);
    void tapMonitor(const PointD &position);
    void drawTouchControls(double pixelRatio);
    void countFrame(std::chrono::steady_clock::time_point begin, std::chrono::steady_clock::time_point end);
    // the pilot's head turned by the mouse look, in the boat's camera frame
    Matrix lookTransform() const;

private:
    util::event<> m_eventSuccess;
    util::event<> m_eventAbort;
    util::event<> m_eventRestart;
    util::event<> m_eventLoad;
    util::event<> m_eventFailure;
    util::event<> m_eventDied;

    game::Boat *m_boat;
    fight::Scenario *m_scenario;
    int m_lastTicks;
    int m_frames = 0;
    FrameRate m_frameRate;
    std::chrono::steady_clock::time_point m_frameBegin;     // of the last frame
    std::chrono::steady_clock::time_point m_countBegin;     // of the second being counted
    int m_countFrames = 0;
    double m_countLongest = 0;
    double m_countWork = 0;
    int m_navPoint;

    bool m_wide;
    Rect m_rectGL;
    RectF m_rectHUD;
    Point m_center;
    Vector2D m_centerNDC;   // m_center in the 640x480 view's normalized device coordinates
    float m_focal;          // of the cockpit art's camera, in pixels of the 640x480 view

    std::shared_ptr<Cockpit3D> m_model;
    std::unique_ptr<gfx::Framebuffer> m_instruments;   // the instruments, projected onto the model
    Vector2D m_look;        // yaw (left) and pitch (up) of the pilot's head in radians
    bool m_looking;         // a mouse button is held
    PointD m_lookStart;     // where the drag started
    Vector2D m_lookFrom;    // and the look direction then
    Vector2D m_lookStick = Vector2D(0);    // the right stick
    Vector2D m_padStick = Vector2D(0);     // the left stick
    Vector2D m_touchStick = Vector2D(0);   // the touch screen's
    bool m_shoulder = false;               // LB held: the buttons' second set
    std::map<int, input::KeyEvent> m_heldKeys;  // the keys the buttons held stand for
    int m_repeat = -1;      // the button that changes the speed again and again while held
    float m_repeatWait = 0;
    std::unique_ptr<TouchControls> m_touch;
    int m_weaponPage = 0;   // the next of F1-F4 for LB+Up
    bool m_integerScale;
    Matrix m_integerScaleProjectionMatrix;
    Matrix m_integerScaleProjectionMatrixInverted;
    Matrix m_noScaleProjectionMatrix;
    Matrix m_noScaleProjectionMatrixInverted;

    const gfx::ColorTable m_colorTable;
    gfx::Font m_fontGreen;
    gfx::Font m_fontRed;
    gfx::Font m_fontYellow;

    ui::Label m_cockpit;
    ui::Label m_integerScaleWidget;
    ui::Label m_noScaleWidget;
    std::vector<std::unique_ptr<ui::Widget>> m_children;
    SonarMonitor* m_sonar1;
    SonarMonitor* m_sonar2;
    ui::Widget* m_debugOverlay;
    bool m_debugOverlayVisible;
    MissionMenu* m_menu;
    Pages* m_pages;
    std::vector<SlaveMonitor*> m_slaveMonitors;

    bool m_paused;
    bool m_frozen;          // mission time and sounds stopped (pause or menu)
    float m_dimming = 1;    // the pause's fade: 1 normally, 0 when paused
    sfx::Music m_music;     // the mission's CD track
    bool m_successShown;
    bool m_failureShown;
    float m_deathTime;      // mission time of the player's death, -1 while alive
};


} // namespace hud


#endif // HUD_HUD_H
