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
}

} // namespace cps2recomp
