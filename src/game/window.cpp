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
#include "gfx/image.h"

#include <QDataStream>
#include <QImage>
#include <QKeyEvent>
#include <QPixmap>
#include <QScreen>
#include <QSettings>
#include <QTimer>
#include <iostream>

namespace game {

Window::Window() :
    m_renderer(NULL)
{
    // the depth and stencil buffers of the QGLWidget this replaced; gfx::Pipeline's shader needs
    // OpenGL 3.2 (core profile) or OpenGL ES 3
    QSurfaceFormat format;
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    if (QOpenGLContext::openGLModuleType() == QOpenGLContext::LibGLES) {
        format.setRenderableType(QSurfaceFormat::OpenGLES);
        format.setVersion(3, 0);
    } else {
        format.setRenderableType(QSurfaceFormat::OpenGL);
        format.setVersion(3, 2);
        format.setProfile(QSurfaceFormat::CoreProfile);
    }
    setSurfaceType(QWindow::OpenGLSurface);
    setFormat(format);
    m_context.setFormat(format);
    if (!m_context.create() || m_context.format().majorVersion() < 3)
        qFatal("Cannot create an OpenGL 3.2 or OpenGL ES 3 context");
    m_offscreen.setFormat(m_context.format());
    m_offscreen.create();
    m_context.makeCurrent(&m_offscreen);

    const gfx::ColorTable colorTable("gfx:pal/gui/cursor.pal");
    // as Qt takes it, until the window is SDL's
    const gfx::Image cursor = gfx::Image::loadCursor("gfx:img/desktop/gui/cur_norm.img", colorTable).convertToFormat(gfx::Image::RGBA8888);
    m_cursor = QCursor(QPixmap::fromImage(QImage(cursor.constBits(), cursor.width(), cursor.height(), cursor.bytesPerLine(), QImage::Format_RGBA8888)), 3, 4);
    setTitle("Vertigo");
    loadSettings();

    QTimer *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &Window::render);
    timer->start(20);
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
    }
}

void Window::render()
{
    if (!isExposed() || !m_context.makeCurrent(this))
        return;

    const QSize viewport = size()*devicePixelRatio();
    if (viewport != m_viewport) {
        m_viewport = viewport;
        glViewport(0, 0, viewport.width(), viewport.height());
    }

    if (m_renderer) {
        // setCursor() recreates the native cursor, so only call it on changes
        const bool cursorVisible = m_renderer->isCursorVisible();
        if (cursorVisible && cursor().shape() != m_cursor.shape())
            setCursor(m_cursor);
        else if (!cursorVisible && cursor().shape() != Qt::BlankCursor)
            setCursor(Qt::BlankCursor);

        m_renderer->setRect(rect());
        m_renderer->draw();
    }

    if (!m_screenshots.isEmpty()) {
        // the frame is still in the back buffer
        gfx::Image image(viewport.width(), viewport.height(), gfx::Image::RGBA8888);
        glReadPixels(0, 0, viewport.width(), viewport.height(), GL_RGBA, GL_UNSIGNED_BYTE, image.bits());
        image = image.flipped().convertToFormat(gfx::Image::RGB888);
        foreach (const QString &file, m_screenshots) {
            if (!image.save(file.toStdString()))
                std::cerr << "Cannot save the screenshot " << file.toUtf8().constData() << std::endl;
        }
        m_screenshots.clear();
    }

    m_context.swapBuffers(this);
}

void Window::swapBuffers()
{
    if (isExposed() && m_context.makeCurrent(this))
        m_context.swapBuffers(this);
}

void Window::saveScreenshot(const QString &fileName)
{
    m_screenshots << fileName;
}

bool Window::event(QEvent *event)
{
    if (event->type() == QEvent::Close)
        saveSettings();
    return QWindow::event(event);
}

void Window::exposeEvent(QExposeEvent *)
{
    render();
}

void Window::keyPressEvent(QKeyEvent *event)
{
    const Qt::KeyboardModifiers modifiers = event->modifiers() & ~Qt::KeypadModifier;
    if ((modifiers == Qt::ControlModifier && event->key() == Qt::Key_F) || (modifiers == Qt::AltModifier && (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter))) {
        setWindowStates(windowStates() ^ Qt::WindowFullScreen);
    }

    if (m_renderer)
        m_renderer->keyPressEvent(event);
}

void Window::keyReleaseEvent(QKeyEvent *event)
{
    if (m_renderer)
        m_renderer->keyReleaseEvent(event);
}

void Window::mousePressEvent(QMouseEvent *event)
{
    if (m_renderer)
        m_renderer->mousePressEvent(event);
}

void Window::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_renderer)
        m_renderer->mouseReleaseEvent(event);
}

void Window::mouseMoveEvent(QMouseEvent *event)
{
    if (m_renderer)
        m_renderer->mouseMoveEvent(event);
}

void Window::moveEvent(QMoveEvent *event)
{
    if (!(windowStates() & Qt::WindowFullScreen))
        m_geometry = geometry();
    QWindow::moveEvent(event);
}

void Window::resizeEvent(QResizeEvent *event)
{
    if (!(windowStates() & Qt::WindowFullScreen))
        m_geometry = geometry();
    QWindow::resizeEvent(event);
}

void Window::saveSettings()
{
    // a maximized window comes back with its size, but not as maximized: on
    // macOS, maximizing a window of that size would shrink it again
    QSettings settings;
    settings.setValue("Window/geometry", m_geometry);
    settings.setValue("Window/fullScreen", bool(windowStates() & Qt::WindowFullScreen));
}

void Window::loadSettings()
{
    QSettings settings;
    QRect geometry = settings.value("Window/geometry").toRect();
    bool fullScreen = settings.value("Window/fullScreen", false).toBool();

    // what QWidget::saveGeometry() stored before (version 3): frame and
    // normal geometry, screen, maximized, full screen, screen width, geometry
    if (!geometry.isValid() && settings.contains("MainWindow/geometry")) {
        QDataStream stream(settings.value("MainWindow/geometry").toByteArray());
        stream.setVersion(QDataStream::Qt_4_0);
        quint32 magic;
        quint16 major, minor;
        QRect frameGeometry, normalGeometry, lastGeometry;
        qint32 screen, screenWidth;
        quint8 maximized, wasFullScreen;
        stream >> magic >> major >> minor >> frameGeometry >> normalGeometry >> screen >> maximized >> wasFullScreen >> screenWidth >> lastGeometry;
        if (stream.status() == QDataStream::Ok && magic == 0x1D9D0CB && major >= 3) {
            geometry = wasFullScreen ? normalGeometry : lastGeometry;
            fullScreen = wasFullScreen;
        }
    }

    if (geometry.isValid())
        setGeometry(geometry);
    else
        resize(1280, 960);
    m_geometry = this->geometry();
    if (fullScreen)
        setWindowStates(Qt::WindowFullScreen);
}

} // namespace game
