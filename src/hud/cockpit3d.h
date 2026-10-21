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

#ifndef HUD_COCKPIT3D_H
#define HUD_COCKPIT3D_H


#include "gfx/image.h"
#include "gfx/mesh.h"
#include "util/geometry3d.h"

#include "util/string.hpp"
#include <atomic>
#include <chrono>
#include <future>
#include <string>
#include <memory>


namespace gfx { class GlbModel; }


namespace hud {


// The 3D cockpit, a model built by data/3D-cockpits, drawn around the pilot's eye in place of the
// 2D art. The model's origin is the eye at rest, in the boat's camera frame (x right, y up, -z
// forward, metres). The HUD draws its instruments into a texture, which drawDecal() projects onto
// the model from the eye, so they stay on their panels when the pilot's head moves.
//
// Vertigo comes with a kit per cockpit (data/3D-cockpits/kits): the model without its texture,
// which it paints from the cockpit art of the game data (CockpitPaint) on a worker thread the first
// time, and keeps in the cache (util::cacheLocation()). A finished model with its texture, e.g. one
// with upscaled art, can take its place in directory().
class Cockpit3D
{
public:
    // where finished models are: setDirectory()'s, else the setting "cockpit/directory", else
    // 3d-cockpits in the data directory
    static std::string directory();
    // where the kits are: cockpits next to the program (in a macOS or iOS app: its resources), or
    // share/vertigo/cockpits in a Linux installation; empty if there are none
    static std::string kitDirectory();
    // the models in this directory for this run (a relative one in the data directory), e.g. a
    // variant of the textures
    static void setDirectory(const std::string &directory);
    // cockpit_NNN.glb of the cockpit NNN in directory(), else its kit, or an empty string if there
    // is neither
    static std::string find(const String &cockpit);
    // the model in this file, shared while it is in use; prepare() loads it
    static std::shared_ptr<Cockpit3D> get(const std::string &fileName);

public:
    explicit Cockpit3D(const std::string &fileName);
    ~Cockpit3D();

public:
    const std::string& fileName() const { return m_fileName; }
    // false once loading failed
    bool isValid() const { return m_state != State::Failed; }
    // loaded and painted, ready to draw
    bool isReady() const { return m_state == State::Ready; }
    // Loads the model (with a GL context), a kit's texture on a worker thread: call it every
    // frame until it is ready (with wait, it waits for the texture). The cockpit art is that of
    // the cockpit in the game data.
    void prepare(const String &cockpit, bool wait = false);
    // the camera of the cockpit art: focal length and optical centre in pixels of the 640x480 view
    void setCamera(float focal, const Vector2D &center);

    // the model with its texture (depth test and writes on); false if it isn't ready
    bool draw(const Matrix &projection, const Matrix &view);
    // a texture covering the 640x480 view (premultiplied alpha) projected onto the model from
    // the eye at rest, over what draw() left in the depth buffer
    void drawDecal(const Matrix &projection, const Matrix &view, unsigned int texture);

private:
    void startPainting(const String &cockpit);
    // the GL mesh and texture: the painted one, or with a null image the model's own
    bool create(const gfx::Image &painted);

private:
    enum class State { Unloaded, Painting, Ready, Failed };

    std::string m_fileName;
    State m_state = State::Unloaded;
    float m_focal = 245.5f;
    Vector2D m_center = Vector2D(320, 240);
    bool m_cameraChanged = false;

    // the textured model, then its triangles in view and the window glass for the decal
    gfx::Mesh m_mesh;
    int m_baseCount = 0;
    int m_decalCount = 0;
    unsigned int m_texture = 0;
    // the model until it is ready, and the texture painted for a kit
    struct Painted
    {
        gfx::Image texture;
        bool cached = false;
        std::string file;
    };
    std::unique_ptr<gfx::GlbModel> m_model;
    std::future<Painted> m_painted;
    std::atomic<bool> m_cancel { false };
    std::chrono::steady_clock::time_point m_started;
    // after the first frame, which reports OpenGL errors (on an iPad the cockpit froze the screen
    // once without any)
    bool m_drawn = false;
};


} // namespace hud


#endif // HUD_COCKPIT3D_H
