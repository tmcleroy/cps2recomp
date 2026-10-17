// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Vampire Savior Definitive: what Vampire Savior 2's engine does otherwise for the characters taken from it, where
// this game's engine has the same code without it (Vampire Savior 2's code compares the character's number there).
// Each is a hook at the place of this game's code that Vampire Savior 2's changed; the place there is in its remark.
// What Vampire Savior 2 changed for every character (its own rules) is not here: the characters taken in play by
// this game's rules.
#include "definitive.h"
#include "vsav.h"

namespace cps2recomp {

namespace {
    using namespace vsav;
    using namespace definitive;

    uint8_t CharacterOf(uint32_t record) {
        return Ram8((record & 0xFFFFFF) + kCharacter);
    }
    // The player a player's record faces (its +32: the other's record, a word)
    uint32_t Opponent(uint32_t record) {
        return 0xFF0000u | Ram16((record & 0xFFFFFF) + 0x32);
    }

    // 00A718 (008F50 there): with Jedah (A1) against anyone but Donovan (A0) and A5+160 at 4, A5+146 is set to 4
    bool JedahAgainst(M68K &c) {
        if (CharacterOf(c.a[0]) != kDonovan) {
            return false;
        }
        c.pc = At(0x00A730);
        return true;
    }

    // 018460, 018508 (016D2C, 016DDC there): the reaction to an attack, by its kind (A3+17; D0 doubled), for the
    // player hit (A1): to one of kind 33, Donovan's is 0F, not the kind's own (0170C8 there)
    bool ReactionByKind(M68K &c) {
        if ((c.d[0] & 0xFFFF) == 0x33 * 2 && CharacterOf(c.a[1]) == kDonovan) {
            SetRam8((c.a[1] & 0xFFFFFF) + 0x54, 0x0F);
            c.cycles += 32;
            c.pc = c.Pop32();
            return true;
        }
        return OffsetTable(c); // (and the kinds Vampire Savior 2 has more of: graft.cfg, extend)
    }

    // 024C80 (0238C4 there): Donovan does not call 004122 (0041BE there) as this state starts
    bool NoCallForDonovan(M68K &c) {
        if (CharacterOf(c.a[6]) != kDonovan) {
            return false;
        }
        c.pc = At(0x024C86);
        return true;
    }

    // 025CA0 (0248FA there): Donovan does not turn about as this state starts
    bool NoTurnForDonovan(M68K &c) {
        if (CharacterOf(c.a[6]) != kDonovan) {
            return false;
        }
        c.pc = At(0x025CA6);
        return true;
    }

    // 05F152 (06B1A0 there): a palette by the character (D6) and its colours (D0), in a table of 32 bytes each
    // (3AC000 here; 3C117C there): Donovan's is Vampire Savior 2's, at D0 + C6 there
    constexpr uint32_t kTheirPalettes = 0x3C117C;
    bool PaletteForDonovan(M68K &c) {
        if ((c.d[6] & 0xFF) != kDonovan || !ExtraBase()) {
            return false;
        }
        c.a[0] = ExtraBase() + kTheirPalettes;
        SetLow16(c.d[0], (c.d[0] + 0xC6) & 0xFFFF);
        c.pc = At(0x05F162); // (on to lsl.w #5,d0 and the lea)
        return true;
    }

    // 07A95C, 07A98C (085A68, 085ABA there): how high the thing is put by its player's (A4) opponent's height
    // (D0: its +36 halved): 8 less for Donovan
    bool HeightAgainstDonovan(M68K &c) {
        if (CharacterOf(Opponent(c.a[4])) == kDonovan) {
            SetLow16(c.d[0], (c.d[0] - 8) & 0xFFFF);
        }
        return false; // (the game's own code goes on)
    }
} // namespace

void definitive::RegisterRules() {
    AddHook(At(0x00A720), "JedahAgainst", JedahAgainst);
    AddHook(At(0x018460), "ReactionByKind", ReactionByKind);
    AddHook(At(0x018508), "ReactionByKind", ReactionByKind);
    AddHook(At(0x024C80), "NoCallForDonovan", NoCallForDonovan);
    AddHook(At(0x025CA0), "NoTurnForDonovan", NoTurnForDonovan);
    AddHook(At(0x05F152), "PaletteForDonovan", PaletteForDonovan);
    AddHook(At(0x07A95C), "HeightAgainstDonovan", HeightAgainstDonovan);
    AddHook(At(0x07A98C), "HeightAgainstDonovan", HeightAgainstDonovan);
}

} // namespace cps2recomp
