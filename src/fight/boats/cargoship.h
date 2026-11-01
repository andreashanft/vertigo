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

#ifndef FIGHT_CARGOSHIP_H
#define FIGHT_CARGOSHIP_H


#include "simpleboat.h"
#include <memory>
#include <vector>
#include "util/string.hpp"


namespace fight {


class CargoShip;
class MountedTurret;
class Module;
class Turret;
class UnitGun;


// One block of a cargo ship as a sonar contact and target of its own, as in the original: the
// ship moves it along and keeps it enabled while the block is there. It shows the ship's shield
// and picture; hits land on the ship's hull.
class CargoSegment : public ActiveObject
{
public:
    CargoSegment(Scenario *scenario, CargoShip *ship, int block, const ObjectInfo &info);

public:
    void place(const BoundingBox &box, const Vector3D &velocity, int shield, int shieldMax);
    float noise() const;
    bool update(float elapsedTime) { return false; }
    void draw() {}
    void drawPreview();
    bool intersect(const Vector3D &start, const Vector3D &dir, float radius, float &distance, Vector3D &normal) { return false; }
    void damage(int kinetic, int shock, const Vector3D &position, ActiveObject *attacker = nullptr) {}
    void destroy() {}
    // the block is gone: a lock on it ends as on any destroyed unit
    void remove() { vanish(); }
    void identify();
    String observationImage() const;

private:
    CargoShip *m_ship;
    int m_block;
};


// A big ship assembled from blocks (cship*/bship* files, [CargoBlockN]): each block is placed
// against a side of an earlier one (ConnectSide: left 1, right 2, front 4, back 8, top 16,
// bottom 32). Blocks of the big ships carry gun turrets ([TurretN]), which fire at hostile units
// on their side of the hull. The ship moves like a boat, with its orders. As in the original it
// takes hits as a whole, on its own shield; a block's own shield only sets off a small explosion
// now and then. When the ship's shield is used up it brakes to a stop and its blocks blow up one
// after another, each shaking and sinking, and stay targets until they are gone; the ship counts
// as destroyed when the first block is gone (docs/ROADMAP.md, appendix A). The master monitor
// shows a picture of it (SHP*.IMG) rather than its model.
class CargoShip : public SimpleBoat
{
public:
    CargoShip(Scenario *scenario, txt::DesFile &file, const ObjectInfo &info, float yaw);
    ~CargoShip();

public:
    int blocks() const { return int(m_blocks.size()); }
    int turrets() const { return int(m_turrets.size()); }
    // the turrets as units of their own; the scenario owns them
    std::vector<MountedTurret *> turretUnits() const;
    // the height of the hull's bottom below the ship's position
    float draught() const { return -m_localBox.minPoint().z; }

public:
    bool update(float elapsedTime);
    void draw();
    void arm(int model, txt::DesFile &file) {}
    bool engagesHostiles() const { return false; }
    bool isBigShip() const { return true; }
    bool isContact() const { return false; }
    String observationImage() const { return m_picture; }
    // the master monitor's picture for a ship file (cship*, bship*)
    static String pictureFor(const String &desName);
    void setPicture(const String &picture) { m_picture = picture; }
    // the blocks as sonar contacts; the scenario owns them
    std::vector<CargoSegment *> createSegments();
    void drawBlock(int index);
    bool collidesWithObjects() const { return false; }
    bool intersect(const Vector3D &start, const Vector3D &dir, float radius, float &distance, Vector3D &normal);
    void damage(int kinetic, int shock, const Vector3D &position, ActiveObject *attacker = nullptr);
    void destroy();
    bool isDying() const override { return SimpleBoat::isDying() || m_exploding; }
    // an Explode order blows it up block by block, as when its armour is gone; paralysed, it
    // stops dead where it is (docs/ROADMAP.md, appendix A)
    bool startWreck() override { destroy(); return true; }
    bool startTumbling() override;
    // the ship hands its orders to its turrets, and lets them rest while it sleeps
    void orderStarted(const OrderSlot *slot);
    void sleep();

private:
    struct Block
    {
        Module *module;
        float scale;        // metres per model unit
        Vector3D position;  // ship coordinates (metres)
        BoundingBox box;    // ship coordinates
        bool destroyed = false;
        CargoSegment *segment = nullptr;
        bool gouraud = false;   // [Display] ShadingMode g (docs/ROADMAP.md, appendix M)
        float radius = 0;       // the model's farthest vertex from its origin (metres) ...
        float flatRadius = 0;   // ... from its vertical
        float top = 0;          // its highest point over its origin
        int shield = 0;     // its own kinetic shield, for the explosions under fire
        int shieldMax = 0;
        bool hitHard = false;
        // its part of the ship's destruction: the time it started, the next step, and how it
        // shakes, sinks and turns (ship coordinates)
        float start = -1;
        int step = 0;
        Vector3D shake;
        float sinkSpeed = 0;
        float sinkAcceleration = 0;
        float sink = 0;
        Vector2D spin;
        glm::mat3 tilt = glm::mat3(1);
    };

    // a block's gun turret ([TurretN] of its file), a unit of its own (MountedTurret)
    struct ShipTurret
    {
        MountedTurret *unit;
        int block;
    };

    Vector3D toWorld(const Vector3D &local) const { return m_position + m_orientationInverted*local; }
    Vector3D toLocal(const Vector3D &world) const { return m_orientation*(world - m_position); }

    // the block's part of the sequence that follows the ship's destruction
    void updateBlock(int index, float elapsedTime);
    Vector3D blockCenter(const Block &block) const; // world
    Vector3D blockOrigin(const Block &block) const; // world: the model's origin, as it has moved
    float blockRadius(const Block &block) const;
    Matrix blockTransform(const Block &block) const; // ship coordinates
    Matrix blockMotion(const Block &block) const;
    void updateLocalBox();
    void updateSegments();

private:
    std::vector<Block> m_blocks;
    std::vector<ShipTurret> m_turrets;
    String m_picture;
    bool m_exploding = false;
    bool m_paralysed = false;
    int m_nextBlock = 0;                // the next block to blow up, and when
    float m_nextExplosion = 0;
};


} // namespace fight


#endif // FIGHT_CARGOSHIP_H
