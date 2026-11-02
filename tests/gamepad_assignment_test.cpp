// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Input Lab contributors
#include "../port/gamepad_assignment.h"
#include <cassert>
#include <cstdio>
#include <filesystem>
int main() {
    using namespace port;
    GamepadAssignments config;
    AssignGamepad(config,1,"pad-A");
    auto result=ResolveGamepadAssignments(config,{{11,"pad-A","Identical model"}});
    assert(result[0]==0 && result[1]==11);
    result=ResolveGamepadAssignments(config,{{22,"pad-B","Identical model"},{11,"pad-A","Identical model"}});
    assert(result[0]==22 && result[1]==11);
    result=ResolveGamepadAssignments(config,{{11,"pad-A","Identical model"},{22,"pad-B","Identical model"}});
    assert(result[0]==22 && result[1]==11);
    AssignGamepad(config,0,"pad-A"); assert(config.device[1]=="none");
    result=ResolveGamepadAssignments(config,{{11,"pad-A","Same"},{12,"pad-A","Same"}});
    assert(result[0]==0); // ambiguous stable identity is never guessed
    const auto path=(std::filesystem::temp_directory_path()/"recomp-gamepad-assignment-test.ini").string();
    assert(SaveGamepadAssignments(path,config));
    assert(LoadGamepadAssignments(path).device==config.device); std::filesystem::remove(path);
    const auto axis=recomp::PadFromText("leftx-");
    assert(axis.axis==SDL_GAMEPAD_AXIS_LEFTX && axis.direction==-1 && recomp::PadText(axis)=="leftx-");
    std::puts("Gamepad assignments: reserved P2, enumeration reorder, ambiguity, reassignment and persistence passed");
}
