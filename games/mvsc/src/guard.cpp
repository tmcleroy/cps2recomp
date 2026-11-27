// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Marvel vs. Capcom: the game's own tests of guarding, read for the training dummy's guard (training.cpp) and the ring
// out's coach (battle.cpp). Nothing here changes the game; it reads what the game itself goes by.
//
//   - Whether something is coming to guard: 0173E6, which a character holding back asks before it guards (017294):
//     the other side's list of what can hit (a long at FFDDBA, its count the word at FFDDB6, for player 2's side;
//     FFDDBE and FFDDB8 for player 1's), any record of it whose attack's phase (+64, its top three bits) is 80 or on.
//   - How a hit must be guarded: 01BAB8, when a hit lands on a character guarding (+B0 24 on the floor, its posture +B4
//     0 standing, 2 crouching; 26 in the air, which guards everything). The hit's entry is the attacker's table of
//     them (a long by its +52 at 0E68AE) and its +80 (0x20 bytes an entry, as 01A934 takes it for the one hit); the low
//     nybble of the entry's byte +4 says: 0 either way; 2 not crouching (an overhead: crouching, with the stick down,
//     it hits); 4 not standing (a low: standing, without the stick down, it hits); bit 3 of it no guard at all. Checked: a standing guard let Ryu's crouching short and sweep through, a crouching one
//     guarded them and his standing punches.
#include "game.h"
#include "lift.h"
#include "mvsc.h"

namespace cps2recomp {

namespace {
    using namespace mvsc;

    constexpr uint32_t kHitList[2] = {0xFFDDBE, 0xFFDDBA}; // (by the guarding side: the other side's list)
    constexpr uint32_t kHitCount[2] = {0xFFDDB8, 0xFFDDB6};
    constexpr uint32_t kAttackPhase = 0x64, kSide = 0x02, kAttack = 0x80;
    constexpr uint32_t kHitTables = 0x0E68AE, kHitSize = 0x20, kHitGuard = 0x04;
    enum : uint8_t { kGuardEither = 0, kGuardHigh = 2, kGuardLow = 4 };

    // What of the other side's is coming at `player` (0173E6), or 0
    uint32_t Coming(uint32_t player) {
        const int side = Ram8(player + kSide) ? 1 : 0;
        const uint32_t list = Ram32(kHitList[side]) & 0xFFFFFF;
        const int count = Ram16(kHitCount[side]);
        if (list < 0xFF0000 || count <= 0 || count > 64) {
            return 0;
        }
        for (int n = 0; n < count; n++) {
            const uint32_t record = Ram32(list + 4 * static_cast<uint32_t>(n)) & 0xFFFFFF;
            if (record >= 0xFF0000 && (Ram8(record + kAttackPhase) & 0xE0) >= 0x80) {
                return record;
            }
        }
        return 0;
    }
} // namespace

// Something of the other side's is near enough for the character on point of `player`'s record to guard
bool AttackInRange(M68K &, uint32_t player) {
    return Coming(player) != 0;
}

// ... and it must be guarded standing (an overhead: its entry's guard is 2)
bool GuardStanding(M68K &c, uint32_t player) {
    const uint32_t attacker = Coming(player);
    if (!attacker) {
        return false;
    }
    const uint32_t who = Ram16(attacker + kCharacter);
    if (who >= 0x80) {
        return false;
    }
    const uint32_t table = c.Read(kHitTables + who * 2, 2) & 0xFFFFFF;
    const uint32_t entry = table + (Ram8(attacker + kAttack) & 0x7Fu) * kHitSize;
    return (static_cast<uint8_t>(c.Read(entry + kHitGuard, 0)) & 0x0F) == kGuardHigh;
}

} // namespace cps2recomp
