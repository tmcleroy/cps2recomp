// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Street Fighter Alpha 3: what the game's own sources add to the port (see PROGRESS.md).
#include "alternative.h"
#include "battle.h"
#include "colours.h"
#include "game.h"
#include "machine.h"
#include "session.h"

#include <cstdlib>

namespace cps2recomp {

void RegisterCoins(); // coins.cpp
void CoinsFrame(Machine &machine);
void CoinsState(Archive &ar);
void RegisterTitle(); // title.cpp
void TitleFrame(Machine &machine);
void RegisterBoard(); // board.cpp
void BoardBeforeFrame(Machine &machine);
void BoardState(Archive &ar);
void RegisterMusic();    // music.cpp
void RegisterCheats();   // cheats.cpp
void CheatsFrame(Machine &machine);
void RegisterModes();    // modes.cpp
void RegisterTraining(); // training.cpp
void RegisterCourt();    // court.cpp (before the battle's: its page is among theirs)
void RegisterBattle();   // battle.cpp
void RegisterChoosing(); // choosing.cpp
void ChoosingBeforeFrame(Machine &machine);
void RegisterWidescreen(); // widescreen.cpp
void RegisterComputer();   // computer.cpp
void WidescreenBeforeFrame(Machine &machine);
void WidescreenState(Archive &ar);
bool WidePlay();
void RegisterModel(); // model.cpp
void ModelBeforeFrame(Machine &machine);
void ModelFrame(Machine &machine);
void ModelState(Archive &ar);
bool ModelViewer();
void RegisterColours(); // colours.cpp
void LiveTagState(Archive &ar);
void RegisterLiveTag(); // livetag.cpp
void RegisterStages();  // stages.cpp
void LiveTagBeforeFrame(Machine &machine);

void RegisterGame() {
    RegisterModes();
    RegisterTraining();
    RegisterCourt();
    RegisterBattle();
    RegisterLiveTag();
    RegisterModel();
    RegisterColours();
    RegisterChoosing();
    RegisterWidescreen();
    RegisterComputer();
    RegisterCoins();
    RegisterTitle();
    RegisterBoard();
    RegisterMusic();
    RegisterCheats();
    TheGame().beforeFrame = [](Machine &machine) {
        BoardBeforeFrame(machine);
        ChoosingBeforeFrame(machine);
        WidescreenBeforeFrame(machine);
        session::BeforeFrame(machine);
        modes::BeforeFrame(machine);
        BattleBeforeFrame(machine);
        LiveTagBeforeFrame(machine);
        colours::BeforeFrame(machine);
        ModelBeforeFrame(machine);
        // (for finding what the objects are, SFA3_HIDE_SETS=MASK: the objects of those colour sets not drawn)
        static const char *hide = getenv("SFA3_HIDE_SETS");
        if (hide) {
            static const uint32_t mask = static_cast<uint32_t>(strtoul(hide, nullptr, 0));
            machine.video.hideObject = [](uint32_t, uint16_t attributes) { return ((mask >> (attributes & 0x1F)) & 1) != 0; };
        }
    };
    // (the training aids see what the frame did before the cheats put things back)
    TheGame().frame = [](Machine &machine) {
        session::Frame(machine);
        modes::Frame(machine);
        BattleFrame(machine);
        ModelFrame(machine);
        TitleFrame(machine);
        CoinsFrame(machine);
        CheatsFrame(machine);
    };
    // (run-ahead: not in the modes, whose workings a save state does not wholly hold)
    TheGame().noRunAhead = [] { return modes::AttractMode() || modes::StageViewer() || BattleMode() || ModelViewer(); };
    // (what the sources keep between frames, with save states and run-ahead's frames taken back)
    TheGame().state = [](Archive &ar) {
        CoinsState(ar);
        BoardState(ar);
        session::State(ar);
        modes::State(ar);
        BattleState(ar);
        ModelState(ar);
        WidescreenState(ar);
        LiveTagState(ar);
    };
    // With the arcade timing, the game's own code runs in place of the rewrites but where they carry what the port adds
    // (game.h, rewritesNeeded): the modes, widescreen
    TheGame().rewritesNeeded = [] {
        return session::InSession() || modes::AttractMode() || modes::StageViewer() || BattleMode() || ModelViewer() || WidePlay() ||
               FeatureOn("widescreen");
    };
    // Its own, never set for the other games by the collection's launcher: Damage level has Super Puzzle Fighter II
    // Turbo's choices (Low, Normal, High, Maximum), where it means the counter gems an attack sends
    // The training aids of this game's own work only in a training session (the toolkit's own are so already)
    TrainingAids({"stun_readout", "custom_combo_readout", "throw_range", "fixed_damage", "dummy_stun_mash", "dummy_counter_hit", "dummy_computer"});
    OwnFeatures({"board_damage", "survival_best", "ringout_best", "model_animation", "colours_animation"});
    // Other games' stages: last, as it goes around the frame's callbacks and the state's (stages.cpp)
    RegisterStages();
}

} // namespace cps2recomp
