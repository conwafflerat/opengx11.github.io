cmake_minimum_required(VERSION 3.18)
project(sonic2 CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# ---- SDL2 ---------------------------------------------------------------
find_package(SDL2 QUIET)
if(NOT SDL2_FOUND)
    FetchContent_Declare(SDL2
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-2.30.3
        GIT_SHALLOW TRUE
    )
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON  CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(SDL2)
endif()

# ---- Nuked-OPN2 (YM2612 FM synthesis) ----------------------------------
FetchContent_Declare(nuked_opn2
    GIT_REPOSITORY https://github.com/nukeykt/Nuked-OPN2.git
    GIT_TAG master
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(nuked_opn2)
add_library(nuked_opn2_lib STATIC ${nuked_opn2_SOURCE_DIR}/ym3438.c)
target_include_directories(nuked_opn2_lib PUBLIC ${nuked_opn2_SOURCE_DIR})

# ---- Sources ------------------------------------------------------------
set(SOURCES
    src/main.cpp
    src/hardware/vdp.cpp
    src/hardware/sound.cpp
    src/hardware/input.cpp
    src/assets/rom.cpp
    src/game/globals.cpp
    src/game/object.cpp
    src/game/sonic.cpp
    src/game/level.cpp
    src/game/main_loop.cpp
)

# ---- Platform-specific executable type ---------------------------------
if(ANDROID)
    # Android: build a shared library (.so) loaded by the SDL2 Java glue
    add_library(sonic2 SHARED ${SOURCES})
elseif(IOS)
    # iOS: build an app bundle
    add_executable(sonic2 MACOSX_BUNDLE ${SOURCES})
    set_target_properties(sonic2 PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/platform/ios/Info.plist
        XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET "13.0"
        XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"  # iPhone + iPad
    )
else()
    add_executable(sonic2 ${SOURCES})
endif()

target_include_directories(sonic2 PRIVATE src)

# ---- Link libraries -----------------------------------------------------
if(SDL2_FOUND)
    target_link_libraries(sonic2 PRIVATE SDL2::SDL2 SDL2::SDL2main nuked_opn2_lib)
else()
    target_link_libraries(sonic2 PRIVATE SDL2-static nuked_opn2_lib)
endif()

# ---- Windows: keep console window for error output ----------------------
if(WIN32)
    set_target_properties(sonic2 PROPERTIES WIN32_EXECUTABLE FALSE)
endif()

# ---- macOS: set minimum deployment target and bundle metadata -----------
if(APPLE AND NOT IOS)
    set_target_properties(sonic2 PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "Sonic the Hedgehog 2"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.yourname.sonic2"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0"
        XCODE_ATTRIBUTE_MACOSX_DEPLOYMENT_TARGET "11.0"
    )
    target_link_libraries(sonic2 PRIVATE "-framework CoreAudio" "-framework AudioToolbox")
endif()

# ---- Android: link SDL2 activity + OpenGL ES ---------------------------
if(ANDROID)
    find_library(ANDROID_LOG_LIB log)
    find_library(ANDROID_LIB android)
    find_library(GLES_LIB GLESv1_CM)
    target_link_libraries(sonic2 PRIVATE
        ${ANDROID_LOG_LIB} ${ANDROID_LIB} ${GLES_LIB}
    )
endif()

# ---- Copy ROM next to the binary if present at configure time -----------
if(EXISTS ${CMAKE_SOURCE_DIR}/sonic2.md)
    configure_file(${CMAKE_SOURCE_DIR}/sonic2.md ${CMAKE_BINARY_DIR}/sonic2.md COPYONLY)
endif()
