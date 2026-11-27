// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Super Street Fighter II Turbo: the fighting modes of the menu's "Alternative modes" page (survival, the endless battle,
// the ring out) are the toolkit's (fight/battle.h); this says where the game keeps what they need, and makes the ring
// out's own movements of a player at its step of a fight.
#include "alternative.h"
#include "battle.h"
#include "ssf2t.h"

#include <cstdlib>
#include <iterator>

namespace cps2recomp {

bool CanTag(int player); // livetag.cpp
void TagIn(int player, int character, int colours);
bool TaggedIn(int player);
void TagDifficulty(int level);
int TagDifficultyNow();
void TagOffStage(int player, bool off);
void TagUntouchable(int player);

namespace {
    using namespace ssf2t;

    constexpr uint32_t kAnimate = At(0x002EE2); // the animation on by a step (the frame's time down, then the next frame)
    constexpr uint32_t kFly = At(0x002F50);     // a step through the air: the speeds by their pulls, the place by the speeds
    // Words, 256ths of a pixel a step: the speeds across and up, and what is taken off them each step (a jump's: 002F46)
    constexpr uint32_t kSpeedX = 0x3C, kSpeedY = 0x3E, kPullX = 0x40, kPullY = 0x42;
    constexpr int16_t kFlungAcross = 0xC0; // knocked out: away over the end, three quarters of a pixel a step
    constexpr int16_t kFlungUp = 0x600;    //   and up, six pixels a step at first
    constexpr int16_t kPull = 0x68;        //   less this each step
    constexpr int kGoneBelow = 400;        // how far under the floor one falling stops (long out of the picture)
    // The part of a player's record that is how it moves and is drawn (its steps, its frame, its speeds, its boxes):
    // taken from its record on the old stage when it comes down onto the next, but for its place
    constexpr uint32_t kMovingFrom = 0x03, kMovingTo = 0x60;
    constexpr uint32_t kActive = 0x00;
    // The game's random number, a word (002E96 steps it, a shift register: it must never be 0)
    constexpr uint32_t kRandom = At(0xFF82D0);
    // The characters a player can be: all of them, but in a game whose bosses a player cannot choose (the World
    // Warrior: family.h's SSF2T_FAMILY_CHOOSABLE), the first so many (M. Bison and Sagat played by a player there sent
    // the game off into nothing as their fight began)
#ifdef SSF2T_FAMILY_CHOOSABLE
    constexpr int kChoosable = SSF2T_FAMILY_CHOOSABLE;
#else
    constexpr int kChoosable = kCharacters;
#endif

    // The panel and the stage's pieces: everything but the fighters
    bool NotTheFighters(uint32_t tile, uint16_t attributes) {
        return ThePanel(tile, attributes) || IsStagePiece(attributes);
    }

    // An attack is coming at the player of `side`, as the game's guard test has it (07AF3A, its first part, and 07AFBA):
    // the thing the other player last threw (its records' word +1D4, or +2AE) within 0xE0 across coming this way or
    // 0x70 going the other, or the other player attacking within 0xE0 (the player's word +182, how far apart they are)
    bool AttackComing(int side) {
        const uint32_t player = Player(side), other = Player(1 - side);
        uint16_t thrown = Ram16(other + 0x1D4);
        if (!thrown && family::kThrownToo) {
            thrown = Ram16(other + family::kThrownToo);
        }
        if (thrown) {
            const uint32_t record = 0xFF0000 | thrown;
            const int16_t apart = static_cast<int16_t>(Ram16(record + kX) - Ram16(player + kX));
            const bool coming = apart >= 0 ? Ram8(record + kFacesRight) == 0 : Ram8(record + kFacesRight) != 0;
            if (std::abs(apart) <= (coming ? 0xE0 : 0x70)) {
                return true;
            }
        }
        return Ram8(other + kAttacking) && Ram16(player + 0x182) <= 0xE0;
    }

    // The player is its own master: in charge of itself (not reeling, guarding a hit, knocked down or thrown)
    bool OwnMaster(uint32_t player) {
        const uint8_t state = Ram8(player + kState);
        return state == kStand || state == kCrouch || state == kJump || state == kNormal || state == kSpecial ||
               (state == kGuard && !Ram8(player + kGuarded));
    }

    // Who plays the player may change: not while it fights a move or a jump (the computer's moves and a person's keep
    // where they are in bytes of their own, +180-+188 and the step +04, and the other's routine for the move went into
    // the middle of its tables: Fei Long's at 095C90, Ken's at 08031C), only standing or crouching, or before the fight
    bool HandOver(uint32_t player) {
        const uint8_t state = Ram8(player + kState);
        return Ram8(player + kFighting) != 2 ||
               ((state == kStand || state == kCrouch) && !Ram8(player + kAttacking) && !Ram8(player + kInAir));
    }
} // namespace

// The game's guard test for the player of `side` (training.cpp's dummy guards by it), and whether who plays a player may
// change now (its dummy played by the game)
bool AttackComingAt(int side) {
    return AttackComing(side);
}
bool HandsOver(uint32_t player) {
    return HandOver(player);
}

// The ring out's own movements (fight/battle.h, RingOutMove) at a player's step (A6): true when it has made the step,
// the game's own routine for the player not run (training.cpp's PlayerStep, at 0782D4)
bool RingOutStep(M68K &c) {
    const uint32_t player = c.a[6] & 0xFFFFFF;
    const RingOut ringOut = RingOutMove(player);
    if (ringOut.move == RingOut::kFlungOut) {
        // Knocked over the end of the stage: up and away, in the air, as it was hit (no longer kept in)
        SetRam8(player + kInAir, 1);
        SetRam8(player + kHitStop, 0);
        SetRam16(player + kSpeedX, static_cast<uint16_t>(ringOut.side * kFlungAcross));
        SetRam16(player + kSpeedY, static_cast<uint16_t>(kFlungUp));
        SetRam16(player + kPullX, 0);
        SetRam16(player + kPullY, static_cast<uint16_t>(kPull));
        Call(c, kFly);
        Call(c, kAnimate);
        return true;
    }
    if (ringOut.move == RingOut::kFalling) {
        // Through the air and the floor
        if (static_cast<int16_t>(Ram16(player + kY)) > static_cast<int16_t>(kGround) - kGoneBelow) {
            Call(c, kFly);
        }
        Call(c, kAnimate);
        return true;
    }
    if (ringOut.move == RingOut::kDropsIn) {
        // Coming down onto the next stage: as it was in its jump over the old stage, at its place here; the game's jump
        // goes on from there and lands it
        for (uint32_t at = kMovingFrom; at < kMovingTo; at++) {
            if (at < kX || at >= kY + 4) {
                SetRam8(player + at, ringOut.record[at]);
            }
        }
        SetRam8(player + kInAir, ringOut.record[kInAir]);
        SetRam8(player + kAttacking, ringOut.record[kAttacking]);
        SetRam32(player + kX, static_cast<uint32_t>(ringOut.x) << 16);
        SetRam32(player + kY, static_cast<uint32_t>(ringOut.y) << 16);
    }
    return false;
}

void RegisterBattle() {
    battle::Description d;
    d.resultNow = [] {
        const uint8_t result = Ram8(kRoundResult);
        return static_cast<uint8_t>(result == 0xFF ? 3 : result);
    };
    d.cameraX = kCameraX;
    d.cameraY = kCameraY;
    d.cameraLeft = kCameraLeft;
    d.cameraRight = kCameraRight;
    d.player1 = kPlayer1;
    d.playerSize = kPlayerSize;
    d.playerRecords = 2;
    d.thrown = kThrown;
    d.thrownSize = kThrownSize;
    d.thrownRecords = kThrownRecords;
    d.active = kActive;
    d.x = kX;
    d.y = kY;
    d.character = kCharacter;
    d.colours = kColourChoice;
    d.random = kRandom;
    d.computer = kPerson;
    d.computerIsZero = true;
    d.control = kState;
    d.ownMaster = OwnMaster;
    d.handOver = HandOver;
    d.halfWidth = 0x1C2;
    d.health = kHealth;
    d.healthCeiling = kHealthBefore;
    d.healthFull = kHealthFull;
    d.healthBack = 0x18; // (a sixth of it all)
    d.stages = kStages;
    d.stageToRound = 2000; // (about 1,700 from switching on, and a few)
    d.characterSpan = kCharacters;
    d.choosable = [](int n) { return n < kChoosable; };
    d.rosterNumber = [](int choice) { return choice >= 1 && choice <= kChoosable ? choice - 1 : -1; };
    d.fighterChoices = {"Random"};
    d.fighterChoices.insert(d.fighterChoices.end(), std::begin(kCharacterNames), std::begin(kCharacterNames) + kChoosable);
    d.tuneOff = kTuneOff;
    d.inGame = [] { return Ram16(kMode) == kModeGame; };
    d.choosing = modes::Choosing;
    d.toRound = modes::ToRound;
    d.playSound = modes::PlaySound;
    d.wanted = modes::Wanted;
    d.cameraAt = modes::CameraAt;
    d.endModes = modes::End;
    d.stageViewer = modes::StageViewer;
    d.wideSide = WideSide;
    d.wideMargin = WideMargin;
    d.wideEnds = WideEnds;
    d.notTheStage = NotTheStage;
    d.thePanel = ThePanel;
    d.notTheFighters = NotTheFighters;
    d.playerSets = [](int n) { return kPlayerSets[n]; };
    d.inAir = [](uint32_t player) { return Ram8(player + kInAir) != 0; };
    d.atJumpTop = [](uint32_t player) { return Ram8(player + kInAir) && static_cast<int16_t>(Ram16(player + kSpeedY)) <= 0; };
    // The team battle, both styles (livetag.cpp: the tag style's swap; one health bar is all the game has, and it has
    // no entrances)
    d.teams = true;
    d.teamLinesY = 0.19f; // (under the bars and the names)
    d.difficulty = TagDifficulty;
    d.difficultyNow = TagDifficultyNow;
    d.canAct = [](int side) { return CanAct(Player(side)); };
    d.attackComing = [](Machine &, int side) { return AttackComing(side); };
    d.canTag = CanTag;
    d.tagIn = TagIn;
    d.taggedIn = TaggedIn;
    d.offStage = TagOffStage;
    d.untouchable = TagUntouchable;
    d.downSound = 0; // (not found yet)
    d.speedX = kSpeedX;
    d.speedY = kSpeedY;
    d.speedWords = true;
    d.floor = kGround;
    d.meter = family::kTurbo ? kSuperGauge : 0; // (the super combo gauge, which the setting up leaves as it was)
    d.meterSize = family::kTurbo ? 1 : 0;
    battle::Register(d);
    battle::RegisterTeamTag(0.25f);
}

} // namespace cps2recomp
