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

#ifndef SURFACE_TESSELATOR_H
#define SURFACE_TESSELATOR_H

#include "betaspline.h"

#include <vector>
#include "util/geometry3d.h"

namespace fight {

class Element;
class Surface;

class Tesselator
{
public:
    Tesselator(int MaxLevel, Surface *surface);

public:
	float heightAt(const Vector2D &pos);
    float heightAt(const Vector2D &pos, Vector3D &normal);
    void tesselate(Element &element, int level, const Vector3D &scale, const std::vector<char> &textureMap, const std::vector<char> &textureDir, int mapping);
    bool intersect(const Vector3D &start, const Vector3D &end, float radius, Vector3D &position, Vector3D &normal);

private:
    void InitIndices(int MaxLevel);
    Vector3D shadingNormal(const Vector3D &normal, const Vector3D &position, const Vector3D &scale) const;
    // the spline's height and normal (in the map's units) at u, v of cell x, y, where the ground
    // runs straight between the corners turned into that (Surface::settle)
    void straighten(int x, int y, float u, float v, float &z, Vector3D &normal) const;
    float groundZ(BetaSpline &spline, int x, int y, float u, float v) const;
    Vector3D groundNormal(BetaSpline &spline, int x, int y, float u, float v) const;
    void PrepareElementSubset(Element &element, int level, const Vector3D &scale, int x, int y, int textureId, const Vector2D &t0, const Vector2D &tu, const Vector2D &tv);

private:
	std::vector<BetaSpline> m_splines;
    std::vector<unsigned short> indices;

    Surface *m_surface;
};

} // namespace fight

#endif // SURFACE_TESSELATOR_H
