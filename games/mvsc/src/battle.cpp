// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Marvel vs. Capcom: the fighting modes of the menu's "Alternative modes" page (survival, the endless battle,
// the ring out) are the toolkit's (fight/battle.h); this says where the game keeps what they need. A side is a team of
// two here: the modes follow the character on point (the record the game says, mvsc.h), and a match is one round, won
// when both of a side's characters are down. The ring out's own movements at a character's step are the engine's
// shared code (fight/marvel.h, at the hook training.cpp places); the computer's side is coached in it (below).
#include "alternative.h"
#include "battle.h"
#include "marvel.h"
#include "mvsc.h"

#include <iterator>

namespace cps2recomp {

bool AttackInRange(M68K &c, uint32_t player); // guard.cpp

namespace {
    using namespace mvsc;

    // The ring out's coach (fight/battle.h): the game's computer fights a ring out as a fight, so a side it plays is
    // coached to walk the other to an end, throw or sweep it over, and get out of its own corner, as Super Turbo's is.
    // A character can act: its own master, standing, walking, crouching or guarding (24, standing or crouching by its
    // posture; 26 is the guard in the air: guard.cpp), as the engine's (fight/marvel.cpp) has it.
    bool CanAct(uint32_t player) {
        const uint8_t action = Ram8(player + kAction);
        return Ram16(player + kControl) == 0 &&
               (action == kActionStand || action == kActionWalk || action == kActionCrouch || action == kActionGuard);
    }
    constexpr uint32_t kDifficulty = 0xFFF956; // the board's difficulty, 0 easy to 7 expert, as the game has it (board.cpp)

    constexpr uint32_t kHuman = 0x03;      // a byte of a record: 1 a person plays the character, 0 the computer (the
                                           // players' records in one- and two-player games compared)
    constexpr uint32_t kActive = 0x00;     // a byte of a record: not 0 while it is in use
    constexpr uint32_t kResult = 0xFF4005; // the round's result: 1 player 1's side has won, 2 player 2's (a knockout and a
                                           // time over watched)
} // namespace

void RegisterBattle() {
    battle::Description d;
    d.result = kResult;
    d.roundsToWin = 1; // (a tag match is decided in one round: online play's win streaks)
    d.cameraXAt = marvel::CameraXAt;
    d.cameraYAt = marvel::CameraYAt;
    d.cameraEnds = [](int &left, int &right) {
        left = static_cast<int16_t>(Ram16(CameraRecord() + kCameraLeft));
        right = static_cast<int16_t>(Ram16(CameraRecord() + kCameraRight));
    };
    d.player1 = kPlayer1;
    d.playerSize = kPlayerSize;
    d.playerRecords = 4;
    d.active = kActive;
    d.x = kX;
    d.y = kY;
    d.character = kCharacter + 1; // (the low byte of the word)
    d.computer = kHuman;
    d.computerIsZero = true;
    d.partner = kPartners;
    d.playerAt = Player;
    d.partnerAt = PartnerOf;
    d.control = kControl + 1;
    d.controlOwn = 0;
    d.health = kHealth;
    d.healthCeiling = kRedHealth;
    d.healthFull = kHealthFull;
    d.healthBack = 0x18; // (a sixth of it all)
    d.stages = kStages;
    d.stageToRound = 2300;
    d.characterSpan = 2 * kCharacters;
    d.choosable = Choosable;
    d.rosterNumber = [](int choice) {
        const int number = choice >= 1 && choice <= kCharacters ? (choice - 1) * 2 : -1;
        return Choosable(number) ? number : -1;
    };
    d.fighterChoices = {"Random"};
    d.fighterChoices.insert(d.fighterChoices.end(), std::begin(kCharacterNames), std::end(kCharacterNames));
    d.inGame = [] { return Ram8(kPlaying) != 0; };
    d.choosing = modes::Choosing;
    d.toRound = modes::ToRound;
    d.playSound = modes::PlaySound;
    d.wanted = modes::Wanted;
    d.cameraAt = modes::CameraAt;
    d.endModes = modes::End;
    d.stageViewer = modes::StageViewer;
    d.wideSide = marvel::WideSide;
    d.wideMargin = marvel::WideMargin;
    d.wideEnds = marvel::WideEnds;
    d.notTheStage = NotTheStage;
    d.thePanel = ThePanel;
    d.notTheFighters = [](uint32_t tile, uint16_t attributes) { return ThePanel(tile, attributes) || (attributes & 0x1F) >= 0x1A; };
    // (a side's colour sets: its point character's three, and its partner's)
    d.playerSets = [](int n) {
        const unsigned first = n == 0 ? kPlayer1Set : kPlayer2Set;
        return (0x7u << first) | (0x7u << (first + 6));
    };
    d.pictureLeft = kPictureLeft;
    d.yDown = true;
    d.fixedHalfWidth = kWall; // (the game keeps a player's middle this far inside the picture's edges: 013AD6)
    d.inAir = [](uint32_t player) { return static_cast<int16_t>(Ram16(player + kY)) != Ground(); };
    d.atJumpTop = [](uint32_t player) { return static_cast<int16_t>(Ram16(player + kY)) != Ground() && static_cast<int32_t>(Ram32(player + kSpeedY)) >= 0; };
    d.canAct = [](int side) { return CanAct(Player(side)); };
    d.difficultyNow = [] { return static_cast<int>(Ram8(kDifficulty) & 7); };
    d.attackComing = [](Machine &machine, int side) { return AttackInRange(machine.cpu, Player(side)); };
    d.throwReach = 72; // (two characters standing together are 60 to 68 apart; Ryu's throw took Spider-Man 62 away)
    battle::Register(d);
}

} // namespace cps2recomp
