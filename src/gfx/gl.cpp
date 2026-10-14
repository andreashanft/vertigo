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

#include "gl.hpp"

#include <SDL3/SDL_video.h>
#include <cstring>
#include <type_traits>

// (decltype of the declarations: the gl* names are the pointers by now)
#define GFX_GL_DEFINE_1_1(name) decltype(gfx_##name) gfx_##name = nullptr;
#define GFX_GL_DEFINE(name, type) decltype(gfx_##name) gfx_##name = nullptr;
GFX_GL_FUNCTIONS_1_1(GFX_GL_DEFINE_1_1)
GFX_GL_FUNCTIONS(GFX_GL_DEFINE)
GFX_GL_OPTIONAL_FUNCTIONS(GFX_GL_DEFINE)

namespace gfx::gl {


namespace {

bool s_es = false;

} // namespace


bool load(std::string *missing)
{
    bool ok = true;
    auto fetch = [&](auto &pointer, const char *name, bool required) {
        pointer = reinterpret_cast<std::remove_reference_t<decltype(pointer)>>(SDL_GL_GetProcAddress(name));
        if (!pointer && required && ok) {
            ok = false;
            if (missing)
                *missing = name;
        }
    };
#define GFX_GL_FETCH_1_1(name) fetch(gfx_##name, #name, true);
#define GFX_GL_FETCH(name, type) fetch(gfx_##name, #name, true);
#define GFX_GL_FETCH_OPTIONAL(name, type) fetch(gfx_##name, #name, false);
    GFX_GL_FUNCTIONS_1_1(GFX_GL_FETCH_1_1)
    GFX_GL_FUNCTIONS(GFX_GL_FETCH)
    GFX_GL_OPTIONAL_FUNCTIONS(GFX_GL_FETCH_OPTIONAL)
    if (ok) {
        const char *version = reinterpret_cast<const char *>(glGetString(GL_VERSION));
        s_es = version && std::strncmp(version, "OpenGL ES", 9) == 0;
    }
    return ok;
}


bool isES()
{
    return s_es;
}


bool hasContext()
{
    return SDL_GL_GetCurrentContext() != nullptr;
}


bool hasExtension(const char *name)
{
    GLint count = 0;
    glGetIntegerv(GL_NUM_EXTENSIONS, &count);
    for (GLint i = 0; i < count; i++) {
        const char *extension = reinterpret_cast<const char *>(glGetStringi(GL_EXTENSIONS, GLuint(i)));
        if (extension && std::strcmp(extension, name) == 0)
            return true;
    }
    return false;
}


} // namespace gfx::gl
