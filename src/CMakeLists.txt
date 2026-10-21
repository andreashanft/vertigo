include_directories(${GLM_INCLUDE_DIR})
include_directories(${OPENAL_INCLUDE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
# stb_image and stb_image_write
include_directories(SYSTEM ${PROJECT_SOURCE_DIR}/3rdparty/stb)

set(SOURCES
    fight/boundingbox.cpp
    fight/boundingbox.h
    fight/collisionmanager.cpp
    fight/collisionmanager.h
    fight/collisionmesh.cpp
    fight/collisionmesh.h
    fight/condition.cpp
    fight/condition.h
    fight/conditionmanager.cpp
    fight/conditionmanager.h
    fight/enginesounds.cpp
    fight/enginesounds.h
    fight/headmotion.cpp
    fight/headmotion.h
    fight/module.cpp
    fight/orders.cpp
    fight/orders.h
    fight/module.h
    fight/modulemanager.cpp
    fight/modulemanager.h
    fight/scenario.cpp
    fight/scenario.h
    fight/sonar.cpp
    fight/sonar.h
    fight/target.cpp
    fight/target.h

    fight/effects/billboard.cpp
    fight/effects/billboard.h
    fight/effects/effect.cpp
    fight/effects/effect.h
    fight/effects/effectmanager.cpp
    fight/effects/effectmanager.h
    fight/effects/projectile.cpp
    fight/effects/projectile.h
    fight/effects/trash.cpp
    fight/effects/trash.h
    fight/effects/particles.cpp
    fight/effects/particles.h

    fight/objects/activeobject.cpp
    fight/objects/activeobject.h
    fight/objects/building.cpp
    fight/objects/building.h
    fight/objects/mine.cpp
    fight/objects/mine.h
    fight/objects/navpoint.cpp
    fight/objects/navpoint.h
    fight/boats/cargoship.cpp
    fight/boats/cargoship.h
    fight/objects/placeholder.cpp
    fight/objects/placeholder.h
    fight/objects/decoycontact.cpp
    fight/objects/decoycontact.h
    fight/objects/object.cpp
    fight/objects/object.h
    fight/objects/simpleobject.cpp
    fight/objects/simpleobject.h
    fight/objects/turret.cpp
    fight/objects/turret.h
    fight/objects/turretbase.cpp
    fight/objects/turretbase.h

    fight/boats/player.cpp
    fight/boats/playerturret.cpp
    fight/boats/playerturret.h
    fight/boats/player.h
    fight/boats/simpleboat.cpp
    fight/boats/simpleboat.h
    fight/boats/tank.cpp
    fight/boats/tank.h

    fight/surface/betaspline.cpp
    fight/surface/betaspline.h
    fight/surface/element.cpp
    fight/surface/element.h
    fight/surface/surface.cpp
    fight/surface/surface.h
    fight/surface/tesselator.cpp
    fight/surface/tesselator.h

    fight/weapons/gun.cpp
    fight/weapons/gun.h
    fight/weapons/torpedo.cpp
    fight/weapons/torpedo.h
    fight/weapons/unitgun.cpp
    fight/weapons/unitgun.h

    game/area.cpp
    game/area.h
    game/boat.cpp
    game/boat.h
    game/briefing.cpp
    game/briefing.h
    game/chapter.cpp
    game/chapter.h
    game/departure.cpp
    game/departure.h
    game/depot.cpp
    game/depot.h
    game/desktop.cpp
    game/desktop.h
    game/dialog.cpp
    game/dialog.h
    game/dialogframe.cpp
    game/dialogframe.h
    game/encom.cpp
    game/encom.h
    game/gamedata.cpp
    game/gamedata.h
    game/input.cpp
    game/input.hpp
    game/items.cpp
    game/items.h
    game/main.cpp
    game/mainmenu.cpp
    game/mainmenu.h
    game/menu.cpp
    game/menu.h
    game/minimovie.cpp
    game/minimovie.h
    game/mission.cpp
    game/mission.h
    game/movie.cpp
    game/movie.h
    game/notebook.cpp
    game/notebook.h
    game/openmission.cpp
    game/openmission.h
    game/renderer.cpp
    game/renderer.h
    game/room.cpp
    game/room.h
    game/savedgame.h
    game/station.cpp
    game/station.h
    game/task.cpp
    game/task.h
    game/training.cpp
    game/training.h
    game/vertigo.cpp
    game/vertigo.h
    game/window.cpp
    game/window.h

    gfx/colortable.cpp
    gfx/colortable.h
    gfx/font.cpp
    gfx/font.h
    gfx/fontmanager.cpp
    gfx/fontmanager.h
    gfx/framebuffer.cpp
    gfx/framebuffer.hpp
    gfx/gl.cpp
    gfx/gl.hpp
    gfx/glbmodel.cpp
    gfx/glbmodel.h
    gfx/image.cpp
    gfx/image.h
    gfx/mesh.cpp
    gfx/mesh.h
    gfx/pipeline.cpp
    gfx/pipeline.h
    gfx/rgb.cpp
    gfx/rgb.h
    gfx/texture.cpp
    gfx/texture.h
    gfx/texturemanager.cpp
    gfx/texturemanager.h
    gfx/video.cpp
    gfx/video.h
    gfx/cliprect.cpp
    gfx/cliprect.h

    hud/cockpit3d.cpp
    hud/cockpit3d.h
    hud/cockpitpaint.cpp
    hud/cockpitpaint.h
    hud/hud.cpp
    hud/hud.h
    hud/crosshair.cpp
    hud/crosshair.h
    hud/heading.cpp
    hud/heading.h
    hud/beta.cpp
    hud/beta.h
    hud/meter.cpp
    hud/meter.h
    hud/digiblock.cpp
    hud/digiblock.h
    hud/radiomessage.cpp
    hud/radiomessage.h
    hud/energybar.cpp
    hud/energybar.h
    hud/shield.cpp
    hud/shield.h
    hud/activesonar.cpp
    hud/alert.cpp
    hud/alert.h
    hud/activesonar.h
    hud/master.cpp
    hud/master.h
    hud/mastermonitor.cpp
    hud/mastermonitor.h
    hud/voxelradar.cpp
    hud/lamp.cpp
    hud/lamp.h
    hud/voxelradar.h
    hud/slavemonitor.cpp
    hud/slavemonitor.h
    hud/sonarmonitor.cpp
    hud/symbols.cpp
    hud/pages.cpp
    hud/symbols.h
    hud/sonarmonitor.h
    hud/autopilot.cpp
    hud/autopilot.h
    hud/debugoverlay.cpp
    hud/debugoverlay.h
    hud/missionmenu.cpp
    hud/missionmenu.h
    hud/touchcontrols.cpp
    hud/touchcontrols.h
    hud/pause.cpp
    hud/pause.h
    hud/horizon.cpp
    hud/horizon.h

    sfx/music.cpp
    sfx/music.hpp
    sfx/openal.hpp
    sfx/sound.cpp
    sfx/sound.h
    sfx/soundsystem.cpp
    sfx/soundsystem.h
    sfx/stream.cpp
    sfx/stream.h
    sfx/samplemap.cpp
    sfx/samplemap.h

    txt/parse.cpp
    txt/parse.hpp
    txt/value.cpp
    txt/value.h
    txt/desfile.cpp
    txt/desfile.h
    txt/stringtable.cpp
    txt/stringtable.h

    ui/arrow.cpp
    ui/arrow.h
    ui/button.cpp
    ui/button.h
    ui/frame.cpp
    ui/frame.h
    ui/itemlist.cpp
    ui/itemlist.h
    ui/label.cpp
    ui/label.h
    ui/list.cpp
    ui/list.h
    ui/widget.cpp
    ui/widget.h

    util/bytereader.hpp
    util/cdimage.cpp
    util/cdimage.hpp
    util/commandline.cpp
    util/commandline.hpp
    util/containers.hpp
    util/core.h
    util/datapath.cpp
    util/datetime.cpp
    util/datetime.hpp
    util/datetimetext.cpp
    util/datetimetext.hpp
    util/datapath.h
    util/deferreddeletable.cpp
    util/deferreddeletable.h
    util/error.cpp
    util/error.h
    util/event.hpp
    util/file.cpp
    util/file.h
    util/json.cpp
    util/json.hpp
    util/path.cpp
    util/path.hpp
    util/random.cpp
    util/random.h
    util/settings.cpp
    util/settings.hpp
    util/rect.hpp
    util/ticks.h
    util/timer.hpp
    util/size.hpp
    util/string.cpp
    util/string.hpp
)

if(IOS)
    # an app bundle; the identifier and the team that signs it for an iPad can be set with
    # -DVERTIGO_BUNDLE_ID=… and -DVERTIGO_DEVELOPMENT_TEAM=… (the simulator needs neither)
    set(VERTIGO_BUNDLE_ID "com.vertigo.Vertigo" CACHE STRING "The app's bundle identifier")
    set(VERTIGO_DEVELOPMENT_TEAM "" CACHE STRING "The team that signs the app for devices")
    set(ASSETS ${PROJECT_SOURCE_DIR}/ios/Assets.xcassets)
    add_executable(vertigo MACOSX_BUNDLE ${SOURCES} ${ASSETS})
    set_source_files_properties(${ASSETS} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    set_target_properties(vertigo PROPERTIES
        OUTPUT_NAME Vertigo
        MACOSX_BUNDLE_INFO_PLIST ${PROJECT_SOURCE_DIR}/ios/Info.plist
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER ${VERTIGO_BUNDLE_ID}
        XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "${VERTIGO_DEVELOPMENT_TEAM}"
        XCODE_ATTRIBUTE_CODE_SIGN_STYLE Automatic
        # iPad only: the game is 4:3
        XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY 2
        XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME AppIcon
        XCODE_ATTRIBUTE_MARKETING_VERSION ${PROJECT_VERSION}
        XCODE_ATTRIBUTE_CURRENT_PROJECT_VERSION 1
        # Xcode runs it optimised: a Debug build draws several times slower (20 instead of 120
        # frames a second on an iPad Pro from 2018)
        XCODE_GENERATE_SCHEME ON
        XCODE_SCHEME_LAUNCH_CONFIGURATION RelWithDebInfo)
    # (Apple deprecated OpenGL ES, but it is in the SDK)
    target_compile_definitions(vertigo PRIVATE GLES_SILENCE_DEPRECATION)
    target_link_libraries(vertigo "-framework OpenGLES")
    # the audio session, in Objective-C++
    target_sources(vertigo PRIVATE sfx/audiosession.hpp sfx/audiosession.mm)
    set_source_files_properties(sfx/audiosession.mm PROPERTIES COMPILE_OPTIONS -fobjc-arc)
    target_link_libraries(vertigo "-framework AVFoundation")
else()
    add_executable(vertigo WIN32 ${SOURCES})
endif()
# the 3D cockpits' kits (data/3D-cockpits/kits), which Vertigo paints from the game's cockpit art:
# next to the program (in the app's resources on iOS), and in share/vertigo/cockpits when installed
file(GLOB VERTIGO_KITS CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/data/3D-cockpits/kits/*.glb)
if(IOS)
    target_sources(vertigo PRIVATE ${VERTIGO_KITS})
    set_source_files_properties(${VERTIGO_KITS} PROPERTIES MACOSX_PACKAGE_LOCATION Resources/cockpits)
elseif(VERTIGO_KITS)
    add_custom_command(TARGET vertigo POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:vertigo>/cockpits
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${VERTIGO_KITS} $<TARGET_FILE_DIR:vertigo>/cockpits/
        VERBATIM)
    install(FILES ${VERTIGO_KITS} DESTINATION share/vertigo/cockpits)
endif()
# the version, for the main menu and --help
target_compile_definitions(vertigo PRIVATE VERTIGO_VERSION="${PROJECT_VERSION}")
target_link_libraries(vertigo SDL3::SDL3 ${OPENAL_LIBRARY})
# the help page shows the version with the last commit and its date (the original shows its own)
set(VERTIGO_COMMIT "")
find_package(Git QUIET)
if(GIT_FOUND AND EXISTS ${PROJECT_SOURCE_DIR}/.git)
    execute_process(COMMAND ${GIT_EXECUTABLE} log -1 --format=%h%x20%cd --date=format:%d/%m/%Y
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        OUTPUT_VARIABLE VERTIGO_COMMIT OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    # configure again after a commit or a branch switch (in a worktree .git is a file naming the
    # worktree's own directory, and the branches live in the common one)
    execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse --absolute-git-dir --path-format=absolute --git-common-dir
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        OUTPUT_VARIABLE VERTIGO_GIT_DIRS OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    string(REPLACE "\n" ";" VERTIGO_GIT_DIRS "${VERTIGO_GIT_DIRS}")
    list(LENGTH VERTIGO_GIT_DIRS VERTIGO_GIT_DIR_COUNT)
    if(VERTIGO_GIT_DIR_COUNT EQUAL 2)
        list(GET VERTIGO_GIT_DIRS 0 VERTIGO_GIT_DIR)
        list(GET VERTIGO_GIT_DIRS 1 VERTIGO_GIT_COMMON_DIR)
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${VERTIGO_GIT_DIR}/HEAD)
        file(READ ${VERTIGO_GIT_DIR}/HEAD VERTIGO_GIT_HEAD)
        if(VERTIGO_GIT_HEAD MATCHES "^ref: (.*)")
            string(STRIP "${CMAKE_MATCH_1}" VERTIGO_GIT_REF)
            if(EXISTS ${VERTIGO_GIT_COMMON_DIR}/${VERTIGO_GIT_REF})
                set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${VERTIGO_GIT_COMMON_DIR}/${VERTIGO_GIT_REF})
            endif()
        endif()
    endif()
endif()
# on the help page alone, so a commit doesn't rebuild everything
set_source_files_properties(hud/pages.cpp PROPERTIES COMPILE_DEFINITIONS VERTIGO_COMMIT="${VERTIGO_COMMIT}")
if(APPLE)
    # Apple deprecated its OpenAL, but it is in the SDKs
    set_source_files_properties(sfx/music.cpp sfx/sound.cpp sfx/soundsystem.cpp sfx/stream.cpp PROPERTIES COMPILE_OPTIONS -Wno-deprecated-declarations)
    # the region's date format for the saved games
    target_link_libraries(vertigo "-framework CoreFoundation")
endif()
if(WIN32)
    # CommandLineToArgvW() for the arguments in UTF-8
    target_link_libraries(vertigo shell32)
    # windows.h without its min and max macros, which break std::min and std::max
    target_compile_definitions(vertigo PRIVATE NOMINMAX)
endif()
# (GLM 1.0 also has a library, which the game doesn't need)
if(TARGET glm::glm-header-only)
    target_link_libraries(vertigo glm::glm-header-only)
elseif(TARGET glm::glm)
    target_link_libraries(vertigo glm::glm)
endif()
if(NOT IOS)
    install(TARGETS vertigo DESTINATION bin)
endif()
