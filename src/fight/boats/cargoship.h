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


class Module;
class Turret;
class UnitGun;


// A big ship assembled from blocks (cship*/bship* files, [CargoBlockN]): each block is placed
// against a side of an earlier one (ConnectSide: left 1, right 2, front 4, back 8, top 16,
// bottom 32) and has its own shield. Blocks of the big ships carry gun turrets ([TurretN]),
// which fire at hostile units on their side of the hull. The ship moves like a boat, with its
// orders; it is destroyed when its own shield is used up or all of its blocks are.
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
        int shield;
        bool destroyed = false;
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
        int shield;
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
    void destroyBlock(Block &block);
    void destroyTurret(ShipTurret &turret);
    void updateLocalBox();

private:
    std::vector<Block> m_blocks;
    std::vector<ShipTurret> m_turrets;
};


} // namespace fight


#endif // FIGHT_CARGOSHIP_H
