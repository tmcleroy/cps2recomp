// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Marvel vs. Capcom: tag-team play for four people (fight/tag.h); this says where the game keeps the character on
// point and the choosing.
#include "alternative.h"
#include "battle.h"
#include "marvel.h"
#include "session.h"
#include "state.h"
#include "tag.h"
#include "mvsc.h"

#include <algorithm>

namespace cps2recomp {

namespace {
    using namespace mvsc;

    // A byte of each choosing record (kChoosing): the picks taken, 0-2 (counted up as each is taken: 00BCDA; traced
    // through choosing). Its first byte, the record's step, is 0 while the side is not choosing.
    constexpr uint32_t kPicksTaken = 0x03;

    // Whether a side starts with its second character: a player who holds LP+MP+HP as the choosing ends has the game
    // swap the side's two picks (kPicks) and their colours (00BB2C), and the round's records are made from them, so
    // that the record's kWhichPick no longer says who chose its character. 0 again while the side chooses its first
    // character (00BD28). Kept with save states.
    uint8_t g_swapped[2];

    // 00BB2C: the game's step has just turned from the choosing to the match. The game swaps a side's picks if kPlaying
    // is above 0, the side is in (kPlayersIn) and its panel has the three punches held (00BB44-00BBB2); seen here before
    // it does. Only looks.
    bool OrderChosen(M68K &) {
        if (static_cast<int8_t>(Ram8(kPlaying)) > 0) {
            for (int side = 0; side < 2; side++) {
                if ((Ram8(kPlayersIn) >> side) & 1 && (Ram16(kPanel[side]) & 0x70) == 0x70) {
                    g_swapped[side] ^= 1;
                }
            }
        }
        return false;
    }

    // 00BD28: a side's first pick is the character under its cursor (A4: its choosing record, +02 the side), kept in
    // kPicks. Only looks.
    bool FirstPick(M68K &c) {
        g_swapped[Ram8((c.a[4] & 0xFFFFFF) + 2) & 1] = 0;
        return false;
    }
} // namespace

// Which of its side's people a record's character is (0 the side's first person, 1 the second)
int TagPick(uint32_t record) {
    const int side = SideOf(record);
    return side < 0 ? -1 : (Ram8((record & 0xFFFFFF) + kWhichPick) & 1) ^ g_swapped[side];
}

void TagState(Archive &ar) {
    // Upstream version 11 had no MVC order part; the four-player preview's version 11 already did.
    if (ar.version != 0 && (ar.version < 11 || (ar.version == 11 && !ar.NextMark(0x54414F52)))) {
        g_swapped[0] = g_swapped[1] = 0;
        return;
    }
    ar.Mark(0x54414F52); // TAOR
    ar.Pod(g_swapped);
}

void RegisterTag() {
    AddHook(HookAt(0x00BB2C), "TagOrderChosen", OrderChosen);
    AddHook(HookAt(0x00BD28), "TagFirstPick", FirstPick);
    tag::Description d;
    d.onPoint = [](int side) { return Ram8(kPlaying) != 0 && Ram16(kGameStep) == kGameMatch ? TagPick(Player(side)) : -1; };
    d.picking = [](int side) {
        const uint32_t record = kChoosing + static_cast<uint32_t>(side) * kChoosingSize;
        if (Ram16(kGameStep) != kGameChoosing || Ram8(record) == 0) {
            return -1;
        }
        return Ram8(record) == kChoosingDone ? 2 : std::min<int>(Ram8(record + kPicksTaken), 2);
    };
    d.fighting = [] { return Ram16(kGameStep) == kGameMatch && Ram8(kFight) == kFighting; };
    d.otherMode = [] { return session::InSession() || modes::AttractMode() || modes::StageViewer() || BattleMode() || marvel::ModelViewer(); };
    d.labelY = 0.19f;
    tag::Register(d);
}

} // namespace cps2recomp
