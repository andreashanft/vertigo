/***************************************************************************
 *  Copyright (C) 2026  Andreas Hanft                                      *
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


// hud::CockpitPaint: a kit of data/3D-cockpits/kits painted from a made-up cockpit art (the game's
// art isn't in the repository), and the art's see-through area

#include "check.hpp"

#include "gfx/glbmodel.h"
#include "hud/cockpitpaint.h"

#include <glm/geometric.hpp>
#include <algorithm>
#include <cmath>
#include <vector>


namespace {

// a smooth made-up art: red across, green down
void artColour(float x, float y, float *rgb)
{
    rgb[0] = 0.2f + 0.6f*x/640;
    rgb[1] = 0.15f + 0.7f*y/480;
    rgb[2] = 0.35f;
}

}


int main()
{
    // the see-through area: the colour key where it fills at least 40 pixels
    {
        gfx::Image art(640, 480, gfx::Image::ARGB32);
        art.fill(gfx::rgb(80, 80, 80));
        for (int y = 100; y < 110; y++)
            for (int x = 100; x < 110; x++)
                art.setPixel(x, y, gfx::rgba(0, 0, 0, 0));     // 100 pixels: window
        for (int y = 200; y < 205; y++)
            for (int x = 200; x < 205; x++)
                art.setPixel(x, y, gfx::rgba(0, 0, 0, 0));     // 25 pixels: black paint
        const std::vector<std::uint8_t> see = hud::CockpitPaint::seeThrough(art);
        CHECK_EQUAL(int(see.size()), 640*480);
        CHECK_EQUAL(int(see[105*640 + 105]), 1);
        CHECK_EQUAL(int(see[202*640 + 202]), 0);
        CHECK_EQUAL(int(see[50*640 + 50]), 0);
    }

    // the Gator's kit painted at 1024 x 1024
    gfx::GlbModel kit;
    CHECK(kit.load(VERTIGO_KITS_DIR "/cockpit_001.glb"));
    const util::Json &info = kit.extras()["vertigo_kit"];
    CHECK_EQUAL(info["cockpit"].toString(), std::string("001"));
    hud::CockpitPaint::Input input;
    input.kit = &kit;
    input.art = gfx::Image(640, 480, gfx::Image::RGB888);
    for (int y = 0; y < 480; y++)
        for (int x = 0; x < 640; x++) {
            float rgb[3];
            artColour(x + 0.5f, y + 0.5f, rgb);
            for (int k = 0; k < 3; k++)
                input.art.scanLine(y)[3*x + k] = std::uint8_t(std::lround(rgb[k]*255));
        }
    input.see.assign(640*480, 0);
    input.focal = float(info["camera"][0].toDouble());
    input.center = Vector2D(info["camera"][1].toDouble(), info["camera"][2].toDouble());
    input.size = 1024;
    input.eyeScale = 2;
    const gfx::Image texture = hud::CockpitPaint::paint(input);
    CHECK_EQUAL(texture.width(), 1024);
    CHECK_EQUAL(texture.height(), 1024);
    CHECK(texture.format() == gfx::Image::RGB888);

    // where the eye sees the master monitor's bezel, the texture has the art's colour there
    std::vector<float> errors;
    for (const gfx::GlbModel::Mesh &mesh : kit.meshes()) {
        if (mesh.name != "MasterMonitor_Bezel")
            continue;
        CHECK_EQUAL(mesh.normals.size(), mesh.positions.size());
        for (std::size_t t = 0; t + 2 < mesh.indices.size(); t += 3) {
            const std::uint32_t a = mesh.indices[t], b = mesh.indices[t + 1], c = mesh.indices[t + 2];
            const Vector3D p = (mesh.positions[a] + mesh.positions[b] + mesh.positions[c])/3.0f;
            const Vector3D n = glm::normalize(mesh.normals[a] + mesh.normals[b] + mesh.normals[c]);
            const Vector2D uv = (mesh.texCoords[a] + mesh.texCoords[b] + mesh.texCoords[c])/3.0f;
            const float x = input.center.x + input.focal*p.x/-p.z, y = input.center.y - input.focal*p.y/-p.z;
            if (glm::dot(n, -glm::normalize(p)) < 0.5f || x < 20 || x > 620 || y < 20 || y > 460)
                continue;
            float expected[3];
            artColour(x, y, expected);
            const std::uint8_t *got = texture.constScanLine(std::min(int(uv.y*1024), 1023)) + 3*std::min(int(uv.x*1024), 1023);
            float error = 0;
            for (int k = 0; k < 3; k++)
                error = std::max(error, std::abs(got[k]/255.0f - expected[k]));
            errors.push_back(error);
        }
    }
    CHECK(errors.size() > 50);
    if (!errors.empty()) {
        std::nth_element(errors.begin(), errors.begin() + errors.size()/2, errors.end());
        const float median = errors[errors.size()/2];
        std::cout << errors.size() << " bezel triangles, median error " << median << std::endl;
        CHECK(median < 0.04f);
    }
    return test::result();
}
