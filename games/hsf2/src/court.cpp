// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Hyper Street Fighter II: the court (the toolkit's ball game, fight/court.h; docs/COURT.md), as Super Turbo's
// (games/ssf2t/src/court.cpp, whose code this is with this game's places and its attacks' wider entries): where the game keeps what the ball needs (the fighters' boxes and their
// attacks, how strong each is), and what is done to a fighter (frozen by a hit on the ball, made to reel or knocked
// down by the ball, its health kept full). The game's own code is used as it is, hooked: the players' push (0C3A22)
// is the court's while it runs (each kept on its half), the throw tests (0046E0, the computer's 06C82A) say no, and
// what the ball does to a fighter is made where the game looks for a hit at the end of the fighter's step (069384).
#include "alternative.h"
#include "court.h"
#include "hsf2.h"
#include "state.h"

#include <cstdio>
#include <cstdlib>
#include <vector>

namespace cps2recomp {

namespace {
    using namespace hsf2;

    // In an attack's entry (twenty bytes: the box's four words, then what the attack does, as Super Turbo's sixteen are
    // after their four bytes; the hit at 0C25FC on reads them): +08 bit 7 a special move's, +09 its number (the same
    // while the same attack is out: the victim keeps it at +4A so that one attack hits once, 0C25FC), +0C the button's
    // strength in its low two bits (0 light, 1 medium, 2 hard: kept at the victim's +4C, 0C2666)
    constexpr uint32_t kAttackKind = 0x08, kAttackNumber = 0x09, kAttackStrength = 0x0C;
    // In a player's record, what a hit leaves for the fighter's step to find (069384, as Super Turbo's 07AC78: the health below what it was
    // before, or +49 set for a guarded hit, then 07ACFC on): the reaction +4B (0C a hit on its feet, 12 knocked down, 06
    // guarded; copied to +55, which the reeling goes by: 078F4E), the strength +4C (its reel's length), the way pushed
    // +54 (1 to the right: 0BE7A0), and what more the hit says (+19E a special's, +2A1 and +2A3 a dizzy's, +71 and +19C,
    // all left 0)
    constexpr uint32_t kReactionNext = 0x4B, kStrengthNext = 0x4C, kPushedRight = 0x54;
    constexpr uint32_t kHitMore[] = {0x71, 0x19C, 0x19E, 0x2A1, 0x2A3};
    constexpr uint8_t kReactionHit = 0x0C, kReactionKnockDown = 0x12, kReactionGuarded = 0x06;
    // The reaction being played (+55, from +4B), and the stun built up (+5E: the ball and the fighters' hits make
    // nobody dizzy, kept at 0)
    constexpr uint32_t kReaction = 0x55, kStun = 0x5E;
    constexpr uint8_t kStruckStop = 8;
    constexpr uint32_t kHalfWidth = 0x1C2;
    // A super combo: the gauge (0x30 full) spent in one go
    constexpr uint8_t kGaugeSpent = 0x18;
    // A byte: not 0 once a round's first hit has landed (0693AC): kept so, or the game hands out its FIRST ATTACK bonus
    // when the ball knocks someone down
    constexpr uint32_t kFirstAttackMade = 0xFF8C1A;
    // The game's hit sounds (queued at 006ECC as a hit lands, with the training's dummy hit by each button): a light,
    // medium and hard hit, and specials and supers as hard ones
    constexpr uint16_t kHitSounds[5] = {0x004C, 0x004D, 0x004B, 0x004B, 0x004B};

    // (for finding the facts: HSF2_COURT_LOG=1 writes each attack's entry as it comes out)
    void Log(Machine &machine, uint32_t record, uint32_t entry) {
        static const bool on = getenv("HSF2_COURT_LOG") != nullptr;
        static uint32_t last[16] = {};
        const int slot = record >= kThrown ? 2 + static_cast<int>((record - kThrown) / kThrownSize) : static_cast<int>((record - kPlayer1) / kPlayerSize);
        if (!on || slot < 0 || slot >= 16 || last[slot] == entry) {
            return;
        }
        last[slot] = entry;
        fprintf(stderr, "court: record %06X state %02X entry %06X:", record, Ram8(record + kState), entry);
        for (uint32_t i = 4; i < kAttackBoxSize; i++) {
            fprintf(stderr, " %02X", machine.Read8(entry + i));
        }
        fprintf(stderr, "\n");
    }

    struct Spent {
        uint8_t gauge[2] = {0, 0};
        bool spending[2] = {false, false};
    } g_spent;
    void WatchGauges() {
        for (int side = 0; side < 2; side++) {
            const uint32_t player = Player(side);
            const uint8_t gauge = Ram8(player + kSuperGauge);
            if (gauge + kGaugeSpent <= g_spent.gauge[side]) {
                g_spent.spending[side] = true;
            } else if (CanAct(player)) {
                g_spent.spending[side] = false;
            }
            g_spent.gauge[side] = gauge;
        }
    }

    bool IsTable(uint32_t address) {
        return address >= 0x1000 && address < 0x400000;
    }

    // The box of a kind named in a record's frame (0: none), as the hit tests find it (0BF534, 0BF558-0BF588): its number
    // from the frame, its table from the record's box tables
    uint32_t BoxOf(Machine &machine, uint32_t record, uint32_t number, uint32_t table, uint32_t size) {
        const uint32_t frame = Ram32(record + kFrame), tables = Ram32(record + kBoxTables);
        if (!IsTable(frame) || !IsTable(tables)) {
            return 0;
        }
        const uint8_t which = machine.Read8(frame + number);
        if (!which) {
            return 0;
        }
        return tables + static_cast<uint32_t>(static_cast<int16_t>(machine.Read16(tables + table))) + which * size;
    }

    // A box placed in the stage as the hit tests place it (words: its middle from where the record stands, across and
    // up, half its width and height: training.cpp's AddBox)
    bool Place(Machine &machine, uint32_t record, uint32_t box, bool, court::Shape &shape) {
        const int across = static_cast<int16_t>(machine.Read16(box));
        shape.halfWidth = static_cast<int16_t>(machine.Read16(box + 4));
        shape.halfHeight = static_cast<int16_t>(machine.Read16(box + 6));
        shape.x = static_cast<int16_t>(Ram16(record + kX)) + (Ram8(record + kFacesRight) ? -across : across);
        shape.y = static_cast<int16_t>(Ram16(record + kY)) + static_cast<int16_t>(machine.Read16(box + 2));
        return shape.halfWidth > 0 && shape.halfHeight > 0;
    }

    void AddAttack(Machine &machine, uint32_t record, int side, uint32_t box, bool thrown, std::vector<court::Shape> &out) {
        Log(machine, record, box);
        court::Shape shape;
        if (!Place(machine, record, box, true, shape)) {
            return;
        }
        shape.kind = court::Shape::kAttack;
        shape.side = side;
        shape.thrown = thrown;
        shape.id = (record & 0xFFFF) << 8 | machine.Read8(box + kAttackNumber);
        const uint8_t strength = machine.Read8(box + kAttackStrength) & 3;
        const bool special = thrown || (machine.Read8(box + kAttackKind) & 0x80);
        shape.power = g_spent.spending[side] ? court::kSuper
                    : special               ? court::kSpecial
                    : strength >= 2         ? court::kHard
                    : strength == 1         ? court::kMedium
                                            : court::kLight;
        out.push_back(shape);
    }

    void Shapes(Machine &machine, std::vector<court::Shape> &out) {
        WatchGauges();
        constexpr struct {
            uint32_t number, table;
        } kHurt[] = {{kFrameHead, kTableHead}, {kFrameBody, kTableBody}, {kFrameLegs, kTableLegs}};
        for (int side = 0; side < 2; side++) {
            const uint32_t player = Player(side);
            if (Ram8(player + kFighting) != 2) {
                continue;
            }
            if (const uint32_t box = BoxOf(machine, player, kFrameAttack, kTableAttack, kAttackBoxSize)) {
                AddAttack(machine, player, side, box, false, out);
            }
            for (const auto &hurt : kHurt) {
                court::Shape shape;
                const uint32_t box = BoxOf(machine, player, hurt.number, hurt.table, kBoxSize);
                if (box && Place(machine, player, box, false, shape)) {
                    shape.kind = court::Shape::kBody;
                    shape.side = side;
                    out.push_back(shape);
                }
            }
        }
        // What they threw (the pool from FF97A2, records of a player's layout; the thrower's record at +26)
        for (int n = 0; n < kThrownRecords; n++) {
            const uint32_t thrown = kThrown + static_cast<uint32_t>(n) * kThrownSize;
            if (Ram8(thrown) != 1) {
                continue;
            }
            const uint32_t owner = 0xFF0000 | Ram16(thrown + kThrownOwner);
            const int side = owner == Player(0) ? 0 : owner == Player(1) ? 1 : -1;
            if (side < 0) {
                continue;
            }
            if (const uint32_t box = BoxOf(machine, thrown, kFrameAttack, kTableAttack, kAttackBoxSize)) {
                AddAttack(machine, thrown, side, box, true, out);
            }
        }
    }

    // 0C3A22: the two players pushed apart by their push boxes. On the court each is kept on
    // its half instead (they never meet)
    bool PushApart(M68K &c) {
        if (!court::On()) {
            return false;
        }
        for (int side = 0; side < 2; side++) {
            const uint32_t player = Player(side);
            const int x = static_cast<int16_t>(Ram16(player + kX));
            const int kept = court::KeepToHalf(side, x, static_cast<int16_t>(Ram16(player + kHalfWidth)));
            if (kept != x) {
                SetRam32(player + kX, static_cast<uint32_t>(kept) << 16);
            }
        }
        Return(c);
        return true;
    }

    // 0046E0: the throw test (where its ways in meet), and 06C82A, the computer's own. On the court each goes to its
    // "no" (00486C, 06C91E: D0 0), as nobody is thrown there
    bool NoThrow(M68K &c, uint32_t no) {
        if (!court::On()) {
            return false;
        }
        c.pc = no;
        return true;
    }

    // 069384: at the end of a fighter's step (a person's and the computer's), where the game looks for a
    // hit that has landed on it. What the ball has done to the fighter (court.h, ReactionWanted) is put there as a hit
    // leaves it, and the game's own reeling (a reel on its feet, a knock-down, a guard) runs from here on, pushed away
    // from the ball.
    bool BallReaction(M68K &c) {
        const uint32_t player = c.a[6] & 0xFFFFFF;
        const int side = player == Player(0) ? 0 : player == Player(1) ? 1 : -1;
        if (side < 0 || !court::On() || Ram8(player + kFighting) != 2) {
            return false;
        }
        int away = 0;
        const court::Reaction reaction = court::ReactionWanted(side, away);
        if (reaction == court::kNoReaction) {
            return false;
        }
        static constexpr uint8_t kStrengths[] = {0, 0, 0, 1, 2, 2};
        for (const uint32_t at : kHitMore) {
            SetRam8(player + at, 0);
        }
        SetRam8(player + kStrengthNext, kStrengths[reaction]);
        SetRam8(player + kHitStop, reaction == court::kKnockDown ? 0 : kStruckStop);
        if (away) {
            SetRam8(player + kPushedRight, away > 0 ? 1 : 0);
        }
        if (reaction == court::kGuard) {
            SetRam8(player + kReactionNext, kReactionGuarded);
            SetRam8(player + kGuarded, 1);
        } else {
            SetRam8(player + kReactionNext, reaction == court::kKnockDown ? kReactionKnockDown : kReactionHit);
            // (the game's sign of a hit: the health below what it was before the hit; the step makes them alike)
            SetRam16(player + kHealthBefore, static_cast<uint16_t>(Ram16(player + kHealth) + 1));
        }
        return false;
    }
} // namespace

void RegisterCourt() {
    court::Description d;
    d.shapes = Shapes;
    d.fighterX = [](int side) { return static_cast<int>(static_cast<int16_t>(Ram16(Player(side) + kX))); };
    d.fighterY = [](int side) { return static_cast<int>(static_cast<int16_t>(Ram16(Player(side) + kY))); };
    d.place = [](int side, int x) { SetRam32(Player(side) + kX, static_cast<uint32_t>(x) << 16); };
    d.halfWidth = [](int side) { return static_cast<int>(static_cast<int16_t>(Ram16(Player(side) + kHalfWidth))); };
    d.canAct = [](int side) { return CanAct(Player(side)); };
    d.guarding = [](int side) {
        // (holding back, away from the way it faces: the stick's right bit facing left, its left bit facing right)
        const uint32_t player = Player(side);
        const uint16_t stick = Ram16(kControls[side]);
        const bool back = Ram8(player + kFacesRight) ? (stick & 2) != 0 : (stick & 1) != 0;
        const uint8_t state = Ram8(player + kState);
        return back && (CanAct(player) || state == kGuard || state == kJump ||
                        (state == kReeling && Ram8(player + kReaction) == kReactionGuarded));
    };
    d.stunned = [](int side) {
        const uint32_t player = Player(side);
        const uint8_t state = Ram8(player + kState);
        return state == kDown || (state == kReeling && Ram8(player + kReaction) != kReactionGuarded && Ram8(player + kReaction) != 0x08);
    };
    d.floor = kGround;
    d.pictureFloor = 239;
    d.pictureTop = 40;
    d.panel = ThePanel; // (the bars, names and clock not shown: court.h)
    d.wideStage = WideStage;
    d.freeze = [](int side, int steps) { SetRam8(Player(side) + kHitStop, static_cast<uint8_t>(steps)); };
    d.playSound = modes::PlaySound;
    for (int n = 0; n < 5; n++) {
        d.hitSounds[n] = kHitSounds[n];
    }
    d.everyFrame = [] {
        for (int side = 0; side < 2; side++) {
            // (health kept full; a hit the fighter's step has not found yet, the health below what it was before, kept
            // so by one, for the step to find)
            const uint32_t player = Player(side);
            const bool pending = Ram16(player + kHealth) != Ram16(player + kHealthBefore);
            SetRam16(player + kHealth, pending ? kHealthFull - 1 : kHealthFull);
            SetRam16(player + kHealthBefore, kHealthFull);
            SetRam16(player + kStun, 0);
        }
        SetRam8(kFirstAttackMade, 1);
    };
    d.state = [](Archive &ar) { ar.Pod(g_spent); }; // (the gauges' last values: in a save state, for rollback)
    court::Register(d);
    AddHook(HookAt(0x0C3A22), "CourtPushApart", PushApart);
    AddHook(HookAt(0x0046E0), "CourtNoThrow", [](M68K &c) { return NoThrow(c, 0x00486C); });
    AddHook(HookAt(0x06C82A), "CourtNoComputerThrow", [](M68K &c) { return NoThrow(c, 0x06C91E); });
    AddHook(HookAt(0x069384), "CourtBallReaction", BallReaction);
}

} // namespace cps2recomp
