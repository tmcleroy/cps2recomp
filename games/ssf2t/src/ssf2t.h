// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Super Street Fighter II Turbo (Super Street Fighter II X in Japan): names for what is known of the game's memory and
// routines, each found by experiment in this game (PROGRESS.md). Its variables are reached from A5 = FF8000, as in the
// Alpha games, but it is an older program: the steps, the players' records and the panel are laid out its own way,
// and nothing is taken from the later games unchecked. Some facts were first named by the FBNeo training script for
// this game (peon2/fbneo-training-mode, games/ssf2t); each was looked at again here.
#pragma once

#include "game.h"
#include "lift.h"
#include "machine.h"
#include "places.h"

#include <cstdint>

namespace ssf2t {

using cps2recomp::M68K;

// --- where the game is ---

// Words from A5, each a step (in twos) of the one before. The first is 2 once a game is being played (the board's
// start and its show step through it before: title.cpp). The second is 0 while the players choose their characters
// and 4 during the match (2 for a frame between). The third is the match's: 2 and 4 the versus screen, 6 and 8 a
// round's introduction, 0A the round being fought, 0C to 10 its end (then 6 again for the next round). Traced with
// --watch through a one-player game.
constexpr uint32_t kMode = 0xFF8000, kGameStep = At(0xFF8004), kMatchStep = At(0xFF8008), kRoundStep = At(0xFF800C);
enum : uint16_t { kModeGame = 2, kGameChoosing = 0, kGameMatch = 4, kMatchFought = 0x0A };

// Credits paid for and not yet used, a byte (RAM dumps after one, two and three coins: 1, 2, 3; written at 002418)
constexpr uint32_t kCredits = At(0xFF82E9);

// Who is in: a byte, bit 0 player 1, bit 1 player 2 (RAM dumps before and after each Start: 0, 1, 3)
constexpr uint32_t kPlayersIn = At(0xFF82F0);

// The game's own free play. The operator's coin mode (the test menu's System Configuration, Coin: 00 to 0E in twos
// one coin for one to six credits and two to four coins a credit, 10 a coin to start and one to continue) is kept in
// kCoinMode; the board's start (00823A, the record copied at 00831C) takes its two coin words from a table by it
// (008536) and sets kFreePlay for mode 12, which the menu never offers. The game tests kFreePlay wherever it counts
// credits (the title says FREE PLAY, and Start lets a player in). Seen with the byte poked to 1.
constexpr uint32_t kCoinMode = At(0xFF8342), kFreePlay = At(0xFF82DF), kCoinsPerCredit1 = At(0xFF82AE), kCoinsPerCredit2 = At(0xFF82B6);
constexpr uint8_t kCoinFreePlay = 0x12;
constexpr uint16_t kFreePlayCoins = 0x0101; // (the table's word for mode 12)

// The round's clock: seconds left, a BCD byte (99 at a round's start, 00D780), and the frames left of the second in the
// byte after it (40 a second here: 00FE74 counts it down, and at nothing a second is taken off at 00FE8A). Found with
// --watch through a fight; FBNeo's script names the first.
constexpr uint32_t kRoundClock = At(0xFF8DCE), kClockFrames = At(0xFF8DCF);
constexpr uint32_t kClockTick = At(0x00FE74); // subq.b #1 off the second's frames

// The stage a match is fought on: a word, the number of the character whose stage it is (0 to 15; stage n is
// character n's). In a one-player game it is the computer's next opponent too (taken from the ladder's list at A5+618A,
// 0039A6); a match's stage is set up from it at 0CCC5C (copied to A5+CB0). Each picture seen with its low byte held at
// each number.
constexpr uint32_t kStage = At(0xFF8C4E);
constexpr uint32_t kStageMade = At(0x0CCC5C); // move.w 0xC4E(a5),d0: the stage being set up for a match
constexpr int kStages = family::kStages;
// Who a player is (a player's byte kCharacter): RAM dumps through the ladder (Guile 3, Fei Long 14, Blanka 2 against
// player 1's Ryu 0), the rest by the stages', which are in the same order (the names: family.h)
constexpr int kCharacters = family::kCharacters;
using family::kCharacterNames;
using family::kStageNames;
using family::kStageTunes;
constexpr uint16_t kTuneChoose = family::kTuneChoose, kTuneVersus = family::kTuneVersus;

// The sound queue: a ring of sixteen-byte requests at A5-7582, the word at A5-7586 the place of the next (007A84 puts
// one in, a long a code with the code in its low word; 0079E8 sends the oldest to the sound CPU), as in the later
// games. FF05 asks for the music to stop (007B22).
constexpr uint32_t kSoundQueue = At(0xFF0A7E), kSoundQueueEnd = At(0xFF0A7A);
constexpr uint16_t kTuneOff = family::kTuneOff;
// (a game whose queue is of words, two bytes each: family.h)
constexpr uint16_t kSoundEntry = family::kSoundEntry, kSoundQueueMask = family::kSoundQueueMask;

// The camera's record (0CC4A6 steps it; the stage's own scroll screens follow it): its place across (the picture's
// left edge in the stage) and up are words at +6 and +0A (FF8ED4, FF8ED8: FBNeo's script has the first). Its ends are
// words from A5: C54 and C56 across, C58 and C5A up. Every word holding it across moved together with it in RAM dumps as
// the players walked to each end (FF8FEA-FF8FF0, written at 0CDC7E); the game makes the scroll screens' own words from
// it each frame. 256 to 640 across on every stage seen.
constexpr uint32_t kCamera = At(0xFF8ECE), kCameraX = At(0xFF8ED4), kCameraY = At(0xFF8ED8);
using family::kCameraCopies; // (family.h)
constexpr uint32_t kCameraEndLeft = At(0xFF8C54), kCameraEndRight = At(0xFF8C56), kCameraEndBottom = At(0xFF8C58), kCameraEndTop = At(0xFF8C5A);
constexpr int kCameraLeft = 0x100, kCameraRight = 0x280;
// Where the players stand at a round's start, and how far they can go (player 1 walked to each end, the camera there)
constexpr uint16_t kStartX[2] = {0x228, 0x2D8};
constexpr int kStageLeft = 0x128, kStageRight = 0x3D8;

// The game's loop of introduction, title, notices, a demonstration fight and rankings (kMode, traced with no coin in;
// the steps between are fades): 2 the introduction and the title (kGameStep 16 on), 4 to 0C the QSound notice and the
// maker's, 0E a demonstration fight (a game the computer plays for both: its own steps under it as a game's), 10 to 16
// the rankings; then 0 and on from the start. With no coin in, the board's start ends at frame 805 and the loop begins.
// (each game's: family.h)
constexpr uint16_t kIntro = family::kIntro, kDemo = family::kDemo, kLoopEnd = family::kLoopEnd;
constexpr int kBootFrames = family::kBootFrames;

// The round's result: 1 player 1 has won (player 2's health below nothing), 2 player 2, FF neither (a double knock-out
// or a draw at time over); 0 while it is fought (00FC88, the round's end, 00FCE6 and on)
constexpr uint32_t kRoundResult = At(0xFF8DD2);

// --- the players ---

// Each player's record, 0x400 apart, the first at FF844E (A5+44E: cleared at 003616, 0x380 bytes each, with the
// second at A5+84E); FBNeo's script has them there too
constexpr uint32_t kPlayer1 = At(0xFF844E), kPlayerSize = family::kPlayerSize;
inline uint32_t Player(int n) {
    return kPlayer1 + static_cast<uint32_t>(n) * kPlayerSize;
}
// Health: a word, 0x90 full, below nothing knocked out (FFFF); the word after it is what it was before the last hit
// (07AC78 compares the two to see a hit has landed, and makes them alike at 07AD08)
constexpr uint32_t kHealth = 0x2A, kHealthBefore = 0x2C;
constexpr uint16_t kHealthFull = 0x90;
// Bytes of steps: +02 is 2 in a fight (078270 goes on by it); +03 says what is in charge of the player and +04 steps
// within that (0782D4 goes on by +03, 078270's fight step). Traced through a fight of known input against a player
// who stood still, guarded, was hit and was thrown: 00 standing or walking, 02 crouching, 04 a jump (+04 06 a jump
// attack), 06 getting up, 08 guarding (from the moment an attack comes near), 0A a normal attack or a throw, 0C a
// special move, 0E reeling from a hit (07AD56), 14 knocked down (and thrown).
constexpr uint32_t kFighting = 0x02, kState = 0x03, kStateStep = 0x04;
enum : uint8_t { kStand = 0x00, kCrouch = 0x02, kJump = 0x04, kGettingUp = 0x06, kGuard = 0x08, kNormal = 0x0A,
                 kSpecial = 0x0C, kReeling = 0x0E, kDown = 0x14 };
// Where it stands across and its height (upwards; kGround on the floor): longs with 16 bits of fraction (FBNeo's
// script reads the words). The hit test 0BF5AE takes them so.
constexpr uint32_t kX = 0x06, kY = 0x0A;
constexpr uint16_t kGround = 0x28;
constexpr uint32_t kFacesRight = 0x12; // byte: not zero when facing right (player 1 at a round's start: 004FC4); a box's offset across is
                                       // turned about then (0BF5C0)
// The animation frame being shown (a long, in the ROM) and the record's box tables (a long): the hit tests (0BF534,
// 0BF558-0BF588) take a box's number from the frame (bytes: three it can be hit in, its attack, its push box) and the
// box from the table named by the word at that box kind's place in the tables (+0, +2, +4, +6, +8: offsets from the
// tables), four bytes a box (sixteen an attack's): its middle from where the player stands (signed: across, up), half
// its width and half its height. An attack whose byte +5 has its top bit set reaches that far across instead (0BF5AE).
constexpr uint32_t kFrame = 0x1A, kBoxTables = 0x34;
constexpr uint32_t kFrameHead = 0x08, kFrameBody = 0x09, kFrameLegs = 0x0A, kFrameAttack = 0x0C, kFramePush = 0x0D;
// (an attack's and the pushing box's table, and the size of an attack box, differ in the other games: family.h)
constexpr uint32_t kTableHead = 0x0, kTableBody = 0x2, kTableLegs = 0x4, kTableAttack = family::kTableAttack, kTablePush = family::kTablePush;
constexpr uint32_t kBoxSize = 4, kAttackBoxSize = family::kAttackBoxSize;
constexpr uint32_t kHitStop = 0x47; // byte: frames the player stands frozen for while a hit lands (14 a light one)
constexpr uint32_t kGuarded = 0x49; // byte: set when the hit landing on the player is guarded (0BEF32, 0BEF48, 0BEF58)
constexpr uint32_t kInAir = 0x181;  // byte: not zero while in the air
constexpr uint32_t kAttacking = 0x18B; // byte: 1 while an attack is out (normal, special or in a jump)
constexpr uint32_t kSuperGauge = 0x2B4; // byte: the super combo gauge, 0x30 full (FBNeo's script)
constexpr uint32_t kChosen = family::kChosen;  // byte: 1 once the player has chosen a character (RAM dumps as each chose; 00E93A)
constexpr uint32_t kSide = family::kSide;    // byte: 0 player 1, 1 player 2
constexpr uint32_t kPerson = family::kPerson;  // byte: 1 for a person, 0 the computer (RAM dumps of a one- and a two-player game)
constexpr uint32_t kCharacter = family::kCharacter; // byte: who the player is (kCharacterNames; 0 also for Akuma, with kAkuma set)
constexpr uint32_t kOpponent = family::kOpponent;  // word: the other player's record (FFxxxx: movea.w)
constexpr uint32_t kColourChoice = family::kColourChoice; // byte: the set of colours (00E9B8 on: 0-5 the button that chose, light punch first;
                                          // 6 Start; 7 a button held; 8 and 9 the old characters'; 0A and 0B Akuma's)
constexpr uint32_t kOld = 0x3B6;   // byte: the character's old version (Super Street Fighter II's): 015788
constexpr uint32_t kAkuma = 0x3BD; // byte: the player is Akuma (00E96A)
// (the three above are Super Turbo's own, family::kTurbo: in the games without them they are other bytes, or another
// player's, so they are read and written only through these)
inline bool IsAkuma(uint32_t player) {
    return family::kTurbo && cps2recomp::Ram8(player + kAkuma) != 0;
}
inline bool IsOld(uint32_t player) {
    return family::kTurbo && cps2recomp::Ram8(player + kOld) != 0;
}
inline void NotHidden(uint32_t player) { // (Akuma and the old version no more)
    if (family::kTurbo) {
        cps2recomp::SetRam8(player + kAkuma, 0);
        cps2recomp::SetRam8(player + kOld, 0);
    }
}

// The records of what the players throw: eight of 0xC0 bytes from FF97A2, +00 1 for one in use (a player's layout:
// its frame and box tables where a player's are; FBNeo's script), the thrower's record a word at +26 (004968)
constexpr uint32_t kThrown = At(0xFF97A2), kThrownSize = 0xC0, kThrownOwner = 0x26;
constexpr int kThrownRecords = 8;

// The control panel as the game read it this frame, a word a player: the stick below (bit 0 right, 1 left, 2 down, 3
// up), the punches in bits 4-6 and the kicks in 8-10 (FBNeo's script names them; RAM dumps holding each)
constexpr uint32_t kControls[2] = {At(0xFF8086), At(0xFF808A)};

// The player can do anything: in charge of itself, standing, walking or crouching, or in its guarding pose before a hit
inline bool CanAct(uint32_t player) {
    using namespace cps2recomp;
    const uint8_t state = Ram8(player + kState);
    return Ram8(player + kFighting) == 2 && (state == kStand || state == kCrouch || (state == kGuard && !Ram8(player + kGuarded)));
}

// --- routines (addresses in the program) ---

// A hit's damage off the victim's health: sub.w d6,0x2A(a2), D6 the damage, A2 the victim; 0BE646 in the hit of a
// player's attack (0BE580: A6 the attacker, A3 the attack) and 0BEBA6 in that of something thrown (0BEB0E: A6 what was
// thrown). Found counting the writers of both players' health over fights.
constexpr uint32_t kHitDamage = At(0x0BE646), kThrownDamage = At(0x0BEBA6);
// A throw's damage, D4 off the victim's (A3) health: at once (00490A, and 0049B0 in 00492C), or put by in the victim's
// word kDamagePut (004A58, in 004A4C: at once a knockout if it is more than the health) and taken off as the throw ends
// (004C98 half of it, 07AB6E, 07ACEA). Found as the other writers of the health over made-up play with no damage.
inline constexpr uint32_t kThrowDamage[] = {cps2recomp::HookAt(At(0x00490A)), cps2recomp::HookAt(At(0x0049B0)), cps2recomp::HookAt(At(0x004A58))};
constexpr uint32_t kDamagePut = 0x78;

// A player's fight step goes on by kState, once a step for each player: a person's (0782D4, in 078270's fight step
// 0782A0) and the computer's (07BC12, before the computer's plan and its dispatch on kState at 07BC56, which can run
// more than once a step: in 07BB22's fight step 07BBA4)
constexpr uint32_t kPlayerStep = At(0x0782D4), kComputerPlayerStep = At(0x07BC12);
// A hit has been decided and taken (A6 the attacker, A2 the victim; kGuarded says whether guarded): where a player's
// attack's two ways meet (0BE678), and a thrown thing's (0BEBDA)
constexpr uint32_t kHitTaken = At(0x0BE678), kThrownHitTaken = At(0x0BEBDA);
// A hit's damage has a random part (0BEE40): a byte of a row of 32 of the table at 0E7996 (the row by the attack's +0A,
// or +09 when the attacker's health is 0x3C or more), picked by the game's random number (002E96, its low five bits),
// added to the damage (D6) at 0BEE6A, D0 the byte, A1 the table, D2 the row's start plus the pick; the sum is made 1
// where it comes to nothing or less (0BEE72), and both ways go on at 0BEE74. A throw's alike (004B86: D4 the damage,
// 004BDA and 004BDC); the classic games' throws have none (0 there). Read in the listing.
constexpr uint32_t kHitMadeOne = At(0x0BEE72), kHitSummed = At(0x0BEE74), kThrowMadeOne = At(0x004BDA), kThrowSummed = At(0x004BDC);
// The throw test (00461E, 004604, 00460C: every throw a player tries, a person's or the computer's) compares (0046E4)
// the thrower's (A6) box with the other's (A3) place to be thrown by: words of the thrower's record, its box's middle
// across (+64, turned about with the facing as an attack box is) and up (+66) from where it stands, half its width
// (+68) and height (+6A), put there for the throw being tried (from the character's table by the throw, 0B8650; each
// character's own code in the World Warrior), and nothing else writes them; and words of the other's, half the width of
// its place (+6C) and its height (+6E) up from where it stands (a table by character, as a round is set up: 0050CE), in
// Super Turbo +2AA across from there while +2A7 is set. A throw is in range when the two overlap across and that height
// is inside the box. Read in the listing; the test is the same in every game of the family (Super Turbo's alone has +2A7).
constexpr uint32_t kThrowX = 0x64, kThrowY = 0x66, kThrowHalfWidth = 0x68, kThrowHalfHeight = 0x6A;
constexpr uint32_t kThrowableHalfWidth = 0x6C, kThrowableY = 0x6E, kThrowableMoved = 0x2A7, kThrowableX = 0x2AA;
// Stunned: a byte, 1 while the player is (07A7B2 sets it, 07A7D2 clears it as the stun's time, the word +60, runs out).
// A person shortens it (07A80E): each step the stick goes from the middle to a side adds 2 to the byte +125 and a button
// newly pressed 1 (2 in Hyper Fighting), and at 3 or more the step's time comes off +60 at once. The same in every game.
constexpr uint32_t kStunned = 0x124;
// Softening a throw (Super Turbo's: 0047D4, each step of being thrown): a person holding the stick to either side and
// pressing a medium or heavy button new (0x660 of the controls' word) within 0x0E steps of the throw's start (+2BC
// counts them) sets +2BD, and the throw does half its damage (004C88). Not in the other games of the family.
constexpr uint32_t kSoftened = 0x2BD;
// A throw going on: a byte, 1 in the thrower's record and FF in the one thrown (004766, cleared as the throw ends at
// 004E3E); the one thrown stays in its own state meanwhile (+03 standing, say). The same in every game of the family.
constexpr uint32_t kHeldBy = 0x63;
// The guard decision as a hit lands (0BEEA4, src/guard.cpp): the victim's guarding pose is its frame's byte +11 (0 not
// guarding, 1 standing, 2 crouching), the hurt box hit (0 head, 2 body, 4 legs) is put at A5-56EA by the hit test, and a
// jump table by the attack's +0B (0BEED8) goes to one of five routines: most attacks (0BEF66: crouching guards it, standing
// only on the head or the body), guarded only standing (0BEF7C), only crouching (0BEF82, 0BEF8A), either way (0BEF88).
// The classic games have a table of rows instead (family.h). Read in the listing.
constexpr uint32_t kGuardTypes = At(0x0BEED8), kGuardMost = At(0x0BEF66), kGuardStanding = At(0x0BEF7C), kGuardCrouching = At(0x0BEF82),
                   kGuardEither = At(0x0BEF88), kGuardCrouching2 = At(0x0BEF8A);
constexpr uint32_t kFrameGuardPose = 0x11;
constexpr uint32_t kAttackLevel = 0x07, kAttackType = 0x0B; // bytes of an attack box: its height (the classic games) and kind
// The match's first step: the characters are chosen (00D714, the step word under the game's going to 2)
constexpr uint32_t kMatchBegins = At(0x00D714);

// --- the picture ---

// Objects by what they are (for Video::hideObject; found by hiding each colour set in turn in fights on every stage
// and at their starts): 01-05 are player 1's and 10-14 player 2's (the character and what it throws: 0BFA78 copies its
// set of five palettes there), 0A the players' shadows, 0E the sparks of hits, 0B and 1A-1F the stage's own pieces, and
// the rest the panel (07 and 08 the faces, 0C the bars and names, 0D the score, the clock and the round's words, 19 the
// super combo gauges). The stage's scroll screens hold no panel.
using family::IsFighter;    // (each game's sets: family.h)
using family::IsStagePiece;
inline bool IsShadow(uint16_t attributes) {
    return (attributes & 0x1F) == family::kShadowSet;
}
inline bool ThePanel(uint32_t, uint16_t attributes) {
    const unsigned set = attributes & 0x1F;
    return !IsFighter(attributes) && !IsStagePiece(attributes) && !IsShadow(attributes) && set != family::kSparkSet;
}
inline bool NotTheStage(uint32_t, uint16_t attributes) {
    return !IsStagePiece(attributes);
}
// A player's own colour sets (its side's), as a mask of bits
using family::kPlayerSets;

// --- for hooks ---

inline uint16_t Mem16(M68K &c, uint32_t address) { // (anything, the ROM's tables too: the data view)
    return static_cast<uint16_t>(c.Read(address, 1));
}

inline void SetLow16(uint32_t &reg, uint32_t value) {
    reg = (reg & 0xFFFF0000) | (value & 0xFFFF);
}
inline void SetLow8(uint32_t &reg, uint32_t value) {
    reg = (reg & 0xFFFFFF00) | (value & 0xFF);
}

} // namespace ssf2t

namespace cps2recomp {
// The wide picture (widescreen.cpp): the columns it adds on each side (0: none), how far right the stage is drawn in it,
// how far short of each end the camera stops (64, or 0: played as on the board), and a spare's frame drawn as wide as
// the port's (and the port's machine back)
int WideSide();
int WideStage();
int WideMargin();
void WideEnds(int &left, int &right);
void WideForSpare(Machine &machine);
void WideForPort(Machine &machine);
} // namespace cps2recomp
