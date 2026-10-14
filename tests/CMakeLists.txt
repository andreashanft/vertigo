# Tests of the game's own types: strings, data values and dates, images and colours, the parsers
# of the text formats, JSON and CD images

# util::String, txt::Value and util::DateTime
add_executable(stringtest
    stringtest.cpp
    ${PROJECT_SOURCE_DIR}/src/util/string.cpp
    ${PROJECT_SOURCE_DIR}/src/util/datetime.cpp
    ${PROJECT_SOURCE_DIR}/src/txt/value.cpp
)
target_include_directories(stringtest PRIVATE ${PROJECT_SOURCE_DIR}/src)
add_test(NAME string COMMAND stringtest)

# gfx::Image and the colours of gfx/rgb.h
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
add_test(NAME image COMMAND imagetest)

# the parsers of txt/parse.hpp
add_executable(parsetest
    parsetest.cpp
    ${PROJECT_SOURCE_DIR}/src/txt/parse.cpp
    ${PROJECT_SOURCE_DIR}/src/util/string.cpp
)
target_include_directories(parsetest PRIVATE ${PROJECT_SOURCE_DIR}/src)
add_test(NAME parse COMMAND parsetest)

# util::Json
add_executable(jsontest
    jsontest.cpp
    ${PROJECT_SOURCE_DIR}/src/util/json.cpp
    ${PROJECT_SOURCE_DIR}/src/util/string.cpp
)
target_include_directories(jsontest PRIVATE ${PROJECT_SOURCE_DIR}/src)
add_test(NAME json COMMAND jsontest)

# util::CdImage
add_executable(cdimagetest
    cdimagetest.cpp
    ${PROJECT_SOURCE_DIR}/src/util/cdimage.cpp
    ${PROJECT_SOURCE_DIR}/src/util/path.cpp
)
target_include_directories(cdimagetest PRIVATE ${PROJECT_SOURCE_DIR}/src)
add_test(NAME cdimage COMMAND cdimagetest)
