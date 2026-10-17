// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Vampire Savior Definitive: the characters taken in at the character choice. The choice's grid is Vampire
// Savior's; three of its places have a second character, Vampire Savior 2's: Gallon's Donovan, Aulbath's Pyron and
// Sasquatch's Phobos. Start pressed on one of them changes the player's to the other (all three at once: the cast
// of one game or of the other), and the picture, the figure and the name beside follow. The game's own choice code
// does the rest: the cursor moves by the place (+03 in the player's record), what is chosen is the character
// (+382), which these hooks set.
#include "definitive.h"
#include "vsav.h"

#include <iterator>

namespace cps2recomp {

namespace {
    using namespace vsav;
    using namespace definitive;

    constexpr uint32_t kPlace = 0x03, kHeldNow = 0x394, kNumber = 0x381; // (choosing.cpp)
    constexpr uint16_t kStartHeld = 0x8000, kPunches = 0x0700, kKicks = 0x7000;
    // The places and their second characters
    struct Second {
        uint8_t place, character;
    };
    constexpr Second kSeconds[] = {{0x02, kDonovan}, {0x09, kPyron}, {0x0A, kPhobos}};
    // Each player's choice of cast (a byte each, 0 Vampire Savior's, 1 Vampire Savior 2's), in the work memory's
    // top, which the game never writes (graft.cfg), so that a save state keeps it
    constexpr uint32_t kSecondCast = 0xFFFFFA;

    const Second *SecondAt(uint8_t place) {
        for (const Second &s : kSeconds) {
            if (s.place == place) {
                return &s;
            }
        }
        return nullptr;
    }
    const Second *SecondOf(uint8_t character) {
        for (const Second &s : kSeconds) {
            if (s.character == character) {
                return &s;
            }
        }
        return nullptr;
    }
    // The character at the player's place, by the cast chosen
    void SetCharacter(uint32_t me) {
        const uint8_t place = Ram8(me + kPlace);
        const Second *second = SecondAt(place);
        const bool theirs = Ram8(kSecondCast + (Ram8(me + kNumber) & 1)) != 0;
        SetRam8(me + kCharacter, second && theirs ? second->character : place);
    }

    // The grid's faces: one picture of them all (the object at FFB800, its picture 2689FA's), this game's; where
    // the cast shown is Vampire Savior 2's, the three places' faces are its three's, from its graphics (its grid has
    // them in other places: each is put as that grid puts a face in the place here), in colours of their own
    // (palettes the screen leaves free; the screen's 32 are at 3A3800 here, 3BAF3C there)
    constexpr uint32_t kGrid = 0xFFB800, kGridPicture = 0x2689FA;
    constexpr uint32_t kOurGridPalettes = 0x3A3800, kTheirGridPalettes = 0x3BAF3C, kPalettes = 0x90C000;
    // Which cast the grid shows (a byte, 1 Vampire Savior 2's: kSecondCast's neighbour, for a save state too)
    constexpr uint32_t kGridCast = 0xFFFFFC;
    struct Face {
        uint16_t ours;               // this game's face (its tile) at the place
        uint16_t theirs, attributes; // Vampire Savior 2's: its tile, size and palette here
        int16_t dx, dy;              // ... where, from this game's (as its own grid has a face in that place)
        uint8_t theirPalette;        // its colours there
    };
    constexpr Face kFaces[] = {
        {0xB4E3, 0xB10B, 0x1216, 6, 7, 0x05},  // Gallon's place: Donovan
        {0xB509, 0xB0F5, 0x1217, -8, -1, 0x11}, // Aulbath's: Pyron
        {0xB50B, 0xB108, 0x1219, -16, 0, 0x13}, // Sasquatch's: Phobos
    };

    // Which cast the grid shows: the player choosing's; with both choosing, of different casts, each in turn
    // (about a second each). A player is choosing while its frames come (the 68000's cycle of its last)
    uint64_t g_choosingAt[2] = {};
    constexpr uint64_t kStillChoosing = 0x100000; // (about four frames)
    void ShowGrid(M68K &c, uint32_t me) {
        if (!ExtraBase()) {
            return;
        }
        const int player = Ram8(me + kNumber) & 1;
        g_choosingAt[player] = c.cycles;
        bool theirs = Ram8(kSecondCast + player) != 0;
        const bool otherChoosing = c.cycles - g_choosingAt[player ^ 1] < kStillChoosing;
        if (otherChoosing && (Ram8(kSecondCast + (player ^ 1)) != 0) != theirs) {
            theirs = (c.cycles >> 24) & 1;
        }
        SetRam8(kGridCast, theirs ? 1 : 0);
        // Their colours, once the screen has faded in (a face's palette, Demitri's, as the screen's are)
        const uint8_t *demitri = Program(kOurGridPalettes + 0x06 * 32 + 2);
        if (c.bus.Read16(kPalettes + 0x06 * 32 + 2) != (0xF000 | (demitri[0] << 8) | demitri[1])) {
            return;
        }
        for (const Face &face : kFaces) {
            const uint8_t *from = ExtraProgram(kTheirGridPalettes + face.theirPalette * 32);
            const uint32_t to = (face.attributes & 0x1F) * 32;
            for (uint32_t i = 0; i < 16; i++) {
                const uint16_t colour = static_cast<uint16_t>(0xF000 | (from[i * 2] << 8) | from[i * 2 + 1]);
                c.bus.Write16(kPalettes + to + i * 2, colour);
            }
        }
    }

    // 01B2BE (the pictures' drawing: an object's four words, D0-D3, to A2): the grid's pieces at the three places
    // are Vampire Savior 2's faces, where it shows that cast. An object is drawn from that game's graphics when its
    // program was read just before it is written (Machine::ObjectWritten).
    uint16_t Moved(uint32_t place, int by) {
        return static_cast<uint16_t>((place & 0xFC00) | ((place + by) & 0x3FF));
    }
    bool GridFace(M68K &c) {
        if ((c.a[6] & 0xFFFFFF) != kGrid || Ram32(kGrid + 0x1C) != kGridPicture || !Ram8(kGridCast) || !ExtraBase()) {
            return false;
        }
        for (const Face &face : kFaces) {
            if ((c.d[2] & 0xFFFF) != face.ours) {
                continue;
            }
            SetLow16(c.d[0], Moved(c.d[0], face.dx));
            SetLow16(c.d[1], Moved(c.d[1], face.dy));
            SetLow16(c.d[2], face.theirs);
            SetLow16(c.d[3], face.attributes);
            c.bus.Read16(ExtraBase()); // (so that the object is drawn from their graphics)
            return false;
        }
        return false;
    }

    // 0209E0: the choice begins at the character chosen last; a character taken in is at its place, the cast its
    bool ChoiceBegins(M68K &c) {
        const uint32_t me = c.a[6] & 0xFFFFFF;
        if (const Second *second = SecondOf(Ram8(me + kPlace))) {
            SetRam8(me + kPlace, second->place);
            SetRam8(kSecondCast + (Ram8(me + kNumber) & 1), 1);
        }
        return false;
    }

    // 020A84 (every frame of the choice, after the cursor has moved; D3 the buttons newly pressed): Start on a
    // place with a second character changes the cast
    bool ChoiceFrame(M68K &c) {
        const uint32_t me = c.a[6] & 0xFFFFFF;
        const uint32_t cast = kSecondCast + (Ram8(me + kNumber) & 1);
        if (SecondAt(Ram8(me + kPlace)) && (c.d[3] & kStartHeld)) {
            SetRam8(cast, Ram8(cast) ? 0 : 1);
        }
        SetCharacter(me);
        ShowGrid(c, me);
        return false;
    }

    // 020A90 (a character chosen, before its colours: 020B38): Dark Gallon is still Gallon's place with Start held
    // and two punches or two kicks, whichever cast
    bool ChoiceMade(M68K &c) {
        const uint32_t me = c.a[6] & 0xFFFFFF;
        const uint16_t held = Ram16(me + kHeldNow);
        const uint16_t buttons = held & (kPunches | kKicks);
        const bool two = (buttons & kPunches) && (buttons & kPunches) != 0x0100 && (buttons & kPunches) != 0x0200 &&
                         (buttons & kPunches) != 0x0400;
        const bool twoKicks = (buttons & kKicks) && (buttons & kKicks) != 0x1000 && (buttons & kKicks) != 0x2000 &&
                              (buttons & kKicks) != 0x4000;
        if (Ram8(me + kPlace) == 0x02 && (held & kStartHeld) && (two || twoKicks)) {
            SetRam8(me + kCharacter, 0x02);
        }
        return false;
    }
} // namespace

void definitive::RegisterChoosing() {
    AddHook(At(0x0209E0), "ChoiceBegins", ChoiceBegins);
    AddHook(At(0x020A84), "ChoiceFrame", ChoiceFrame);
    AddHook(At(0x020A90), "ChoiceMade", ChoiceMade);
    AddHook(At(0x01B2BE), "GridFace", GridFace);
}

} // namespace cps2recomp
