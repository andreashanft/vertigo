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


namespace fight {


class CargoShip;
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
    void identify();
    QString observationImage() const;

private:
    CargoShip *m_ship;
    int m_block;
};


// A big ship assembled from blocks (cship*/bship* files, [CargoBlockN]): each block is placed
// against a side of an earlier one (ConnectSide: left 1, right 2, front 4, back 8, top 16,
// bottom 32). Blocks of the big ships carry gun turrets ([TurretN]), which fire at hostile units
// on their side of the hull. The ship moves like a boat, with its orders. As in the original it
// takes hits as a whole, on its own shield; when that is used up it counts as destroyed, stops,
// and its blocks blow up one after another. The master monitor shows a picture of it (SHP*.IMG)
// rather than its model.
class CargoShip : public SimpleBoat
{
public:
    CargoShip(Scenario *scenario, txt::DesFile &file, const ObjectInfo &info, float yaw);
    ~CargoShip();

public:
    int blocks() const { return int(m_blocks.size()); }
    int turrets() const { return int(m_turrets.size()); }
    // the height of the hull's bottom below the ship's position
    float draught() const { return -m_localBox.minPoint().z; }

public:
    bool update(float elapsedTime);
    void draw();
    void arm(int model, txt::DesFile &file) {}
    bool engagesHostiles() const { return false; }
    bool isContact() const { return false; }
    QString observationImage() const { return m_picture; }
    // the master monitor's picture for a ship file (cship*, bship*)
    static QString pictureFor(const QString &desName);
    void setPicture(const QString &picture) { m_picture = picture; }
    // the blocks as sonar contacts; the scenario owns them
    std::vector<CargoSegment *> createSegments();
    void drawBlock(int index);
    bool collidesWithObjects() const { return false; }
    bool intersect(const Vector3D &start, const Vector3D &dir, float radius, float &distance, Vector3D &normal);
    void damage(int kinetic, int shock, const Vector3D &position, ActiveObject *attacker = nullptr);
    void destroy();

private:
    struct Block
    {
        Module *module;
        float scale;        // metres per model unit
        Vector3D position;  // ship coordinates (metres)
        BoundingBox box;    // ship coordinates
        bool destroyed = false;
        CargoSegment *segment = nullptr;
    };

    struct ShipTurret
    {
        std::unique_ptr<Turret> model;
        std::unique_ptr<UnitGun> gun;
        int block;
        float scale;
        Vector3D position;  // ship coordinates, on the hull
        bool below;         // hangs under the hull, upside down
        float yaw;          // in the mount's frame, positive to the right
        float pitch;
        float restYaw;
        float turnRate;
        float range;
        bool destroyed = false;
        ActiveObject *target = nullptr;
        float nextScan = 0;
    };

    // ship coordinates <-> the turret's mount frame (turned over for turrets under the hull)
    static Vector3D toMount(const ShipTurret &turret, const Vector3D &v) { return turret.below ? Vector3D(-v.x, v.y, -v.z) : v; }
    Vector3D toWorld(const Vector3D &local) const { return m_position + m_orientationInverted*local; }
    Vector3D toLocal(const Vector3D &world) const { return m_orientation*(world - m_position); }
    Vector3D turretAim(const ShipTurret &turret) const; // world direction
    void updateTurret(ShipTurret &turret, float elapsedTime);
    ActiveObject* findTarget(const ShipTurret &turret) const;
    bool isTargetValid(const ActiveObject *unit) const;
    // blows up the next block of the sequence that follows the ship's destruction
    void explodeNextBlock();
    void updateLocalBox();
    void updateSegments();

private:
    std::vector<Block> m_blocks;
    std::vector<ShipTurret> m_turrets;
    QString m_picture;
    bool m_exploding = false;
    std::vector<int> m_explosionOrder;  // the blocks still to blow up, the next one last
    float m_nextExplosion = 0;
};


} // namespace fight


#endif // FIGHT_CARGOSHIP_H
