# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Input Lab contributors
# Compile the unchanged, game-independent runtime once for collection builds.
cmake_minimum_required(VERSION 3.28)
project(cps2_cheat_test_core LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(SDL3 REQUIRED CONFIG)
find_package(ZLIB REQUIRED)
get_filename_component(CPS2_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
add_library(cps2_cheat_test_core STATIC
    "${CPS2_ROOT}/video/video.cpp"
    "${CPS2_ROOT}/sound/z80.cpp"
    "${CPS2_ROOT}/sound/qsound.cpp"
    "${CPS2_ROOT}/sound/driver.cpp"
    "${CPS2_ROOT}/machine/m68k.cpp"
    "${CPS2_ROOT}/machine/machine.cpp"
    "${CPS2_ROOT}/machine/sound_board.cpp"
    "${CPS2_ROOT}/machine/soundtrack.cpp"
    "${CPS2_ROOT}/machine/eeprom.cpp"
    "${CPS2_ROOT}/machine/romset.cpp"
    "${CPS2_ROOT}/machine/cps2crypt.cpp"
    "${CPS2_ROOT}/machine/kabuki.cpp"
    "${CPS2_ROOT}/runtime/recomp.cpp"
    "${CPS2_ROOT}/runtime/recomp_z80.cpp"
    "${CPS2_ROOT}/port/present.cpp"
    "${CPS2_ROOT}/port/shading.cpp"
    "${CPS2_ROOT}/port/zoom.cpp"
    "${CPS2_ROOT}/port/menu.cpp"
    "${CPS2_ROOT}/port/hud.cpp"
    "${CPS2_ROOT}/port/soundtrack.cpp"
    "${CPS2_ROOT}/recomp-common/launcher/src/online.cpp"
    "${CPS2_ROOT}/recomp-common/launcher/src/run_program.cpp"
    "${CPS2_ROOT}/port/decoders.cpp"
    "${CPS2_ROOT}/recomp-common/menu.cpp"
    "${CPS2_ROOT}/recomp-common/settings.cpp"
    "${CPS2_ROOT}/runtime/game.cpp"
    "${CPS2_ROOT}/runtime/board.cpp"
    "${CPS2_ROOT}/runtime/spare.cpp"
    "${CPS2_ROOT}/fight/frames.cpp"
    "${CPS2_ROOT}/fight/training.cpp"
    "${CPS2_ROOT}/fight/colours.cpp"
    "${CPS2_ROOT}/fight/colour_editor.cpp"
    "${CPS2_ROOT}/fight/battle.cpp"
    "${CPS2_ROOT}/fight/alternative.cpp"
    "${CPS2_ROOT}/fight/session.cpp"
    "${CPS2_ROOT}/fight/tag.cpp"
    "${CPS2_ROOT}/fight/marvel.cpp"
    "${CPS2_ROOT}/beat/modes.cpp"
    "${CPS2_ROOT}/beat/model.cpp"
    "${CPS2_ROOT}/model/model.cpp"
)
target_include_directories(cps2_cheat_test_core PRIVATE "${CPS2_ROOT}/runtime" "${CPS2_ROOT}/port" "${CPS2_ROOT}/machine" "${CPS2_ROOT}/fight" "${CPS2_ROOT}/beat" "${CPS2_ROOT}/model")
target_link_libraries(cps2_cheat_test_core PRIVATE SDL3::SDL3 ZLIB::ZLIB)
target_compile_options(cps2_cheat_test_core PRIVATE -fsigned-char -Wall -Wextra -Wno-unused-parameter -Wno-unused-label -Wno-unused-variable -Wno-parentheses-equality)


foreach(game sfa sfz2al ssf2t)
    if(game STREQUAL "ssf2t")
        set(hook "${CPS2_ROOT}/games/${game}/src/inputlab_cheats.cpp")
    else()
        set(hook "${CPS2_ROOT}/games/${game}/src/inputlab_damage.cpp")
    endif()
    add_executable(${game}_policy_tests damage_port_tests.cpp ${hook}
        "${CPS2_ROOT}/runtime/no_blocks.cpp" "${CPS2_ROOT}/runtime/no_z80_blocks.cpp")
    string(TOUPPER "${game}" game_define)
    target_compile_definitions(${game}_policy_tests PRIVATE "LAB_${game_define}")
    target_include_directories(${game}_policy_tests PRIVATE "${CPS2_ROOT}/runtime" "${CPS2_ROOT}/machine"
        "${CPS2_ROOT}/games/${game}" "${CPS2_ROOT}/games/sfa2/src" "${CPS2_ROOT}/fight")
    target_link_libraries(${game}_policy_tests PRIVATE cps2_cheat_test_core SDL3::SDL3 ZLIB::ZLIB)
endforeach()
add_executable(sfa2_damage_tests "${CPS2_ROOT}/games/sfa2/tests/damage_tests.cpp"
    "${CPS2_ROOT}/games/sfa2/src/damage.cpp" "${CPS2_ROOT}/runtime/no_blocks.cpp" "${CPS2_ROOT}/runtime/no_z80_blocks.cpp")
target_compile_definitions(sfa2_damage_tests PRIVATE INPUTLAB_SFA2)
target_include_directories(sfa2_damage_tests PRIVATE "${CPS2_ROOT}/runtime" "${CPS2_ROOT}/port" "${CPS2_ROOT}/machine"
    "${CPS2_ROOT}/fight" "${CPS2_ROOT}/games/sfa2")
target_link_libraries(sfa2_damage_tests PRIVATE cps2_cheat_test_core SDL3::SDL3 ZLIB::ZLIB)
add_executable(ssf2t_colour_tests ssf2t_colour_tests.cpp "${CPS2_ROOT}/games/ssf2t/src/inputlab_cheats.cpp"
    "${CPS2_ROOT}/runtime/no_blocks.cpp" "${CPS2_ROOT}/runtime/no_z80_blocks.cpp")
target_include_directories(ssf2t_colour_tests PRIVATE "${CPS2_ROOT}/runtime" "${CPS2_ROOT}/machine"
    "${CPS2_ROOT}/games/ssf2t/src")
target_link_libraries(ssf2t_colour_tests PRIVATE cps2_cheat_test_core SDL3::SDL3 ZLIB::ZLIB)
if(WIN32)
    foreach(target sfa_policy_tests sfz2al_policy_tests ssf2t_policy_tests sfa2_damage_tests ssf2t_colour_tests)
        target_link_libraries(${target} PRIVATE ws2_32 iphlpapi)
    endforeach()
endif()
