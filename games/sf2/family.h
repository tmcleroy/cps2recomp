// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Street Fighter II: The World Warrior as one of the games built from Super Street Fighter II Turbo's sources
// (games/ssf2t/src; places.h there): where the places those sources name are in this game, and what is this game's
// own. Champion Edition was made from this program (tools/portmap.py places two thirds of Champion Edition's instructions
// here), and its places were carried from Champion Edition's family.h through that map and checked; its work RAM is
// Champion Edition's but moved by a few bytes (the players' records at FF83C6), and the records are laid out alike.
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
    {0x002EE2, 0x00276E}, // Champion Edition's 0022D8, placed by portmap (an instruction) by hand
    {0x002F50, 0x0027DC}, // Champion Edition's 002346, placed by portmap (an instruction) by hand
    {0x0043F4, 0x002D98}, // Champion Edition's 003224, placed by portmap (an instruction) by hand
    {0x004634, 0x002EA0}, // the throw test where its ways in meet (tst.b 0x191(a6)) (the court) by hand
    {0x0047D0, 0x002FCC}, // the throw test's no (moveq #0,d0; rts) (the court) by hand
    {0x00490A, 0x0032D6}, // Champion Edition's 003810, placed by portmap (an instruction) by hand
    {0x0049B0, 0x00304A}, // Champion Edition's 003512, placed by portmap (an instruction) by hand
    {0x004A58, 0x0030E4}, // Champion Edition's 0035AC, placed by portmap (an instruction) by hand
    {0x004BDA, 0x000000}, // none: a classic throw's damage has no random part by hand
    {0x004BDC, 0x000000}, // none: a classic throw's damage has no random part by hand
    {0x00502A, 0x003412}, // Champion Edition's 00394C, placed by portmap (named so) by hand
    {0x008FFC, 0x000000}, // none: the Board page is the DIP switches (game.cfg) by hand
    {0x00D69C, 0x007E60}, // addq.w #2,0x0(a5): a game begins (--watch FF8000 with Start pressed) by hand
    {0x00D714, 0x007F34}, // addq.w #2,0x8(a5): the match's first step (--watch FF8008) by hand
    {0x00E93A, 0x000000}, // none: no hidden characters to choose (choosing.cpp) by hand
    {0x00EA5C, 0x000000}, // none: as above by hand
    {0x00FCE2, 0x008F5A}, // Champion Edition's 0092DE, placed by portmap (an instruction) by hand
    {0x00FCE6, 0x008F5E}, // Champion Edition's 0092E2, placed by portmap (an instruction) by hand
    {0x00FD62, 0x008FFE}, // Champion Edition's 009382, placed by portmap (an instruction) by hand
    {0x00FD66, 0x009002}, // Champion Edition's 009386, placed by portmap (an instruction) by hand
    {0x00FDEE, 0x0090AE}, // Champion Edition's 009432, placed by portmap (an instruction) by hand
    {0x00FE0C, 0x0090C0}, // Champion Edition's 009444, placed by portmap (an instruction) by hand
    {0x00FE74, 0x009122}, // Champion Edition's 0094A6, placed by portmap (an instruction) by hand
    {0x0102F2, 0x0096BE}, // Champion Edition's 009A2A, placed by portmap (an instruction) by hand
    {0x0104B2, 0x000000}, // none: the panel has no faces by hand
    {0x078030, 0x0284E4}, // Champion Edition's 0379F0, placed by portmap (an instruction) by hand
    {0x07804A, 0x028512}, // Champion Edition's 037A1E, placed by portmap (an instruction) by hand
    {0x078052, 0x02851E}, // Champion Edition's 037A26, placed by portmap (an instruction) by hand
    {0x078270, 0x02AB98}, // a person's step: 0284E4 calls it for a person (portmap took the computer's, 0287BA) by hand
    {0x0782D4, 0x02AC1A}, // moveq #0,d0 before the person's dispatch on +3 (Champion Edition's 037D14) by hand
    {0x07AC78, 0x02C9B8}, // the hit looked for at the end of the step (tst.b 0x49(a6); move.w 0x2A(a6),d3) (the court) by hand
    {0x07BB82, 0x000000}, // none: the computer's set-up has no plan round (0287CC) by hand
    {0x07BC12, 0x028864}, // cmpi.b #$E,0x3(a6): the computer's fight step (Champion Edition's 03A612) by hand
    {0x07D6F4, 0x000000}, // none: the computer's set-up has no character tables (0287CC) by hand
    {0x07E274, 0x000000}, // none: the computer throws through the same test (the court) by hand
    {0x07E36A, 0x000000}, // none: as above (the court) by hand
    {0x07E36E, 0x02936C}, // the computer's plan (bsr 2936C in its set-up, 0287DA) by hand
    {0x07F1C6, 0x003342}, // the stun's most: the word of cmpi.w #$1E,0x5E(a3) at 003340 (a fixed 1E) by hand
    {0x090F6A, 0x000000}, // none: Vega is not played (the computer's boss), no jump to the wall by hand
    {0x0A3472, 0x000000}, // none: Vega is not played (the computer's boss), no jump to the wall by hand
    {0x0B3156, 0x000000}, // none: Vega is not played (the computer's boss), no jump to the wall by hand
    {0x0BE646, 0x07D3A2}, // Champion Edition's 09502A, placed by portmap (an instruction) by hand
    {0x0BE678, 0x07D3BA}, // Champion Edition's 095042, placed by portmap (an instruction) by hand
    {0x0BEBA6, 0x07D514}, // Champion Edition's 0951D2, placed by portmap (an instruction) by hand
    {0x0BEBDA, 0x07D528}, // Champion Edition's 0951E6, placed by portmap (an instruction) by hand
    {0x0BEE72, 0x07D852}, // moveq #1,d6 after a hit's random part is added (0BEE40's code alike) by hand
    {0x0BEE74, 0x07D854}, // mulu d6,d5 after it, where both ways meet by hand
    {0x0BEED8, 0x000000}, // none: the classic guard decision goes by a table of rows (family.h, kGuardRows) by hand
    {0x0BEF66, 0x000000}, // none: the classic guard decision goes by a table of rows (family.h, kGuardRows) by hand
    {0x0BEF7C, 0x000000}, // none: the classic guard decision goes by a table of rows (family.h, kGuardRows) by hand
    {0x0BEF82, 0x000000}, // none: the classic guard decision goes by a table of rows (family.h, kGuardRows) by hand
    {0x0BEF88, 0x000000}, // none: the classic guard decision goes by a table of rows (family.h, kGuardRows) by hand
    {0x0BEF8A, 0x000000}, // none: the classic guard decision goes by a table of rows (family.h, kGuardRows) by hand
    {0x0BF662, 0x07E516}, // the players pushed apart (its clr.w; lea of player 1) (the court) by hand
    {0x0CC4E4, 0x08365E}, // Champion Edition's 09C77C, placed by portmap (an instruction) by hand
    {0x0CC630, 0x083762}, // Champion Edition's 09C880, placed by portmap (an instruction) by hand
    {0x0CCC5C, 0x083CA4}, // Champion Edition's 09CDC2, placed by portmap (an instruction) by hand
    {0x100000, 0x100000}, // Champion Edition's 100000, placed by portmap (named so) by hand
    {0x24EAE8, 0x09A0B4}, // the computer's two words a character (movea.l #$9A0B4 at 0287F0) by hand
    {0xFF0A7A, 0xFF8022}, // Champion Edition's FF8022, placed by portmap (named so) by hand
    {0xFF0A7E, 0xFF81A2}, // Champion Edition's FF819E, placed by portmap (named so) by hand
    {0xFF8004, 0xFF8004}, // Champion Edition's FF8004, placed by portmap (named so) by hand
    {0xFF8008, 0xFF8008}, // Champion Edition's FF8008, placed by portmap (named so) by hand
    {0xFF800C, 0xFF800C}, // Champion Edition's FF800C, placed by portmap (named so) by hand
    {0xFF8086, 0xFF807E}, // Champion Edition's FF807A, placed by portmap (named so) by hand
    {0xFF808A, 0xFF8082}, // Champion Edition's FF807E, placed by portmap (named so) by hand
    {0xFF82AE, 0x000000}, // none used: the coins are DIP switches by hand
    {0xFF82B6, 0x000000}, // none used: as above by hand
    {0xFF82D0, 0xFF82C4}, // Champion Edition's FF82C0, placed by portmap (named so) by hand
    {0xFF82D3, 0xFF82C7}, // the difficulty's byte (written at 0018B8, Champion Edition's +2C3 four on) by hand
    {0xFF82DA, 0xFF82CE}, // Champion Edition's FF82CA, placed by portmap (named so) by hand
    {0xFF82DD, 0x000000}, // none: demo sound is a DIP switch by hand
    {0xFF82DF, 0xFF82D3}, // Champion Edition's FF82CF, placed by portmap (named so) by hand
    {0xFF82E9, 0xFF82DD}, // Champion Edition's FF82D9, placed by portmap (named so) by hand
    {0xFF82F0, 0xFF82E4}, // Champion Edition's FF82E0, placed by portmap (named so) by hand
    {0xFF8342, 0x000000}, // none: the coins are DIP switches by hand
    {0xFF83A9, 0x000000}, // none: no speeds by hand
    {0xFF844E, 0xFF83C6}, // Champion Edition's FF83BE, placed by portmap (named so) by hand
    {0xFF8C4E, 0xFF89CA}, // Champion Edition's FF89BE, placed by portmap (named so) by hand
    {0xFF8C54, 0xFF89D0}, // Champion Edition's FF89C4, placed by portmap (named so) by hand
    {0xFF8C56, 0xFF89D2}, // Champion Edition's FF89C6, placed by portmap (named so) by hand
    {0xFF8C58, 0xFF89D4}, // Champion Edition's FF89C8, placed by portmap (named so) by hand
    {0xFF8C5A, 0xFF89D6}, // Champion Edition's FF89CA, placed by portmap (named so) by hand
    {0xFF8DCE, 0xFF8ACE}, // Champion Edition's FF8ABE, placed by portmap (named so) by hand
    {0xFF8DCF, 0xFF8ACF}, // Champion Edition's FF8ABF, placed by portmap (named so) by hand
    {0xFF8DD2, 0xFF8AD2}, // Champion Edition's FF8AC2, placed by portmap (named so) by hand
    {0xFF8DDC, 0xFF8ADC}, // Champion Edition's FF8ACC, placed by portmap (named so) by hand
    {0xFF8DDE, 0xFF8ADE}, // Champion Edition's FF8ACE, placed by portmap (named so) by hand
    {0xFF8DEE, 0x000000}, // none: no first attack (the court) by hand
    {0xFF8ECE, 0xFF8BD2}, // Champion Edition's FF8BBE, placed by portmap (named so) by hand
    {0xFF8ED4, 0xFF8BD8}, // Champion Edition's FF8BC4, placed by portmap (named so) by hand
    {0xFF8ED8, 0xFF8BDC}, // Champion Edition's FF8BC8, placed by portmap (named so) by hand
    {0xFF8F90, 0x000000}, // none: the camera up has no flag of its own by hand
    {0xFF954C, 0xFF9160}, // Champion Edition's FF914C, placed by portmap (named so) by hand
    {0xFF9580, 0xFF917C}, // Champion Edition's FF9168, placed by portmap (named so) by hand
    {0xFF97A2, 0xFF938A}, // Champion Edition's FF9376, placed by portmap (named so) by hand
    {0xFFA9A2, 0xFFA58A}, // Champion Edition's FFA576, placed by portmap (named so) by hand
    // places: end
};
inline constexpr auto kPlaces = std::to_array(kPlaceList);

// What Super Turbo has and this game has not: the super combo gauge, the hidden characters, the speeds, a fighter's reach
inline constexpr bool kTurbo = false;

// The sound program (the classic board's, on its Z80 with the YM2151 and the OKI): its table of songs is at 1106, two
// bytes a song (the place in the ROM, high byte first: 01EC and 002D read it so), 0x8E of them (the byte at 1100); a song
// whose data starts with 0 is music (01FA: the others are effects, by priority, and the OKI's samples, bit 7)
inline constexpr uint32_t kSongTable = 0x1106, kSongAddresses = 0, kSongCount = 0x8C, kSongEntry = 2;
inline constexpr uint16_t kTuneTitle = 0, kTuneChoose = 0x0E, kTuneFinal = 0, kTuneBonus = 0;
// The music's names (CPS2RECOMP_SOUNDTRACK_LOG over attract mode with its sound and the smoke movie's ladder, pictures
// where each starts): each stage's tune (the table at 0063BE) and its faster version once a fighter's health is low (79
// on, by stage), and the tunes between the fights
inline std::vector<cps2recomp::Song> Songs() {
    return {
        {0x01, "ryu", "Ryu's stage"},           {0x02, "honda", "E. Honda's stage"},
        {0x03, "blanka", "Blanka's stage"},     {0x05, "guile", "Guile's stage"},
        {0x04, "ken", "Ken's stage"},           {0x06, "chunli", "Chun-Li's stage"},
        {0x07, "zangief", "Zangief's stage"},   {0x08, "dhalsim", "Dhalsim's stage"},
        {0x0C, "bison", "M. Bison's stage"},    {0x0B, "sagat", "Sagat's stage"},
        {0x09, "balrog", "Balrog's stage"},     {0x0A, "vega", "Vega's stage"},
        {0x79, "ryu_later", "Ryu's stage, faster (a fighter's health low)"},
        {0x7A, "honda_later", "E. Honda's stage, faster (a fighter's health low)"},
        {0x7B, "blanka_later", "Blanka's stage, faster (a fighter's health low)"},
        {0x7C, "guile_later", "Guile's stage, faster (a fighter's health low)"},
        {0x7D, "ken_later", "Ken's stage, faster (a fighter's health low)"},
        {0x7E, "chunli_later", "Chun-Li's stage, faster (a fighter's health low)"},
        {0x7F, "zangief_later", "Zangief's stage, faster (a fighter's health low)"},
        {0x80, "dhalsim_later", "Dhalsim's stage, faster (a fighter's health low)"},
        {0x84, "bison_later", "M. Bison's stage, faster (a fighter's health low)"},
        {0x81, "balrog_later", "Balrog's stage, faster (a fighter's health low)"},
        {0x82, "vega_later", "Vega's stage, faster (a fighter's health low)"},
        {0x83, "sagat_later", "Sagat's stage, faster (a fighter's health low)"},
        {0x16, "title", "The introduction and the title", true},
        {0x0E, "player_select", "Player select"},
        {0x0F, "versus", "Versus screen", true},
        {0x10, "winner", "A match won", true},
        {0x0D, "bonus", "A bonus stage"},
    };
}

// The characters' colours: one set each (no mirror match here), one palette: the common block of object palettes the
// game copies to the board as a round is set up (08ACBA, 001B44: set n+1 is character n's, as in Champion Edition).
// The program is pairs of byte-wide files, so no PalMod file.
inline constexpr bool kColours = true;
inline constexpr const char *kColourSetNames[] = {"Normal"};
inline constexpr int kPalettesInSet = 1;
inline constexpr int (*kPalettesOf)(int) = nullptr;
inline uint32_t ColourSet(int who, int, int &count) {
    count = 1;
    return 0x08ACBA + static_cast<uint32_t>(who + 1) * 0x20;
}
inline uint32_t ColourShown(int, int who, int choice) {
    return 0x900000 + static_cast<uint32_t>(who + 1 + (choice ? 1 : 0)) * 0x20;
}
inline constexpr uint32_t kColourRanges[][2] = {{0x08ACDA, 0x08AE5A}};
inline constexpr const char *kColourFile = nullptr;
inline constexpr uint32_t kColourFileAt = 0x080000, kColourFileSize = 0x80000, kColourFileSameFrom = 0x080000, kColourFileSameTo = 0x090000;
inline constexpr const char *kColourFileWhose = "";
inline constexpr const char *kColourFeatureWords =
    "Characters in the colours of the files in the \"palettes\" folder in the game's data folder: text files, one a "
    "character (Ryu.txt). With no files the game's own colours. They are taken when the colours are next copied to the "
    "board: at the next round.";
inline constexpr const char *kColourHelp =
    "# To change them, copy this file to the palettes folder and change the colours there. [Normal] is the\n"
    "# character's one set, a line of 16 colours. The 16th colour of a line is never seen. A colour is #RRGGBB\n"
    "# or #RGB, rounded to the board's 16 levels of each; - keeps the game's.\n";

// --- What differs between the games built from these sources ---

// The players' records, 0x300 apart from FF83BE (the low part laid out as Super Turbo's: every offset to +1FF the
// sources use compared over the instructions portmap paired); the bytes after it are Super Turbo's 0x100 lower but the
// colours (+2B4: never other here) and "has chosen" (+28F, set at 0079B0 as the choice is made final)
inline constexpr uint32_t kPlayerSize = 0x300;
inline constexpr uint32_t kChosen = 0x28F, kSide = 0x281, kPerson = 0x28E, kTagged = 0x290, kCharacter = 0x291,
                          kButtonsNow = 0x292, kButtonsBefore = 0x294, kOpponent = 0x29E, kOpponentCharacter = 0x2A2,
                          kColourChoice = 0x2B4;
// None of these here: one thing thrown at a time, no being seen from the other player, keeping in always (003224 has
// no tests first); a round's start clears the records to +280 (0028C2, 0xA0 longs each)
inline constexpr uint32_t kThrownToo = 0, kSeenAsOther = 0, kNotKeptIn = 0, kClearedTo = 0x280;
// The computer's plan for the character, a long the computer's set-up writes (its sign of having been set up); 0:
// none kept in the record (the set-up's +273, 2, is the sign then)
inline constexpr uint32_t kPlanTable = 0;

// The box tables' kinds (src/ssf2t.h's kTable...: head +0, body +2, legs +4 in every game): an attack's and the pushing
// box's places in the tables, and the bytes of an attack box (this game's hit test, 07E3D2 and 07E43C: an attack's
// table at +8, twelve bytes a box, the pushing box's at +A)
inline constexpr uint32_t kTableAttack = 0x8, kTablePush = 0xA, kAttackBoxSize = 0x0C;

// The guard decision as a hit lands (src/guard.cpp): a table of rows by the guarding pose, at 07D95A (read at 07D90A).
// Super Turbo's kind of it goes by the attack's +0B through the table's routines (the bytes of the table:
// kGuardTypeBytes, 0 for none); the classic games' by the attack's +07 through a row of bytes for each guarding pose
// (kGuardRows: where the rows' table is, 0 for none)
inline constexpr uint32_t kGuardTypeBytes = 0, kGuardRows = 0x07D95A;

// The loop with no coin in (--watch FF8000-FF8007, pictures every 250 frames): the board's start steps 2 to 10 and ends
// at frame 874 (Champion Edition's steps); 2 the introduction and the title (the title from FF8004 = 8), 4 and 8 a fade, 0A the demonstration (the
// player select and a fight the computer plays for both), 0C to 12 the rankings, then 0 again
inline constexpr uint16_t kIntro = 0x02, kTitleStep = 0x08, kDemo = 0x0A, kLoopEnd = 0x14, kBooted = 0x10;
inline constexpr int kBootFrames = 880; // (its board's start ends at frame 874)

// The twelve characters (the stage word and the character byte: stage n is character n's)
inline constexpr int kCharacters = 12, kStages = 12;
inline constexpr const char *kCharacterNames[kCharacters] = {"Ryu",     "E. Honda", "Blanka",   "Guile", "Ken",    "Chun-Li",
                                                             "Zangief", "Dhalsim",  "M. Bison", "Sagat", "Balrog", "Vega"};
inline constexpr const char *kStageNames[kStages] = {
    "Ryu's (the castle)",      "E. Honda's (the bathhouse)", "Blanka's (the river)",    "Guile's (the air base)",
    "Ken's (the harbour)",     "Chun-Li's (the market)",     "Zangief's (the factory)", "Dhalsim's (the palace)",
    "M. Bison's (the temple)", "Sagat's (the Buddha)",       "Balrog's (Las Vegas)",    "Vega's (the cage)"};
// Each stage's tune: the game's own table by stage (0063BE, read at 0063B0 as a round's music is asked for)
inline constexpr uint16_t kStageTunes[kStages] = {0x01, 0x02, 0x03, 0x05, 0x04, 0x06, 0x07, 0x08, 0x0C, 0x0B, 0x09, 0x0A};
inline constexpr uint16_t kTuneVersus = 0x0F;
// The sound queue: a ring of 64 words at A5+19E, the word at A5+22 the place of the next (006342 puts one in; 006302
// gives the oldest to the sound board, its low byte the code at 800181); F0 everything stopped
inline constexpr uint16_t kSoundEntry = 2, kSoundQueueMask = 0x7E, kTuneOff = 0x00F0;

// The words holding the camera's place across (RAM dumps through a fight: the camera's own and one more)
inline constexpr uint32_t kCameraCopies[] = {0xFF8BD8, 0xFF8CF4}; // (Champion Edition's, 0x14 on)

// The routines that have an object drawn only while it is near the board's picture (Super Turbo's 003036, 00306E and
// 0030A6, and a fourth of the World Warrior's line: 0 where a game has none): where each tests the object's place
// across, and where they all go on to have it drawn. src/widescreen.cpp widens them.
inline constexpr uint32_t kNearPicture1 = 0x0028D2, kNearPicture2 = 0x00290A, kNearPicture3 = 0x002940, kNearPicture4 = 0x002988;
inline constexpr uint32_t kNearPictureDrawn = 0x002960;

// Free play is DIP switch C's bit 2 (on at 0), which the game reads into A5+2CF (00171A)
inline constexpr bool kFreePlayDip = true;
inline constexpr int kFreePlayBank = 2, kFreePlayBit = 2;
// The Board page is game.cfg's (the DIP switches)
inline constexpr bool kBoardInSources = false;

// Objects by what they are (object dumps through fights): 01 player 1, 02 player 2, 0A the shadows, 0C the bars, 0D the
// score, the clock and the round's words, 0B, 0E and 0F the sparks of hits and what is thrown, 1A-1F the stage's pieces
inline bool IsFighter(uint16_t attributes) {
    const unsigned set = attributes & 0x1F;
    return set == 0x01 || set == 0x02;
}
inline bool IsStagePiece(uint16_t attributes) {
    return (attributes & 0x1F) >= 0x1A;
}
inline constexpr unsigned kShadowSet = 0x0A, kSparkSet = 0x0E;
inline constexpr uint32_t kPlayerSets[2] = {1u << 1, 1u << 2};

// The characters a player can choose: the eight world warriors (the four bosses are the computer's alone; battle.cpp)
#define SSF2T_FAMILY_CHOOSABLE 8

// The Ball battle's court, each stage's side (games/ssf2t/src/widescreen.cpp; by kStage): the classic stages' planes
// end sooner than Super Turbo's on E. Honda's and M. Bison's (black past 128), the rest as Super Turbo's
#define SSF2T_FAMILY_COURT_SIDES {128, 128, 144, 192, 128, 128, 192, 192, 128, 160, 96, 192}
// ... but a CPS-1 places its objects across in 512 columns only (games/ssf2t/src/widescreen.cpp, kCourtMost): the
// court is the widescreen picture, 64 a side, on every stage, or the fighters went undrawn in its outer columns
#define SSF2T_FAMILY_COURT_MOST 64

// The Ball battle's hit sounds (games/ssf2t/src/court.cpp): light, medium, hard, specials and supers as hard ones
// (queued as the training's dummy was hit by each button, against the strength the hit leaves at the victim's +4C)
#define SSF2T_FAMILY_HIT_SOUNDS {0x002B, 0x002C, 0x002A, 0x002A, 0x002A}

} // namespace ssf2t::family
