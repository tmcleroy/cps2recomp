// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Hyper Street Fighter II: The Anniversary Edition: names for what is known of the game's memory and routines, each
// found by experiment in this game (PROGRESS.md). The program grew out of Super Street Fighter II Turbo's
// (games/ssf2t): most of its routines are Super Turbo's, moved, with its variables (from A5 = FF8000) moved too. Each
// of Super Turbo's facts was carried over by lining the two programs' code up instruction by instruction (the same
// instruction reading another variable says where that variable went), and then looked at again here. The game's
// own parts (choosing a character's version, the menus between the fights) were traced here. Some facts were first
// named by the FBNeo training script for this game (peon2/fbneo-training-mode, games/hsf2).
#pragma once

#include "game.h"
#include "lift.h"
#include "machine.h"

#include <cstdint>

namespace hsf2 {

using cps2recomp::M68K;

// --- where the game is ---

// The program is a few tasks taking turns (the kernel at 0013C0 starts one: a byte a slot, not zero while it runs, in
// the table at A5-7D7E, 0x20 bytes a slot). Each keeps its step in the same words from A5, so the words alone do not
// say what is going on: the task does. Slot 0x20 is the show (introduction, title, notices, a demonstration fight,
// rankings; started at 008476), 0x40 the title with credits in (009B52), 0xA0 the game (started at 009D40 when Start
// is pressed: 00B160). Read from the table in RAM dumps of each.
constexpr uint32_t kShowTask = 0xFF02A2, kTitleTask = 0xFF02C2, kGameTask = 0xFF0322;

// Words from A5, each a step (in twos) of the one before. In the game's task the first is 2 while the players choose
// (the speed, the character's version, the character: the second word steps through them), 4 during a match, 8 once
// the one player has lost (continue and game over). In a match the second word is the match's step: 2 and 4 the versus
// screen, 6 a round's introduction, 8 the round, 0A its end, 0C the next round's start (then 6 again). In step 8 the
// third word is 0 while the round is called ("FIGHT", about 120 frames: the players stand, their routines not run) and
// 2 while it is fought. Traced with --watch through a one-player game. (Super Turbo has them a word lower: its game's
// step is its second word, the match's its third, and its fought step comes after the call.)
constexpr uint32_t kMode = 0xFF8000, kMatchStep = 0xFF8004, kRoundStep = 0xFF8008, kRoundStep2 = 0xFF800C;
enum : uint16_t { kModeChoosing = 2, kModeMatch = 4, kModeLost = 8 };
enum : uint16_t { kMatchIntroduction = 0x06, kMatchRound = 0x08, kMatchRoundEnd = 0x0A, kMatchNextRound = 0x0C };
constexpr uint16_t kRoundFought = 0x02; // (kRoundStep in kMatchRound)

inline bool InGame() {
    return cps2recomp::Ram8(kGameTask) != 0;
}
inline bool Choosing() {
    return InGame() && cps2recomp::Ram16(kMode) == kModeChoosing;
}
inline bool InMatch() {
    return InGame() && cps2recomp::Ram16(kMode) == kModeMatch;
}
inline bool Fought() {
    return InMatch() && cps2recomp::Ram16(kMatchStep) == kMatchRound && cps2recomp::Ram16(kRoundStep) == kRoundFought;
}

// Credits paid for and not yet used, a byte (RAM dumps after one, two, three and four coins, two coins a credit:
// 0, 1, 1, 2; Super Turbo's FF82E9)
constexpr uint32_t kCredits = 0xFF830A;

// Who is in: a byte, bit 0 player 1, bit 1 player 2 (RAM dumps before and after each Start: 0, 1, 3)
constexpr uint32_t kPlayersIn = 0xFF8316;

// The game's own free play. The operator's coin mode (the test menu's Coin, record byte +1F) is kept in kCoinMode; the
// board's start (00762A, the record from A5-62F2 copied at 0076F8) takes its two coin words from the table at 007912 by
// it and sets kFreePlay for mode 0x24 (0x22 is a coin to start and one to continue). The game tests kFreePlay wherever it
// counts credits. A new board's mode is 0x12, two coins a credit.
constexpr uint32_t kCoinMode = 0xFF82C4, kFreePlay = 0xFF82C0, kCoinsPerCredit1 = 0xFF829E, kCoinsPerCredit2 = 0xFF82A6;
constexpr uint8_t kCoinFreePlay = 0x24;
constexpr uint16_t kFreePlayCoins = 0x0101; // (the table's word for mode 0x24)

// The round's clock: seconds left, a BCD byte (99 at a round's start, 00B26A), and the frames left of the second in the
// byte after it (40 a second: 00DA76 counts it down, and at nothing a second is taken off). FBNeo's script has the first.
constexpr uint32_t kRoundClock = 0xFF8BFC, kClockFrames = 0xFF8BFD;
constexpr uint32_t kClockTick = 0x00DA76; // subq.b #1 off the second's frames

// The stage a match is fought on: a word, the number of the character whose stage it is (0 to 15, in the characters'
// order; FBNeo's script writes it, and the Ryu against Ken game was fought on 4, Ken's). A match's stage is set up from
// it at 0DA446 (Super Turbo's 0CCC5C).
constexpr uint32_t kStage = 0xFF8B64;
constexpr uint32_t kStageMade = 0x0DA446; // move.w 0xB64(a5),d0: the stage being set up for a match
constexpr int kStages = 16;
// Who a player is (a player's byte kCharacter): the same numbers as Super Turbo's (Ryu 0 and Ken 4 in RAM dumps)
constexpr int kCharacters = 16;
inline constexpr const char *kCharacterNames[kCharacters] = {"Ryu",     "E. Honda", "Blanka", "Guile",  "Ken",      "Chun-Li",
                                                             "Zangief", "Dhalsim",  "M. Bison", "Sagat", "Balrog",  "Vega",
                                                             "Cammy",   "T. Hawk",  "Fei Long", "Dee Jay"};
inline constexpr const char *kStageNames[kStages] = {
    "Ryu's (the castle)",        "E. Honda's (the bathhouse)", "Blanka's (the river)",       "Guile's (the air base)",
    "Ken's (the harbour)",       "Chun-Li's (the market)",     "Zangief's (the factory)",    "Dhalsim's (the palace)",
    "M. Bison's (the temple)",   "Sagat's (the Buddha)",       "Balrog's (Las Vegas)",       "Vega's (the cage)",
    "Cammy's (the castle ruins)", "T. Hawk's (Mexico)",             "Fei Long's (the garden)",    "Dee Jay's (the beach)"};
// Each stage's tune, and the character choice's and the versus screen's (music.cpp)
inline constexpr uint16_t kStageTunes[kStages] = {0x02, 0x03, 0x05, 0x07, 0x01, 0x04, 0x06, 0x08,
                                                  0x0C, 0x0B, 0x09, 0x0A, 0x0E, 0x0F, 0x0D, 0x10};
constexpr uint16_t kTuneChoose = 0x34, kTuneVersus = 0x35;

// The sound queue: a ring of sixteen-byte requests at A5-72FC, the word at A5-7300 the place of the next (006ECC puts
// one in, 006E30 sends the oldest to the sound CPU: Super Turbo's 007A84 and 0079E8). FF05 asks for the music to stop.
constexpr uint32_t kSoundQueue = 0xFF0D04, kSoundQueueEnd = 0xFF0D00;
constexpr uint16_t kTuneOff = 0xFF05;

// The camera's record (0D9C8E steps it): its place across (the picture's left edge in the stage) and up are words at +6
// and +0A (FF8CC2, FF8CC6). Its ends are words from A5: B50 and B52 across, B54 and B56 up. The other words holding
// it across, written with it each frame (Super Turbo's FF8FEA-FF8FF0, 0x212 lower here).
constexpr uint32_t kCamera = 0xFF8CBC, kCameraX = 0xFF8CC2, kCameraY = 0xFF8CC6;
inline constexpr uint32_t kCameraCopies[] = {0xFF8CC2, 0xFF8DD8, 0xFF8DDA, 0xFF8DDC, 0xFF8DDE};
constexpr uint32_t kCameraEndLeft = 0xFF8B50, kCameraEndRight = 0xFF8B52, kCameraEndBottom = 0xFF8B54, kCameraEndTop = 0xFF8B56;
constexpr int kCameraLeft = 0x100, kCameraRight = 0x280;
// Where the players stand at a round's start, and how far they can go
constexpr uint16_t kStartX[2] = {0x228, 0x2D8};
constexpr int kStageLeft = 0x128, kStageRight = 0x3D8;

// The show (kMode in the show's task, traced with no coin in, with pictures every 40 frames): 2 and 4 the anniversary's
// mark and the maker's, 6 the introduction (each old game's title in turn, then this one's: its own step, kMatchStep's
// word, is 0x20 as "HYPER" comes in, frame 2501, and 0x22 with the whole title), 8 the health warning, 0A the QSound
// notice, 0C a demonstration fight, 0E the rankings; then 0 and on from the start. The board's start before it steps 2
// to 10 in the boot task, and the show begins at frame 837.
enum : uint16_t { kIntro = 0x06, kHealthWarning = 0x08, kDemo = 0x0C, kLoopEnd = 0x10 };
constexpr uint16_t kTitleStep = 0x20;
constexpr int kBootFrames = 840;

// The round's result: 1 player 1 has won, 2 player 2, FF neither; 0 while it is fought (Super Turbo's FF8DD2)
constexpr uint32_t kRoundResult = 0xFF8C03;

// --- the players ---

// Each player's record, 0x400 apart, the first at FF833C (A5+33C; FBNeo's script has its health at FF8366)
constexpr uint32_t kPlayer1 = 0xFF833C, kPlayerSize = 0x400;
inline uint32_t Player(int n) {
    return kPlayer1 + static_cast<uint32_t>(n) * kPlayerSize;
}
// The record's layout is Super Turbo's to +2B4 (its routines read the same offsets, instruction for instruction); the
// bytes after it are laid out anew, for the versions.
// Health: a word, 0x90 full, below nothing knocked out (FFFF); the word after it is what it was before the last hit
constexpr uint32_t kHealth = 0x2A, kHealthBefore = 0x2C;
constexpr uint16_t kHealthFull = 0x90;
// Bytes of steps: +02 is 2 in a fight; +03 says what is in charge of the player and +04 steps within that (066BBE goes
// on by +03): 00 standing or walking, 02 crouching, 04 a jump, 06 getting up, 08 guarding, 0A a normal attack or a
// throw, 0C a special move, 0E reeling from a hit, 14 knocked down (and thrown). Super Turbo's, as FBNeo's script has
// them for this game (0E in hit stun).
constexpr uint32_t kFighting = 0x02, kState = 0x03, kStateStep = 0x04;
enum : uint8_t { kStand = 0x00, kCrouch = 0x02, kJump = 0x04, kGettingUp = 0x06, kGuard = 0x08, kNormal = 0x0A,
                 kSpecial = 0x0C, kReeling = 0x0E, kDown = 0x14 };
// Where it stands across and its height: longs with 16 bits of fraction (the floor 0x28)
constexpr uint32_t kX = 0x06, kY = 0x0A;
constexpr uint16_t kGround = 0x28;
constexpr uint32_t kFacesRight = 0x12; // byte: not zero when facing right
// The animation frame being shown (a long) and the record's box tables (a long). The hit tests (0C38FE, 0C3928-0C3968)
// take a box's number from the frame (bytes: three it can be hit in, a fourth tested only once one of those is hit,
// its attack, its push box) and the box from the table named by the word at that box kind's place in the tables
// (offsets from the tables): eight bytes a box, 0x14 an attack's, words (0C398E: its middle from where the player
// stands, across and up, half its width and half its height). Super Turbo's had no fourth and were bytes, four a box
// and 0x10 an attack's.
constexpr uint32_t kFrame = 0x1A, kBoxTables = 0x34;
constexpr uint32_t kFrameHead = 0x08, kFrameBody = 0x09, kFrameLegs = 0x0A, kFrameAttack = 0x0C, kFramePush = 0x0D;
constexpr uint32_t kTableHead = 0x0, kTableBody = 0x2, kTableLegs = 0x4, kTableAttack = 0x8, kTablePush = 0xA;
constexpr uint32_t kBoxSize = 8, kAttackBoxSize = 0x14;
constexpr uint32_t kHitStop = 0x47; // byte: frames the player stands frozen for while a hit lands
constexpr uint32_t kGuarded = 0x49; // byte: set when the hit landing on the player is guarded (0C30D8, 0C30EE, 0C30FE)
constexpr uint32_t kInAir = 0x181;  // byte: not zero while in the air
constexpr uint32_t kAttacking = 0x18B; // byte: 1 while an attack is out
constexpr uint32_t kSuperGauge = 0x2B4; // byte: the super combo gauge (FBNeo's script: FF85F0)
constexpr uint32_t kSide = 0x301;    // byte: 0 player 1, 1 player 2 (Super Turbo's +381)
constexpr uint32_t kPerson = 0x328;  // byte: 1 for a person, 0 the computer (Super Turbo's +38E)
// The character's version (the PL TYPE choice: 00C8E4 takes it from the table at 00C902 by the cursor, +90), each a
// game of the series as it played: chosen with each in turn and read back
constexpr uint32_t kVersion = 0x32A;
enum : uint8_t { kSuperTurbo = 0x00, kSuper = 0x02, kWorldWarrior = 0x04, kChampion = 0x06, kHyperFighting = 0x08 };
constexpr uint32_t kCharacter = 0x32B; // byte: who the player is (kCharacterNames; Super Turbo's +391)
// Akuma is a seventeenth character of the Super Turbo version (its colour table, 0C3E70, has seventeen characters for
// it, sixteen for Super, twelve for Champion Edition and Hyper Fighting, eight for World Warrior), not a flag as there
constexpr uint8_t kAkuma = 16;
// The set of colours: 0-5 the button that chose (light punch first), 6 Start (RAM dumps choosing Ryu with each); the
// palette routine (0C3DF4) takes the version's table, then the character's, then this
constexpr uint32_t kColourChoice = 0x32C;
constexpr uint32_t kChosen = 0x8C; // byte: 1 once the player has chosen a character (00C364)
constexpr uint32_t kOpponent = 0x344;  // word: the other player's record (Super Turbo's +39E)

// The records of what the players throw: eight of 0xC0 bytes from FF9554 (cleared at 0038AC), +00 1 for one in use,
// the thrower's record a word at +26
constexpr uint32_t kThrown = 0xFF9554, kThrownSize = 0xC0, kThrownOwner = 0x26;
constexpr int kThrownRecords = 8;

// The control panel as the game read it this frame, a word a player (FBNeo's script reads player 1's at FF8076)
constexpr uint32_t kControls[2] = {0xFF8076, 0xFF807A};

// The player can do anything: in charge of itself, standing, walking or crouching, or in its guarding pose before a hit
inline bool CanAct(uint32_t player) {
    using namespace cps2recomp;
    const uint8_t state = Ram8(player + kState);
    return Ram8(player + kFighting) == 2 && (state == kStand || state == kCrouch || (state == kGuard && !Ram8(player + kGuarded)));
}

// --- routines (addresses in the program) ---

// A hit's damage off the victim's health: sub.w d6,0x2A(a2), D6 the damage, A2 the victim; 0C26A0 in the hit of a
// player's attack and 0C2C90 in that of something thrown (Super Turbo's 0BE646 and 0BEBA6)
constexpr uint32_t kHitDamage = 0x0C26A0, kThrownDamage = 0x0C2C90;
// A throw's damage, D4 off the victim's (A3) health: at once (0049AE, 004A52), or put by in the victim's word kDamagePut
// (004AFE) and taken off as the throw ends (Super Turbo's 00490A, 0049B0, 004A58); and the holds' and the throws that
// go on (004E3E, 004F4C: Super Turbo has them too, 004D9E and 004EAA, but its made-up play never reached them), hooked
// where D4 is made (004E18, 004F2E), before the knockout's voice is chosen by it. Found as the writers of player 1's
// health over made-up play with no damage: with the five hooks only the hits' routines (with nothing) and the round's
// set-up write it.
inline constexpr uint32_t kThrowDamage[] = {cps2recomp::HookAt(0x0049AE), cps2recomp::HookAt(0x004A52), cps2recomp::HookAt(0x004AFE),
                                            cps2recomp::HookAt(0x004E18), cps2recomp::HookAt(0x004F2E)};
constexpr uint32_t kDamagePut = 0x78;

// A player's fight step goes on by kState: a person's (066BBE) and the computer's (06A246)
constexpr uint32_t kPlayerStep = 0x066BBE, kComputerPlayerStep = 0x06A246;
// A hit has been decided and taken (A6 the attacker, A2 the victim): a player's attack's (0C26D4), a thrown thing's
// (0C2CDA)
constexpr uint32_t kHitTaken = 0x0C26D4, kThrownHitTaken = 0x0C2CDA;
// A hit's damage has a random part (0C2FD4 on, Super Turbo's 0BEE40): a byte of a row of 32 of the table at 0E7CDC
// picked by the game's random number, added to the damage (D6), D0 the byte, A1 the table, D2 the row's start plus the
// pick; the sum is made 1 where it comes to nothing or less (0C2FEE), and both ways go on at 0C2FF0. A throw's alike
// (D4: 004C7A, 004C82, 004C84). Read in the listing.
constexpr uint32_t kHitMadeOne = 0x0C2FEE, kHitSummed = 0x0C2FF0, kThrowMadeOne = 0x004C82, kThrowSummed = 0x004C84;
// The throw test (Super Turbo's 0046E4; here 00478C on) compares the thrower's box, words of its record put there for the
// throw being tried, with the other's place to be thrown by, as Super Turbo's does (src/training.cpp; the offsets are
// Super Turbo's): across (+64, turned about with the facing) and up (+66), half its width (+68) and height (+6A); the
// other's half width (+6C), height (+6E), and +2AA across from it while +2A7 is set. Read in the listing.
constexpr uint32_t kThrowX = 0x64, kThrowY = 0x66, kThrowHalfWidth = 0x68, kThrowHalfHeight = 0x6A;
constexpr uint32_t kThrowableHalfWidth = 0x6C, kThrowableY = 0x6E, kThrowableMoved = 0x2A7, kThrowableX = 0x2AA;
// Stunned: a byte, 1 while the player is (068EC2 sets it); a person mashing shortens it (+125 counts it: 068F48), as in
// Super Turbo
constexpr uint32_t kStunned = 0x124;
// A throw going on: 1 in the thrower's record, FF in the one thrown (00480E; cleared at 004EE2), who stays in its state
constexpr uint32_t kHeldBy = 0x63;
// Softening a throw (004870): the one thrown and the thrower both of Super Turbo's version, the stick to either side and
// a medium or heavy button new within 0x0E steps of the throw's start sets +2BD, and the throw does half its damage
constexpr uint32_t kSoftened = 0x2BD;
// The guard decision as a hit lands (0C3044 on): by the attacker's version (kVersion), Super Turbo's (a jump table by the
// attack's +0F at 0C307E, 0x4C bytes, to five routines: most attacks 0C310C, standing only 0C3124, crouching only 0C312A
// and 0C3132, either way 0C3130) or, for the World Warrior's, Champion Edition's and Hyper Fighting's, the classic games'
// (from 0C3138: the attack's +0B through a row of four bytes for the guarding pose, the rows' places at 0C31B4). The
// victim's guarding pose is its frame's +11 (1 standing, 2 crouching); the hurt box hit is put at A5-5C04. Read in the
// listing.
constexpr uint32_t kGuardTypes = 0x0C307E, kGuardTypeBytes = 0x4C, kGuardMost = 0x0C310C, kGuardStanding = 0x0C3124,
                   kGuardCrouching = 0x0C312A, kGuardCrouching2 = 0x0C3132, kGuardRows = 0x0C31B4;
constexpr uint32_t kFrameGuardPose = 0x11;
constexpr uint32_t kAttackLevel = 0x0B, kAttackType = 0x0F; // bytes of an attack box: its height (the classic way) and kind
// The match's first step: the characters are chosen (00B1FA, the match step going to 2)
constexpr uint32_t kMatchBegins = 0x00B1FA;

// --- the picture ---

// Objects by what they are (for Video::hideObject; found by hiding each colour set in turn in fights on every stage):
// 01-05 are player 1's and 06-0A player 2's (the character and what it throws: 0C3DF4 copies its set of five palettes
// to 900020 and 9000C0; Super Turbo's second player was at 10-14), 18 the players' shadows, and the panel 00 (free
// play, credits), 0C (the bars and names), 0D (the score, the clock), 13 and 15 (the gauges' words), 16 and 17 (the
// faces), 19 (the gauges), 0E (the flags of the screens between the fights). The rest are the stage's own pieces (0B and
// 1A-1F seen: the hanging goods of Chun-Li's, the signs of E. Honda's), as in Super Turbo. (A ladder of every stage with
// each set hidden in turn, a picture every 150 frames, against one with none hidden.)
inline bool IsFighter(uint16_t attributes) {
    const unsigned set = attributes & 0x1F;
    return set >= 0x01 && set <= 0x0A;
}
inline bool IsShadow(uint16_t attributes) {
    return (attributes & 0x1F) == 0x18;
}
inline bool ThePanel(uint32_t, uint16_t attributes) {
    switch (attributes & 0x1F) {
    case 0x00: case 0x0C: case 0x0D: case 0x0E: case 0x13: case 0x15: case 0x16: case 0x17: case 0x19:
        return true;
    default:
        return false;
    }
}
inline bool IsStagePiece(uint16_t attributes) {
    return !IsFighter(attributes) && !IsShadow(attributes) && !ThePanel(0, attributes);
}
inline bool NotTheStage(uint32_t, uint16_t attributes) {
    return !IsStagePiece(attributes);
}
// A player's own colour sets (its side's), as a mask of bits
constexpr uint32_t kPlayerSets[2] = {0x3Eu, 0x7C0u};

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

} // namespace hsf2

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
