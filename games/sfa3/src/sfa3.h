// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Street Fighter Alpha 3: names for what is known of the game's memory and routines (found by experiment: PROGRESS.md
// says how). The game is built as Street Fighter Alpha 2 is (its variables reached from A5 = FF8000, the players'
// records 0x400 apart from FF8400 with the same four steps at their head, many of the variables at the same places),
// so many names are the same as there; each one here was checked in this game, and where a field is somewhere else or
// means something else, it says so.
#pragma once

#include "game.h"
#include "lift.h"
#include "machine.h"

#include <cstdint>

namespace sfa3 {

using cps2recomp::M68K;

// --- where the game is ---

// Words from A5, each a step (in twos) of the one before. The first goes 2 to 8 while the board starts, then 0 and
// its show (6 the notices on, 0x10 the introduction, the title at its end, ...), 2 once a game is being played
constexpr uint32_t kMode = 0xFF8000, kGameStep = 0xFF8004, kMatchStep = 0xFF8008, kRoundStep = 0xFF800C;
// The introduction's own count of frames (a word, from 0 as it begins)
constexpr uint32_t kIntroFrames = 0xFF8006;

// Credits paid for and not yet used, and the coins counted towards the next (RAM dumps around coins put in: the
// board starts at two coins a credit)
constexpr uint32_t kCredits = 0xFF8076, kCoins = 0xFF80C3;
// The coin mode (the test menu's, from the settings record: 0018F2) and what the board's start sets from it
// (0018F6 on): mode 0x12 is free play, which sets kFreePlay and both coin words to 0101
constexpr uint32_t kCoinMode = 0xFF8097, kFreePlay = 0xFF809C, kCoinsPerCredit1 = 0xFF8072, kCoinsPerCredit2 = 0xFF807A;
constexpr uint8_t kCoinFreePlay = 0x12;
constexpr uint16_t kFreePlayCoins = 0x0101;

// Where the board is (a word, the region doubled: 0 Japan, 2 USA), which picks the board's factory settings (the table
// at 00235A: seven records of 0x30, the test menu's settings record's layout), and the test menu's regulation
// (System configuration; the record's +06): 0 red blood and the full effects, the factory setting in Japan, 1 elsewhere
// (01F9C4 whitens the blood's colours, palette 25's 13 and 14). Both from HSFA's index (Rickow/HSFA, MIT), seen here:
// 0002 and 1 on this set, and red blood with A5+BB held at 0.
constexpr uint32_t kRegion = 0xFF8090, kRegulation = 0xFF80BB;

// Step values: FF8004 the match (2 choosing), FF8008 a round, FF800C the round being fought (2 and 4 its
// introduction, 8 on its end), from --watch over a versus game
enum : uint16_t { kGameChoosing = 2, kGameMatch = 4, kMatchRound = 6, kRoundFought = 6 };

// The round's clock: seconds left (99 at most, set at 0090D0) and the frames of the second, taken at kClockTick
// (009BFE: `subq.b #1` off the frames; at nothing a second goes, 009C08)
constexpr uint32_t kClock = 0xFF8109, kClockFrames = 0xFF810A;
constexpr uint8_t kClockFull = 99;
constexpr uint32_t kClockTick = 0x009BFE;
// From A5: the round's result (009BEE: 1 player 1 won, 2 player 2), time over (009C14)
constexpr uint32_t kRoundResult = 0xFF810C, kTimeOver = 0xFF810D;
// From A5: a bit for each player whose pose at the round's end is over (029E10, by the player's +101; the next round
// waits for them: 00A0F8), and the ending to be shown (00A92C, from A5+132: the character's number; 0x20 at 025DF6),
// both named in HSFA's index and seen here (03 at the end of a round of the ladder; 18, E. Honda's, at its end)
constexpr uint32_t kPosesOver = 0xFF810B, kEnding = 0xFF81C6;

// Who a player is (a player's byte kCharacter): each number seen in a fight with player 2's byte held at it from the
// start (and its name plate). 12, 14 and 17 are not characters of this version (wrong name plates, a broken picture,
// a Ryu in other colours), 13 is Gen's other stance, which he changes to and back in a fight; from 20 on the board
// resets.
constexpr int kCharacters = 0x20;
inline constexpr const char *kCharacterNames[kCharacters] = {
    "Ryu",     "Ken",   "Akuma",  "Charlie", "Chun-Li", "Adon",     "Sodom", "Guy",   "Birdie", "Rose",  "M. Bison",
    "Sagat",   "Dan",   "Sakura", "Rolento", "Dhalsim", "Zangief",  "Gen",   nullptr, "Gen",    nullptr, "Balrog",
    "Cammy",   nullptr, "E. Honda", "Blanka", "R. Mika", "Cody",    "Vega",  "Karin", "Juli",   "Juni"};
// The 28 fighters, in the order the menus offer them (the character choice's own: the grid's rows from the top)
constexpr int kFighters = 28;
inline constexpr uint8_t kFighterNumbers[kFighters] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
                                                       0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x15, 0x16,
                                                       0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F};

// The stage a match is fought on: a word, the stage's number doubled (A5+100, from A5+174 as a round is set up:
// 0090B8). The match is made at 00B778, which takes the stage from a character (00B7FA: the table at 00BBA8, a
// character's stage; M. Bison's, Balrog's, Juli's and Juni's is 15, and 0A, the final stage, is only the match
// making's). Each seen with A5+174 held at its number: 12-14 and 17 are copies of others, and from 1E on too.
constexpr uint32_t kStage = 0xFF8100, kStageNext = 0xFF8174;
constexpr int kStages = 26;
inline constexpr uint8_t kStageNumbers[kStages] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C,
                                                   0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x15, 0x16, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D};
inline constexpr const char *kStageNames[kStages] = {
    "Ryu's (the castle at sunset)", "Ken's (the resort)",          "Akuma's (the cave)",          "Charlie's (the hangar)",
    "Chun-Li's (the market)",       "Adon's (the temple ruins)",   "Sodom's (the street at night)", "Guy's (the arches)",
    "Birdie's (the scrapyard)",     "Rose's (the stairway)",       "The final stage (the skulls)", "Sagat's (the reclining Buddha)",
    "Dan's (the playground)",       "Sakura's (the shopping street)", "Rolento's (the tank depot)", "Dhalsim's (the temple)",
    "Zangief's (the furnace)",      "Gen's (the harbour)",         "M. Bison's (the rainy garden)", "Cammy's (the windmills)",
    "E. Honda's (the bath house)",  "Blanka's (the jungle river)", "R. Mika's (the wrestling show)", "Cody's (the jail)",
    "Vega's (the cage)",            "Karin's (the shopping street at dusk)"};
// The menus' stage number (1 the first) for the game's, and back
inline int StageOfNumber(int number) {
    for (int n = 0; n < kStages; n++) {
        if (kStageNumbers[n] == number) {
            return n + 1;
        }
    }
    return 0;
}

// The sound queue: a ring of sixteen-byte requests at A5+5E80, the word at A5+6E80 the place of the next (0030D4 sends
// the oldest, from A5+6E82, to the sound CPU); FF05 asks for the music to stop before another
constexpr uint32_t kSoundQueue = 0xFFDE80, kSoundQueueEnd = 0xFFEE80;
constexpr uint16_t kTuneOff = 0xFF05;
// Each stage's tune (kStageNames' order): a fight's tune is a character's theme (003A30, the table at 003ABA), so a
// stage's is its character's, and the final stage's the final fight's (music.cpp); and the character choice's and the
// one before a match (the character's introduction, which is this game's versus screen)
inline constexpr uint16_t kStageTunes[26] = {0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x20, 0x14, 0x15,
                                             0x16, 0x17, 0x18, 0x19, 0x1A, 0x13, 0x1F, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26};
constexpr uint16_t kTuneChoose = 0x02, kTuneVersus = 0x08;

// The camera: the picture's left edge in the stage and its bottom (the stage's own scroll screen's record, FF8280),
// with every place the game keeps it across (found moving together in RAM dumps), as Alpha 2's. 256 to 640 across;
// the players stand from 280 to 1000 and begin a round at 544 and 736 (dumps with them walked to each end).
constexpr uint32_t kCameraX = 0xFF8290, kCameraY = 0xFF8294;
inline constexpr uint32_t kCameraCopies[] = {0xFF8290, 0xFF8026, 0xFF82A0, 0xFF82A8};
constexpr int kCameraLeft = 256, kCameraRight = 640;
constexpr uint16_t kStartX[2] = {544, 736};
constexpr int kStageLeft = 280, kStageRight = 1000;

// --- the players ---

// A player's record; the second player's follows the first's (and two more after them, for the dramatic battle's
// third fighter: 01BF22 clears four)
constexpr uint32_t kPlayer1 = 0xFF8400, kPlayerSize = 0x400;
inline uint32_t Player(int n) {
    return kPlayer1 + static_cast<uint32_t>(n) * kPlayerSize;
}
constexpr uint32_t kActive = 0x00; // byte: 1 while the record is in use
// Four bytes of steps, each a step of the one before (2 in a fight)
constexpr uint32_t kState = 0x04, kControl = 0x05, kAction = 0x06, kActionStep = 0x07;
// Health: what the player has (a hit takes it off at once, 0240D2; below zero: knocked out, 023F1C) and what the bar
// shows (0B9174 brings it down after). Full is 0x90 (027BAE, as a player is set up).
constexpr uint32_t kHealth = 0x50, kHealthShown = 0x52; // words
constexpr uint16_t kHealthFull = 0x90;

constexpr uint32_t kFacesRight = 0x0B; // byte: not zero when facing right (box offsets, kept as if facing left, are turned about)
constexpr uint32_t kColourSet = 0x0D;  // byte: the first of the player's colour sets in the palette buffer
constexpr uint32_t kX = 0x10;          // word: where it stands across the stage (a long, 16 bits of fraction)
constexpr uint32_t kY = 0x14;          // word: height, upwards; kGround on the floor
constexpr uint16_t kGround = 0x28;
constexpr uint32_t kFrame = 0x1C;      // long: the animation frame being shown (in the ROM; 02A770 sets one)
constexpr uint32_t kFrameTime = 0x32;  // byte: frames it is still shown for (02A7EA steps it)
constexpr uint32_t kInAir = 0x31;      // byte: not zero while in the air (01 a jump, FF thrown up by a hit)
constexpr uint32_t kOpponent = 0x38;   // word: the other player's record (FFxxxx)
constexpr uint32_t kJumpWay = 0x3C;    // byte: a jump's way (1 forward, FF back, 0 straight up)
// Longs, 16 bits of fraction: the speeds across and up, and what is taken off them each step (traced through a jump:
// up 0x70000 as it leaves the floor, less 0x5000 a step; 01B0C4 flies by them)
constexpr uint32_t kSpeedX = 0x40, kSpeedY = 0x44, kPullX = 0x48, kPullY = 0x4C;
// byte: how a hit has it reel, by the hit's reaction kReaction (028BE6); 8 a knock-down (the table at 0292A0: its steps,
// +07, 2 flying, 6 landing, 8 lying while +DF counts down, 0A getting up), 4 hit in the air (028F02), 6 hit up into the air (02B330 lets it recover there, as 8 while flying), 0C stunned (its time
// the word +3A: 0299EE; the knock-down that stuns goes on to it from its end, 0297D4)
constexpr uint32_t kReelKind = 0x54;
constexpr uint8_t kReelKnockedDown = 8, kReelInAir = 4, kReelJuggled = 6, kReelStunned = 0x0C;
constexpr uint32_t kReaction = 0x55;   // byte: the last hit's reaction, read as the reeling begins (028BE6; FF guarded, 0D a sweep's
                                       // knock-down, 12 knocked down into the air, as a knock-out is)
constexpr uint32_t kHalfWidth = 0xA6;  // word: half the player's width (what the picture's edges keep in: 02ADC4)
constexpr uint32_t kGuardedHit = 0x55; // byte: FF when the last hit was guarded (02336C)
constexpr uint32_t kSide = 0x59;       // byte: which side it is on (a player's own things share it)
constexpr uint32_t kHitStop = 0x5F;    // byte: frames the player stands frozen for while a hit lands (both players)
constexpr uint32_t kCharacter = 0x102; // byte: who the player is (kCharacterNames; 027C54 sets the character up by it)
constexpr uint32_t kSpecialGauge = 0x11E; // word: the super combo gauge (0x90 full in every ism: 02EFDC; the A-ism's levels
                                          // 0x30 each)
constexpr uint16_t kSpecialGaugeFull = 0x90;
constexpr uint32_t kColourChoice = 0x128; // byte: the colours, 0 when the ism was chosen with a punch, 1 with a kick
constexpr uint32_t kComputer = 0x125;  // byte: 1 while the game plays this player (027C94 takes the computer's way for it;
                                       // found 1 for the opponent in a one-player game, 0 in a two-player one)
// The V-ism's custom combo: going on while kCustomCombo is set, its time left a word (027CEC: one less every three
// frames, the gauge brought down with it)
constexpr uint32_t kCustomCombo = 0xB9, kCustomComboTime = 0xBA;
// The stun (023C30, in a hit's landing): what has built up, how much stuns the player (40 at first, 5 more after
// each stun), the frames until it drains again (0xB4 after a hit: 0232B2), and set while stunned
constexpr uint32_t kStun = 0x2CC, kStunMost = 0x2CD, kStunTimer = 0x2CB, kStunned = 0x2CE; // bytes
// The animations of the player's character: four lists of word offsets to the frames (the setting up of a character
// makes them; 02A708 takes from the first, 02A758 the second, 02A75E the third, the reelings', 02A710 the fourth, the
// attacks': unless +EB is set, it first takes the attack's +23E and +293 from the character's table, by its ism, in the
// first of the tables at 0DCB12; HSFA's index named 02A710 for the fourth list, where this had only its tail, 02A74E)
constexpr uint32_t kAnimations[4] = {0x2B0, 0x2B4, 0x2B8, 0x2BC}; // longs
// The control panel as the player holds it, by its facing: bit 0 set while it holds away from the other (and can
// guard: 02AB34)
constexpr uint32_t kHeld = 0x37D;

// Boxes (the hit test 022F86, the boxes' test 0246FC). The record has the tables (longs kHeadBoxes, kBodyBoxes,
// kLegBoxes, kPushBoxes: eight bytes a box; kAttackBoxes: thirty-two an attack, the box first and then what the attack
// does), and the numbers of the boxes in use: those it can be hit in and its push box are the record's bytes
// kBoxNumbers + 0-3 (the animation frame's +08 names a set of four: 02A77A), its attack the frame's byte kFrameAttack. A
// box: words, its middle from where the object stands (across: towards the way it faces), half its width and height.
constexpr uint32_t kHeadBoxes = 0x90, kBodyBoxes = 0x94, kLegBoxes = 0x98, kPushBoxes = 0x9C, kAttackBoxes = 0xA0; // longs
constexpr uint32_t kBoxNumbers = 0xC8;  // bytes: head, body, legs, push
constexpr uint32_t kFrameAttack = 0x09; // byte of the animation frame
constexpr uint32_t kFramePose = 0x0D;   // ... its guarding pose (1 standing, 2 crouching, 3 in the air: 023E26)
constexpr uint32_t kFrameSize = 0x14;   // a player's frames follow one another
constexpr uint32_t kBoxSize = 8, kAttackBoxSize = 0x20;
constexpr uint32_t kBoxX = 0, kBoxY = 2, kBoxHalfWidth = 4, kBoxHalfHeight = 6;
// What the attack box's entry says of guarding it (023E26): a bit for each pose that cannot guard it (bit 0
// standing, 1 crouching, 2 in the air)
constexpr uint32_t kAttackUnguardable = 0x15;
// What the players throw: two pools of twelve records of 0x100 (the guard-range test, 02AB24, goes through both),
// +00 1 for one in use, laid out as a player's for its boxes
constexpr uint32_t kThrown = 0xFF9400, kThrownOther = 0xFFA000, kThrownSize = 0x100;
constexpr int kThrownRecords = 12;

// The damage of a hit or a throw (0240BC: D4 off the health of the player A1; one routine for all of them: the only
// writer of the health in fights over three made-up games of 30,000 frames, both players)
constexpr uint32_t kTakeDamage = 0x0240D2;

// The control panel as the game read it this frame, from A5: a word a player, buttons above (punches in bits 8-10,
// kicks in 12-14) and the stick below (bit 0 right, 1 left, 2 down, 3 up)
constexpr uint32_t kControls[2] = {0xFF8058, 0xFF805C};
// Who is in a game (bit 0 player 1, bit 1 player 2), and who has chosen (its character and its ism): RAM traced
// through the character choice
constexpr uint32_t kPlayersIn = 0xFF80A8, kPlayersChosen = 0xFF80AC;

// --- routines (addresses in the program) ---

constexpr uint32_t kPlayerDispatch = 0x027CBA; // a fighting player's routine goes on by kControl (027C94: once a step for
                                               // each player, a person's and the computer's; the four records in an
                                               // order drawn by the random number each step, 0277EE: HSFA's index)
constexpr uint32_t kSetAnimation = 0x02A762;   // D0: the animation of that number in the list at A0
constexpr uint32_t kSetReelAnimation = 0x02A75E; // D0: the animation of that number in the reelings' list
constexpr uint32_t kAnimate = 0x02A7EA;        // the animation on by a step
constexpr uint32_t kFly = 0x01B0C4;            // a step through the air: the place by the speeds, the speeds by their pulls

// The player is free to do anything: in charge of itself, and standing, crouching, walking or in its guarding pose
enum : uint8_t { kControlOwn = 0x00, kControlReeling = 0x02 };
enum : uint8_t { kActionStand = 0x00, kActionCrouch = 0x02, kActionWalk = 0x04, kActionJump = 0x06, kActionAttack = 0x0A, kActionGuardPose = 0x0C };
inline bool CanAct(uint32_t player) {
    using namespace cps2recomp;
    const uint8_t action = Ram8(player + kAction);
    return Ram8(player + kControl) == kControlOwn &&
           (action == kActionStand || action == kActionCrouch || action == kActionWalk || action == kActionGuardPose);
}

// --- for rewrites and hooks: memory and registers by name ---

inline uint8_t Mem8(M68K &c, uint32_t address) {
    return static_cast<uint8_t>(c.Read(address, 0));
}
inline uint16_t Mem16(M68K &c, uint32_t address) {
    return static_cast<uint16_t>(c.Read(address, 1));
}
inline uint32_t Mem32(M68K &c, uint32_t address) {
    return c.Read(address, 2);
}
inline void SetLow16(uint32_t &reg, uint32_t value) {
    reg = (reg & 0xFFFF0000) | (value & 0xFFFF);
}
inline void SetLow8(uint32_t &reg, uint32_t value) {
    reg = (reg & 0xFFFFFF00) | (value & 0xFF);
}

} // namespace sfa3

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
// Objects by what they are (modes.cpp; for Video::hideObject): the panel, the stage's own pieces, all but those
bool ThePanel(uint32_t tile, uint16_t attributes);
bool IsStagePiece(uint16_t attributes);
bool NotTheStage(uint32_t tile, uint16_t attributes);
} // namespace cps2recomp
