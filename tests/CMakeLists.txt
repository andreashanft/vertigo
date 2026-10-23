# Tests of the game's own types: strings, data values and dates, images and colours, the parsers
# of the text formats, JSON, CD images, the hash that checks downloads and the vector font

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

# util::Sha256
add_executable(sha256test
    sha256test.cpp
    ${PROJECT_SOURCE_DIR}/src/util/sha256.cpp
    ${PROJECT_SOURCE_DIR}/src/util/path.cpp
)
target_include_directories(sha256test PRIVATE ${PROJECT_SOURCE_DIR}/src)
add_test(NAME sha256 COMMAND sha256test)

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

# hud::CockpitPaint with a kit of data/3D-cockpits/kits
find_package(Threads REQUIRED)
add_executable(cockpitpainttest
    cockpitpainttest.cpp
    ${PROJECT_SOURCE_DIR}/src/hud/cockpitpaint.cpp
    ${PROJECT_SOURCE_DIR}/src/gfx/glbmodel.cpp
    ${PROJECT_SOURCE_DIR}/src/gfx/colortable.cpp
    ${PROJECT_SOURCE_DIR}/src/gfx/image.cpp
    ${PROJECT_SOURCE_DIR}/src/gfx/rgb.cpp
    ${PROJECT_SOURCE_DIR}/src/util/datapath.cpp
    ${PROJECT_SOURCE_DIR}/src/util/error.cpp
    ${PROJECT_SOURCE_DIR}/src/util/file.cpp
    ${PROJECT_SOURCE_DIR}/src/util/json.cpp
    ${PROJECT_SOURCE_DIR}/src/util/path.cpp
    ${PROJECT_SOURCE_DIR}/src/util/string.cpp
)
target_include_directories(cockpitpainttest PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_include_directories(cockpitpainttest SYSTEM PRIVATE ${PROJECT_SOURCE_DIR}/3rdparty/stb)
target_compile_definitions(cockpitpainttest PRIVATE VERTIGO_KITS_DIR="${PROJECT_SOURCE_DIR}/data/3D-cockpits/kits")
target_link_libraries(cockpitpainttest Threads::Threads)
if(TARGET glm::glm-header-only)
    target_link_libraries(cockpitpainttest glm::glm-header-only)
elseif(TARGET glm::glm)
    target_link_libraries(cockpitpainttest glm::glm)
endif()
add_test(NAME cockpitpaint COMMAND cockpitpainttest)

# gfx::VectorFont, the large silver font redrawn as vectors
add_executable(vectorfonttest
    vectorfonttest.cpp
    ${PROJECT_SOURCE_DIR}/src/gfx/colortable.cpp
    ${PROJECT_SOURCE_DIR}/src/gfx/image.cpp
    ${PROJECT_SOURCE_DIR}/src/gfx/rgb.cpp
    ${PROJECT_SOURCE_DIR}/src/gfx/vectorfont.cpp
    ${PROJECT_SOURCE_DIR}/src/util/datapath.cpp
    ${PROJECT_SOURCE_DIR}/src/util/error.cpp
    ${PROJECT_SOURCE_DIR}/src/util/file.cpp
    ${PROJECT_SOURCE_DIR}/src/util/json.cpp
    ${PROJECT_SOURCE_DIR}/src/util/path.cpp
    ${PROJECT_SOURCE_DIR}/src/util/string.cpp
)
target_include_directories(vectorfonttest PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_include_directories(vectorfonttest SYSTEM PRIVATE ${PROJECT_SOURCE_DIR}/3rdparty/stb)
add_test(NAME vectorfont COMMAND vectorfonttest)
