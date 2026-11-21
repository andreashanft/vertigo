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

#ifndef FIGHT_COLLISIONMANAGER_H
#define FIGHT_COLLISIONMANAGER_H


#include <functional>
#include <vector>
#include "util/geometry3d.h"


namespace fight {


class Object;


class CollisionCache
{
public:
    CollisionCache();

public:
    void addObject(Object *object, bool collision, const Vector3D &position, const Vector3D &normal);
    bool testObject(Object *object, bool &collision, Vector3D &position, Vector3D &normal);

private:
    struct CacheEntry
    {
        Object *object;
        bool collision;
        Vector3D position;
        Vector3D normal;
    };

private:
    std::vector<CacheEntry> m_entries;
};


class CollisionManager
{
public:
    CollisionManager();

public:
    void addObject(Object *object);
    void removeObject(Object *object);
    // the nearest object a sphere moving from start to end hits; accept() can leave objects out
    Object* testCollision(const Vector3D &start, const Vector3D &end, float radius, Vector3D &position, Vector3D &normal, CollisionCache *collisionCache = nullptr, const Object *ignore = nullptr,
                          const std::function<bool(const Object *)> &accept = nullptr);
    // The nearest unit that a unit meets on its way from start to end, the unit being a sphere
    // of this radius. A unit with a contact sphere (Object::contactRadius()) is met as that
    // sphere about its position, any other object by its shape as in testCollision(). position
    // takes the moving sphere's centre at the contact, normal the direction away from the other.
    Object* testContact(const Vector3D &start, const Vector3D &end, float radius, Vector3D &position, Vector3D &normal, const Object *ignore = nullptr,
                        const std::function<bool(const Object *)> &accept = nullptr);

private:

private:
    std::vector<Object*> m_objects;
};


} // namespace fight


#endif // FIGHT_COLLISIONMANAGER_H
