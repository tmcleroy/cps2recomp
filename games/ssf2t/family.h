// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Super Street Fighter II Turbo as one of the games made from its program, which build its sources (src/): what is its
// own. The places its sources name are its own, so there is no list of them (src/places.h).
#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "game.h"

namespace ssf2t::family {

struct Place {
    uint32_t ssf2t, here;
};
inline constexpr std::array<Place, 0> kPlaces = {};

// What Super Turbo has and Super Street Fighter II (games/ssf2, made from the same program) has not: the super combo
// gauge, the hidden characters (Akuma, the old versions), the four speeds of the Board page
inline constexpr bool kTurbo = true;

// The sound program's table of songs (src/music.cpp): the entry of song 0, four bytes a song (the count, 0x1FF, the word
// six bytes before)
inline constexpr uint32_t kSongTable = 0x9006, kSongAddresses = 0, kSongCount = 0x1FF, kSongEntry = 4;
// Songs of its own: the introduction and title, player select, the final fight (and its faster version, one on)
inline constexpr uint16_t kTuneTitle = 0xD4, kTuneChoose = 0xD3, kTuneFinal = 0xD5, kTuneBonus = 0; // (no bonus stages)

// The music's names: Super Turbo's list (src/music.cpp) with the songs above; a game with a list of its own gives it here
inline std::vector<cps2recomp::Song> Songs() {
    return {};
}

// The characters' colour sets (src/colours.cpp): their names, in the game's order; five palettes in each, for every
// character; the program file they are in and the part of it that must be the game's; whose set it is
// (custom colours and the colour editor: src/colours.cpp)
inline constexpr bool kColours = true;
// A character's set of colours: where its first palette is in the ROM (the data view) and how many palettes it has, from
// the table at 0x0BFAF2 (a word a character, to its list of a word a set, to the set: its count, then its palettes); and the
// palettes the board shows a player's in (0BFA78 copies them: player 1's from set 1, player 2's from set 10)
inline uint32_t ColourSet(int who, int set, int &count) {
    constexpr uint32_t kTable = 0x0BFAF2;
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
inline constexpr uint32_t kColourRanges[][2] = {{0x0BFB2C, 0x0C6320}}; // (every set, Akuma's last)

inline constexpr const char *kColourSetNames[] = {"LP", "MP", "HP", "LK", "MK", "HK", "Start", "Held", "OldP", "OldK"};
inline constexpr int kPalettesInSet = 5;
inline constexpr int (*kPalettesOf)(int) = nullptr;
inline constexpr const char *kColourFile = "sfxj.04a";
inline constexpr uint32_t kColourFileAt = 0x080000, kColourFileSize = 0x80000, kColourFileSameFrom = 0x080000, kColourFileSameTo = 0x090000;
inline constexpr const char *kColourFileWhose = "Super Street Fighter II X's, 940223";
inline constexpr const char *kColourFeatureWords =
    "Characters in the colours of the files in the \"palettes\" folder in the game's data folder: a sfxj.04a changed with "
    "PalMod, and text files, one a character (Cammy.txt), which go before it. With no files the game's own colours. They are "
    "taken when a player is coloured: at the next round.";
inline constexpr const char *kColourHelp =
    "# To change them, copy this file to the palettes folder and change the colours there. Each set is\n"
    "# chosen by the button that chose the character ([LP] to [HK]), Start, a button held ([Held]) or the\n"
    "# old version's code ([OldP], [OldK]), and has five lines of 16 colours; the character is drawn in\n"
    "# them. The 16th colour of a line is never seen. A colour is #RRGGBB or #RGB, rounded to the board's\n"
    "# 16 levels of each; - keeps the game's. Sets and lines that are left out keep the game's colours.\n";

// --- What differs between the games built from these sources (src/ssf2t.h names each) ---

// The players' records: how far apart, and the bytes after the part all the games lay out alike (to +1FF). Found in
// Super Turbo by RAM dumps and its code (src/ssf2t.h says how each); a game without one has 0 (and the sources leave it)
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
// box's places in the tables, and the bytes of an attack box (Super Turbo's hit test, 0BF534 and 0BF588)
inline constexpr uint32_t kTableAttack = 0x6, kTablePush = 0x8, kAttackBoxSize = 0x10;

// The guard decision as a hit lands (src/guard.cpp): Super Turbo's: a jump table of 38 words by the attack's +0B, at
// 0BEED8 (src/ssf2t.h). Super Turbo's kind of it goes by the attack's +0B through the table's routines (the bytes of
// the table: kGuardTypeBytes, 0 for none); the classic games' by the attack's +07 through a row of bytes for each
// guarding pose (kGuardRows: where the rows' table is, 0 for none)
inline constexpr uint32_t kGuardTypeBytes = 0x4C, kGuardRows = 0;

// The game's loop with no coin in (src/ssf2t.h): the introduction and the title's step, the title's step under it, the
// demonstration fight's, the loop's end, the frame the board's start is over by, and the step the board's start ends in
inline constexpr uint16_t kIntro = 0x02, kTitleStep = 0x16, kDemo = 0x0E, kLoopEnd = 0x18, kBooted = 0x10;
inline constexpr int kBootFrames = 810;

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
inline constexpr uint32_t kCameraCopies[] = {0xFF8ED4, 0xFF8FEA, 0xFF8FEC, 0xFF8FEE, 0xFF8FF0};

// The routines that have an object drawn only while it is near the board's picture (Super Turbo's 003036, 00306E and
// 0030A6, and a fourth of the World Warrior's line: 0 where a game has none): where each tests the object's place
// across, and where they all go on to have it drawn. src/widescreen.cpp widens them.
inline constexpr uint32_t kNearPicture1 = 0x003046, kNearPicture2 = 0x00307E, kNearPicture3 = 0x0030B4, kNearPicture4 = 0;
inline constexpr uint32_t kNearPictureDrawn = 0x0030D4;

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
