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


#include "gfx/mesh.h"
#include "util/geometry3d.h"

#include <QString>
#include <memory>


namespace hud {


// The 3D cockpit, a model built and textured from the cockpit art by data/3D-cockpits, drawn
// around the pilot's eye in place of the 2D art. The model's origin is the eye at rest, in the
// boat's camera frame (x right, y up, -z forward, metres). The HUD draws its instruments into a
// texture, which drawDecal() projects onto the model from the eye, so they stay on their panels
// when the pilot's head moves.
class Cockpit3D
{
public:
    // where the models are: the setting "cockpit/directory", else 3d-cockpits in the data directory
    static QString directory();
    // cockpit_NNN.glb of the cockpit NNN, or an empty string if there is none
    static QString find(const QString &cockpit);
    // the model in this file, shared while it is in use; it is loaded on the first draw
    static std::shared_ptr<Cockpit3D> get(const QString &fileName);

public:
    explicit Cockpit3D(const QString &fileName);
    ~Cockpit3D();

public:
    const QString& fileName() const { return m_fileName; }
    // false once loading failed
    bool isValid() const { return m_state != State::Failed; }
    // the camera of the cockpit art: focal length and optical centre in pixels of the 640x480 view
    void setCamera(float focal, const Vector2D &center);

    // the model with its texture (depth test and writes on); false if it couldn't be loaded
    bool draw(const Matrix &projection, const Matrix &view);
    // a texture covering the 640x480 view (premultiplied alpha) projected onto the model from
    // the eye at rest, over what draw() left in the depth buffer
    void drawDecal(const Matrix &projection, const Matrix &view, unsigned int texture);

private:
    bool create();

private:
    enum class State { Unloaded, Ready, Failed };

    QString m_fileName;
    State m_state = State::Unloaded;
    float m_focal = 245.5f;
    Vector2D m_center = Vector2D(320, 240);
    bool m_cameraChanged = false;

    // the textured model, then its triangles in view and the window glass for the decal
    gfx::Mesh m_mesh;
    int m_baseCount = 0;
    int m_decalCount = 0;
    unsigned int m_texture = 0;
};


} // namespace hud


#endif // HUD_COCKPIT3D_H
