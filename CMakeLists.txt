cmake_minimum_required(VERSION 3.11 FATAL_ERROR)

# iPad: configured with -G Xcode -DCMAKE_SYSTEM_NAME=iOS (see the README)
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    cmake_minimum_required(VERSION 3.24 FATAL_ERROR)
    set(CMAKE_OSX_DEPLOYMENT_TARGET 18.6 CACHE STRING "The oldest iPadOS Vertigo runs on")
endif()

# 1.0.0 will be the feature-complete clone; see CLAUDE.md for the patch number
project(vertigo VERSION 0.9.7)

if(IOS)
    enable_language(OBJCXX)
    # No package manager there: SDL is built from its source release, pinned by its SHA-256
    # (-DFETCHCONTENT_SOURCE_DIR_SDL3=<directory> takes an unpacked copy instead). OpenAL and
    # OpenGL ES are the system's, found by Xcode in the SDK it builds for (device or simulator).
    include(FetchContent)
    FetchContent_Declare(SDL3
        URL https://github.com/libsdl-org/SDL/releases/download/release-3.4.18/SDL3-3.4.18.tar.gz
        URL_HASH SHA256=9c75cf16330322c217dedd2e0609f1124f1b54b8633e763467b4684d0f4334a3
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(SDL3)
    set(OPENAL_LIBRARY "-framework OpenAL")
    # GLM is headers only, so the host's do
    find_package(glm REQUIRED NO_CMAKE_FIND_ROOT_PATH)
else()
    # the window, the input and the OpenGL context
    find_package(SDL3 REQUIRED CONFIG COMPONENTS SDL3)
    find_package(OpenAL REQUIRED)
    find_package(glm REQUIRED)
endif()
add_definitions(-DGLM_ENABLE_EXPERIMENTAL)
# GLM >= 0.9.9 no longer zero-initializes vectors and identity-initializes
# matrices by default; the code relies on the old behavior
add_definitions(-DGLM_FORCE_CTOR_INIT)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

set(CMAKE_CXX_STANDARD 17)

add_subdirectory(src)

if(NOT IOS)
    add_subdirectory(data)
    enable_testing()
    add_subdirectory(tests)
endif()
