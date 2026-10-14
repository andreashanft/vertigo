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
