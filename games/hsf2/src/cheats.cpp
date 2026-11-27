// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Hyper Street Fighter II: cheats (optional features: the launcher's Features page and the in-game menu switch
// them). Made-up play needs them to go up the ladder to its end ('make explore', game.cfg's explore features).
#include "battle.h"
#include "session.h"
#include "hsf2.h"

#include <algorithm>

namespace cps2recomp {

void TagKnockDown(uint32_t player); // livetag.cpp

namespace {
    using namespace hsf2;

    // Which player's record an address is (0, 1), or -1
    int PlayerOf(uint32_t record) {
        record &= 0xFFFFFF;
        return record == Player(0) ? 0 : record == Player(1) ? 1 : -1;
    }

    // `damage` (D6, D4 or D0, its low word) is about to come off the health of the player whose record is `victim`
    // (anything else's is left alone). The training session is told of it. Health below nothing after it is a
    // knockout, so one-hit knockouts take off more than a full bar; with no knockouts (a training session, the modes
    // that want it) a hit takes no more than the health there is.
    bool DamageTo(uint32_t &damage, uint32_t victim) {
        const int player = PlayerOf(victim);
        if (player < 0) {
            return false;
        }
        if (damage & 0xFFFF) {
            session::Hit(player, damage & 0xFFFF, 0, false);
        }
        if (FeatureOn(player == 0 ? "no_damage" : "no_damage_2")) {
            SetLow16(damage, 0);
        } else if (player == 1 && FeatureOn("one_hit")) {
            SetLow16(damage, kHealthFull + 1);
        }
        const int16_t health = static_cast<int16_t>(Ram16(Player(player) + kHealth));
        if ((session::NoKnockOut() || TeamHoldsKnockOut(player)) && static_cast<int16_t>(damage & 0xFFFF) > health) {
            SetLow16(damage, static_cast<uint32_t>(health > 0 ? health : 0));
        }
        // (the team battle's tag style: a bar emptied with a partner left is a knock-down, not a knock-out)
        if (TeamHoldsKnockOut(player) && static_cast<int16_t>(damage & 0xFFFF) >= health && !Ram8(Player(player) + kGuarded)) {
            TagKnockDown(Player(player));
        }
        return false;
    }
    // The random part of a hit's damage (0C2FD4 on) and of a throw's (004C3A on): D0 a byte of a row of 32 (A1 the table, D2 the
    // row's start plus the game's random pick) has been added to the damage (D6, a throw's D4), and the sum made 1 where it
    // came to nothing or less (0C2FEE, 004C82). With fixed damage the row's average is added in its place, where the sum is
    // next used (0C2FF0, 004C84: both ways meet there; the random number still drawn, so the game goes on as it would): a
    // combo always does the same damage. (Hooked at the instructions after the sum, which begin blocks of the game's own
    // translated code already, so that with the feature off the game runs exactly as before.)
    struct Sum {
        bool madeOne = false; // (the sum came to nothing or less: what it was without the byte)
        int16_t without = 0;
    } g_sum[2]; // (a hit's, a throw's: only from one instruction to the next)
    bool MadeOne(M68K &c, int which, uint32_t damage) {
        g_sum[which] = {true, static_cast<int16_t>(static_cast<int16_t>(damage) - static_cast<int16_t>(c.d[0]))};
        return false;
    }
    bool Summed(M68K &c, int which, uint32_t &damage) {
        const Sum sum = g_sum[which];
        g_sum[which] = {};
        if (!FeatureOn("fixed_damage")) {
            return false;
        }
        const uint32_t row = (c.a[1] & 0xFFFFFF) + ((c.d[2] & 0xFFFF) & ~0x1Fu);
        int total = 0;
        for (uint32_t n = 0; n < 32; n++) {
            total += static_cast<int8_t>(*Program(row + n)); // (the data view, with none of the board's waits)
        }
        const int average = total >= 0 ? (total + 16) / 32 : -((-total + 16) / 32);
        const int without = sum.madeOne ? sum.without : static_cast<int16_t>(damage) - static_cast<int16_t>(c.d[0]);
        SetLow16(damage, static_cast<uint32_t>(std::max(without + average, 1)));
        return false;
    }
} // namespace

void RegisterCheats() {
    // A hit's (D6 off A2's health), a thrown thing's, and a throw's: D4 off A3's at once (two ways), or put by at +78
    // (004AFE, which sees to the knockout itself) to be taken at the throw's end (004D3C, 069278, 0693F8); and the holds'
    // and the throws that go on (004E18, 004F2E: hsf2.h)
    AddHook(kHitDamage, "HitDamage", [](M68K &c) { return DamageTo(c.d[6], c.a[2]); });
    AddHook(kThrownDamage, "ThrownDamage", [](M68K &c) { return DamageTo(c.d[6], c.a[2]); });
    for (const uint32_t at : kThrowDamage) {
        AddHook(at, "ThrowDamage", [](M68K &c) { return DamageTo(c.d[4], c.a[3]); });
    }
    // 00DA76: subq.b #1 off the frames of the round's second (at nothing a second goes). With the cheat the frame is
    // given back first, so the clock stands still.
    AddHook(kClockTick, "ClockTick", [](M68K &) {
        if (FeatureOn("infinite_time") || session::NoClock()) {
            SetRam8(kClockFrames, static_cast<uint8_t>(Ram8(kClockFrames) + 1));
        }
        return false;
    });
    AddHook(HookAt(kHitMadeOne), "HitMadeOne", [](M68K &c) { return MadeOne(c, 0, c.d[6]); });
    AddHook(HookAt(kHitSummed), "HitSummed", [](M68K &c) { return Summed(c, 0, c.d[6]); });
    AddHook(HookAt(kThrowMadeOne), "ThrowMadeOne", [](M68K &c) { return MadeOne(c, 1, c.d[4]); });
    AddHook(HookAt(kThrowSummed), "ThrowSummed", [](M68K &c) { return Summed(c, 1, c.d[4]); });
    Game &game = TheGame();
    game.features.push_back({"no_damage", "Cheats", "No damage", "Player 1's health does not go down."});
    game.features.push_back({"no_damage_2", "Cheats", "No damage, player 2", "Player 2's health does not go down (the opponent's, in a one-player game)."});
    game.features.push_back({"one_hit", "Cheats", "One-hit knockouts", "Player 2 (the opponent, in a one-player game) goes down with the first hit."});
    game.features.push_back({"infinite_time", "Cheats", "Infinite time", "The round's clock stands still."});
    game.features.push_back({"fixed_damage", "Training mode", "Fixed damage",
                             "No random part in a hit's damage (the average in its place), so a combo always does the same."});
    TrainingAids({"fixed_damage"});
}

} // namespace cps2recomp
