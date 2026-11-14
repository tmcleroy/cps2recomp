// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Morphus56K. Part of cps2recomp.
//
// Marvel vs. Capcom: Cross Fever, the Dreamcast's mode for four people, on top of the toolkit's tag team (fight/tag.h;
// tag.cpp here says where the game keeps the character on point, and whose each character is). On the board a side's
// two characters both read the side's one panel, which in a Duo Team Attack (the partner in beside the character on
// point for a time) moves both. The Dreamcast's executable tests its Cross Fever flag in fifty places; those that change
// the fight are done here, each at the board's routine that the Dreamcast's does the same work in:
//
//   - Each character reads its own person's controller all the time: the Dreamcast reads a character's stream by its
//     byte +275 (the board's +281, 1 off point: 8C029E40) and turns the side's selector over where the bytes are swapped
//     (8C02CAA2 and on) and where a side starts with its second character (8C03AA0C), so that each stream stays with its
//     person. Here 01368E, by whose each character is (tag.cpp).
//   - The Duo Team Attack's time is counted down only while the game's count of steps is odd, so it lasts twice as long
//     (8C02CF20). Here 015F68.
//   - Every seventh step of a match each side's hyper combo gauge goes up by one, of the 0x90 a level (8C03606A: the
//     Dreamcast's gauge routine, the board's 01CD1E, for the first character of each side, when its own count of the
//     steps, 8C1E24A8, comes to seven). Here 01CEAA, with the same count.
//   - The character off point, when it is in the fight but not in a Duo Team Attack (coming in for a Variable Counter,
//     going off after an attack), has its own person's controls read and its moves looked for, unless the side's moves
//     are held (8C03025C passes it by in the other modes; the Dreamcast's step has nothing of the board's 00D4AC). Here
//     00D4AC and 018DB6.
//   - The taunt goes while the person on point holds Start (8C032674: that controller's Start as it is held, not as it
//     is pressed). Here 01A196. (In its versus modes the Dreamcast also wants the player's taunt button held alone,
//     8C4EBF9A, light punch unless changed, since Start alone pauses its game; here Start is the taunt, as on the board.)
//
// The rest is the board's, which the Dreamcast's game shares: the attack's length (016E02: by the gauge's levels, and
// half the bar) and who stays in at its end (015FF0 goes by what each character is doing, as the Dreamcast's does).
// Every place is from the listing (tools/lst.py); traced with games/mvsc/movies/cross_fever.txt.
#include "alternative.h"
#include "state.h"
#include "tag.h"
#include "mvsc.h"

namespace cps2recomp {

int TagPick(uint32_t record); // tag.cpp

namespace {
    using namespace mvsc;

    constexpr const char *kFeature = "cross_fever";

    // On with tag team's people routed (co-op or 2 vs 2, and no mode of the sources' own)
    bool On() {
        return FeatureOn(kFeature) && tag::Routing();
    }

    // A person's controls as the game reads a panel (000D2E): the stick and the punches in the low byte, the kicks in
    // bits 8-10
    uint16_t PanelWord(uint16_t held) {
        return static_cast<uint16_t>((held & 0x7F) | (((held >> 7) & 7) << 8));
    }

    // Bytes of a character's record: 1 while it is off point (kept by the swaps); and the game's: bit n set while side n's
    // moves are held (FF4031: a tag on its way, 014ACC), bits 2 and 4 while nobody's are looked for (FF400F)
    constexpr uint32_t kOffPoint = 0x281;
    constexpr uint32_t kMovesHeld = 0xFF4031, kNoMoves = 0xFF400F;

    // Off point, outside its side's Duo Team Attack (kDuoOn)
    bool OffPoint(uint32_t record) {
        const int side = SideOf(record);
        return side >= 0 && Ram8(record + kOffPoint) != 0 && !((Ram8(kDuoOn) >> side) & 1);
    }

    // 01368E: a record a person plays (A6) reads its side's panel: the word into D0 (its high half 0), through the table
    // at 0136F4 (into A0: kPanel). In Cross Fever each record reads its own person's controls instead, whatever it is
    // doing (on point, beside the other in a Duo Team Attack or a team super, coming in, resting). So the person on point
    // is the one who tags out, as on the Dreamcast: the routing's panel, which has the partner's tag buttons too, is not
    // read, and the partner's buttons are its own character's.
    bool ReadPanel(M68K &c) {
        if (!On()) {
            return false;
        }
        const uint32_t record = c.a[6] & 0xFFFFFF;
        const int side = SideOf(record);
        if (side < 0) {
            return false;
        }
        const int person = tag::Person(side, TagPick(record));
        if (person < 0) {
            return false;
        }
        const uint16_t word = PanelWord(tag::Held(person));
        c.a[0] = kPanel[side];
        c.d[0] = word;
        // (the flags as move.w (a0),d0 leaves them, and X as the two add.w before it)
        c.flagN = (word & 0x8000) != 0;
        c.flagZ = word == 0;
        c.flagV = c.flagC = c.flagX = false;
        Spend(c, 50); // (the six instructions' time, moveq to move.w)
        c.pc = 0x01369E;
        return true;
    }

    // 015F68: a side's time for its Duo Team Attack (A4: kDuoTime's word) taken one off. In Cross Fever only on odd steps
    // (kStepCount); on even ones on to 015F6C, as when the time is held, the attack's bit set again.
    bool DuoTime(M68K &c) {
        if (!On() || (Ram8(kStepCount + 1) & 1)) {
            return false;
        }
        Spend(c, 20); // (subq.w and ble's time)
        c.pc = 0x015F6C;
        return true;
    }

    // The steps counted for the gauge, as the Dreamcast counts them (8C1E24A8): one more each time its 8C036052 runs
    // (once a step of a match, from the intro on: 8C0221E8, the board's 00CEE2), and in Cross Fever back to 0 at seven,
    // when both sides get their 1. (The Dreamcast's goes on counting outside Cross Fever; here it stops at seven, which
    // comes to the same.) Kept with save states and across matches, but cleared when the board boots.
    uint8_t g_gaugeSteps;

    // 000FAC: the board's boot calls its settings reader (007E7A; board.cpp hooks its return at 000FB2).
    // Machine::Reset clears the board's RAM, not this host-side count. Reset it here even if Cross Fever is off,
    // so a reset/rehost starts with the same gauge phase as a fresh process. Ordinary match transitions keep it.
    bool GaugeBoot(M68K &) {
        g_gaugeSteps = 0;
        return false;
    }

    // 01CEAA, once a step of a match (00CEE2's jsr is the only way in: 00CEE8 is on the stack), before the health a
    // resting character gets back. In Cross Fever, at every seventh step (g_gaugeSteps), the gauge routine (01CD1E: D0
    // added to A6's side, with the board's own tests: in the fight, not in a Duo Team Attack, levels and their sound)
    // for player 1's first character and then player 2's, as the Dreamcast's 8C036052 does before its own part, which is
    // this routine. The game runs it as if called from here, and it comes back here: the registers are kept on the stack
    // under a long saying which call is done, so a save state taken in the middle goes on as it would have.
    constexpr uint32_t kFromTheStep = 0x00CEE8;
    constexpr uint32_t kFirstDone = 0x43460001, kSecondDone = 0x43460002; // ("CF")
    bool GaugeStep(M68K &c) {
        const uint32_t top = c.Read(c.a[7], 2);
        if (top == kFromTheStep) {
            if (g_gaugeSteps < 7) {
                g_gaugeSteps++;
            }
            if (!On() || g_gaugeSteps < 7) {
                return false;
            }
            g_gaugeSteps = 0;
            if (Ram32(kPlayer1 + kPartner) != kPlayer1 + kPartners ||
                Ram32(kPlayer1 + kPlayerSize + kPartner) != kPlayer1 + kPlayerSize + kPartners) {
                return false;
            }
            for (int n = 14; n >= 0; n--) { // (movem.l d0-d7/a0-a6,-(a7))
                c.Push32(n < 8 ? c.d[n] : c.a[n - 8]);
            }
            c.Push32(kFirstDone);
            c.Push32(0x01CEAA);
            c.a[6] = kPlayer1;
            c.d[0] = 1;
            Spend(c, 160);
            c.pc = 0x01CD1E;
            return true;
        }
        if (top == kFirstDone) {
            c.Write(c.a[7], 2, kSecondDone);
            c.Push32(0x01CEAA);
            c.a[6] = kPlayer1 + kPlayerSize;
            c.d[0] = 1;
            Spend(c, 40 + 132); // (and the registers' taking back after it: a hook that lets the block run takes no time)
            c.pc = 0x01CD1E;
            return true;
        }
        if (top == kSecondDone) {
            c.a[7] += 4;
            for (int n = 0; n < 15; n++) { // (movem.l (a7)+,d0-d7/a0-a6)
                const uint32_t value = c.Pop32();
                (n < 8 ? c.d[n] : c.a[n - 8]) = value;
            }
        }
        return false;
    }

    // 00D4AC: a record's controls at the start of its reading (the record in A6). While a round is fought (kFight's word
    // below 0x0A) the board clears the character's off point and goes past its reading, its moves and the rest of its
    // controls (to 00D5A4), unless it is in a Duo Team Attack. In Cross Fever on to its reading (00D4E6) as for the
    // others, as the Dreamcast does.
    bool PartnerControls(M68K &c) {
        if (!On() || !OffPoint(c.a[6] & 0xFFFFFF) || Ram16(kFight) >= 0x0A) {
            return false;
        }
        Spend(c, 74); // (the tests' time, as the board's code takes it)
        c.pc = 0x00D4E6;
        return true;
    }

    // 018DB6: a record's moves looked for (by its character, 018DD6), unless nobody's are (kNoMoves), or it is off point
    // outside its side's Duo Team Attack, or its side's moves are held (kMovesHeld). In Cross Fever the character off
    // point is passed by only when its side's moves are held.
    bool PartnerMoves(M68K &c) {
        const uint32_t record = c.a[6] & 0xFFFFFF;
        if (!On() || (Ram8(kNoMoves) & 0x14) || !OffPoint(record)) {
            return false;
        }
        const int side = SideOf(record);
        c.d[0] = (c.d[0] & 0xFFFFFF00) | static_cast<uint32_t>(side); // (move.b 0x2(a6),d0)
        c.flagZ = !((Ram8(kMovesHeld) >> side) & 1);
        Spend(c, 74);
        c.pc = c.flagZ ? 0x018DD6 : 0x018DEE;
        return true;
    }

    // 01A196: a character's taunt, when its side's Start (the record's +26E, bit 0 player 1's side, bit 1 player 2's,
    // its last step's in +26F) is pressed now. In Cross Fever while it is held: the panel's Start is the person's on point
    // (the routing's), as the Dreamcast's is that person's controller's.
    bool Taunt(M68K &c) {
        if (!On()) {
            return false;
        }
        const uint32_t record = c.a[6] & 0xFFFFFF;
        const int side = SideOf(record);
        if (side < 0) {
            return false;
        }
        const uint8_t start = Ram8(record + 0x26E);
        c.d[0] = (c.d[0] & 0xFFFFFF00) | start;
        c.d[1] = static_cast<uint32_t>(side);
        c.flagZ = !((start >> side) & 1);
        Spend(c, 50);
        c.pc = c.flagZ ? 0x01A21C : 0x01A1B0;
        return true;
    }
} // namespace

void CrossFeverState(Archive &ar) {
    // Version 11 existed both with this preview's clock and without it in upstream MVC.
    if (ar.version != 0 && (ar.version < 11 || (ar.version == 11 && !ar.NextMark(0x43465653)))) {
        g_gaugeSteps = 0;
        return;
    }
    ar.Mark(0x43465653); // CFVS
    ar.Pod(g_gaugeSteps);
}

void RegisterCrossFever() {
    AddHook(HookAt(0x000FAC), "CrossFeverGaugeBoot", GaugeBoot);
    AddHook(HookAt(0x01368E), "CrossFeverPanel", ReadPanel);
    AddHook(HookAt(0x015F68), "CrossFeverDuoTime", DuoTime);
    AddHook(HookAt(0x01CEAA), "CrossFeverGauge", GaugeStep);
    AddHook(HookAt(0x00D4AC), "CrossFeverPartnerControls", PartnerControls);
    AddHook(HookAt(0x018DB6), "CrossFeverPartnerMoves", PartnerMoves);
    AddHook(HookAt(0x01A196), "CrossFeverTaunt", Taunt);
    TheGame().features.push_back({kFeature, modes::kModes, "Cross Fever",
                                  "With Tag team on: each character is moved by its own person all the time, also beside the "
                                  "other in a Duo Team Attack, and the attack lasts twice as long, as in the Dreamcast's Cross "
                                  "Fever mode. Both teams' hyper combo gauges fill slowly by themselves. The person on point "
                                  "tags out (HP+HK); the partner can't call a tag.",
                                  kFeatureMatch});
}

} // namespace cps2recomp
