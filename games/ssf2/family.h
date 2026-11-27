// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Super Street Fighter II as one of the games built from Super Street Fighter II Turbo's sources (games/ssf2t/src;
// places.h there): where the places those sources name are in this game, and what is this game's own.
#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "game.h"

namespace ssf2t::family {

struct Place {
    uint32_t ssf2t, here;
};
inline constexpr Place kPlaceList[] = {
    // places: begin (tools/places.py; a line ending "by hand" is kept)
    {0x002EE2, 0x002D88}, // an instruction, 12 on alike
    {0x002F50, 0x002DF6}, // an instruction, 15 on alike
    {0x0043F4, 0x0040DC}, // an instruction, 7 on alike
    {0x00490A, 0x004488}, // moved as the code before or after it, 6 on alike
    {0x0049B0, 0x004534}, // an instruction, 6 on alike
    {0x004A58, 0x0045D2}, // an instruction, 6 on alike
    {0x004BDA, 0x00475A}, // moveq #1,d4 after a throw's random part is added (the code alike) by hand
    {0x004BDC, 0x00475C}, // mulu d4,d3 after it, where both ways meet by hand
    {0x00502A, 0x004A54}, // a place the code names so (1/1)
    {0x008FFC, 0x008380}, // moved as the code before or after it, 1 on alike; as many at 00836E: check
    {0x00D69C, 0x00C11A}, // the same entry of a jump table, 9 on alike
    {0x00D714, 0x00C192}, // the same entry of a jump table, 2 on alike
    {0x00E93A, 0x00CF70}, // an instruction, 2 on alike
    {0x00EA5C, 0x00D00C}, // an instruction, 3 on alike
    {0x00FCE2, 0x00DE98}, // an instruction, 6 on alike
    {0x00FCE6, 0x00DE9C}, // an instruction, 5 on alike
    {0x00FD62, 0x00DF0C}, // an instruction, 6 on alike
    {0x00FD66, 0x00DF10}, // an instruction, 5 on alike
    {0x00FDEE, 0x00DF8C}, // an instruction, 5 on alike
    {0x00FE0C, 0x00DFA6}, // an instruction, 6 on alike
    {0x00FE74, 0x00E018}, // an instruction, 3 on alike
    {0x0102F2, 0x00E5D2}, // the panel's names and faces of both players (lea 709090, 7090D8; portmap's own pair) by hand
    {0x0104B2, 0x000000}, // none: no routine makes the faces' palettes apart (livetag.cpp) by hand
    {0x078030, 0x06AB1A}, // an instruction, 6 on alike
    {0x07804A, 0x06AB48}, // an instruction, 4 on alike
    {0x078052, 0x06AB50}, // an instruction, 2 on alike
    {0x078270, 0x06AE52}, // an instruction, 3 on alike
    {0x0782D4, 0x06AEA4}, // an instruction, 4 on alike
    {0x07BB82, 0x06DDBE}, // an instruction, 10 on alike
    {0x07BC12, 0x06DE2C}, // an instruction, 9 on alike
    {0x07D6F4, 0x06F8B0}, // an instruction, 2 on alike
    {0x07E36E, 0x070434}, // an instruction, 4 on alike
    {0x07F1C6, 0x0711BC}, // a place the code names so (1/1)
    {0x090F6A, 0x07D27A}, // Vega's jump to the wall (the same code; the instructions before and after it alike) by hand
    {0x0A3472, 0x000000}, // none: one copy of Vega's jump to the wall with the stick (the other is 090F6A's) by hand
    {0x0B3156, 0x0892BA}, // Vega's jump to the wall without the stick (the same code) by hand
    {0x0BE646, 0x0921EE}, // an instruction, 3 on alike
    {0x0BE678, 0x09220C}, // an instruction, 6 on alike
    {0x0BEBA6, 0x092634}, // sub.w d6,0x2A(a2) in the thrown thing's hit (the same code around it) by hand
    {0x0BEBDA, 0x09264E}, // move.b 0x7(a3),0x19C(a2) after it, where its ways meet by hand
    {0x0BEE72, 0x092986}, // moveq #1,d6 after a hit's random part is added (0BEE40's code alike) by hand
    {0x0BEE74, 0x092988}, // mulu d6,d5 after it, where both ways meet by hand
    {0x0BEED8, 0x0929E8}, // the guard decision's jump table by the attack's +0B (28 entries here) by hand
    {0x0BEF66, 0x092A46}, // its routine for most attacks by hand
    {0x0BEF7C, 0x092A5C}, // its routine guarded standing only by hand
    {0x0BEF82, 0x092A62}, // its routine guarded crouching only by hand
    {0x0BEF88, 0x092A68}, // its routine guarded either way by hand
    {0x0BEF8A, 0x092A6A}, // its routine guarded crouching only, the other by hand
    {0x0CC4E4, 0x09BC80}, // the same entry of a jump table, 4 on alike
    {0x0CC630, 0x09BD98}, // the camera's place up (the jump table's entry; no test of a flag of its own first) by hand
    {0x0CCC5C, 0x09C3B2}, // an instruction, 11 on alike
    {0x100000, 0x100000}, // a place the code names so (12/12)
    {0x24EAE8, 0x1ADC06}, // a place the code names so (1/1)
    {0xFF0A7A, 0xFF0A72}, // a variable the code names so (4/4)
    {0xFF0A7E, 0xFF0A76}, // a variable the code names so (3/3)
    {0xFF8004, 0xFF8004}, // a variable the code names so (84/84)
    {0xFF8008, 0xFF8008}, // a variable the code names so (87/88)
    {0xFF800C, 0xFF800C}, // a variable the code names so (118/118)
    {0xFF8086, 0xFF8086}, // a variable the code names so (15/15)
    {0xFF808A, 0xFF808A}, // a variable the code names so (15/15)
    {0xFF82AE, 0xFF82AE}, // a variable the code names so (1/1)
    {0xFF82B6, 0xFF82B6}, // a variable the code names so (1/1)
    {0xFF82D0, 0xFF82D0}, // a variable the code names so (1/1)
    {0xFF82D3, 0xFF82D3}, // a variable between two that moved alike
    {0xFF82DA, 0xFF82DA}, // a variable the code names so (4/4)
    {0xFF82DD, 0xFF82DD}, // a variable the code names so (1/1)
    {0xFF82DF, 0xFF82DF}, // a variable the code names so (20/20)
    {0xFF82E9, 0xFF82E9}, // a variable the code names so (35/35)
    {0xFF82F0, 0xFF82F0}, // a variable the code names so (24/24)
    {0xFF8342, 0xFF8342}, // a variable the code names so (1/1)
    {0xFF83A9, 0x000000}, // none: no speeds (board.cpp) by hand
    {0xFF844E, 0xFF83CE}, // a variable the code names so (51/51)
    {0xFF8C4E, 0xFF8BCE}, // a variable the code names so (45/45)
    {0xFF8C54, 0xFF8BD4}, // a variable the code names so (3/3)
    {0xFF8C56, 0xFF8BD6}, // a variable the code names so (3/3)
    {0xFF8C58, 0xFF8BD8}, // a variable the code names so (3/3)
    {0xFF8C5A, 0xFF8BDA}, // a variable the code names so (3/3)
    {0xFF8DCE, 0xFF8CCE}, // a variable the code names so (23/23)
    {0xFF8DCF, 0xFF8CCF}, // a variable the code names so (2/2)
    {0xFF8DD2, 0xFF8CD2}, // a variable the code names so (38/41)
    {0xFF8DDC, 0xFF8CDC}, // a variable the code names so (7/7)
    {0xFF8DDE, 0xFF8CDE}, // a variable the code names so (9/9)
    {0xFF8ECE, 0xFF8DCE}, // a variable the code names so (24/24)
    {0xFF8ED4, 0xFF8DD4}, // a variable the code names so (38/39)
    {0xFF8ED8, 0xFF8DD8}, // a variable the code names so (27/28)
    {0xFF8F90, 0x000000}, // none: the camera up has no flag of its own holding it (widescreen.cpp) by hand
    {0xFF954C, 0xFF944C}, // a variable the code names so (4/4)
    {0xFF9580, 0xFF9480}, // a variable the code names so (5/5)
    {0xFF97A2, 0xFF96A2}, // a variable the code names so (2/2)
    {0xFFA9A2, 0xFFA8A2}, // a variable the code names so (3/3)
    // places: end
};
inline constexpr auto kPlaces = std::to_array(kPlaceList);

// Super Turbo's own, which this game has not: the super combo gauge (its byte is never used here, src/training.cpp), the
// hidden characters (Akuma and the old versions: src/choosing.cpp), the four speeds (src/board.cpp). (Its player's
// record is Super Turbo's to +3B1: every offset the sources name, compared over the instructions portmap paired.)
inline constexpr bool kTurbo = false;

// The sound program's table of songs (src/music.cpp): its version is Super Turbo's (1.04), but the table is at 7C16 (the
// count, 0x1FF, at 7C10: the program's ld hl,7C10 at 01E1, where Super Turbo's reads 9000), its songs in the same order.
// Its entries give a song's place with a bank byte of 10 (108411: 0A71 makes the bank from it, which goes round the
// ROM's eight), so only the bits of the 128 KB ROM are the place: 63 songs are music so, Super Turbo's but five
inline constexpr uint32_t kSongTable = 0x7C16, kSongAddresses = 0x1FFFF, kSongCount = 0x1FF, kSongEntry = 4;
// Its own songs (CPS2RECOMP_SOUNDTRACK_LOG over attract mode with its sound and the ladder): the introduction and title
// 33, player select 34, the bonus stages 3B; the last fight has M. Bison's stage's tune (0C), none of its own
inline constexpr uint16_t kTuneTitle = 0x33, kTuneChoose = 0x34, kTuneFinal = 0, kTuneBonus = 0x3B;

// The music's names: Super Turbo's list (src/music.cpp) with the songs above; a game with a list of its own gives it here
inline std::vector<cps2recomp::Song> Songs() {
    return {};
}

// The characters' colour sets (src/colours.cpp): eight, chosen as in Super Turbo (00CF70 on: the six buttons, Start, a
// button held), each a count of palettes and the palettes, from the table at 093CDC (a word a character, to its list of
// eight sets): one palette for most, three for Ryu, Guile, Ken, Chun-Li, Sagat and Dee Jay, eight for M. Bison (his
// cape's colours). Read from the ROM.
// (custom colours and the colour editor: src/colours.cpp)
inline constexpr bool kColours = true;
// A character's set of colours: where its first palette is in the ROM (the data view) and how many palettes it has, from
// the table at 0x093CDC (a word a character, to its list of a word a set, to the set: its count, then its palettes); and the
// palettes the board shows a player's in (0BFA78 copies them: player 1's from set 1, player 2's from set 10)
inline uint32_t ColourSet(int who, int set, int &count) {
    constexpr uint32_t kTable = 0x093CDC;
    const auto word = [](uint32_t at) { return static_cast<uint32_t>((cps2recomp::Program(at)[0] << 8) | cps2recomp::Program(at)[1]); };
    const uint32_t list = kTable + word(kTable + static_cast<uint32_t>(who) * 2);
    const uint32_t first = list + word(list + static_cast<uint32_t>(set) * 2);
    count = static_cast<int>(word(first));
    return first + 2;
}
inline uint32_t ColourShown(int player, int, int) {
    return player ? 0x900200 : 0x900020;
}
// Every place of the characters' colours (custom colours put the player's over them)
inline constexpr uint32_t kColourRanges[][2] = {{0x093D0C, 0x0961FC}};

inline constexpr const char *kColourSetNames[] = {"LP", "MP", "HP", "LK", "MK", "HK", "Start", "Held"};
inline constexpr int kPalettesInSet = 8;
inline int PalettesOf(int number) {
    constexpr uint32_t kTable = 0x093CDC;
    const auto word = [](uint32_t at) { return static_cast<int>((cps2recomp::Program(at)[0] << 8) | cps2recomp::Program(at)[1]); };
    const uint32_t list = kTable + static_cast<uint32_t>(word(kTable + static_cast<uint32_t>(number) * 2));
    return word(list + static_cast<uint32_t>(word(list)));
}
inline constexpr int (*kPalettesOf)(int) = PalettesOf;
inline constexpr const char *kColourFile = "ssfe.04";
inline constexpr uint32_t kColourFileAt = 0x080000, kColourFileSize = 0x80000, kColourFileSameFrom = 0x080000, kColourFileSameTo = 0x090000;
inline constexpr const char *kColourFileWhose = "Super Street Fighter II's, World 931005";
inline constexpr const char *kColourFeatureWords =
    "Characters in the colours of the files in the \"palettes\" folder in the game's data folder: a ssfe.04 changed with "
    "PalMod, and text files, one a character (Cammy.txt), which go before it. With no files the game's own colours. They are "
    "taken when a player is coloured: at the next round.";
inline constexpr const char *kColourHelp =
    "# To change them, copy this file to the palettes folder and change the colours there. Each set is\n"
    "# chosen by the button that chose the character ([LP] to [HK]), Start or a button held ([Held]), and\n"
    "# has one to eight lines of 16 colours (by character); the character is drawn in them. The 16th colour\n"
    "# of a line is never seen. A colour is #RRGGBB or #RGB, rounded to the board's 16 levels of each; -\n"
    "# keeps the game's. Sets and lines that are left out keep the game's colours.\n";

// --- What differs between the games built from these sources (src/ssf2t.h names each) ---

// The players' records: how far apart, and the bytes after the part all the games lay out alike (to +1FF). Super Turbo's
// to +3B1 here (every offset compared over the instructions portmap paired); found in Super Turbo by RAM dumps and its code (src/ssf2t.h says how each); a game without one has 0 (and the sources leave it)
inline constexpr uint32_t kPlayerSize = 0x400;
inline constexpr uint32_t kChosen = 0x284, kSide = 0x381, kPerson = 0x38E, kTagged = 0x390, kCharacter = 0x391,
                          kButtonsNow = 0x392, kButtonsBefore = 0x394, kOpponent = 0x39E, kOpponentCharacter = 0x3A2,
                          kColourChoice = 0x3B1;
// ... and what only some have: the second thing thrown (+2AE), being seen from where the other player is (+1F5), not
// kept inside the picture (+19D), and the part of the record a round's start clears (to +380)
inline constexpr uint32_t kThrownToo = 0x2AE, kSeenAsOther = 0x1F5, kNotKeptIn = 0x19D, kClearedTo = 0x380;
// The computer's plan for the character, a long the computer's set-up writes (its sign of having been set up); 0:
// none kept in the record (the set-up's +273, 2, is the sign then)
inline constexpr uint32_t kPlanTable = 0x1E0;

// The box tables' kinds (src/ssf2t.h's kTable...: head +0, body +2, legs +4 in every game): an attack's and the pushing
// box's places in the tables, and the bytes of an attack box (this game's hit test, 093676 and 0936D0: twelve bytes an
// attack box)
inline constexpr uint32_t kTableAttack = 0x6, kTablePush = 0x8, kAttackBoxSize = 0x0C;

// The guard decision as a hit lands (src/guard.cpp): a jump table of 28 words by the attack's +0B, at 0929E8 (Super
// Turbo's 0BEED8; Super Turbo has 38). Super Turbo's kind of it goes by the attack's +0B through the table's routines
// (the bytes of the table: kGuardTypeBytes, 0 for none); the classic games' by the attack's +07 through a row of bytes
// for each guarding pose (kGuardRows: where the rows' table is, 0 for none)
inline constexpr uint32_t kGuardTypeBytes = 0x38, kGuardRows = 0;

// The game's loop with no coin in (src/ssf2t.h): the introduction and the title's step, the title's step under it, the
// demonstration fight's, the loop's end, the frame the board's start is over by, and the step the board's start ends in
inline constexpr uint16_t kIntro = 0x02, kTitleStep = 0x16, kDemo = 0x0E, kLoopEnd = 0x18, kBooted = 0x10;
inline constexpr int kBootFrames = 810; // (this game's boot, the warning and the introduction, as Super Turbo's: pictures)

// The characters and the stages (stage n is character n's)
inline constexpr int kCharacters = 16, kStages = 16;
inline constexpr const char *kCharacterNames[kCharacters] = {"Ryu",     "E. Honda", "Blanka", "Guile",  "Ken",      "Chun-Li",
                                                             "Zangief", "Dhalsim",  "M. Bison", "Sagat", "Balrog",  "Vega",
                                                             "Cammy",   "T. Hawk",  "Fei Long", "Dee Jay"};
inline constexpr const char *kStageNames[kStages] = {
    "Ryu's (the castle)",        "E. Honda's (the bathhouse)", "Blanka's (the river)",       "Guile's (the air base)",
    "Ken's (the harbour)",       "Chun-Li's (the market)",     "Zangief's (the factory)",    "Dhalsim's (the palace)",
    "M. Bison's (the temple)",   "Sagat's (the Buddha)",       "Balrog's (Las Vegas)",       "Vega's (the cage)",
    "Cammy's (the castle ruins)", "T. Hawk's (Mexico)",             "Fei Long's (the garden)",    "Dee Jay's (the beach)"};
// Each stage's tune (CPS2RECOMP_SOUNDTRACK_LOG over a fight on each), and the versus screen's
inline constexpr uint16_t kStageTunes[kStages] = {0x02, 0x03, 0x05, 0x07, 0x01, 0x04, 0x06, 0x08,
                                                  0x0C, 0x0B, 0x09, 0x0A, 0x0E, 0x0F, 0x0D, 0x10};
inline constexpr uint16_t kTuneVersus = 0x35;
// The sound queue's requests: sixteen bytes each, the ring 0x1000 (FF05 the music stopped)
inline constexpr uint16_t kSoundEntry = 0x10, kSoundQueueMask = 0xFF0, kTuneOff = 0xFF05;

// Every word holding the camera's place across (the first the camera's own)
inline constexpr uint32_t kCameraCopies[] = {0xFF8DD4, 0xFF8EEA, 0xFF8EEC, 0xFF8EEE, 0xFF8EF0}; // (Super Turbo's, 0x100 lower: places.py)

// The routines that have an object drawn only while it is near the board's picture (Super Turbo's 003036, 00306E and
// 0030A6, and a fourth of the World Warrior's line: 0 where a game has none): where each tests the object's place
// across, and where they all go on to have it drawn. src/widescreen.cpp widens them.
inline constexpr uint32_t kNearPicture1 = 0x002EEC, kNearPicture2 = 0x002F24, kNearPicture3 = 0x002F5A, kNearPicture4 = 0;
inline constexpr uint32_t kNearPictureDrawn = 0x002F7A;

// Free play: the game's own coin mode for it (src/coins.cpp), or a DIP switch's bit (bank, bit) on a board that has one
inline constexpr bool kFreePlayDip = false;
inline constexpr int kFreePlayBank = 0, kFreePlayBit = 0;
// The Board page is in the sources (src/board.cpp), not game.cfg's DIP switches
inline constexpr bool kBoardInSources = true;

// Objects by what they are (src/ssf2t.h): 01-05 player 1's and 10-14 player 2's (the character and what it throws:
// 0BFA78 copies its five palettes there), 0A the shadows, 0E the sparks of hits, 0B and 1A-1F the stage's own pieces,
// the rest the panel. Found by hiding each colour set in turn in fights on every stage.
inline bool IsFighter(uint16_t attributes) {
    const unsigned set = attributes & 0x1F;
    return (set >= 0x01 && set <= 0x05) || (set >= 0x10 && set <= 0x14);
}
inline bool IsStagePiece(uint16_t attributes) {
    const unsigned set = attributes & 0x1F;
    return set == 0x0B || set >= 0x1A;
}
inline constexpr unsigned kShadowSet = 0x0A, kSparkSet = 0x0E;
inline constexpr uint32_t kPlayerSets[2] = {0x3Eu, 0x1Fu << 16};

} // namespace ssf2t::family
