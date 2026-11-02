// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Super Street Fighter II Turbo: the hidden characters with no code (the Game page's "Hidden characters"). The game
// has two: Akuma (a code on the stick, then the three punches at once: 00E942) and each character's old version, as it
// was in Super Street Fighter II (another code: 015788). With the feature, two buttons pressed at once as a character is
// chosen give its old version, and the three punches at once give Akuma.
//
// The choice is the game's own: a button newly pressed on the cursor's character chooses it and its colours (00E9FE on:
// light punch to heavy kick, sets 0 to 5; 00EA5C where they meet), and the choice is made final once that button is let
// go (00E93A; held a while it gives set 7). What was pressed is kept from the first, and the hidden character made at
// the second as the game's codes make it: Akuma's flag and Ryu's number with its own colours (0A, or 0B when the other
// player is Akuma in 0A), or the old version's flag with its own colours (8 from punches, 9 from kicks, the other one
// when the other player is the same old character in them).
#include "ssf2t.h"

#include "state.h"

namespace cps2recomp {
void ApplyInputLabCharacter(uint32_t player);

namespace {
    using namespace ssf2t;

    constexpr uint32_t kButtonsNow = 0x392, kButtonsBefore = 0x394; // words: the buttons held this frame and the last
    constexpr uint16_t kPunches = 0x070, kKicks = 0x700;
    constexpr uint8_t kAkumaColours = 0x0A, kOldPunchColours = 0x08, kOldKickColours = 0x09;
    uint16_t g_pressed[2] = {0, 0}; // what chose each player's character (kept with the game's state)

    int PlayerOf(uint32_t record) {
        record &= 0xFFFFFF;
        return record == Player(0) ? 0 : record == Player(1) ? 1 : -1;
    }
    int Bits(uint16_t word) {
        int bits = 0;
        for (; word; word &= static_cast<uint16_t>(word - 1)) {
            bits++;
        }
        return bits;
    }

    // 00EA5C: a button has chosen the character of the player at A4
    bool Chose(M68K &c) {
        const uint32_t player = c.a[4] & 0xFFFFFF;
        const int n = PlayerOf(player);
        if (n >= 0) {
            g_pressed[n] = Ram16(player + kButtonsNow) & static_cast<uint16_t>(~Ram16(player + kButtonsBefore)) & (kPunches | kKicks);
        }
        return false;
    }

    // 00E93A: the choice is final
    bool Final(M68K &c) {
        const uint32_t player = c.a[4] & 0xFFFFFF;
        const int n = PlayerOf(player);
        if (n < 0) {
            return false;
        }
        if (!FeatureOn("hidden_characters")) {
            ApplyInputLabCharacter(player);
            return false;
        }
        const uint16_t pressed = g_pressed[n];
        g_pressed[n] = 0;
        const uint32_t other = Player(1 - n);
        if ((pressed & kPunches) == kPunches) {
            SetRam8(player + kAkuma, 1);
            SetRam8(player + kCharacter, 0);
            SetRam8(player + kOld, 0);
            const bool otherAkuma = Ram8(other + kAkuma) && Ram8(other + kColourChoice) == kAkumaColours;
            SetRam8(player + kColourChoice, otherAkuma ? kAkumaColours + 1 : kAkumaColours);
        } else if (Bits(pressed) >= 2) {
            SetRam8(player + kOld, 1);
            uint8_t colours = (pressed & kPunches) ? kOldPunchColours : kOldKickColours;
            if (Ram8(other + kOld) && Ram8(other + kCharacter) == Ram8(player + kCharacter) && Ram8(other + kColourChoice) == colours) {
                colours = colours == kOldPunchColours ? kOldKickColours : kOldPunchColours;
            }
            SetRam8(player + kColourChoice, colours);
        }
        ApplyInputLabCharacter(player);
        return false;
    }
} // namespace

void ChoosingState(Archive &ar) {
    ar.Mark(0x43484F4F); // CHOO
    ar.Pod(g_pressed);
}

void RegisterChoosing() {
    AddHook(HookAt(0x00EA5C), "Chose", Chose);
    AddHook(HookAt(0x00E93A), "ChoiceFinal", Final);
    TheGame().features.push_back({"hidden_characters", "Game", "Hidden characters",
                                  "The hidden characters with no code: press two buttons at once as you choose a character for its "
                                  "old version (as in Super Street Fighter II), or the three punches at once for Akuma."});
}

} // namespace cps2recomp
