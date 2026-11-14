// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Marvel vs. Capcom: what the game's own sources add to the port (see PROGRESS.md).
#include "alternative.h"
#include "battle.h"
#include "colours.h"
#include "game.h"
#include "marvel.h"
#include "session.h"
#include "tag.h"
#include "mvsc.h"

namespace cps2recomp {

void DescribeEngine(); // engine.cpp
void RegisterCoins(); // coins.cpp
void CoinsFrame(Machine &machine);
void RegisterCheats(); // cheats.cpp
void RegisterTitle(); // title.cpp
void TitleFrame(Machine &machine);
void RegisterBoard(); // board.cpp
void BoardBeforeFrame(Machine &machine);
void BoardState(Archive &ar);
void RegisterMusic(); // music.cpp
void RegisterModes(); // modes.cpp
void ModesState(Archive &ar);
void RegisterTraining(); // training.cpp
void RegisterChoosing(); // choosing.cpp
void RegisterColours(); // colours.cpp
void RegisterBattle(); // battle.cpp
void ChoosingBeforeFrame(Machine &machine);
void RegisterTag(); // tag.cpp
void TagState(Archive &ar);
void RegisterCrossFever(); // crossfever.cpp
void CrossFeverState(Archive &ar);

// 001920, the game's random number: a long divs, then the number kept (001932), as in X-Men vs. Street Fighter
// (001608). The board takes an interrupt between any two instructions, and the frame's interrupt asks for a
// random number of its own: a block starting after the divs (where nothing is done) lets translated code take it where
// the board does, or the game's numbers part from the board's.
static void RandomNumberAsTheBoardDoes() {
    AddHook(HookAt(0x001930), "AfterRandomDivide", [](M68K &) { return false; });
}

void RegisterStages(); // stages.cpp

void RegisterGame() {
    DescribeEngine();
    RandomNumberAsTheBoardDoes();
    RegisterModes();
    RegisterTraining();
    RegisterChoosing();
    // (the model viewer: the camera's way across chosen at 0CB2B2, down at 0CB4F8; the characters are about twice as tall
    // as the Alpha games')
    marvel::RegisterModel(HookAt(0x0CB2B2), 80, HookAt(0x0CB4F8));
    RegisterColours();
    RegisterBattle();
    // (widescreen: the camera kept between its ends at 0CDBB6, a character kept inside the picture at 013AD6)
    marvel::RegisterWidescreen(HookAt(0x0CDBB6), HookAt(0x013AD6));
    RegisterTag();
    RegisterCrossFever();
    RegisterCoins();
    RegisterCheats();
    RegisterTitle();
    RegisterBoard();
    RegisterMusic();
    TheGame().beforeFrame = [](Machine &machine) {
        tag::BeforeFrame(machine);
        BoardBeforeFrame(machine);
        ChoosingBeforeFrame(machine);
        marvel::WidescreenBeforeFrame(machine);
        session::BeforeFrame(machine);
        modes::BeforeFrame(machine);
        BattleBeforeFrame(machine);
        colours::BeforeFrame(machine);
        marvel::ModelBeforeFrame(machine);
    };
    // (the training aids see what the frame did before the cheats put things back)
    TheGame().frame = [](Machine &machine) {
        session::Frame(machine);
        modes::Frame(machine);
        BattleFrame(machine);
        marvel::ModelFrame(machine);
        TitleFrame(machine);
        CoinsFrame(machine);
        tag::Frame(machine);
    };
    // (what the sources keep between frames, with save states and run-ahead's frames taken back)
    TheGame().state = [](Archive &ar) {
        BoardState(ar);
        session::State(ar);
        modes::State(ar);
        ModesState(ar);
        BattleState(ar);
        marvel::ModelState(ar);
        marvel::WidescreenState(ar);
        TagState(ar);
        CrossFeverState(ar);
    };
    // (run-ahead: not in the modes, whose workings a save state does not wholly hold)
    TheGame().noRunAhead = [] { return modes::AttractMode() || modes::StageViewer() || BattleMode() || marvel::ModelViewer(); };
    // With the arcade timing, the game's own code runs in place of the rewrites but where they carry what the port adds
    TheGame().rewritesNeeded = [] {
        return session::InSession() || modes::AttractMode() || modes::StageViewer() || BattleMode() || marvel::ModelViewer() || marvel::WidePlay() ||
               FeatureOn("widescreen");
    };
    // Its own, never set for the other games by the collection's launcher: Damage level has other choices in the Alpha
    // games (Low, Medium, Normal, High); the best scores and the animations are this game's
    OwnFeatures({"board_damage", "board_mode_select", "survival_best", "ringout_best", "model_animation", "colours_animation"});
    // Other games' stages: last, as it goes around the frame's callbacks and the state's (stages.cpp)
    RegisterStages();
}

} // namespace cps2recomp
