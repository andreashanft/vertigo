# util::String and txt::Value, compared with QString and QVariant while Qt is linked; with the
# game data's directory as the argument also on all of its text files
add_executable(stringtest
    stringtest.cpp
    ${PROJECT_SOURCE_DIR}/src/util/string.cpp
    ${PROJECT_SOURCE_DIR}/src/txt/value.cpp
)
target_include_directories(stringtest PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(stringtest Qt${QT_VERSION_MAJOR}::Core)
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
    ${PROJECT_SOURCE_DIR}/src/util/string.cpp
)
target_include_directories(imagetest PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_include_directories(imagetest SYSTEM PRIVATE ${PROJECT_SOURCE_DIR}/3rdparty/stb)
target_link_libraries(imagetest Qt${QT_VERSION_MAJOR}::Gui)
add_test(NAME image COMMAND imagetest)
