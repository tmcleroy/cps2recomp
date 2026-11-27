// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Street Fighter II': Champion Edition as one of the games built from Super Street Fighter II Turbo's sources
// (games/ssf2t/src; places.h there): where the places those sources name are in this game, and what is this game's
// own. Super Turbo's program grew out of this one (through Super Street Fighter II): tools/portmap.py places a quarter of
// Super Turbo's instructions here at once, and the players' records, the camera's and the steps are laid out alike.
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
    {0x002EE2, 0x0022D8}, // an instruction, 12 on alike
    {0x002F50, 0x002346}, // an instruction, 15 on alike
    {0x0043F4, 0x003224}, // keeping a player inside the picture (clr.b 0x48(a6) first: no tests of +19D, +1F5) by hand
    {0x004634, 0x00335C}, // the throw test where its ways in meet (move.b 0x281(a6),...; tst.b 0x191(a6)) (the court) by hand
    {0x0047D0, 0x003492}, // the throw test's no (moveq #0,d0; rts) (the court) by hand
    {0x00490A, 0x003810}, // sub.w d4,0x2A(a3), a throw's damage at once (after its tst.b 9D4) by hand
    {0x0049B0, 0x003512}, // moved as the code before or after it, 6 on alike
    {0x004A58, 0x0035AC}, // move.w d4,0x78(a3), a throw's damage put by (after bsr 3626) by hand
    {0x004BDA, 0x000000}, // none: a classic throw's damage has no random part by hand
    {0x004BDC, 0x000000}, // none: a classic throw's damage has no random part by hand
    {0x00502A, 0x00394C}, // a place the code names so (1/1)
    {0x008FFC, 0x000000}, // none: the Board page is the DIP switches (game.cfg) by hand
    {0x00D69C, 0x00796A}, // moved as the code before or after it, 9 on alike
    {0x00D714, 0x0079E2}, // the same entry of a jump table, 2 on alike
    {0x00E93A, 0x000000}, // none: no hidden characters to choose (choosing.cpp) by hand
    {0x00EA5C, 0x000000}, // none: as above by hand
    {0x00FCE2, 0x0092DE}, // an instruction, 6 on alike
    {0x00FCE6, 0x0092E2}, // an instruction, 5 on alike
    {0x00FD62, 0x009382}, // an instruction, 6 on alike
    {0x00FD66, 0x009386}, // an instruction, 5 on alike
    {0x00FDEE, 0x009432}, // an instruction, 5 on alike
    {0x00FE0C, 0x009444}, // an instruction, 6 on alike
    {0x00FE74, 0x0094A6}, // subq.b #1,0xABF(a5): the round clock's frames of the second by hand
    {0x0102F2, 0x009A2A}, // an instruction, 1 on alike
    {0x0104B2, 0x000000}, // none: the panel has no faces by hand
    {0x078030, 0x0379F0}, // a player's routine: tst.b 0x28E(a6), a person's step or the computer's (0379E0 calls) by hand
    {0x07804A, 0x037A1E}, // an instruction, 4 on alike
    {0x078052, 0x037A26}, // an instruction, 2 on alike
    {0x078270, 0x037CC2}, // an instruction, 3 on alike
    {0x0782D4, 0x037D14}, // an instruction, 4 on alike
    {0x07AC78, 0x039CF0}, // the hit looked for at the end of the step (cmpi.b #2,2(a6); tst.b 0x49(a6)) (the court) by hand
    {0x07BB82, 0x03A5A8}, // an instruction, 6 on alike
    {0x07BC12, 0x03A612}, // an instruction, 9 on alike
    {0x07D6F4, 0x03BD90}, // an instruction, 1 on alike
    {0x07E274, 0x000000}, // none: the computer throws through the same test (the court) by hand
    {0x07E36A, 0x000000}, // none: as above (the court) by hand
    {0x07E36E, 0x03C714}, // an instruction, 4 on alike
    {0x07F1C6, 0x03D0C4}, // the stun's most: the word of cmpi.w #$1F,0x5E(a6) at 03D0C2 (a fixed 1F) by hand
    {0x090F6A, 0x045E1E}, // Vega's jump to the wall (the same code, found by its instructions) by hand
    {0x0A3472, 0x000000}, // none: Vega's jump to the wall has one copy (090F6A's) by hand
    {0x0B3156, 0x000000}, // none: as above by hand
    {0x0BE646, 0x09502A}, // sub.w d6,0x2A(a2), a player's attack's damage (clr.b 0x71(a2) after it) by hand
    {0x0BE678, 0x095042}, // clr.b 0x71(a2), where that hit's ways meet by hand
    {0x0BEBA6, 0x0951D2}, // sub.w d6,0x2A(a2), a thrown thing's damage (portmap took the player's) by hand
    {0x0BEBDA, 0x0951E6}, // where that hit's ways meet by hand
    {0x0BEE72, 0x095574}, // moveq #1,d6 after a hit's random part is added (0BEE40's code alike) by hand
    {0x0BEE74, 0x095576}, // mulu d6,d5 after it, where both ways meet by hand
    {0x0BEED8, 0x000000}, // none: the classic guard decision goes by a table of rows (family.h, kGuardRows) by hand
    {0x0BEF66, 0x000000}, // none: the classic guard decision goes by a table of rows (family.h, kGuardRows) by hand
    {0x0BEF7C, 0x000000}, // none: the classic guard decision goes by a table of rows (family.h, kGuardRows) by hand
    {0x0BEF82, 0x000000}, // none: the classic guard decision goes by a table of rows (family.h, kGuardRows) by hand
    {0x0BEF88, 0x000000}, // none: the classic guard decision goes by a table of rows (family.h, kGuardRows) by hand
    {0x0BEF8A, 0x000000}, // none: the classic guard decision goes by a table of rows (family.h, kGuardRows) by hand
    {0x0BF662, 0x09626A}, // the players pushed apart (its clr.w; lea of player 1) (the court) by hand
    {0x0CC4E4, 0x09C77C}, // the same entry of a jump table, 1 on alike
    {0x0CC630, 0x09C880}, // the camera's place up (tests ACE and ACC: no flag of its own first) by hand
    {0x0CCC5C, 0x09CDC2}, // where the code calls or jumps to it (1/1), not its first instruction's place
    {0x100000, 0x100000}, // a place the code names so (6/7)
    {0x24EAE8, 0x0C6698}, // a place the code names so (1/1)
    {0xFF0A7A, 0xFF8022}, // the sound queue's word: the place of the next (006342) by hand
    {0xFF0A7E, 0xFF819E}, // the sound queue: a ring of words (006342) by hand
    {0xFF8004, 0xFF8004}, // a variable the code names so (14/14)
    {0xFF8008, 0xFF8008}, // a variable the code names so (45/46)
    {0xFF800C, 0xFF800C}, // a variable the code names so (57/60)
    {0xFF8086, 0xFF807A}, // a variable the code names so (8/9)
    {0xFF808A, 0xFF807E}, // a variable the code names so (7/8)
    {0xFF82AE, 0xFF82A2}, // a variable between two that moved alike
    {0xFF82B6, 0xFF82AA}, // a variable between two that moved alike
    {0xFF82D0, 0xFF82C0}, // a variable the code names so (1/1)
    {0xFF82D3, 0xFF82C3}, // a variable between two that moved alike
    {0xFF82DA, 0xFF82CA}, // a variable the code names so (2/2)
    {0xFF82DD, 0x000000}, // none: demo sound is a DIP switch by hand
    {0xFF82DF, 0xFF82CF}, // a variable the code names so (2/2)
    {0xFF82E9, 0xFF82D9}, // a variable the code names so (4/4)
    {0xFF82F0, 0xFF82E0}, // a variable the code names so (8/8)
    {0xFF8342, 0x000000}, // none: the coins are DIP switches by hand
    {0xFF83A9, 0x000000}, // none: no speeds by hand
    {0xFF844E, 0xFF83BE}, // a variable the code names so (30/32)
    {0xFF8C4E, 0xFF89BE}, // a variable the code names so (22/22)
    {0xFF8C54, 0xFF89C4}, // a variable the code names so (3/3)
    {0xFF8C56, 0xFF89C6}, // a variable the code names so (3/3)
    {0xFF8C58, 0xFF89C8}, // a variable the code names so (3/3)
    {0xFF8C5A, 0xFF89CA}, // a variable the code names so (3/3)
    {0xFF8DCE, 0xFF8ABE}, // a variable the code names so (14/14)
    {0xFF8DCF, 0xFF8ABF}, // a variable between two that moved alike
    {0xFF8DD2, 0xFF8AC2}, // a variable the code names so (13/17)
    {0xFF8DDC, 0xFF8ACC}, // a variable the code names so (3/3)
    {0xFF8DDE, 0xFF8ACE}, // a variable the code names so (4/4)
    {0xFF8DEE, 0x000000}, // none: no first attack (the court) by hand
    {0xFF8ECE, 0xFF8BBE}, // a variable the code names so (3/3)
    {0xFF8ED4, 0xFF8BC4}, // a variable the code names so (15/16)
    {0xFF8ED8, 0xFF8BC8}, // a variable the code names so (9/10)
    {0xFF8F90, 0x000000}, // none: the camera up has no flag of its own by hand
    {0xFF954C, 0xFF914C}, // a variable the code names so (4/4)
    {0xFF9580, 0xFF9168}, // a variable the code names so (4/5)
    {0xFF97A2, 0xFF9376}, // a variable the code names so (2/2)
    {0xFFA9A2, 0xFFA576}, // a variable the code names so (2/2)
    // places: end
};
inline constexpr auto kPlaces = std::to_array(kPlaceList);

// What Super Turbo has and this game has not: the super combo gauge, the hidden characters, the speeds, a fighter's reach
inline constexpr bool kTurbo = false;

// The sound program (the classic board's, on its Z80 with the YM2151 and the OKI): its table of songs is at 1106, two
// bytes a song (the place in the ROM, high byte first: 01EC and 002D read it so), 0x8E of them (the byte at 1100); a song
// whose data starts with 0 is music (01FA: the others are effects, by priority, and the OKI's samples, bit 7)
inline constexpr uint32_t kSongTable = 0x1106, kSongAddresses = 0, kSongCount = 0x8E, kSongEntry = 2;
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

// The characters' colours: two sets each, one palette a set. The normal colours are the common block of object
// palettes the game copies to the board as a round is set up (0A4268, 001B66: set n+1 is character n's), and the other
// colours, for the same character twice, a table by character (096804: a word a character, to a count and its
// palettes), copied over the next set (a fighter is drawn in set character + 1 + its colours byte: object dumps of
// E. Honda against E. Honda, 2 and 3, set 3 the palette at 096858). PalMod's file for the game is the second program file.
inline constexpr bool kColours = true;
inline constexpr const char *kColourSetNames[] = {"Normal", "Other"};
inline constexpr int kPalettesInSet = 1;
inline constexpr int (*kPalettesOf)(int) = nullptr;
inline uint32_t ColourSet(int who, int set, int &count) {
    count = 1;
    if (set == 0) {
        return 0x0A4268 + static_cast<uint32_t>(who + 1) * 0x20;
    }
    constexpr uint32_t kOther = 0x096804;
    const auto word = [](uint32_t at) { return static_cast<uint32_t>((cps2recomp::Program(at)[0] << 8) | cps2recomp::Program(at)[1]); };
    const uint32_t first = kOther + word(kOther + static_cast<uint32_t>(who) * 2);
    count = static_cast<int>(word(first));
    return first + 2;
}
inline uint32_t ColourShown(int, int who, int choice) {
    return 0x900000 + static_cast<uint32_t>(who + 1 + (choice ? 1 : 0)) * 0x20;
}
inline constexpr uint32_t kColourRanges[][2] = {{0x0A4288, 0x0A4408}, {0x096834, 0x096AEC}};
inline constexpr const char *kColourFile = "s92_22b.7f";
inline constexpr uint32_t kColourFileAt = 0x080000, kColourFileSize = 0x80000, kColourFileSameFrom = 0x080000, kColourFileSameTo = 0x090000;
inline constexpr const char *kColourFileWhose = "Champion Edition's, World 920513";
inline constexpr const char *kColourFeatureWords =
    "Characters in the colours of the files in the \"palettes\" folder in the game's data folder: a s92_22b.7f changed "
    "with PalMod, and text files, one a character (Ryu.txt), which go before it. With no files the game's own colours. "
    "They are taken when the colours are next copied to the board: at the next round.";
inline constexpr const char *kColourHelp =
    "# To change them, copy this file to the palettes folder and change the colours there. [Normal] is the\n"
    "# character's own, [Other] its colours when both players have chosen it; each has one line of 16\n"
    "# colours. The 16th colour of a line is never seen. A colour is #RRGGBB or #RGB, rounded to the board's\n"
    "# 16 levels of each; - keeps the game's. Sets and lines that are left out keep the game's colours.\n";

// --- What differs between the games built from these sources ---

// The players' records, 0x300 apart from FF83BE (the low part laid out as Super Turbo's: every offset to +1FF the
// sources use compared over the instructions portmap paired); the bytes after it are Super Turbo's 0x100 lower but the
// colours (+2B4: 0 or 1, the other colours for the same character twice, 00865E/00868A) and "has chosen" (+2B5, set at
// 0085E6 when a button chose)
inline constexpr uint32_t kPlayerSize = 0x300;
inline constexpr uint32_t kChosen = 0x2B5, kSide = 0x281, kPerson = 0x28E, kTagged = 0x290, kCharacter = 0x291,
                          kButtonsNow = 0x292, kButtonsBefore = 0x294, kOpponent = 0x29E, kOpponentCharacter = 0x2A2,
                          kColourChoice = 0x2B4;
// None of these here: one thing thrown at a time, no being seen from the other player, keeping in always (003224 has
// no tests first); a round's start clears the records to +280 (0028C2, 0xA0 longs each)
inline constexpr uint32_t kThrownToo = 0, kSeenAsOther = 0, kNotKeptIn = 0, kClearedTo = 0x280;
// The computer's plan for the character, a long the computer's set-up writes (its sign of having been set up); 0:
// none kept in the record (the set-up's +273, 2, is the sign then)
inline constexpr uint32_t kPlanTable = 0x1E0;

// The box tables' kinds (src/ssf2t.h's kTable...: head +0, body +2, legs +4 in every game): an attack's and the pushing
// box's places in the tables, and the bytes of an attack box (this game's hit test, 096126 and 096190: an attack's
// table at +8, twelve bytes a box, the pushing box's at +A)
inline constexpr uint32_t kTableAttack = 0x8, kTablePush = 0xA, kAttackBoxSize = 0x0C;

// The guard decision as a hit lands (src/guard.cpp): a table of rows by the guarding pose, at 09567C (read at 09562C).
// Super Turbo's kind of it goes by the attack's +0B through the table's routines (the bytes of the table:
// kGuardTypeBytes, 0 for none); the classic games' by the attack's +07 through a row of bytes for each guarding pose
// (kGuardRows: where the rows' table is, 0 for none)
inline constexpr uint32_t kGuardTypeBytes = 0, kGuardRows = 0x09567C;

// The loop with no coin in (--watch FF8000-FF8007, pictures every 250 frames): the board's start steps 2 to 10 and ends
// at frame 857; 2 the introduction and the title (the title from FF8004 = 8), 4 and 8 a fade, 0A the demonstration (the
// player select and a fight the computer plays for both), 0C to 12 the rankings, then 0 again
inline constexpr uint16_t kIntro = 0x02, kTitleStep = 0x08, kDemo = 0x0A, kLoopEnd = 0x14, kBooted = 0x10;
inline constexpr int kBootFrames = 860;

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
inline constexpr uint32_t kCameraCopies[] = {0xFF8BC4, 0xFF8CE0};

// The routines that have an object drawn only while it is near the board's picture (Super Turbo's 003036, 00306E and
// 0030A6, and a fourth of the World Warrior's line: 0 where a game has none): where each tests the object's place
// across, and where they all go on to have it drawn. src/widescreen.cpp widens them.
inline constexpr uint32_t kNearPicture1 = 0x00243C, kNearPicture2 = 0x002474, kNearPicture3 = 0x0024AA, kNearPicture4 = 0x0024F4;
inline constexpr uint32_t kNearPictureDrawn = 0x0024CA;

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

// The Ball battle's court, each stage's side (games/ssf2t/src/widescreen.cpp; by kStage): the classic stages' planes
// end sooner than Super Turbo's on E. Honda's and M. Bison's (black past 128), the rest as Super Turbo's
#define SSF2T_FAMILY_COURT_SIDES {128, 128, 144, 192, 128, 128, 192, 192, 128, 160, 96, 192}

// The Ball battle's hit sounds (games/ssf2t/src/court.cpp): light, medium, hard, specials and supers as hard ones
// (queued as the training's dummy was hit by each button, against the strength the hit leaves at the victim's +4C)
#define SSF2T_FAMILY_HIT_SOUNDS {0x002B, 0x002C, 0x002A, 0x002A, 0x002A}

} // namespace ssf2t::family
