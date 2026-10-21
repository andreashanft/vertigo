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

#include "renderer.h"
#include "window.h"

#include "gfx/colortable.h"
#include "gfx/gl.hpp"
#include "gfx/image.h"
#include "util/error.h"
#include "util/settings.hpp"

#include <SDL3/SDL.h>
#include <glm/geometric.hpp>
#include <algorithm>
#include <iostream>
#include <sstream>

namespace game {

namespace {

// SDL's keys as ours: by the character the layout gives them, the number pad's digits as
// digits (with KeypadModifier); minus, equal and period by their place
input::Key key(SDL_Keycode code, SDL_Scancode place, int &modifiers)
{
    switch (place) {
    case SDL_SCANCODE_MINUS: return input::Key_Minus;
    case SDL_SCANCODE_EQUALS: return input::Key_Equal;
    case SDL_SCANCODE_PERIOD: return input::Key_Period;
    default: break;
    }
    if (code >= SDLK_0 && code <= SDLK_9)
        return input::Key(input::Key_0 + int(code - SDLK_0));
    if (code >= SDLK_A && code <= SDLK_Z)
        return input::Key(input::Key_A + int(code - SDLK_A));
    if (code >= SDLK_F1 && code <= SDLK_F12)
        return input::Key(input::Key_F1 + int(code - SDLK_F1));
    if (code >= SDLK_KP_1 && code <= SDLK_KP_9) {
        modifiers |= input::KeypadModifier;
        return input::Key(input::Key_1 + int(code - SDLK_KP_1));
    }
    switch (code) {
    case SDLK_KP_0: modifiers |= input::KeypadModifier; return input::Key_0;
    case SDLK_KP_ENTER: modifiers |= input::KeypadModifier; return input::Key_Enter;
    case SDLK_SPACE: return input::Key_Space;
    case SDLK_GRAVE: return input::Key_QuoteLeft;
    case SDLK_ESCAPE: return input::Key_Escape;
    case SDLK_TAB: return input::Key_Tab;
    case SDLK_BACKSPACE: return input::Key_Backspace;
    case SDLK_RETURN: return input::Key_Return;
    case SDLK_UP: return input::Key_Up;
    case SDLK_DOWN: return input::Key_Down;
    case SDLK_LEFT: return input::Key_Left;
    case SDLK_RIGHT: return input::Key_Right;
    default: return input::Key_None;
    }
}


int modifiers(SDL_Keymod mod)
{
    int result = input::NoModifier;
    if (mod & SDL_KMOD_SHIFT)
        result |= input::ShiftModifier;
    if (mod & SDL_KMOD_ALT)
        result |= input::AltModifier;
#ifdef SDL_PLATFORM_APPLE
    // the Command key is Control, as Qt had it on macOS (Cmd+F for full screen)
    if (mod & SDL_KMOD_GUI)
        result |= input::ControlModifier;
    if (mod & SDL_KMOD_CTRL)
        result |= input::MetaModifier;
#else
    if (mod & SDL_KMOD_CTRL)
        result |= input::ControlModifier;
    if (mod & SDL_KMOD_GUI)
        result |= input::MetaModifier;
#endif
    return result;
}


input::MouseButton button(Uint8 button)
{
    switch (button) {
    case SDL_BUTTON_LEFT: return input::LeftButton;
    case SDL_BUTTON_RIGHT: return input::RightButton;
    case SDL_BUTTON_MIDDLE: return input::MiddleButton;
    default: return input::NoButton;
    }
}


// SDL's gamepad buttons as ours (the Guide button, the paddles and the touchpad aren't used)
input::Button padButton(Uint8 button)
{
    switch (button) {
    case SDL_GAMEPAD_BUTTON_SOUTH: return input::Button_South;
    case SDL_GAMEPAD_BUTTON_EAST: return input::Button_East;
    case SDL_GAMEPAD_BUTTON_WEST: return input::Button_West;
    case SDL_GAMEPAD_BUTTON_NORTH: return input::Button_North;
    case SDL_GAMEPAD_BUTTON_BACK: return input::Button_Back;
    case SDL_GAMEPAD_BUTTON_START: return input::Button_Start;
    case SDL_GAMEPAD_BUTTON_LEFT_STICK: return input::Button_LeftStick;
    case SDL_GAMEPAD_BUTTON_RIGHT_STICK: return input::Button_RightStick;
    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER: return input::Button_LeftShoulder;
    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: return input::Button_RightShoulder;
    case SDL_GAMEPAD_BUTTON_DPAD_UP: return input::Button_Up;
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN: return input::Button_Down;
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT: return input::Button_Left;
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: return input::Button_Right;
    default: return input::Button_None;
    }
}

// a stick counts from this part of its way on (worn sticks don't quite come back to the middle);
// a trigger is pressed when pulled this far and released when let go to the second
const float StickDeadZone = 0.2f;
const float TriggerPress = 0.5f;
const float TriggerRelease = 0.3f;

} // namespace


Window::Window(bool focusable)
{
    // the depth and stencil buffers of the QGLWidget this replaced; gfx::Pipeline's shader needs
    // OpenGL 3.2 (core profile) or OpenGL ES 3
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
#if defined(SDL_PLATFORM_IOS) || defined(SDL_PLATFORM_ANDROID)
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#else
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
#endif
#if defined(SDL_PLATFORM_IOS)
    // the screen, in landscape either way round
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
#endif
    SDL_WindowFlags flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_HIDDEN;
    if (!focusable)
        flags |= SDL_WINDOW_NOT_FOCUSABLE;
#if defined(SDL_PLATFORM_IOS)
    // (without the status bar)
    flags |= SDL_WINDOW_FULLSCREEN;
#endif
    // 4x multisampling smooths the edges of the 3D cockpit's (and the world's) polygons; without
    // it if the system has none
    for (int samples : { 4, 0 }) {
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, samples > 0);
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, samples);
        m_window = SDL_CreateWindow("Vertigo", 1280, 960, flags);
        if (!m_window)
            util::fatal((std::string("Cannot create the window: ") + SDL_GetError()).c_str());
        m_context = SDL_GL_CreateContext(m_window);
        if (m_context || samples == 0)
            break;
        SDL_DestroyWindow(m_window);
    }
    std::string missing;
    if (!m_context || !SDL_GL_MakeCurrent(m_window, m_context))
        util::fatal((std::string("Cannot create an OpenGL 3.2 or OpenGL ES 3 context: ") + SDL_GetError()).c_str());
    if (!gfx::gl::load(&missing))
        util::fatal(("The OpenGL context lacks " + missing).c_str());
    SDL_GL_SetSwapInterval(1);

    const gfx::ColorTable colorTable("gfx:pal/gui/cursor.pal");
    gfx::Image cursor = gfx::Image::loadCursor("gfx:img/desktop/gui/cur_norm.img", colorTable).convertToFormat(gfx::Image::RGBA8888);
    if (SDL_Surface *surface = SDL_CreateSurfaceFrom(cursor.width(), cursor.height(), SDL_PIXELFORMAT_RGBA32, cursor.bits(), cursor.bytesPerLine())) {
        m_cursor = SDL_CreateColorCursor(surface, 3, 4);
        SDL_DestroySurface(surface);
    }
    if (m_cursor)
        SDL_SetCursor(m_cursor);
#if !defined(SDL_PLATFORM_IOS)
    // (on iOS the window is the screen)
    loadSettings();
#endif
}

Window::~Window()
{
    if (m_cursor)
        SDL_DestroyCursor(m_cursor);
    if (m_context)
        SDL_GL_DestroyContext(m_context);
    if (m_window)
        SDL_DestroyWindow(m_window);
}

void Window::show()
{
    SDL_ShowWindow(m_window);
    m_shown = true;
}

void Window::close()
{
    if (m_closed)
        return;
#if !defined(SDL_PLATFORM_IOS)
    saveSettings();
#endif
    m_closed = true;
    SDL_HideWindow(m_window);
}

int Window::width() const
{
    int w = 0;
    SDL_GetWindowSize(m_window, &w, nullptr);
    return w;
}

int Window::height() const
{
    int h = 0;
    SDL_GetWindowSize(m_window, nullptr, &h);
    return h;
}

double Window::devicePixelRatio() const
{
    return SDL_GetWindowPixelDensity(m_window);
}

void Window::setRenderer(Renderer *renderer)
{
    if (m_renderer) {
        m_renderer->deactivate();
        m_renderer->setWindow(NULL);
    }

    m_renderer = renderer;

    if (m_renderer) {
        m_renderer->setWindow(this);
        m_renderer->activate();
        // a stick held through the change
        for (int stick = input::LeftStick; stick <= input::RightStick; stick++)
            if (m_sticks[stick] != Vector2D(0))
                m_renderer->stickEvent(input::Stick(stick), m_sticks[stick]);
    }
}

void Window::render()
{
    if (!isVisible() || m_background)
        return;

    int viewportWidth = 0, viewportHeight = 0;
    SDL_GetWindowSizeInPixels(m_window, &viewportWidth, &viewportHeight);
    if (viewportWidth != m_viewportWidth || viewportHeight != m_viewportHeight) {
        m_viewportWidth = viewportWidth;
        m_viewportHeight = viewportHeight;
        glViewport(0, 0, viewportWidth, viewportHeight);
    }

    if (m_renderer) {
        const bool cursorVisible = m_renderer->isCursorVisible();
        if (cursorVisible != m_cursorVisible) {
            m_cursorVisible = cursorVisible;
            if (cursorVisible)
                SDL_ShowCursor();
            else
                SDL_HideCursor();
        }
        const bool textInput = m_renderer->wantsTextInput();
        if (textInput != SDL_TextInputActive(m_window)) {
            if (textInput) {
                const Rect area = m_renderer->textInputArea();
                const PointD topLeft = m_renderer->imageToScreen(PointD(area.x, area.y));
                const PointD bottomRight = m_renderer->imageToScreen(PointD(area.x + area.width, area.y + area.height));
                const SDL_Rect rect = { int(topLeft.x), int(topLeft.y), int(bottomRight.x - topLeft.x), int(bottomRight.y - topLeft.y) };
                SDL_SetTextInputArea(m_window, &rect, 0);
                SDL_StartTextInput(m_window);
            } else {
                SDL_StopTextInput(m_window);
            }
        }

        m_renderer->setRect(Rect(0, 0, width(), height()));
        m_renderer->draw();
    }

    if (!m_screenshots.empty()) {
        // the frame is still in the back buffer
        gfx::Image image(viewportWidth, viewportHeight, gfx::Image::RGBA8888);
        glReadPixels(0, 0, viewportWidth, viewportHeight, GL_RGBA, GL_UNSIGNED_BYTE, image.bits());
        image = image.flipped().convertToFormat(gfx::Image::RGB888);
        for (const std::string &file : m_screenshots) {
            if (!image.save(file))
                std::cerr << "Cannot save the screenshot " << file << std::endl;
        }
        m_screenshots.clear();
    }

    SDL_GL_SwapWindow(m_window);
}

void Window::swapBuffers()
{
    if (isVisible() && !m_background)
        SDL_GL_SwapWindow(m_window);
}

void Window::saveScreenshot(const std::string &fileName)
{
    m_screenshots.push_back(fileName);
}

void Window::handle(const SDL_Event &event)
{
    switch (event.type) {
    case SDL_EVENT_QUIT:
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        close();
        break;
    // on the iPad: interrupted (a call, Siri, the app switcher), a mission pauses; in the
    // background nothing draws until the app comes back
    case SDL_EVENT_WILL_ENTER_BACKGROUND:
        if (m_renderer)
            m_renderer->pause();
        break;
    case SDL_EVENT_DID_ENTER_BACKGROUND:
        glFinish();
        m_background = true;
        break;
    case SDL_EVENT_WILL_ENTER_FOREGROUND:
        m_background = false;
        break;
    case SDL_EVENT_WINDOW_MOVED:
    case SDL_EVENT_WINDOW_RESIZED:
        if (!(SDL_GetWindowFlags(m_window) & SDL_WINDOW_FULLSCREEN)) {
            SDL_GetWindowPosition(m_window, &m_geometry[0], &m_geometry[1]);
            SDL_GetWindowSize(m_window, &m_geometry[2], &m_geometry[3]);
        }
        break;
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP: {
        int mods = modifiers(event.key.mod);
        const input::Key k = key(event.key.key, event.key.scancode, mods);
        if (k == input::Key_None)
            break;
        input::KeyEvent keyEvent = input::alternative(input::KeyEvent(k, mods));
        if (event.type == SDL_EVENT_KEY_DOWN)
            keyPressEvent(&keyEvent);
        else if (m_renderer)
            m_renderer->keyReleaseEvent(&keyEvent);
        break;
    }
    case SDL_EVENT_TEXT_INPUT: {
        input::KeyEvent text(input::Key_None, input::NoModifier, String::fromUtf8(event.text.text));
        if (m_renderer)
            m_renderer->keyPressEvent(&text);
        break;
    }
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP: {
        const input::MouseButton b = button(event.button.button);
        if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
            m_buttons |= b;
        else
            m_buttons &= ~b;
        input::MouseEvent mouse(PointD(event.button.x, event.button.y), b, m_buttons, event.button.which == SDL_TOUCH_MOUSEID);
        if (m_renderer) {
            if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
                m_renderer->mousePressEvent(&mouse);
            else
                m_renderer->mouseReleaseEvent(&mouse);
        }
        break;
    }
    // game controllers: all of them at once, each from when it is connected
    case SDL_EVENT_GAMEPAD_ADDED:
        if (!m_onlyGamepad || event.gdevice.which == m_onlyGamepad)
            SDL_OpenGamepad(event.gdevice.which);
        break;
    case SDL_EVENT_GAMEPAD_REMOVED:
        if (SDL_Gamepad *gamepad = SDL_GetGamepadFromID(event.gdevice.which)) {
            SDL_CloseGamepad(gamepad);
            releaseGamepad();
        }
        break;
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    case SDL_EVENT_GAMEPAD_BUTTON_UP:
        if (!m_onlyGamepad || event.gbutton.which == m_onlyGamepad)
            buttonEvent(padButton(event.gbutton.button), event.gbutton.down);
        break;
    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
        if ((!m_onlyGamepad || event.gaxis.which == m_onlyGamepad) && event.gaxis.axis < SDL_GAMEPAD_AXIS_COUNT)
            axisEvent(event.gaxis.axis, std::max(event.gaxis.value/32767.0f, -1.0f));
        break;
    // a touch screen's fingers (macOS also reports a trackpad's, which only move the mouse)
    case SDL_EVENT_FINGER_DOWN:
    case SDL_EVENT_FINGER_MOTION:
    case SDL_EVENT_FINGER_UP:
    case SDL_EVENT_FINGER_CANCELED:
        if (SDL_GetTouchDeviceType(event.tfinger.touchID) == SDL_TOUCH_DEVICE_DIRECT) {
            input::TouchEvent touch((long long)event.tfinger.fingerID, PointD(event.tfinger.x*width(), event.tfinger.y*height()));
            touchEvent(event.type == SDL_EVENT_FINGER_DOWN ? EventType::TouchPress :
                       event.type == SDL_EVENT_FINGER_MOTION ? EventType::TouchMove : EventType::TouchRelease, &touch);
        }
        break;
    case SDL_EVENT_MOUSE_MOTION: {
        input::MouseEvent mouse(PointD(event.motion.x, event.motion.y), input::NoButton, m_buttons, event.motion.which == SDL_TOUCH_MOUSEID);
        if (m_renderer)
            m_renderer->mouseMoveEvent(&mouse);
        break;
    }
    default:
        break;
    }
}

void Window::buttonEvent(input::Button button, bool pressed)
{
    if (button == input::Button_None || m_padButtons[button] == pressed)
        return;
    m_padButtons[button] = pressed;
    if (!m_renderer)
        return;
    if (pressed)
        m_renderer->buttonPressEvent(button);
    else
        m_renderer->buttonReleaseEvent(button);
}

void Window::axisEvent(int axis, float value)
{
    m_axes[axis] = value;
    if (axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER || axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) {
        const input::Button trigger = axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER ? input::Button_LeftTrigger : input::Button_RightTrigger;
        if (value >= TriggerPress)
            buttonEvent(trigger, true);
        else if (value <= TriggerRelease)
            buttonEvent(trigger, false);
        return;
    }
    // the dead zone is round, and beyond it the stick goes from 0 to 1 (also on the diagonals of
    // sticks that reach further there)
    const int stick = axis < SDL_GAMEPAD_AXIS_RIGHTX ? input::LeftStick : input::RightStick;
    Vector2D position(m_axes[2*stick], m_axes[2*stick + 1]);
    const float length = glm::length(position);
    position = length <= StickDeadZone ? Vector2D(0) : position*((std::min(length, 1.0f) - StickDeadZone)/(1 - StickDeadZone)/length);
    if (position == m_sticks[stick])
        return;
    m_sticks[stick] = position;
    if (m_renderer)
        m_renderer->stickEvent(input::Stick(stick), position);
}

// a controller is gone: nothing of it is held any more
void Window::releaseGamepad()
{
    for (int button = 0; button < input::ButtonCount; button++)
        buttonEvent(input::Button(button), false);
    for (int axis = 0; axis < SDL_GAMEPAD_AXIS_COUNT; axis++)
        axisEvent(axis, 0);
}

void Window::touchEvent(EventType type, input::TouchEvent *event)
{
    if (!m_renderer)
        return;
    if (type == EventType::TouchPress)
        m_renderer->touchPressEvent(event);
    else if (type == EventType::TouchMove)
        m_renderer->touchMoveEvent(event);
    else
        m_renderer->touchReleaseEvent(event);
}

void Window::post(EventType type, const input::KeyEvent &event)
{
    m_posted.push_back({ type, event, input::MouseEvent(), input::TouchEvent() });
}

void Window::post(EventType type, const input::MouseEvent &event)
{
    m_posted.push_back({ type, input::KeyEvent(), event, input::TouchEvent() });
}

void Window::post(EventType type, const input::TouchEvent &event)
{
    m_posted.push_back({ type, input::KeyEvent(), input::MouseEvent(), event });
}

void Window::deliverPosted()
{
    while (!m_posted.empty()) {
        Posted posted = m_posted.front();
        m_posted.pop_front();
        switch (posted.type) {
        case EventType::KeyPress:
            posted.key = input::alternative(posted.key);
            keyPressEvent(&posted.key);
            break;
        case EventType::KeyRelease:
            posted.key = input::alternative(posted.key);
            if (m_renderer)
                m_renderer->keyReleaseEvent(&posted.key);
            break;
        case EventType::MousePress:
            if (m_renderer)
                m_renderer->mousePressEvent(&posted.mouse);
            break;
        case EventType::MouseRelease:
            if (m_renderer)
                m_renderer->mouseReleaseEvent(&posted.mouse);
            break;
        case EventType::MouseMove:
            if (m_renderer)
                m_renderer->mouseMoveEvent(&posted.mouse);
            break;
        case EventType::TouchPress:
        case EventType::TouchMove:
        case EventType::TouchRelease:
            touchEvent(posted.type, &posted.touch);
            break;
        }
    }
}

void Window::keyPressEvent(input::KeyEvent *event)
{
    const int modifiers = event->modifiers() & ~input::KeypadModifier;
    if ((modifiers == input::ControlModifier && event->key() == input::Key_F) || (modifiers == input::AltModifier && (event->key() == input::Key_Return || event->key() == input::Key_Enter)))
        toggleFullScreen();

    if (m_renderer)
        m_renderer->keyPressEvent(event);
}

void Window::toggleFullScreen()
{
    SDL_SetWindowFullscreen(m_window, !(SDL_GetWindowFlags(m_window) & SDL_WINDOW_FULLSCREEN));
}

void Window::saveSettings()
{
    // a maximized window comes back with its size, but not as maximized: on
    // macOS, maximizing a window of that size would shrink it again
    std::ostringstream geometry;
    geometry << m_geometry[0] << " " << m_geometry[1] << " " << m_geometry[2] << " " << m_geometry[3];
    util::Settings::setValue("Window/geometry", geometry.str());
    util::Settings::setValue("Window/fullScreen", bool(SDL_GetWindowFlags(m_window) & SDL_WINDOW_FULLSCREEN));
}

void Window::loadSettings()
{
    // "x y width height"
    int x = 0, y = 0, width = 0, height = 0;
    std::istringstream(util::Settings::value("Window/geometry")) >> x >> y >> width >> height;
    if (width > 0 && height > 0) {
        SDL_SetWindowSize(m_window, width, height);
        SDL_SetWindowPosition(m_window, x, y);
    }
    SDL_GetWindowPosition(m_window, &m_geometry[0], &m_geometry[1]);
    SDL_GetWindowSize(m_window, &m_geometry[2], &m_geometry[3]);
    if (util::Settings::boolValue("Window/fullScreen", false))
        SDL_SetWindowFullscreen(m_window, true);
}

} // namespace game
