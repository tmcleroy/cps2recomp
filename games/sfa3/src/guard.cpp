// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Street Fighter Alpha 3: what the training dummy and the computer's coach need to know of guarding, read the way the
// game itself reads it (nothing here writes the game's memory).
//
// The guard-range test (02AB24) says whether something is near enough for the player to guard: the game asks it of a
// player holding away from the other (or with the board's auto guard), and puts the player in its guarding pose when
// it says yes. Here it is asked without the holding away, for a dummy that is to guard as a person holding away would.
// When a hit lands, the guard decision (023E26) takes the victim's pose (its animation frame's +0D) and the attack's
// bits of the poses that cannot guard it.
#include "sfa3.h"

namespace cps2recomp {

namespace {
    using namespace sfa3;

    constexpr uint32_t kThrownHarmless = 0x68;   // byte of a thrown thing: bit 7, it need not be guarded
    constexpr uint32_t kNoGuardMove = 0x216;     // byte of the other player: set, anything of it is near enough
    constexpr uint32_t kAttacking = 0xA9;        // byte: set while the player's attack is out (FBNeo's training script)
    constexpr uint32_t kAttackHidden = 0xC7;     // byte: bit 7, the attack is not to be guarded yet
    constexpr uint32_t kGuardFlags = 0x132;      // byte: bit 7, the player cannot guard in the air
    constexpr uint32_t kSideOf = kSide;

    // A thing thrown by the other side within 0x50 across (02AB68 on: the two pools of twelve)
    bool ThrownNear(uint32_t player) {
        for (const uint32_t pool : {kThrown, kThrownOther}) {
            for (int n = 0; n < kThrownRecords; n++) {
                const uint32_t thrown = pool + static_cast<uint32_t>(n) * kThrownSize;
                if (Ram8(thrown) != 1 || (Ram8(thrown + kThrownHarmless) & 0x80) || Ram8(thrown + kSideOf) == Ram8(player + kSideOf)) {
                    continue;
                }
                const uint16_t apart = static_cast<uint16_t>(Ram16(thrown + kX) - Ram16(player + kX) + 0x50);
                if (apart < 0xA0) {
                    return true;
                }
            }
        }
        return false;
    }
} // namespace

// An attack of the other's is near enough for the player to guard it (02AB24 without its holding away)
bool AttackInRange(M68K &c, uint32_t player) {
    player &= 0xFFFFFF;
    if (Ram8(player + kCustomCombo) || Ram8(player + 0x24E)) {
        return false; // (in a custom combo of its own, or 24E: the game's test says no)
    }
    if ((Ram8(player + kGuardFlags) & 0x80) && Ram8(player + kInAir)) {
        return false;
    }
    if (ThrownNear(player)) {
        return true;
    }
    const uint32_t other = 0xFF0000u | Ram16(player + kOpponent);
    if (Ram8(other + kCustomCombo) || Ram8(other + kNoGuardMove)) {
        return true;
    }
    if (!Ram8(other + kAttacking) || (Ram8(other + kAttackHidden) & 0x80)) {
        return false;
    }
    // (02AC1A: the other's attack in this frame or the next of the move that has one; near enough by its reach and 0x58)
    uint32_t frame = Ram32(other + kFrame);
    for (int n = 0; n < 64 && frame < 0x400000; n++) {
        if (const uint8_t number = Mem8(c, frame + kFrameAttack)) {
            const uint32_t box = Ram32(other + kAttackBoxes) + static_cast<uint32_t>(static_cast<int8_t>(number)) * kAttackBoxSize;
            int across = static_cast<int16_t>(Mem16(c, box + kBoxX));
            if (Ram8(other + kFacesRight)) {
                across = -across;
            }
            int apart = across + static_cast<int16_t>(Ram16(other + kX)) - static_cast<int16_t>(Ram16(player + kX));
            if (apart < 0) {
                apart = -apart;
            }
            return static_cast<uint16_t>(apart) <= static_cast<uint16_t>(Mem16(c, box + kBoxHalfWidth) + 0x58);
        }
        if (Mem8(c, frame + 1) & 0xC0) {
            // (the move's end or a jump to another: anything within 0x100 either way, 02ABF4)
            const uint16_t apart = static_cast<uint16_t>(Ram16(other + kX) - Ram16(player + kX) + 0x100);
            return apart <= 0x200;
        }
        frame += kFrameSize;
    }
    return false;
}

// The other's attack is to be guarded standing, as a player would: anything but a low (its entry says a standing guard
// cannot take it, and a crouching one can); a jab, an overhead, a jump-in, a fireball all standing
bool GuardStanding(M68K &c, uint32_t player) {
    player &= 0xFFFFFF;
    const uint32_t other = 0xFF0000u | Ram16(player + kOpponent);
    uint32_t frame = Ram32(other + kFrame);
    for (int n = 0; n < 64 && frame < 0x400000; n++, frame += kFrameSize) {
        if (const uint8_t number = Mem8(c, frame + kFrameAttack)) {
            const uint8_t bits = Mem8(c, Ram32(other + kAttackBoxes) + number * kAttackBoxSize + kAttackUnguardable);
            return !((bits & 1) && !(bits & 2));
        }
        if (Mem8(c, frame + 1) & 0x80) {
            break;
        }
    }
    return true;
}

} // namespace cps2recomp
