cmake_minimum_required(VERSION 3.11 FATAL_ERROR)

project(vertigo)

find_package(Qt5OpenGL REQUIRED)
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

set(CMAKE_CXX_STANDARD 11)

add_subdirectory(data)
add_subdirectory(src)
