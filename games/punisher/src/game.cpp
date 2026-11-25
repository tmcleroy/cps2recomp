// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// The Punisher: what the game's own sources add to the port (see PROGRESS.md): free play, cheats, the stage a game
// starts at, and straight to the title.
#include "punisher.h"
#include "state.h"

namespace cps2recomp {

void PunisherWideFrame(Machine &);
void RegisterPunisherWide();
void RegisterPunisherWideBone();
void RegisterBoard(); // board.cpp
void BoardFrame(Machine &machine);
void RegisterMusic(); // music.cpp
void RegisterModes(); // modes.cpp
bool ChoicesLeftOut();
bool TitleReached();
void ModesBeforeFrame(Machine &machine);
void ModesFrame(Machine &machine);
void ModesState(Archive &ar);
int ViewerStage(); // viewer.cpp
int ViewerCharacter();
void ViewerFrame(Machine &machine);
void ViewerGameBegins();
void ViewerState(Archive &ar);

namespace {
    using namespace punisher;

    // 007C76: a game's start has set its stage
    bool GameBegins(M68K &) {
        if (ViewerStage() >= 0) {
            SetRam8(kStage, static_cast<uint8_t>(ViewerStage()));
            ViewerGameBegins();
            return false;
        }
        const int stage = ChoicesLeftOut() ? 0 : FeatureValue("start_stage");
        if (stage > 0 && stage < kStages) {
            SetRam8(kStage, static_cast<uint8_t>(stage));
        }
        return false;
    }

    // "Straight to the title": the board's start, its version, its warning and the story run unseen. (Kept with the
    // state, as everything that changes while the game runs: online play runs frames again from it.)
    struct Title {
        bool skipping = false, fast = false;
        int32_t frames = 0;
    } g_title;

    void TitleFrame(Machine &machine) {
        // (a mode of the menu started: its start is its own, and so is running unseen)
        if (ChoicesLeftOut()) {
            g_title.skipping = false;
        } else {
            if (machine.frame <= 1 && !g_title.skipping) {
                g_title.skipping = FeatureOn("skip_intro");
                g_title.frames = 0;
            }
            if (g_title.skipping && (TitleReached() || Ram8(kPlayersIn) || ++g_title.frames > 6000)) {
                g_title.skipping = false;
            }
        }
        if (g_title.skipping != g_title.fast) {
            g_title.fast = g_title.skipping;
            SetFastForward(g_title.fast);
        }
    }

    void FreePlay() {
        if (FeatureOn("free_play") && !ChoicesLeftOut()) {
            SetRam8(kFreePlay, 1);
            return;
        }
        // (off: the game's own, from the coin mode, as its coin set-up has it)
        const uint8_t own = Ram8(kCoinMode) >= kCoinModeFreePlay ? 1 : 0;
        if ((Ram8(kFreePlay) != 0) != (own != 0)) {
            SetRam8(kFreePlay, own);
        }
    }

    void Cheats() {
        if (!Ram8(kPlayersIn) || Ram16(kStep) != kStepPlay) {
            return; // (not at a stage's end, whose results count the health and time left)
        }
        for (int n = 0; n < kPlayers; n++) {
            const uint32_t p = Player(n);
            if (Ram8(p + kInPlay) != 1) {
                continue;
            }
            const uint16_t health = Ram16(p + kHealth);
            if (FeatureOn("infinite_health") && health != 0 && health < kHealthFull) {
                SetRam16(p + kHealth, kHealthFull);
            }
            if (FeatureOn("infinite_lives") && Ram8(kLives + n) < 2) {
                SetRam8(kLives + n, 2);
            }
        }
        if (FeatureOn("infinite_time")) {
            SetRam16(kClockFrames, kClockTick); // (it never runs out, so the clock stands still)
        }
    }

    void BeforeFrame(Machine &machine) {
        ModesBeforeFrame(machine);
        PunisherWideFrame(machine);
    }

    void Frame(Machine &machine) {
        BoardFrame(machine);
        ModesFrame(machine);
        ViewerFrame(machine);
        FreePlay();
        if (!ChoicesLeftOut()) {
            Cheats(); // (free play and the cheats are not the board's own game)
        }
        TitleFrame(machine);
    }

    void State(Archive &ar) {
        ModesState(ar);
        ar.Mark(0x50554E49); // PUNI
        ar.Pod(g_title);
        ViewerState(ar);
    }
} // namespace

void RegisterGame() {
    Game &game = TheGame();
    RegisterPunisherWide();
    RegisterPunisherWideBone();
    RegisterMusic();
    RegisterBoard();
    RegisterModes();
    game.features.push_back({"free_play", "Game", "Free play", "Start works without a coin (the board's own free play).", kFeatureEnhanced});
    game.features.push_back({"skip_intro", "Game", "Straight to the title",
                             "When the game starts, and when it is reset, goes straight to the title: the board's start, its version, "
                             "its warning and the story are run unseen.",
                             0});
    game.features.push_back({"start_stage", "Game", "Start at stage", "A new game starts at the beginning of the stage chosen.", 0,
                             {"Stage 1: Back to the War", "Stage 2: Raid on Pantaberde", "Stage 3: Waterfront Warfare",
                              "Stage 4: Death on Rails", "Stage 5: Kingpin's Wrath", "Stage 6: Final Punishment"}});
    game.features.push_back({"infinite_health", "Cheats", "Infinite health", "No player's health goes down."});
    game.features.push_back({"infinite_lives", "Cheats", "Infinite lives", "A player who goes down always has another life."});
    game.features.push_back({"infinite_time", "Cheats", "Infinite time", "The stage's clock stands still."});
    AddHook(HookAt(kGameBegins), "GameBegins", GameBegins);
    game.beforeFrame = BeforeFrame;
    game.frame = Frame;
    game.state = State;
    OwnFeatures({"start_stage"});
}

} // namespace cps2recomp
