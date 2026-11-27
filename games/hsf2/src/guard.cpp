// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Hyper Street Fighter II: what the training dummy needs to know of guarding, read the way the game itself reads it
// (nothing here writes the game's memory). Super Turbo's (games/ssf2t/src/guard.cpp) with this game's places, and its
// two ways of deciding, by the attacker's version.
//
// Whether something is near enough to guard is the game's own guard test (battle.cpp's AttackComing, as the computer's
// coach reads it). Whether it is guarded standing or crouching is the game's guard decision as a hit lands (0C3044 on,
// hsf2.h): it takes the victim's guarding pose (its frame's +11: 1
// standing, 2 crouching), the hurt box the attack reached (head, body or legs) and the attack's own bytes. Here it is
// asked of the attack coming (the attacker's frame now, or the first of its move to have an attack box) for both poses
// and all three boxes, before it lands.
#include "hsf2.h"

namespace cps2recomp {

bool AttackComingAt(int side); // battle.cpp

namespace {
    using namespace hsf2;

    constexpr uint32_t kFrameBytes = 0x18, kFrameFollows = 0x02, kFrameJump = 0x18; // (model.cpp)
    constexpr uint16_t kForGood = 0x0FFF;
    // (read as the bus reads, with none of the board's waits: this is the port's, not the game's)
    uint32_t Long(M68K &c, uint32_t at) {
        return (static_cast<uint32_t>(c.bus.Read16(at & 0xFFFFFE)) << 16) | c.bus.Read16((at + 2) & 0xFFFFFE);
    }
    // (a table read through the program counter: the deciphered program)
    int16_t Opcode16(uint32_t at) {
        const uint8_t *bytes = ProgramOpcodes(at);
        return static_cast<int16_t>((bytes[0] << 8) | bytes[1]);
    }
    constexpr uint32_t kLastThrown = 0x1D4; // word: the record of the thing the player last threw (battle.cpp)

    // The attack box of the move a record (a player, a thing thrown) is in: its frame's, or the first frame on that
    // has one (following the animation's jumps; at most a second of frames on)
    uint32_t AttackAhead(M68K &c, uint32_t record) {
        const uint32_t tables = Ram32(record + kBoxTables);
        uint32_t frame = Ram32(record + kFrame);
        if (tables >= 0x400000) {
            return 0;
        }
        for (int n = 0; n < 60 && frame < 0x400000; n++) {
            if (const uint8_t number = static_cast<uint8_t>(c.bus.Read8((frame + kFrameAttack) & 0xFFFFFF))) {
                return tables + static_cast<uint32_t>(static_cast<int16_t>(c.bus.Read16((tables + kTableAttack) & 0xFFFFFE))) + number * kAttackBoxSize;
            }
            if (c.bus.Read16((frame) & 0xFFFFFE) == kForGood) {
                break;
            }
            const uint32_t next = c.bus.Read16((frame + kFrameFollows) & 0xFFFFFE) & 0x8000 ? Long(c, frame + kFrameJump) : frame + kFrameBytes;
            if (next == frame) {
                break;
            }
            frame = next;
        }
        return 0;
    }

    // The game's decision for an attack box reaching a player in a guarding pose (1 standing, 2 crouching) on a hurt
    // box (0 head, 2 body, 4 legs): guarded or not
    bool Guards(M68K &c, uint32_t attack, int pose, int box, bool classic) {
        const uint8_t type = c.bus.Read8((attack + kAttackType) & 0xFFFFFF);
        if (!classic) {
            // (0C306E: the routine by the attack's kind; one the table does not have is taken as guarded either way)
            if (type & 1 || type >= kGuardTypeBytes) {
                return true;
            }
            const uint32_t routine = kGuardTypes + static_cast<uint32_t>(Opcode16(kGuardTypes + type));
            if (routine == kGuardMost) {
                return pose == 2 || ((0x0B >> (box >> 1)) & 1); // (0C310C: standing, the head and the body only)
            }
            if (routine == kGuardStanding) {
                return pose == 1;
            }
            if (routine == kGuardCrouching || routine == kGuardCrouching2) {
                return pose == 2;
            }
            return true;
        }
        // (the classic way, 0C3138: by the attack's +0B, a row of four bytes for the pose, one a hurt box; but an attack of
        // +0B 3 and kind 26 guarded standing only, and one of +0B 4 and kind 24 taken as 2; the rows' places and the
        // rows read through A1, the data view)
        uint8_t level = c.bus.Read8((attack + kAttackLevel) & 0xFFFFFF);
        if (level == 3 && type == 0x26) {
            return pose == 1;
        }
        if (level == 4 && type == 0x24) {
            level = 2;
        }
        const auto word = [](uint32_t at) { return static_cast<int16_t>((Program(at)[0] << 8) | Program(at)[1]); };
        const uint32_t row = kGuardRows + static_cast<uint32_t>(word(kGuardRows + static_cast<uint32_t>(pose) * 2));
        return *Program(row + (level & 3) * 4u + static_cast<uint32_t>(box >> 1)) != 0;
    }
} // namespace

// Something of the other player's is near enough for the player to guard it (the game's own test)
bool AttackInRange(M68K &, uint32_t player) {
    return AttackComingAt(player == Player(0) ? 0 : 1);
}

// The attack coming at the player is to be guarded standing: standing guards it wherever it lands and crouching does
// not (an overhead, a jump-in); crouching when it is the other way about (a low) or guards more of it (most attacks of
// Super Turbo's kind, which a standing guard lets through on the legs); standing when both guard it alike
bool GuardStanding(M68K &c, uint32_t player) {
    const uint32_t other = Player(player == Player(0) ? 1 : 0);
    // (the other's own attack while it attacks, else the thing it last threw while that flies, else its move's)
    uint32_t attack = Ram8(other + kAttacking) ? AttackAhead(c, other) : 0;
    if (const uint16_t thrown = Ram16(other + kLastThrown); !attack && thrown && Ram8(0xFF0000u | thrown) == 1) {
        attack = AttackAhead(c, 0xFF0000u | thrown);
    }
    if (!attack) {
        attack = AttackAhead(c, other);
    }
    if (!attack) {
        return true;
    }
    const bool classic = Ram8(other + kVersion) >= kWorldWarrior; // (0C3048: the attacker's version)
    int standing = 0, crouching = 0;
    for (const int box : {0, 2, 4}) {
        standing += Guards(c, attack, 1, box, classic);
        crouching += Guards(c, attack, 2, box, classic);
    }
    return standing >= crouching;
}

} // namespace cps2recomp
