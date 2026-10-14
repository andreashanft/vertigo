# Tests that compare our replacements of Qt's types with Qt (strings, data values, images,
# colours, regular expressions, JSON, dates). The game doesn't need Qt; these are only built where
# Qt 6 is installed.
find_package(Qt6 QUIET COMPONENTS Core Gui)
if(NOT Qt6_FOUND)
    message(STATUS "Qt 6 not found: the tests that compare with Qt are left out")
    return()
endif()

# util::String and txt::Value, compared with QString and QVariant while Qt is linked; with the
# game data's directory as the argument also on all of its text files
add_executable(stringtest
    stringtest.cpp
    ${PROJECT_SOURCE_DIR}/src/util/string.cpp
    ${PROJECT_SOURCE_DIR}/src/util/datetime.cpp
    ${PROJECT_SOURCE_DIR}/src/txt/value.cpp
)
target_include_directories(stringtest PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(stringtest Qt6::Core)
add_test(NAME string COMMAND stringtest)

# gfx::Image and gfx::Rgb, compared with QImage and QColor while Qt is linked; with the game
# data's directory as the argument also on the JPEG images of the 3D cockpits
add_executable(imagetest
    imagetest.cpp
    ${PROJECT_SOURCE_DIR}/src/gfx/colortable.cpp
    ${PROJECT_SOURCE_DIR}/src/gfx/image.cpp
    ${PROJECT_SOURCE_DIR}/src/gfx/rgb.cpp
    ${PROJECT_SOURCE_DIR}/src/util/datapath.cpp
    ${PROJECT_SOURCE_DIR}/src/util/error.cpp
    ${PROJECT_SOURCE_DIR}/src/util/file.cpp
    ${PROJECT_SOURCE_DIR}/src/util/path.cpp
    ${PROJECT_SOURCE_DIR}/src/util/string.cpp
)
target_include_directories(imagetest PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_include_directories(imagetest SYSTEM PRIVATE ${PROJECT_SOURCE_DIR}/3rdparty/stb)
target_link_libraries(imagetest Qt6::Gui)
add_test(NAME image COMMAND imagetest)

# the parsers of txt/parse.hpp, compared with the regular expressions they replace while Qt is
# linked; with the game data's directory as the argument also on every line of its text files
add_executable(parsetest
    parsetest.cpp
    ${PROJECT_SOURCE_DIR}/src/txt/parse.cpp
    ${PROJECT_SOURCE_DIR}/src/util/string.cpp
)
target_include_directories(parsetest PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(parsetest Qt6::Core)
add_test(NAME parse COMMAND parsetest)

# util::Json, compared with QJsonDocument while Qt is linked; with a directory as the argument
# also on the JSON of every .glb file in it
add_executable(jsontest
    jsontest.cpp
    ${PROJECT_SOURCE_DIR}/src/util/json.cpp
)
target_include_directories(jsontest PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(jsontest Qt6::Core)
add_test(NAME json COMMAND jsontest)
