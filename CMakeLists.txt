cmake_minimum_required(VERSION 3.11 FATAL_ERROR)

project(vertigo)

# Qt 6, or Qt 5.15; whichever comes first in CMAKE_PREFIX_PATH
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)
if(QT_VERSION_MAJOR EQUAL 6)
    # QOpenGLFramebufferObject and QOpenGLFunctions left Qt GUI in Qt 6
    find_package(Qt6 REQUIRED COMPONENTS OpenGL)
elseif(Qt5Widgets_VERSION VERSION_LESS 5.15)
    message(FATAL_ERROR "Qt 5.15 or Qt 6 is needed, found Qt ${Qt5Widgets_VERSION}")
endif()
find_package(OpenGL REQUIRED)
if(APPLE)
    # OpenGL is deprecated on macOS but still available
    add_definitions(-DGL_SILENCE_DEPRECATION)
endif()
find_package(OpenAL REQUIRED)
find_package(glm REQUIRED)
add_definitions(-DGLM_ENABLE_EXPERIMENTAL)
# GLM >= 0.9.9 no longer zero-initializes vectors and identity-initializes
# matrices by default; the code relies on the old behavior
add_definitions(-DGLM_FORCE_CTOR_INIT)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

set(CMAKE_CXX_STANDARD 17)

add_subdirectory(data)
add_subdirectory(src)
