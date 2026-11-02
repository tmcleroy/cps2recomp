# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of recomp-common: https://github.com/tmcleroy/recomp-common
#
# The shared launcher with no game: a port's launcher/CMakeLists.txt looks like this. `example` stands in
# for the game program (it only lists some features), so the launcher can be built and its pages pictured
# anywhere:
#   cmake -S launcher/example -B build/launcher && cmake --build build/launcher
#   SDL_VIDEO_DRIVER=dummy SDL_RENDER_DRIVER=software EXAMPLE_HOME=build/home build/launcher/example-launcher --screenshot build/page
cmake_minimum_required(VERSION 3.20)
project(recomp_common_launcher_example CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(LAUNCHER_GAME_NAME "Example Game")
set(LAUNCHER_GAME_EXE example)
set(LAUNCHER_APP_ID org.example.game)
set(LAUNCHER_HOME_ENV EXAMPLE_HOME)
set(LAUNCHER_DISCLAIMER "An example with no game.")
set(LAUNCHER_PORT_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/description.cpp)
# (updates from a repository's releases: this one's has none for the example, so it is always the newest)
set(LAUNCHER_UPDATE_REPO tmcleroy/recomp-common)
set(LAUNCHER_VERSION 0.0.0)
include(../launcher.cmake)

# The stand-in for the game program, beside the launcher
add_executable(example game.cpp ${RECOMP_COMMON_SRC}/settings.cpp)
target_include_directories(example PRIVATE ${RECOMP_COMMON_SRC})

# A vendored copy contains src/ and launcher/, not this repository's tests/.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../tests/player_bindings_test.cpp")
    enable_testing()
    foreach(test player_bindings window_layout)
        add_executable(${test}_test "${CMAKE_CURRENT_SOURCE_DIR}/../../tests/${test}_test.cpp")
        target_link_libraries(${test}_test PRIVATE SDL3::SDL3)
        add_test(NAME ${test} COMMAND ${test}_test)
    endforeach()
endif()
