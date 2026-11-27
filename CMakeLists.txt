# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.

cmake_minimum_required(VERSION 3.28)
project(cps2recomp LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif ()

# The release's version (VERSION): online play's two copies must have the same (port/netplay.cpp)
file(STRINGS "${CMAKE_SOURCE_DIR}/VERSION" RECOMP_VERSION LIMIT_COUNT 1)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/VERSION")
# The commit it was built from, for the logs (a test build has the release's VERSION): RECOMP_COMMIT, or git's
set(RECOMP_COMMIT "$ENV{RECOMP_COMMIT}")
if (RECOMP_COMMIT STREQUAL "")
    execute_process(COMMAND git describe --always --dirty --abbrev=8 --match=NONE WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
                    OUTPUT_VARIABLE RECOMP_COMMIT OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
endif ()
if (RECOMP_COMMIT STREQUAL "")
    set(RECOMP_COMMIT "unknown")
endif ()

# Game selection: the game id names a directory under games/ and the generated code under work/
set(GAME "vampire_savior" CACHE STRING "Game id (directory under games/)")
set(GAME_DIR "${CMAKE_SOURCE_DIR}/../games/${GAME}" CACHE PATH "The game's directory (game.cfg, src/)")
set(WORK_DIR "${CMAKE_SOURCE_DIR}/../work/${GAME}" CACHE PATH "Generated code directory")

# (macOS: 3.4.16 or later. 3.4.14's Metal GPU device takes a frame's fence still being worked on for one signalled, so
# after the first frames every picture is skipped and the window stays black while the game runs and sounds)
if (APPLE)
    find_package(SDL3 3.4.16 REQUIRED CONFIG)
else ()
    find_package(SDL3 REQUIRED CONFIG)
endif ()
find_package(ZLIB REQUIRED)

# Translated game code: work/<game>/gen/native_*.cpp, written by tools/recomp.py.
# Without it the game still builds and runs everything in the interpreter.
file(GLOB GEN_SOURCES CONFIGURE_DEPENDS "${WORK_DIR}/gen/native_*.cpp")
if (GEN_SOURCES)
    message(STATUS "cps2recomp: ${GAME}: translated code found in ${WORK_DIR}/gen")
else ()
    message(STATUS "cps2recomp: ${GAME}: no translated code yet (interpreter only)")
    set(GEN_SOURCES runtime/no_blocks.cpp)
endif ()

# The sound driver translated: work/<game>/gen/z80_*.cpp, written by tools/recompz80.py
file(GLOB GEN_Z80_SOURCES CONFIGURE_DEPENDS "${WORK_DIR}/gen/z80_*.cpp")
if (NOT GEN_Z80_SOURCES)
    set(GEN_Z80_SOURCES runtime/no_z80_blocks.cpp)
endif ()

set(MACHINE_SOURCES
    video/video.cpp video/native.cpp video/native_render.cpp
    sound/z80.cpp sound/qsound.cpp sound/driver.cpp sound/okim6295.cpp third_party/ymfm_opm.cpp
    machine/m68k.cpp machine/machine.cpp machine/cps1_sound.cpp machine/sound_board.cpp machine/soundtrack.cpp machine/eeprom.cpp machine/romset.cpp machine/cps2crypt.cpp machine/kabuki.cpp
    runtime/recomp.cpp runtime/recomp_z80.cpp
)
# The game's own sources (features, routines replaced by hand-written C++); without any, nothing is added
file(GLOB GAME_SOURCES CONFIGURE_DEPENDS "${GAME_DIR}/src/*.cpp")
# ... and those of the games it shares sources with (game.cfg's "sources": another game made from the same program,
# whose sources name its places as At(0x...); the game's own family.h, in its folder, says where they are in it)
file(STRINGS "${GAME_DIR}/game.cfg" GAME_SHARES REGEX "^sources[ \t]*=")
if (GAME_SHARES)
    string(REGEX REPLACE "^sources[ \t]*=[ \t]*" "" GAME_SHARES "${GAME_SHARES}")
    separate_arguments(GAME_SHARES)
    foreach (other ${GAME_SHARES})
        file(GLOB shared CONFIGURE_DEPENDS "${GAME_DIR}/../${other}/src/*.cpp")
        list(APPEND GAME_SOURCES ${shared})
        list(APPEND GAME_INCLUDES "${GAME_DIR}/../${other}/src") # (for the game's own sources, which use them)
        message(STATUS "cps2recomp: ${GAME}: the sources of ${other} too")
    endforeach ()
endif ()
if (NOT GAME_SOURCES)
    set(GAME_SOURCES runtime/no_game.cpp)
endif ()
# (recomp-common/: the code the recomp ports share, a copy of recomp-common's; tools/sync-common.sh)
set(PORT_SOURCES port/main.cpp port/movie.cpp port/scene.cpp port/video_record.cpp port/gpu_picture.cpp port/frontend_sdl.cpp recomp-common/arcade_screen.cpp port/present.cpp port/shading.cpp port/zoom.cpp port/menu.cpp port/hud.cpp port/touch.cpp port/soundtrack.cpp port/netplay.cpp recomp-common/launcher/src/online.cpp recomp-common/launcher/src/run_program.cpp port/decoders.cpp recomp-common/menu.cpp recomp-common/settings.cpp runtime/game.cpp runtime/board.cpp video/lifts.cpp runtime/spare.cpp runtime/guest.cpp fight/frames.cpp fight/training.cpp fight/colours.cpp fight/colour_editor.cpp fight/battle.cpp fight/court.cpp fight/alternative.cpp fight/session.cpp fight/tag.cpp fight/marvel.cpp fight/guest_stages.cpp beat/modes.cpp beat/model.cpp model/model.cpp shmup/modes.cpp ${GAME_SOURCES})

# (the sound files' decoders, third_party/, are other projects' code, compiled as they are)
set_source_files_properties(port/decoders.cpp PROPERTIES COMPILE_OPTIONS "-w")
# (ymfm's published YM2151 implementation for the classic CPS-1 sound board)
set_source_files_properties(third_party/ymfm_opm.cpp PROPERTIES COMPILE_OPTIONS "-w")

# The video recording's encoders (port/video_record.h), other projects' code compiled as it is: minih264 (H.264) and
# minimp4 (the MP4 file), CC0, in third_party/ and compiled by port/encoders*.c; and FAAC 2.2 (AAC), LGPL-2.1,
# downloaded (its release's archive, checked against its SHA-256) and compiled with port/faac_config.h: its plain C
# quantizer only, the SSE2 one left out, so that it is the same everywhere
include(FetchContent)
FetchContent_Declare(faac
    URL https://github.com/knik0/faac/archive/refs/tags/faac-2.2.tar.gz
    URL_HASH SHA256=a93963573907c83e26e8cfabbf80d3a9c360f06ea4ecf1ea6cb74a202494d8d9
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(faac) # (its sources only: it has no CMakeLists.txt, and its own build, meson's, is not used)
file(GLOB FAAC_SOURCES "${faac_SOURCE_DIR}/libfaac/*.c")
list(REMOVE_ITEM FAAC_SOURCES "${faac_SOURCE_DIR}/libfaac/quantize_sse.c")
add_library(video_encoders STATIC port/encoders.c port/encoders_mp4.c ${FAAC_SOURCES})
target_include_directories(video_encoders PUBLIC third_party "${faac_SOURCE_DIR}/include" PRIVATE "${faac_SOURCE_DIR}/libfaac")
# (minih264's colour description on, and its scalable video off: minih264e.h)
target_compile_definitions(video_encoders PRIVATE H264E_VUI_BT709=1 H264E_SVC_API=0 NDEBUG)
target_compile_options(video_encoders PRIVATE -w -include "${CMAKE_CURRENT_SOURCE_DIR}/port/faac_config.h")
set_target_properties(video_encoders PROPERTIES POSITION_INDEPENDENT_CODE ON)
if (UNIX AND NOT APPLE)
    target_link_libraries(video_encoders PRIVATE m)
endif ()

# Switchres (GPL-2.0-or-later, Antonio Giner and others), the modes made for an arcade monitor at the board's exact rate
# (recomp-common/arcade_screen.h): downloaded and linked in statically on Windows by recomp-common's switchres.cmake
include(recomp-common/switchres.cmake)

# (macOS: handing the front to the launcher, port/front_macos.mm)
if (APPLE)
    enable_language(OBJCXX)
    list(APPEND PORT_SOURCES port/front_macos.mm)
endif ()

# The game on cps2recomp's machine (machine/); classic CPS-1 uses the credited ymfm YM2151 library.
# (Android: a library, which the game's activity loads and calls the main of: android/, tools/release/android.sh)
if (ANDROID)
    list(APPEND PORT_SOURCES port/android.cpp)
    add_library(${GAME} SHARED ${PORT_SOURCES} ${MACHINE_SOURCES} ${GEN_SOURCES} ${GEN_Z80_SOURCES})
else ()
    add_executable(${GAME} ${PORT_SOURCES} ${MACHINE_SOURCES} ${GEN_SOURCES} ${GEN_Z80_SOURCES})
endif ()
# The check build ('make check-blocks'): the same, with every translated block run beside the interpreter
# and compared (runtime/recomp.cpp)
add_executable(${GAME}-check EXCLUDE_FROM_ALL ${PORT_SOURCES} ${MACHINE_SOURCES} ${GEN_SOURCES} ${GEN_Z80_SOURCES})
target_compile_definitions(${GAME}-check PRIVATE CPS2RECOMP_CHECK)
# (on Windows the game is a windowed program: no console window opens beside it, whether the launcher starts it
# or a double click does)
if (WIN32)
    set_target_properties(${GAME} PROPERTIES WIN32_EXECUTABLE ON)
    # (what the program is, whose and which version, and its manifest: port/windows/)
    include(port/windows/resources.cmake)
    file(STRINGS "${GAME_DIR}/game.cfg" GAME_NAME_LINE REGEX "^name *=" LIMIT_COUNT 1)
    string(REGEX REPLACE "^name *= *" "" GAME_FULL_NAME "${GAME_NAME_LINE}")
    recomp_windows_resources(${GAME} ${GAME} "${GAME_FULL_NAME}" "${RECOMP_VERSION}")
endif ()
foreach (target ${GAME} ${GAME}-check)
    target_include_directories(${target} PRIVATE runtime port machine fight beat model shmup "${WORK_DIR}/gen" "${GAME_DIR}" ${GAME_INCLUDES})
    target_link_libraries(${target} PRIVATE SDL3::SDL3 ZLIB::ZLIB video_encoders)
    if (APPLE)
        target_link_libraries(${target} PRIVATE "-framework AppKit")
    endif ()
    if (ANDROID)
        target_link_libraries(${target} PRIVATE log)
    endif ()
    if (WIN32)
        target_link_libraries(${target} PRIVATE ws2_32 iphlpapi) # (online play's socket, and this machine's addresses)
        # (online play's requests made with WinHTTP, not a hidden curl.exe, which antivirus programs take for a
        # downloader: recomp-common/launcher/src/winhttp_curl.h)
        target_link_libraries(${target} PRIVATE winhttp)
        target_compile_definitions(${target} PRIVATE RECOMP_WINHTTP=1)
    endif ()
    recomp_switchres(${target})
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wno-unused-parameter -Wno-unused-label -Wno-unused-variable -Wno-parentheses-equality)
    # (a char is signed, as on x86 and Apple's ARM, also on Linux ARM64, whose own is unsigned: every build does the
    # same sums)
    target_compile_options(${target} PRIVATE -fsigned-char)
    target_compile_definitions(${target} PRIVATE RECOMP_VERSION="${RECOMP_VERSION}" RECOMP_COMMIT="${RECOMP_COMMIT}")
endforeach ()
