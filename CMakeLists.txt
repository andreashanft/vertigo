cmake_minimum_required(VERSION 3.11 FATAL_ERROR)

project(vertigo)

# the window, the input and the OpenGL context
find_package(SDL3 REQUIRED CONFIG COMPONENTS SDL3)
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

enable_testing()
add_subdirectory(tests)
