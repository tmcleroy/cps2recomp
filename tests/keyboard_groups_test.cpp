// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Input Lab contributors
#include "../port/keyboards.h"
#include <cassert>
#include <cstdio>
#include <filesystem>
int main() {
    using namespace port;
    KeyboardState state;
    state.Connect("interface-A", "Same model", "physical-A");
    state.Connect("aux-A", "Same model", "physical-A");
    state.Connect("interface-B", "Same model", "physical-B");
    state.Connect("mouse", "Mouse keyboard interface", "physical-mouse");
    KeyboardConfig config{true,false,{"physical-A","physical-B"}};
    state.Focus(true);
    for (int n = 1; n < SDL_SCANCODE_COUNT; ++n) {
        const auto key=static_cast<SDL_Scancode>(n);
        state.Key("interface-A",key,true); state.Key("interface-B",key,true);
        assert(state.ForPlayer(config,0)[n] && state.ForPlayer(config,1)[n]);
        state.Key("interface-A",key,false);
        assert(!state.ForPlayer(config,0)[n] && state.ForPlayer(config,1)[n]);
        state.Key("interface-B",key,false);
        state.Key("interface-A",key,true); state.Key("aux-A",key,true);
        state.Key("interface-A",key,false);
        assert(state.ForPlayer(config,0)[n] && !state.ForPlayer(config,1)[n]);
        state.Key("aux-A",key,false);
        state.Key("mouse",key,true);
        assert(!state.ForPlayer(config,0)[n] && !state.ForPlayer(config,1)[n]);
        state.Key("mouse",key,false);
    }
    for (auto key : {SDL_SCANCODE_S,SDL_SCANCODE_D,SDL_SCANCODE_U}) {
        state.Key("interface-A",key,true); state.Key("interface-B",key,true);
    }
    state.Disconnect("interface-A");
    assert(state.ForPlayer(config,1)[SDL_SCANCODE_S]);
    state.Connect("interface-A","Same model","physical-A");
    assert(state.Interfaces("physical-A")==2 && state.Interfaces("physical-B")==1);
    assert(!AssignKeyboardDevice(config,1,"physical-A"));
    const auto path=(std::filesystem::temp_directory_path()/"recomp-keyboard-group-test.ini").string();
    assert(SaveKeyboards(path,config));
    const auto loaded=LoadKeyboards(path);
    assert(loaded.device==config.device && loaded.enabled && !loaded.mirrorP1);
    std::filesystem::remove(path);
    std::puts("Keyboard groups: all scan codes, cross releases, auxiliary OR, mouse exclusion, reconnect, persistence passed");
}
