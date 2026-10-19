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

#ifndef FIGHT_SURFACE_H
#define FIGHT_SURFACE_H

#include "element.h"
#include "tesselator.h"
#include "gfx/image.h"
#include "gfx/pipeline.h"

#include <map>
#include <vector>
#include "util/string.hpp"

namespace gfx { class ColorTable; class Texture; }

namespace fight {

class Surface
{
public:
    Surface();
    void load(const String &name, int maxheightscale, int mapping);

public:
    // the terrain with the shading tables' own rule, at this share of its brightness (the
    // mission's fade); the pipeline's lighting is left as it was
    void draw(const Vector3D &position, const Vector3D &direction, float brightness = 1);
    float heightAt(float x, float y);
    float heightAt(float x, float y, Vector3D &normal);
    const Vector3D& scale() const { return m_scale; }
    // the water's fog as the map's shading tables have it: a blend by distance from the colours
    // themselves into the fog's colour, a blue that takes the red away, for the sprites. The
    // terrain and the models pick the tables' rows by the game's rule instead (shading)
    const gfx::Pipeline::Fog& fog() const { return m_fog; }
    // the game's rule for the row of the shading table per vertex (loadShading), at this share
    // of its brightness (the mission's fade). The brightness is in multiples of the palette:
    // the models' tables hold the palette in the last row, the terrain's twice its textures
    gfx::Pipeline::TableShading shading(float brightness = 1) const;
    float height(int x, int y) const;
    // false if the texture couldn't be loaded
    bool bindTexture(int textureId);
    // a building's cluster flattens the ground: the cell and its neighbours to the east, south and
    // south-east take the reference cell's height (docs/ROADMAP.md, appendix M)
    void setHeight(int x, int y, int refx, int refy);
    // a building or tower darkens the ground it stands on: the light part of the shading at the
    // cell's four corners is this share of its own (docs/ROADMAP.md, appendix N)
    void darken(int x, int y, float factor);
    // the terrain around the cell is built anew on its next draw
    void invalidate(int x, int y);
    // that share at a point of the map (in cells), between the corners
    float lightFactor(float x, float y) const;
    bool testCollision(const Vector3D &start, const Vector3D &end, float radius, Vector3D &position, Vector3D &normal);

private:
    Element& getElement(int x, int y);
    void loadShading();
    static Vector3D fogColor(const gfx::ColorTable &palette, const std::vector<gfx::Rgb> &table);

private:
    std::vector<gfx::Texture> m_textures;
    std::map<int, Element> m_elements;
    gfx::Image m_heightMap;
    std::vector<float> m_lightFactors; // per corner of the map's cells
    std::vector<char> m_textureMap;
    std::vector<char> m_textureDir;
    Vector3D m_scale;
    int m_mapping;
    gfx::Pipeline::Fog m_fog;
    gfx::Pipeline::TableShading m_shading;

private:
    static const int Level = 3;
    Tesselator m_tesselator;
};

} // namespace fight

#endif // FIGHT_SURFACE_H
