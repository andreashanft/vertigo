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

#ifndef GFX_GL_HPP
#define GFX_GL_HPP

// The OpenGL functions the game uses, fetched from the context with SDL_GL_GetProcAddress()
// (Windows only exports OpenGL 1.1, and the others aren't there before a context is). The
// types and constants are SDL's copy of gl.h and glext.h; the gl* names below are pointers.
// TODO(P5): iOS takes OpenGL ES 3's headers instead.
#include <SDL3/SDL_opengl.h>

#include <string>

// OpenGL 1.1, with the types of SDL's declarations
#define GFX_GL_FUNCTIONS_1_1(X) \
    X(glBindTexture) X(glBlendFunc) X(glClear) X(glClearColor) X(glDeleteTextures) X(glDepthFunc) X(glDepthMask) \
    X(glDisable) X(glDrawArrays) X(glDrawElements) X(glEnable) X(glFrontFace) X(glGenTextures) X(glGetError) \
    X(glGetFloatv) X(glGetIntegerv) X(glGetString) X(glIsEnabled) X(glPixelStorei) X(glReadPixels) X(glTexImage2D) \
    X(glTexParameterf) X(glTexParameteri) X(glTexSubImage2D) X(glViewport)

// later ones, with glext.h's types
#define GFX_GL_FUNCTIONS(X) \
    X(glAttachShader, PFNGLATTACHSHADERPROC) X(glBindAttribLocation, PFNGLBINDATTRIBLOCATIONPROC) \
    X(glBindBuffer, PFNGLBINDBUFFERPROC) X(glBindFramebuffer, PFNGLBINDFRAMEBUFFERPROC) \
    X(glBindRenderbuffer, PFNGLBINDRENDERBUFFERPROC) X(glBindVertexArray, PFNGLBINDVERTEXARRAYPROC) \
    X(glBlendFuncSeparate, PFNGLBLENDFUNCSEPARATEPROC) X(glBufferData, PFNGLBUFFERDATAPROC) \
    X(glBufferSubData, PFNGLBUFFERSUBDATAPROC) X(glCheckFramebufferStatus, PFNGLCHECKFRAMEBUFFERSTATUSPROC) \
    X(glCompileShader, PFNGLCOMPILESHADERPROC) X(glCreateProgram, PFNGLCREATEPROGRAMPROC) \
    X(glCreateShader, PFNGLCREATESHADERPROC) X(glDeleteBuffers, PFNGLDELETEBUFFERSPROC) \
    X(glDeleteFramebuffers, PFNGLDELETEFRAMEBUFFERSPROC) X(glDeleteProgram, PFNGLDELETEPROGRAMPROC) \
    X(glDeleteRenderbuffers, PFNGLDELETERENDERBUFFERSPROC) X(glDeleteShader, PFNGLDELETESHADERPROC) \
    X(glDeleteVertexArrays, PFNGLDELETEVERTEXARRAYSPROC) X(glEnableVertexAttribArray, PFNGLENABLEVERTEXATTRIBARRAYPROC) \
    X(glFramebufferRenderbuffer, PFNGLFRAMEBUFFERRENDERBUFFERPROC) X(glFramebufferTexture2D, PFNGLFRAMEBUFFERTEXTURE2DPROC) \
    X(glGenBuffers, PFNGLGENBUFFERSPROC) X(glGenerateMipmap, PFNGLGENERATEMIPMAPPROC) \
    X(glGenFramebuffers, PFNGLGENFRAMEBUFFERSPROC) X(glGenRenderbuffers, PFNGLGENRENDERBUFFERSPROC) \
    X(glGenVertexArrays, PFNGLGENVERTEXARRAYSPROC) X(glGetProgramInfoLog, PFNGLGETPROGRAMINFOLOGPROC) \
    X(glGetProgramiv, PFNGLGETPROGRAMIVPROC) X(glGetShaderInfoLog, PFNGLGETSHADERINFOLOGPROC) \
    X(glGetShaderiv, PFNGLGETSHADERIVPROC) X(glGetStringi, PFNGLGETSTRINGIPROC) \
    X(glGetUniformLocation, PFNGLGETUNIFORMLOCATIONPROC) X(glLinkProgram, PFNGLLINKPROGRAMPROC) \
    X(glRenderbufferStorage, PFNGLRENDERBUFFERSTORAGEPROC) X(glShaderSource, PFNGLSHADERSOURCEPROC) \
    X(glUniform1f, PFNGLUNIFORM1FPROC) X(glUniform1i, PFNGLUNIFORM1IPROC) X(glUniform2f, PFNGLUNIFORM2FPROC) \
    X(glUniform3fv, PFNGLUNIFORM3FVPROC) X(glUniform4fv, PFNGLUNIFORM4FVPROC) \
    X(glUniformMatrix3fv, PFNGLUNIFORMMATRIX3FVPROC) X(glUniformMatrix4fv, PFNGLUNIFORMMATRIX4FVPROC) \
    X(glUseProgram, PFNGLUSEPROGRAMPROC) X(glVertexAttrib3f, PFNGLVERTEXATTRIB3FPROC) \
    X(glVertexAttribPointer, PFNGLVERTEXATTRIBPOINTERPROC)

// (OpenGL 3.0 on the desktop only; may be missing)
#define GFX_GL_OPTIONAL_FUNCTIONS(X) \
    X(glBindFragDataLocation, PFNGLBINDFRAGDATALOCATIONPROC)

#define GFX_GL_DECLARE_1_1(name) extern decltype(&::name) gfx_##name;
#define GFX_GL_DECLARE(name, type) extern type gfx_##name;
GFX_GL_FUNCTIONS_1_1(GFX_GL_DECLARE_1_1)
GFX_GL_FUNCTIONS(GFX_GL_DECLARE)
GFX_GL_OPTIONAL_FUNCTIONS(GFX_GL_DECLARE)
#undef GFX_GL_DECLARE_1_1
#undef GFX_GL_DECLARE

#define glAttachShader gfx_glAttachShader
#define glBindAttribLocation gfx_glBindAttribLocation
#define glBindBuffer gfx_glBindBuffer
#define glBindFragDataLocation gfx_glBindFragDataLocation
#define glBindFramebuffer gfx_glBindFramebuffer
#define glBindRenderbuffer gfx_glBindRenderbuffer
#define glBindTexture gfx_glBindTexture
#define glBindVertexArray gfx_glBindVertexArray
#define glBlendFunc gfx_glBlendFunc
#define glBlendFuncSeparate gfx_glBlendFuncSeparate
#define glBufferData gfx_glBufferData
#define glBufferSubData gfx_glBufferSubData
#define glCheckFramebufferStatus gfx_glCheckFramebufferStatus
#define glClear gfx_glClear
#define glClearColor gfx_glClearColor
#define glCompileShader gfx_glCompileShader
#define glCreateProgram gfx_glCreateProgram
#define glCreateShader gfx_glCreateShader
#define glDeleteBuffers gfx_glDeleteBuffers
#define glDeleteFramebuffers gfx_glDeleteFramebuffers
#define glDeleteProgram gfx_glDeleteProgram
#define glDeleteRenderbuffers gfx_glDeleteRenderbuffers
#define glDeleteShader gfx_glDeleteShader
#define glDeleteTextures gfx_glDeleteTextures
#define glDeleteVertexArrays gfx_glDeleteVertexArrays
#define glDepthFunc gfx_glDepthFunc
#define glDepthMask gfx_glDepthMask
#define glDisable gfx_glDisable
#define glDrawArrays gfx_glDrawArrays
#define glDrawElements gfx_glDrawElements
#define glEnable gfx_glEnable
#define glEnableVertexAttribArray gfx_glEnableVertexAttribArray
#define glFramebufferRenderbuffer gfx_glFramebufferRenderbuffer
#define glFramebufferTexture2D gfx_glFramebufferTexture2D
#define glFrontFace gfx_glFrontFace
#define glGenBuffers gfx_glGenBuffers
#define glGenerateMipmap gfx_glGenerateMipmap
#define glGenFramebuffers gfx_glGenFramebuffers
#define glGenRenderbuffers gfx_glGenRenderbuffers
#define glGenTextures gfx_glGenTextures
#define glGenVertexArrays gfx_glGenVertexArrays
#define glGetError gfx_glGetError
#define glGetFloatv gfx_glGetFloatv
#define glGetIntegerv gfx_glGetIntegerv
#define glGetProgramInfoLog gfx_glGetProgramInfoLog
#define glGetProgramiv gfx_glGetProgramiv
#define glGetShaderInfoLog gfx_glGetShaderInfoLog
#define glGetShaderiv gfx_glGetShaderiv
#define glGetString gfx_glGetString
#define glGetStringi gfx_glGetStringi
#define glGetUniformLocation gfx_glGetUniformLocation
#define glIsEnabled gfx_glIsEnabled
#define glLinkProgram gfx_glLinkProgram
#define glPixelStorei gfx_glPixelStorei
#define glReadPixels gfx_glReadPixels
#define glRenderbufferStorage gfx_glRenderbufferStorage
#define glShaderSource gfx_glShaderSource
#define glTexImage2D gfx_glTexImage2D
#define glTexParameterf gfx_glTexParameterf
#define glTexParameteri gfx_glTexParameteri
#define glTexSubImage2D gfx_glTexSubImage2D
#define glUniform1f gfx_glUniform1f
#define glUniform1i gfx_glUniform1i
#define glUniform2f gfx_glUniform2f
#define glUniform3fv gfx_glUniform3fv
#define glUniform4fv gfx_glUniform4fv
#define glUniformMatrix3fv gfx_glUniformMatrix3fv
#define glUniformMatrix4fv gfx_glUniformMatrix4fv
#define glUseProgram gfx_glUseProgram
#define glVertexAttrib3f gfx_glVertexAttrib3f
#define glVertexAttribPointer gfx_glVertexAttribPointer
#define glViewport gfx_glViewport

namespace gfx::gl {

// fetches the functions from the current context; false (with the missing one) if one isn't there
bool load(std::string *missing = nullptr);
// OpenGL ES rather than OpenGL
bool isES();
bool hasExtension(const char *name);
// whether a context is current (without one, the GL objects have gone with it)
bool hasContext();

} // namespace gfx::gl

#endif // GFX_GL_HPP
