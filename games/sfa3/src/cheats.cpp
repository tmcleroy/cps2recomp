// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Street Fighter Alpha 3: cheats (optional features: the launcher's Features page and the in-game menu switch them).
// Made-up play needs them to go up the ladder to its end ('make explore', game.cfg's explore features).
#include "battle.h"
#include "session.h"
#include "sfa3.h"

namespace cps2recomp {

namespace {
    using namespace sfa3;

    // Which player's record an address is (0, 1), or -1
    int PlayerOf(uint32_t record) {
        record &= 0xFFFFFF;
        return record == Player(0) ? 0 : record == Player(1) ? 1 : -1;
    }

    // 0240D2: `damage` (D4, its low word) is about to come off the health of the player whose record is A1 (anything
    // else's is left alone). Health below nothing after it is a knockout, so one-hit knockouts take off more than a
    // full bar; with no knockouts (a training session, the modes that want it) a hit takes no more than the health
    // there is.
    bool TakeDamage(M68K &c) {
        const int player = PlayerOf(c.a[1]);
        if (player < 0) {
            return false;
        }
        uint32_t &damage = c.d[4];
        if (FeatureOn(player == 0 ? "no_damage" : "no_damage_2")) {
            SetLow16(damage, 0);
        } else if (player == 1 && FeatureOn("one_hit")) {
            SetLow16(damage, kHealthFull + 1);
        }
        const int16_t health = static_cast<int16_t>(Ram16(Player(player) + kHealth));
        if (session::NoKnockOut() && static_cast<int16_t>(damage & 0xFFFF) > health) {
            SetLow16(damage, static_cast<uint32_t>(health > 0 ? health : 0));
        }
        // (the team battle's tag style: a hit that empties the bar of a fighter with a partner to come in leaves it at
        // nothing, not knocked out (below nothing: 023F1C), and, not guarded, knocks it down as a knock-out's would
        // (reaction 12, read as the reeling begins at its next step); the partner comes in while it lies there)
        if (static_cast<int16_t>(damage & 0xFFFF) >= health && TeamHoldsKnockOut(player)) {
            SetLow16(damage, static_cast<uint32_t>(health > 0 ? health : 0));
            if (Ram8(Player(player) + kReaction) != 0xFF) {
                SetRam8(Player(player) + kReaction, 0x12);
            }
        }
        // (for the training session: the combo readout, and the dummy's "after a hit", go by it; guarded, chip damage)
        session::Hit(player, damage & 0xFFFF, 0, Ram8(Player(player) + kGuardedHit) == 0xFF);
        return false;
    }
    // 023C4C: `add.b d0,0x2CC(a1)`, a hit's stun added to what has built up in the player at A1 (stunned when it reaches
    // +2CD). With no stun nothing is added.
    bool StunAdded(M68K &c) {
        const int player = PlayerOf(c.a[1]);
        if (player >= 0 && FeatureOn(player == 0 ? "no_stun" : "no_stun_2")) {
            SetLow8(c.d[0], 0);
        }
        return false;
    }

    // 024146: `add.b d0,d2`, the random part of a hit's damage added: a byte of a row of 32 (A0: the table at 0C4B3A, the
    // row by the attack's +18) picked by the game's random number (024138). With fixed damage the row's average is
    // added in its place (the random number still drawn, so the game goes on as it would): a combo always does the
    // same damage.
    bool RandomPart(M68K &c) {
        if (!FeatureOn("fixed_damage")) {
            return false;
        }
        int sum = 0;
        for (uint32_t n = 0; n < 32; n++) {
            sum += static_cast<int8_t>(Mem8(c, (c.a[0] & 0xFFFFFF) + n));
        }
        const int average = sum >= 0 ? (sum + 16) / 32 : -((-sum + 16) / 32);
        SetLow8(c.d[0], static_cast<uint8_t>(average));
        return false;
    }
} // namespace

// After every frame: the gauges kept full (in a fight only: the records hold other things outside one)
void CheatsFrame(Machine &) {
    if (Ram16(kMode) != 2 || Ram16(kGameStep) != kGameMatch) {
        return;
    }
    static const char *const kFullGauge[2] = {"full_gauge", "full_gauge_2"};
    for (int n = 0; n < 2; n++) {
        if (FeatureOn(kFullGauge[n]) && Ram8(Player(n) + kState) == 2 && Ram16(Player(n) + kSpecialGauge) < kSpecialGaugeFull) {
            SetRam16(Player(n) + kSpecialGauge, kSpecialGaugeFull);
        }
    }
}

void RegisterCheats() {
    AddHook(HookAt(kTakeDamage), "TakeDamage", TakeDamage);
    // 009BFE: subq.b #1 off the frames of the round's second (at nothing a second goes). With the cheat the frame is
    // given back first, so the clock stands still.
    AddHook(HookAt(kClockTick), "ClockTick", [](M68K &) {
        if (FeatureOn("infinite_time") || session::NoClock()) {
            SetRam8(kClockFrames, static_cast<uint8_t>(Ram8(kClockFrames) + 1));
        }
        return false;
    });
    AddHook(HookAt(0x023C4C), "StunAdded", StunAdded);
    AddHook(HookAt(0x024146), "RandomPart", RandomPart);
    Game &game = TheGame();
    game.features.push_back({"no_damage", "Cheats", "No damage", "Player 1's health does not go down."});
    game.features.push_back({"no_damage_2", "Cheats", "No damage, player 2", "Player 2's health does not go down (the opponent's, in a one-player game)."});
    game.features.push_back({"one_hit", "Cheats", "One-hit knockouts", "Player 2 (the opponent, in a one-player game) goes down with the first hit."});
    game.features.push_back({"full_gauge", "Cheats", "Full gauge", "Player 1's super combo gauge is always full."});
    game.features.push_back({"full_gauge_2", "Cheats", "Full gauge, player 2", "Player 2's super combo gauge is always full."});
    game.features.push_back({"infinite_time", "Cheats", "Infinite time", "The round's clock stands still."});
    game.features.push_back({"no_stun", "Cheats", "No stun", "Player 1 is never stunned (no stun builds up)."});
    game.features.push_back({"no_stun_2", "Cheats", "No stun, player 2", "Player 2 is never stunned (no stun builds up)."});
    game.features.push_back({"fixed_damage", "Training mode", "Fixed damage",
                             "No random part in a hit's damage (the average in its place), so a combo always does the same."});
}

} // namespace cps2recomp
