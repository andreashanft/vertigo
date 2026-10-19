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

#ifndef FIGHT_SCENARIO_H
#define FIGHT_SCENARIO_H

#include <functional>
#include <map>
#include <list>
#include <vector>
#include "util/geometry2d.h"
#include "util/geometry3d.h"
#include "util/deferreddeletable.h"
#include "gfx/texturemanager.h"
#include "game/input.hpp"
#include "txt/desfile.h"
#include "txt/stringtable.h"
#include "boats/player.h"
#include "surface/surface.h"
#include "effects/effectmanager.h"
#include "conditionmanager.h"
#include "collisionmanager.h"
#include "modulemanager.h"
#include "sonar.h"
#include "target.h"
#include "headmotion.h"
#include "util/string.hpp"

namespace game { class Boat; }

namespace fight {


class EngineSounds;
class Torpedo;

class Object;
class NavPoint;

class Scenario : public util::DeferredDeletable
{
public:
    Scenario(const String &name);
    ~Scenario();

    // the game's difficulty: 0 easy, 1 normal, 2 hard. Units whose "diff" key is higher stay out of the mission.
    static int difficulty();
    static void setDifficulty(int difficulty);
    // test mode: use this difficulty without storing it
    static void overrideDifficulty(int difficulty);
    // test mode: the same random numbers in every run (-1: seeded from the clock)
    static void setRandomSeed(int seed);

public:
    struct ObjectRecord
    {
        int id;
        int type;
        String des;
        Object *object;
    };

public:
    const String& name() const { return m_name; }
    const std::vector<ObjectRecord>& objectRecords() const { return m_records; }

    // everything the loader skipped or ignored, one line per occurrence
    StringList loadReport() const;

    const Matrix& projectionMatrix() const { return m_projectionMatrix; }
    const Matrix& projectionMatrixInverted() const { return m_projectionMatrixInverted; }
    const Matrix& cameraMatrix() const { return m_cameraMatrix; }
    const Matrix& cameraMatrixInverted() const { return m_cameraMatrixInverted; }
    const Vector3D& position() const { return m_position; }
    float yaw() const { return m_yaw; }
    float pitch() const { return m_pitch; }
    float height() const { return m_height; }
    float depth() const { return m_depth - m_position.z; }
    // the whole picture's brightness, 0 to 1: fading in at the start and out once the mission
    // has failed, as the original does, times the pause's dimming
    float brightness() const;
    // 1 normally, 0 when paused: the world goes into the water's dark blue
    void setDimming(float dimming) { m_dimming = dimming; }
    // less than this far below the surface the radiation symbol blinks and the monitors flicker,
    // less than RadiationAlertDepth the alert comes too (docs/ROADMAP.md, appendix D)
    static constexpr float RadiationDepth = 99;
    static constexpr float RadiationAlertDepth = 60;
    // the radiation's strength: 0 a hundred metres below the surface and deeper, 1 at the surface
    float radiation() const;
    // In the radiation a monitor shows nothing until its next refresh, at random, the more often
    // the closer the boat is to the surface; each monitor refreshes at its own rate
    enum class Monitor { Master, Slave0, Slave1, Round0, Round1 };
    bool monitorBlanked(Monitor monitor) const;
    // the height of the water's surface
    float surfaceLevel() const { return m_depth; }
    // the throttle set to that speed (m/s), as near the surface
    void holdThrottle(float velocity);
    // the CD track of the mission's music, as numbered on the CD (0: none)
    int cdTrack() const { return m_cdTrack > 0 ? m_cdTrack + 1 : 0; }
    float time() const { return m_time; }
    int buzzers() const { return m_buzzers; }
    bool useBuzzer() { if (m_buzzers <= 0) return false; m_buzzers--; return true; }
    bool blink() const { return static_cast<int>(m_time/0.5f)%2 == 0; }
    int endType() const { return m_endType; }

    int playerId() const { return m_playerId; }
    // the player hit a unit; warns and fails the mission when it is a friend (see friendlyFire in scenario.cpp)
    void playerHit(ActiveObject *unit, bool destroyed);
    // the player's boat is destroyed: the death view watches its destruction sequence from
    // outside (docs/ROADMAP.md, appendix R)
    void destroyPlayer();
    bool playerDestroyed() const { return m_eventPlayerDestroyed.isCompleted(); }

    Player* player() const { return m_player.get(); }
    // The difficulty caps how many units attack the player at once: 4 on easy, 6 on normal, 8 on
    // hard (docs/ROADMAP.md, appendix J). A unit asks before it takes the player as its target and
    // gets no target while the list is full; it lets go when it turns to something else.
    bool engagePlayer(ActiveObject *unit);
    void disengagePlayer(ActiveObject *unit);
    int playerAttackers() const { return int(m_playerAttackers.size()); }
    const std::vector<ActiveObject*>& playerAttackerList() const { return m_playerAttackers; }

    Surface& surface() { return m_surface; }

    // torpedoes in the water; homing ones turn to decoys (buzzers, the Torch) that are nearer than their target
    Torpedo* launchTorpedo(int type, const Vector3D &position, const Vector3D &direction, float speed, ActiveObject *owner, ActiveObject *target);
    const std::list<std::unique_ptr<Torpedo>>& torpedoes() const { return m_torpedoes; }
    int torpedoesLaunched() const { return m_torpedoesLaunched; }
    // homing torpedoes on their way to the unit
    int threatsTo(const ActiveObject *unit) const;
    // the nearest decoy within range: a burning torch (the buzzers' and the Torch's) or a decoy torpedo
    bool decoyNear(const Vector3D &position, float range, const Torpedo *self, Vector3D &decoy) const;

    // the water current's push at a position (m/s²), that of the cell the position is in
    Vector3D current(const Vector3D &position) const;
    struct CurrentCell { int x; int y; Vector3D push; };     // x, y: first map cell of the 2x2 block; the push in m/s²
    const std::vector<CurrentCell>& currentCells() const { return m_currentCells; }
    gfx::TextureManager& textureManager() { return m_textureManager; }
    ModuleManager& moduleManager() { return m_moduleManager; }
    EffectManager& effectManager() { return m_effectManager; }
    CollisionManager& collisionManager() { return m_collisionManager; }
    ConditionManager& conditionManager() { return m_conditionManager; }
    std::vector<ConditionRadio*>& radio() { return m_radio; }
    // the radio messages and the units' events as the log (the M key) lists them, oldest first
    enum class LogColor { Green, Red, Amber };
    struct LogEntry
    {
        float time;     // mission seconds
        String text;
        LogColor color;
    };
    const std::vector<LogEntry>& log() const { return m_log; }
    void log(const String &text, LogColor color);
    // "<name> wurde angegriffen" and the like (FIGHTEXT 5-10), in amber: the original logs a
    // unit's event only when the unit has a name and the mission uses that event
    void logEvent(const String &name, const ConditionEvent *event, txt::StringFight what);
    Sonar& sonar() { return m_sonar; }
    Target& target() { return m_target; }
    // the pilot's head in the cockpit (shown by the 3D cockpit only)
    HeadMotion& headMotion() { return m_headMotion; }
    // an explosion near the player shakes the cockpit; strength 1 for a torpedo
    void explosionAt(const Vector3D &position, float strength);

    std::vector<std::unique_ptr<Object>>::const_iterator begin() const { return m_objects.cbegin(); }
    std::vector<std::unique_ptr<Object>>::const_iterator end() const { return m_objects.cend(); }
    std::vector<NavPoint*>& navPoints() { return m_navPoints; }
    // the player is faction 8: every mission's attitude matrix makes the enemies hostile to it
    static const int PlayerFaction = 8;
    bool isFriend(int iffX, int iffY) const { return m_attitudeMatrix[iffY][iffX] == 0; }
    bool isHostile(int iffA, int iffB) const { return iffA >= 0 && iffA < 9 && iffB >= 0 && iffB < 9 && m_attitudeMatrix[iffB][iffA] == 2; }

public:
    void setBoat(game::Boat *boat);
    // fovY: vertical view angle of the 640x480 view in degrees
    void setRect(const RectF &rect, const Vector2D &center, float fovY = 60.0f);
    void update(float elapsedTime);
    void draw();
    // with another projection and camera, e.g. a wider view or the pilot's head moved
    void draw(const Matrix &projection, const Matrix &camera);
    void keyPressEvent(input::KeyEvent *);
    void keyReleaseEvent(input::KeyEvent *);
    // a game controller's stick steers as far as it is pushed (x right, y back; each from -1 to 1),
    // together with the keys
    void setStick(const Vector2D &stick) { m_stick = stick; }

private:
    // the cells of the towers and active buildings that have appeared, flattened to the height of
    // their corner, with the light part of the ground's shading at the unit's share
    void markGround();
    Vector3D getPosition();
    void loadIssue(const String &text);

private:
    enum Type
    {
        TypeBoat           = 2049,
        TypeBomber         = 2050,
        TypeTank           = 2051,
        TypeTower          = 2052,
        TypeCrawler        = 2053,
        TypeTrash          = 2054,
        TypeBuilding       = 2056,
        TypePlayer         = 2057,
        TypeMine           = 2058,
        TypeTorpedoTower   = 2059,
        TypeSpace          = 2060,
        TypeNavPoint       = 2061,
        TypeActiveBuilding = 2062,
        TypeCargoShip      = 2063,
    };

    Vector3D m_position;
    float m_yaw;
    float m_pitch;
    float m_height;
    int m_buzzers;
    std::unique_ptr<Player> m_player;
    float m_velocityTarget;

    txt::DesFile m_file;
    int m_depth;
    int m_cdTrack;

    Surface m_surface;
    gfx::TextureManager m_textureManager;
    ModuleManager m_moduleManager;
    EffectManager m_effectManager;
    CollisionManager m_collisionManager;
    ConditionManager m_conditionManager;
    std::vector<ConditionRadio*> m_radio;
    std::vector<LogEntry> m_log;
    Sonar m_sonar;
    Target m_target;
    HeadMotion m_headMotion;

    String m_name;
    StringList m_loadIssues;
    std::vector<std::unique_ptr<Object>> m_objects;
    std::vector<ObjectRecord> m_records;
    std::vector<CurrentCell> m_currentCells;
    std::unique_ptr<EngineSounds> m_engineSounds;
    std::list<std::unique_ptr<Torpedo>> m_torpedoes;
    int m_torpedoesLaunched = 0;
    std::map<std::pair<int, int>, Vector3D> m_currentGrid; // by 2x2 block
    struct GroundMark { Object *object; int x, y; float shade; };
    std::vector<GroundMark> m_groundMarks; // the units that haven't appeared yet
    std::vector<NavPoint*> m_navPoints;
    std::vector<ActiveObject*> m_playerAttackers;

    char m_attitudeMatrix[9][9];
    int m_endType;
    int m_playerId;
    int m_friendlyWarnings = 0;
    int m_friendlyKills = 0;
    float m_lastFriendlyWarning = -100;
    ConditionEvent m_eventPlayerDestroyed;

    float m_time;
    float m_failTime = -1;      // when the mission failed, -1 while it hasn't
    // the death view: the camera's place, its heading and elevation round the hull and its
    // distance in hull radii
    void updateDeathView(float elapsedTime);
    Vector3D m_deathCamera;
    float m_deathYaw = 0;
    float m_deathPitch = 0;
    float m_deathDistance = 0;
    float m_dimming = 1;

    float m_left;
    float m_right;
    float m_up;
    float m_down;
    Vector2D m_stick = Vector2D(0);
    int m_forwards;
    int m_backwards;

    Matrix m_projectionMatrix;
    Matrix m_projectionMatrixInverted;
    Matrix m_cameraMatrix;
    Matrix m_cameraMatrixInverted;
};

} // namespace fight

#endif // FIGHT_SCENARIO_H
