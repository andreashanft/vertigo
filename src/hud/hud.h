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

#include <memory>
#include "util/string.hpp"


namespace gfx { class Framebuffer; }


namespace fight { class Scenario; class ActiveObject; }
namespace game { class Boat; }
namespace txt { class DesFile; }


namespace hud {


class SonarMonitor;
class MissionMenu;
class SlaveMonitor;
class Cockpit3D;


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

public:
    // the player leaves the mission: accomplished (autopilot), abort the game, restart the mission, load the last save
    util::event<>& eventSuccess() { return m_eventSuccess; }
    util::event<>& eventAbort() { return m_eventAbort; }
    util::event<>& eventRestart() { return m_eventRestart; }
    util::event<>& eventLoad() { return m_eventLoad; }
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
    // advances the mission without drawing (draw() calls it with the time since the last frame)
    void step(float elapsedTime);

protected:
    void keyPressEvent(input::KeyEvent *);
    void keyReleaseEvent(input::KeyEvent *);
    // mouse look in the 3D cockpit: dragging with a mouse button turns the pilot's head
    void mousePressEvent(input::MouseEvent *);
    void mouseReleaseEvent(input::MouseEvent *);
    void mouseMoveEvent(input::MouseEvent *);

private:
    Rect readRect(const txt::DesFile &file);
    void setFrozen(bool frozen);
    void updateMissionState();
    void leave(util::event<> &event);
    bool drawModel(double pixelRatio);
    void updateLook(float elapsedTime);
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
    std::vector<SlaveMonitor*> m_slaveMonitors;

    bool m_paused;
    bool m_frozen;          // mission time and sounds stopped (pause or menu)
    bool m_successShown;
    bool m_failureShown;
    float m_deathTime;      // mission time of the player's death, -1 while alive
};


} // namespace hud


#endif // HUD_HUD_H
