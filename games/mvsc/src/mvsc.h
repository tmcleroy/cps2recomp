// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Marvel vs. Capcom: names for what is known of the game's memory and routines, each found by experiment in this game
// (PROGRESS.md). The game is Marvel Super Heroes vs. Street Fighter's program carried on (tools/portmap.py places a
// third of that game's instructions here), its variables 0x800 lower again (FF4800 there is FF4000 here) and its
// players' records at FF3000; each place below was found again here.
#pragma once

#include "game.h"
#include "lift.h"
#include "machine.h"

#include <cstdint>

namespace mvsc {

using cps2recomp::M68K;

// --- where the game is ---

// A byte: 8 while a round is being fought (the damage routines test it before taking health)
constexpr uint32_t kFight = 0xFF4000;
enum : uint8_t { kFighting = 0x08 };

// A byte, set once a game is being played, and the byte after it: who is in, bit 0 player 1, bit 1 player 2 (RAM
// dumps of one- and two-player games; the attract mode's fights set 3)
constexpr uint32_t kPlaying = 0xFF4100, kPlayersIn = 0xFF4101;
// Words, each a step of the one before (traced with --watch over a game from Start): the game's (4 while the players
// choose their characters, 00AA5C; 8 in a match, 00BC1E), and the match's
constexpr uint32_t kGameStep = 0xFF4104, kMatchStep = 0xFF4106;
enum : uint16_t { kGameChoosing = 4, kGameMatch = 8 };

// The round's clock: seconds left, a BCD byte (99 at a round's start; a second off at 012BA0, sbcd), and the frames
// left of the second after it (counted down at 012B84). Found by --watch through a fight.
constexpr uint32_t kRoundClock = 0xFF4008, kClockFrames = 0xFF4009;
// A word counted up once a step of the game (00AB60): what the game does every other step goes by bit 0 of its low byte
// (00CAA4, 035EEA)
constexpr uint32_t kStepCount = 0xFF4488;
// The Duo Team Attack: each side's time left in it, a word (player 1's here, player 2's after it), taken one off a step
// at 015F68 while both its characters stand; and a byte, bit 0 player 1's side's and bit 1 player 2's, set while it goes
// on (the routine at 015F34 clears the side's bit and sets it again while there is time left)
constexpr uint32_t kDuoTime = 0xFF4034, kDuoOn = 0xFF4038;

// Credits paid for and not yet used, a byte (RAM dumps after coins)
constexpr uint32_t kCredits = 0xFF4499;

// The game's own free play, one of the operator's coin modes (the test menu's System page, its first option: 00-08
// one coin for 1-9 credits, ..., 12 free play). The board's start reads the settings record (kSettings, 007E7A) and
// keeps the mode in kCoinMode; free play sets kFreePlay (007F36), which the code that lets a player in tests in place
// of the credits. Set alone it is the whole of free play: the title says FREE.
constexpr uint32_t kCoinMode = 0xFF44A5, kFreePlay = 0xFF449B;
constexpr uint8_t kCoinFreePlay = 0x12;

// The operator's settings as the settings memory holds them, read into the game's variables at the board's start
// (007E7A, called from 000FAC, so the call returns to 000FB2, where board.cpp and title.cpp hook). The offsets and
// where each goes are in board.cpp.
constexpr uint32_t kSettings = 0xFFF648;
// The show's scenes are run by a task whose record is at FF2666 (--watch on it through the show): its step byte at +4
// is 2 while a scene runs, 0 between scenes. The title's scene (0D118C on, from frame 1,420 of the board's start) takes
// its first step at 0D1302 (the title is drawn on the scroll screens).
constexpr uint32_t kShowStep = 0xFF266A;

// --- the players ---

// Four records, 0x400 long: player 1's first character at FF3000, player 2's at FF3400, then their second characters
// at FF3800 and FF3C00 (the FBNeo training script for this set names them; confirmed here by the characters and health
// each shows, and the record's long +284, the partner's). As in Marvel Super Heroes vs. Street Fighter, a tag does not
// swap the records' contents: the game keeps a long a side, the record of the character on point (FF40C8 player 1's,
// FF40D0 player 2's: traced over the tag-team movies, and the camera's step reads them, 0CB2B2). A record's byte +281
// is 1 while it rests.
constexpr uint32_t kPlayer1 = 0xFF3000, kPlayerSize = 0x400, kPartners = 0x800;
constexpr uint32_t kOnPoint = 0xFF40C8;
// The record of a side's character on point now (the first character's until the game has made the round's records)
inline uint32_t Player(int n) {
    const uint32_t at = cps2recomp::Ram32(kOnPoint + static_cast<uint32_t>(n) * 8) & 0xFFFFFF;
    return at == kPlayer1 + static_cast<uint32_t>(n) * kPlayerSize || at == kPlayer1 + kPartners + static_cast<uint32_t>(n) * kPlayerSize
               ? at
               : kPlayer1 + static_cast<uint32_t>(n) * kPlayerSize;
}
// ... and of its partner
inline uint32_t PartnerOf(int n) {
    const uint32_t point = Player(n);
    return point < kPlayer1 + kPartners ? point + kPartners : point - kPartners;
}
// Which side a record is (0, 1), or -1 for none of the four
inline int SideOf(uint32_t record) {
    record &= 0xFFFFFF;
    if (record < kPlayer1 || record >= kPlayer1 + 4 * kPlayerSize || (record - kPlayer1) % kPlayerSize) {
        return -1;
    }
    return static_cast<int>((record - kPlayer1) / kPlayerSize) & 1;
}
// Health, a word (0x90 full); a hit takes it at 01B74E and 01BEA0 (sub.w d5,0x270(a6), the victim in A6)
constexpr uint32_t kHealth = 0x270;
constexpr uint16_t kHealthFull = 0x90;
constexpr uint32_t kRedHealth = 0x27A; // what comes back while the character rests (cleared with the health at 01B760)
constexpr uint32_t kPartner = 0x284;   // long: the record of the same side's other character (0131E2)
// A byte: which of the side's picks the record is (0 the one chosen first, 1 the second: kPicks; the records are not
// swapped at a tag, traced through the tag-team movies)
constexpr uint32_t kWhichPick = 0x280;
// The hyper combo gauge, the side's: a byte, its levels (three at most), and a word, the bar towards the next (0x90
// full) (the FBNeo training script names the levels; the bar is the word before, as in the games before)
constexpr uint32_t kGaugeLevel = 0x274, kGaugeBar = 0x272;
constexpr uint8_t kGaugeFull = 3;
constexpr uint16_t kGaugeBarFull = 0x90;

// Where a player stands: a long, the word at +0C across (with 16 bits of fraction after it) and the word at +10 down
// (on the floor at Ground(); smaller is higher)
constexpr uint32_t kX = 0x0C, kY = 0x10;
// Longs, 16 bits of fraction: the speed across, and the speed down and what is added to it each step
constexpr uint32_t kSpeedX = 0x14, kSpeedY = 0x1C, kPullY = 0x20;
// A byte: bit 0 set facing right, bit 1 turned upside down (the hitbox script for this engine names it; confirmed by
// the players' sides)
constexpr uint32_t kFlip = 0x4B;
// Who the character is: a word, even (each number put in player 1's picks, FF40B0, and seen in a fight with its name
// on the bar; 22 is Onslaught, the last opponent (player 2's picks in its fight), and 00 its second form, whose
// pictures come after the others' in the ROM (both fight as Ryu put in the picks: the game makes their fights its own
// way); 2E on reset the board)
constexpr uint32_t kCharacter = 0x52;
constexpr int kCharacters = 23;
inline constexpr const char *kCharacterNames[kCharacters] = {
    "Onslaught (second form)", "War Machine", "Captain America", "Hulk",           "Wolverine",   "Gambit",      "Venom",
    "Spider-Man",  "Roll",        "Ryu",             "Captain Commando", "Chun-Li",   "Jin",         "Zangief",
    "Strider Hiryu", "Morrigan",  "Mega Man",        "Onslaught", "Red Venom", "Orange Hulk",
    "Gold War Machine", "Shadow Lady", "Lilith"};
enum : uint16_t {
    kOnslaught2 = 0x00, kWarMachine = 0x02, kHulk = 0x06, kVenom = 0x0C, kRoll = 0x10, kChunLi = 0x16, kMorrigan = 0x1E,
    kMegaMan = 0x20, kOnslaught = 0x22, kRedVenom = 0x24, kOrangeHulk = 0x26, kGoldWarMachine = 0x28, kShadowLady = 0x2A,
    kLilith = 0x2C
};
// The ones a player chooses on the board, with or without a code (not the last opponent)
inline bool Choosable(int number) {
    return (number & 1) == 0 && number >= kWarMachine && number <= kLilith && number != kOnslaught;
}
// A word: who is in charge of the character, 0 itself (the player)
constexpr uint32_t kControl = 0x06;
// A byte: what the character is doing
constexpr uint32_t kAction = 0xB0;
enum : uint8_t {
    kActionStand = 0x00, kActionWalk = 0x02, kActionJump = 0x06, kActionCrouch = 0x0C, kActionGuard = 0x24,
    kActionCrouchGuard = 0x26, kActionAttack = 0x28, kActionHit = 0x36
};
constexpr uint32_t kAttackPhase = 0x64; // byte: 80 an attack going on, 40 its recovery
constexpr uint32_t kFrame = 0x34;       // long: the animation frame being shown (in the ROM)
constexpr uint32_t kHitStop = 0x85;     // byte: steps both players stay frozen as a hit lands, counted down
// The character's boxes (the hitbox script names their places, the same as X-Men vs. Street Fighter's): a long, the
// table of its boxes, and words, each a box's number in it: two attacks, four places it can be hit and one it can be
// thrown in; a byte, its push box's number in kPushTable. Bytes: attacks hit only while kCanHit is not 0, and it is hit
// only while kCanBeHit is not 0.
constexpr uint32_t kBoxTable = 0x6C, kAttackBoxes = 0x70, kHurtBoxes = 0x74, kThrowableBox = 0x7C, kPushBox = 0xB4;
constexpr uint32_t kCanHit = 0x82, kCanBeHit = 0x83;
constexpr uint32_t kPushTable = 0x0E6FEE;
// What they throw: for each player a list of the records (longs, the newest last) ending at kThrownList (player 2's
// 0x158 on)
constexpr uint32_t kThrownList = 0xFFDF1A, kThrownListSpace = 0x158;

// The picture follows the stage's scroll layer that moves with the fighters: a record (one of three, by the stage, the
// byte kStage), with the camera's place at +0C across and +10 down. Its step (0CB2A2 across, 0CB4E2 down) works on a
// copy of them (+76 and +78), keeps the copy between the ends at +68/+6A across and +6C/+6E down (0CDBB6, 0CDBDA) and
// puts it back (0CDBCE, 0CDBF2), as in Marvel Super Heroes vs. Street Fighter. The picture's left edge is the word
// across + 0x40, its top the word down.
constexpr uint32_t kStage = 0xFF4113;
// The stages, by the byte kStage (0-9; each seen with a training session's stage held at it; named for what they show).
// The one-player game's stages come from a table by a count (0E324C, or 0E326C for some games: 00C2B6); Onslaught's are
// 9 (00B7FC) and 10, where the camera's record is not kept (its first form's city, then space), and 11 is none.
constexpr int kStages = 10;
inline constexpr const char *kStageNames[kStages] = {
    "The bathhouse", "The haunted hall", "The airship", "The laboratory", "The temple", "The command centre",
    "The wreckage",  "The city street",  "The canyon",  "Onslaught's city"};
inline uint32_t CameraRecord() {
    static constexpr int16_t kOffsets[16] = {-0x3DA0, -0x3DA0, -0x3DA0, -0x3D20, -0x3D20, -0x3DA0, -0x3DA0, -0x3E20,
                                             -0x3E20, -0x3CA0, -0x3DA0, -0x3DA0, -0x3DA0, -0x3DA0, -0x3DA0, -0x3DA0};
    return static_cast<uint32_t>(0xFF8000 + kOffsets[cps2recomp::Ram8(kStage) & 0x0F]);
}
constexpr uint32_t kCameraX = 0x0C, kCameraY = 0x10, kCameraWorkX = 0x76, kCameraWorkY = 0x78;
// The other places that keep the camera's copy across between its ends: each stage's own routine has the piece
// (move.w 0x68(a6),d0 ... move.w d0,0x76(a6), 0x18 bytes) that RegisterWidescreen's keepCamera (0CDBB6) is one of, and
// its keeping is over at the piece's end. Widescreen has to keep each 64 nearer the middle, or on those stages the
// camera goes to the board's ends while the players go 64 past them, out of the picture (issue #116). Found as every
// such piece in the listing (tools/lst.py -r).
struct KeptAt {
    uint32_t at, after;
};
inline constexpr KeptAt kCameraKept[] = {
    {cps2recomp::HookAt(0x0CC554), 0x0CC56C}, {cps2recomp::HookAt(0x0CC672), 0x0CC68A},
    {cps2recomp::HookAt(0x0CCB68), 0x0CCB80}, {cps2recomp::HookAt(0x0CCDE4), 0x0CCDFC},
    {cps2recomp::HookAt(0x0CD264), 0x0CD27C}, {cps2recomp::HookAt(0x0CD524), 0x0CD53C},
    {cps2recomp::HookAt(0x0CD696), 0x0CD6AE}, {cps2recomp::HookAt(0x0CD87A), 0x0CD892},
    {cps2recomp::HookAt(0x0CD9F8), 0x0CDA10}, {cps2recomp::HookAt(0x0CE87E), 0x0CE896}};
constexpr uint32_t kCameraLeft = 0x68, kCameraRight = 0x6A, kCameraTop = 0x6C;
// How the camera moves, a byte (0CB2E2 goes by it, or by the one two after it when a point character's +267 says): 0
// after both players, 4 after player 1 alone; the game sets it every frame
constexpr uint32_t kCameraWay = 0xFF4408;
constexpr uint8_t kCameraFollowsOne = 4;
// The camera's ends across, by stage (each read from the record in a training round on the stage): the bathhouse's right
// end and Onslaught's city have their own, the others the same
inline void CameraEnds(int stage, int &left, int &right) {
    left = stage == 9 ? 960 : -64;
    right = stage == 0 ? 304 : left + 384;
}
// The floor's height: the camera's lowest place + 0xD8 (0x1D8 on most stages, 0x1C8 in the haunted hall, 0x3D8 in
// Onslaught's city; read in a training round on each stage)
inline int Ground() {
    return static_cast<int16_t>(cps2recomp::Ram16(CameraRecord() + 0x6E)) + 0xD8;
}
constexpr int kPictureLeft = 0x40;
constexpr int kWall = 0x15; // how near the picture's edge a player can go (013AD6: 0x55 to 0x1AB on from the camera)

// Each player's choosing: a record of 0x30 bytes, player 1's at FF4180 (player 2's at FF41B0: RAM dumps through
// choosing), +02 the side, +0C the character under the cursor; its first byte steps up to 7 once the player has
// chosen both characters, the mode and the speed (then 8 on, the special partner: traced through choosing)
constexpr uint32_t kChoosing = 0xFF4180, kChoosingSize = 0x30;
constexpr uint8_t kChoosingDone = 7;
// The players' picks: words, player 1's first and second character, then player 2's (a round's records are made from
// them, 01318E on: the record's byte +280 says which of the two it is); then a byte each, the colour each was chosen in
constexpr uint32_t kPicks = 0xFF40B0;

// The control panel as read this frame: a word a player, the stick (bit 0 right, 1 left, 2 down, 3 up) and the punches
// (bits 4-6) in its low byte, the kicks in bits 8-10
constexpr uint32_t kPanel[2] = {0xFF444C, 0xFF444E};

// The sound queue: a ring of sixteen-byte requests at FF27D8, 0x400 long, the word at FF27D2 the place of the next
// (0D4C7A puts one in)
constexpr uint32_t kSoundQueue = 0xFF27D8, kSoundQueueEnd = 0xFF27D2;
constexpr uint16_t kSoundQueueMask = 0x3F0;

// --- the characters' colours ---

// A character's colours, in the ROM: a long a character (by its word kCharacter) at kColourTable, the first of its sets,
// two of them (punch, kick) 0x60 apart, each three palettes of 16 colours (the round's start takes it, 0131FC). The
// record's byte kColourChoice picks the set; its long kPalette is where its palettes are in the buffer the board shows
// from: player 1's first character's at set 00, player 2's at 03, their second characters' at 06 and 09 (013186 on).
constexpr uint32_t kColourTable = 0x1C7268;
constexpr int kColourSetsEach = 2, kPalettesInSet = 3, kColoursInPalette = 16;
constexpr uint32_t kColourSetSize = 0x60;
constexpr uint32_t kColourChoice = 0x277, kPalette = 0x288;
constexpr uint32_t kPaletteBuffer = 0x914000;
constexpr unsigned kPlayer1Set = 0x00, kPlayer2Set = 0x03, kShadowSet = 0x0E;

// Objects by what they are (for Video::hideObject; from the object lists of rounds on every stage): colour sets 00-0B
// the fighters (three each, kPalette), 0E their shadows, 0D, 13-15 and 1E-1F the panel, 16-1D the stage's own pieces;
// the partners' marks on the panel are tiles 1CC80-1CCFF in their sides' sets
inline bool NotTheStage(uint32_t, uint16_t attributes) {
    const unsigned set = attributes & 0x1F;
    return set < 0x16 || set >= 0x1E;
}
inline bool ThePanel(uint32_t tile, uint16_t attributes) {
    const unsigned set = attributes & 0x1F;
    return set == 0x0D || (set >= 0x13 && set <= 0x15) || set >= 0x1E || (tile >= 0x1CC80 && tile < 0x1CD00);
}

// --- for hooks ---

inline void SetLow16(uint32_t &reg, uint32_t value) {
    reg = (reg & 0xFFFF0000) | (value & 0xFFFF);
}

} // namespace mvsc
