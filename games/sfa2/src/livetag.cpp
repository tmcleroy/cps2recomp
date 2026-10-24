// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Street Fighter Alpha 2: swapping a player's fighter in the middle of a fight, for the team battle's tag style
// (fight/battle.h). The game sets the player up again itself: its routine's step 0 (021308) clears the record from
// +180 to +3FF and some bytes below, sets the health, puts the player 0x60 to its side of where it is, calls the
// character's own setting up (a routine by number at 9844C) and, in a match's first round, its entrance (02140A). So a
// swap is the character's byte, its colours and the state put at 0; the game does the rest at its next step, with no
// entrance and where the player was (hooks). The colours are put in on the next frame.
//
// For tests, SFA2_SWAP=FRAME:PLAYER:CHARACTER,... (hex character) swaps outside the team battle, at that frame or, with
// SFA2_SWAP_FREE, as soon after as the player can act.
#include "battle.h"
#include "machine.h"
#include "sfa2.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace cps2recomp {

namespace {
    using namespace sfa2;

    constexpr uint32_t kSuperFlash = 0xFF815C; // byte: a super's flash, the players' steps not run (01FF0A)

    // The pools the game takes objects from (each a list of free ones from A5, made at 019238; taken from at 01959E on and
    // given back at 0195DE on): where the objects are, how many and how big, the tag at +8 a free one has, the list's
    // pointer and count, and what the giving back clears besides +00-+0F (bytes, then words, then longs)
    struct Pool {
        uint32_t first;
        int count;
        uint32_t size;
        uint8_t tag;
        uint32_t list, left;
        bool thrown;    // the things thrown: +68, +5B, +30, +26, +28-+2F; two lists, by +71 (2: the second)
        bool clears28;  // +30, +26, +28-+2F
        bool clears26;  // +30, +26
    };
    constexpr Pool kPools[] = {
        {0xFF9400, 16, 0x80, 0x08, 0xFFE0F0, 0xFFE114, true, true, true},
        {0xFF9C00, 10, 0x80, 0x08, 0xFFE0F2, 0xFFE115, true, true, true},
        {0xFFA100, 40, 0x80, 0x10, 0xFFE0F4, 0xFFE116, false, true, true},
        {0xFFB500, 4, 0x80, 0x10, 0xFFE0FA, 0xFFE119, false, true, true},
        {0xFFB700, 16, 0x80, 0x14, 0xFFE0F6, 0xFFE117, false, false, true},
        {0xFFBF00, 8, 0x200, 0x18, 0xFFE0F8, 0xFFE118, false, false, false},
    };
    constexpr uint32_t kOwner = 0x36;   // word: the record of whoever made the object (a player's or another object's)
    constexpr uint32_t kSecondList = 0x71;
    // Words of a player's record that name objects of its own: the ones its character's setting up makes (+28), the
    // last thing thrown (+124)
    constexpr uint32_t kLinks[] = {0x28, 0x2A, 0x2C, 0x2E, 0x124};

    // Every live object `owner` made (and everything those made), given back to its pool as the game gives one back;
    // the links to it of both players' records cleared
    int GiveBack(uint32_t owner) {
        int given = 0;
        for (const Pool &pool : kPools) {
            for (int n = 0; n < pool.count; n++) {
                const uint32_t object = pool.first + static_cast<uint32_t>(n) * pool.size;
                if (!Ram8(object + kActive) || object == owner || Ram16(object + kOwner) != (owner & 0xFFFF)) {
                    continue;
                }
                const bool second = pool.thrown && Ram8(object + kSecondList) == 2;
                for (uint32_t at = 0; at < 0x10; at += 4) {
                    SetRam32(object + at, 0);
                }
                if (pool.thrown) {
                    SetRam8(object + 0x68, 0);
                    SetRam8(object + 0x5B, 0);
                }
                if (pool.clears26) {
                    SetRam8(object + 0x30, 0);
                    SetRam16(object + 0x26, 0);
                }
                if (pool.clears28) {
                    SetRam32(object + 0x28, 0);
                    SetRam32(object + 0x2C, 0);
                }
                SetRam8(object + 0x08, pool.tag);
                const Pool &to = second ? kPools[1] : pool;
                const uint32_t list = 0xFF0000 | ((Ram16(to.list) - 2) & 0xFFFF);
                SetRam16(list, static_cast<uint16_t>(object));
                SetRam16(to.list, static_cast<uint16_t>(list));
                SetRam8(to.left, static_cast<uint8_t>(Ram8(to.left) + 1));
                for (int p = 0; p < 2; p++) {
                    for (uint32_t at : kLinks) {
                        if (Ram16(Player(p) + at) == (object & 0xFFFF)) {
                            SetRam16(Player(p) + at, 0);
                        }
                    }
                }
                given += 1 + GiveBack(object);
            }
        }
        return given;
    }

    // A swap's colours still to come (once the game has set the newcomer up), by player
    bool g_colours[2] = {false, false};
    // A newcomer of a swap: set up with no entrance, and put back where it was swapped in (the setting up puts it
    // 0x60 to its side of where it is)
    bool g_place[2] = {false, false};
    uint32_t g_placeX[2] = {0, 0}, g_placeY[2] = {0, 0};

    // The panel (an object of the last pool, its kind +2 6: 082C1C) has its own picture in its record: the names under
    // the bars at +BE and +C6, made when it is (082EA4: a word by character from 8318E, 0x30 on with A5+90 set, and
    // the second player's 0x60 on from the first's)
    constexpr uint32_t kPanelNames = 0x08318E;
    void Name(M68K &c, int player) {
        for (int n = 0; n < kPools[5].count; n++) {
            const uint32_t object = kPools[5].first + static_cast<uint32_t>(n) * kPools[5].size;
            if (Ram8(object + kActive) && Ram8(object + 0x02) == 6) {
                const uint32_t table = kPanelNames + (Ram16(0xFF8090) ? 0x30 : 0) + (player ? 0x60 : 0);
                SetRam16(object + 0xBE + static_cast<uint32_t>(player) * 8, Mem16(c, table + Ram8(Player(player) + kCharacter) * 2u));
            }
        }
    }

    int Side(uint32_t player) {
        return player == Player(0) ? 0 : player == Player(1) ? 1 : -1;
    }
} // namespace

// The player's fighter can be swapped now: a round is being fought, no super flashing, and it can act (not held,
// reeling or frozen)
bool CanTag(int player) {
    return Ram16(kGameStep) == kGameMatch && Ram16(kMatchStep) == kMatchRound && Ram16(kRoundStep) == kRoundFought &&
           !Ram8(kSuperFlash) && CanAct(player) && !Ram8(Player(player) + kHitStop) && !Ram8(Player(player) + kInAir);
}

// The game has set the player's newcomer up: its routine has gone on from step 0
bool TaggedIn(int player) {
    return Ram8(Player(player) + kState) != 0;
}

// The player's fighter becomes `character` in that set of its colours (-1: the player's), set up by the game anew
// at its next step
void TagIn(int player, int character, int colours) {
    const uint32_t record = Player(player);
    GiveBack(record);
    SetRam8(record + kCharacter, static_cast<uint8_t>(character));
    if (colours >= 0) {
        SetRam8(record + kColourChoice, static_cast<uint8_t>(colours % kColourSetsEach));
    } else {
        const uint32_t other = Player(1 - player);
        if (Ram8(other + kCharacter) == character && Ram8(other + kColourChoice) == Ram8(record + kColourChoice)) {
            SetRam8(record + kColourChoice, static_cast<uint8_t>(Ram8(record + kColourChoice) ^ 1));
        }
    }
    g_place[player] = true;
    g_placeX[player] = Ram32(record + kX);
    g_placeY[player] = Ram32(record + kY);
    SetRam32(record + kState, 0);
    g_colours[player] = true;
}

// 02140A: the setting up of a player, which in a match's first round goes on to the player's entrance: a newcomer
// of a swap, and every fighter of the team battle, begins ready instead (as in the later rounds: 021430)
bool TagEntrance(M68K &c) {
    const int n = Side(c.a[6] & 0xFFFFFF);
    if (n < 0 || (!g_place[n] && !TeamBattle())) {
        return false;
    }
    c.pc = 0x021430;
    return true;
}

// 02145E: the setting up has done the rest (the character's own): a newcomer of a swap put back where it was swapped in
bool TagSetUp(M68K &c) {
    const uint32_t player = c.a[6] & 0xFFFFFF;
    const int n = Side(player);
    if (n >= 0 && g_place[n]) {
        g_place[n] = false;
        SetRam32(player + kX, g_placeX[n]);
        SetRam32(player + kY, g_placeY[n]);
    }
    return false;
}

// Before each frame: the colours of a swap, and the tests' swaps
void LiveTagBeforeFrame(Machine &machine) {
    for (int player = 0; player < 2; player++) {
        if (!g_colours[player] || !TaggedIn(player)) {
            continue;
        }
        // (as the round's start does, 0122E2: the character's set by the player's choice kept at kColours, and its five
        // palettes copied to where the player's go, kColoursTo; the game's routine is not called from here, with the
        // CPU stopped inside another)
        g_colours[player] = false;
        const uint32_t record = Player(player);
        M68K &c = machine.cpu;
        Name(c, player);
        uint32_t from = kColourSets + (static_cast<uint32_t>(Ram8(record + kCharacter)) * kColourSetsEach + Ram8(record + kColourChoice)) *
                                          kColourSetSize;
        SetRam32(record + kColours, from);
        uint32_t to = Ram32(record + kColoursTo) & 0xFFFFFF;
        for (uint32_t n = 0; n < kColourSetSize; n += 2, from += 2, to += 2) {
            const uint16_t bright = static_cast<uint16_t>(c.Read(to, 1) & 0xF000);
            c.Write(to, 1, static_cast<uint16_t>((Mem16(c, from) & 0x0FFF) | bright));
        }
    }

    static const char *swap = getenv("SFA2_SWAP");
    if (!swap) {
        return;
    }
    static const bool free = getenv("SFA2_SWAP_FREE") != nullptr;
    static int frame = 0, done = 0; // (the entries done, a bit each)
    static int wanted[2] = {-1, -1}, since[2] = {0, 0};
    static bool playing = false, reported = false;
    frame++;
    // (what the tests look for: the board reset, a swap not set up in time, a player not the character swapped to)
    playing = playing || (Ram16(kMode) == 2 && Ram16(kGameStep) == kGameMatch);
    if (playing && Ram16(kMode) != 2 && !reported) {
        reported = true;
        fprintf(stderr, "swap test: reset at %d\n", frame);
    }
    for (int player = 0; player < 2; player++) {
        if (wanted[player] >= 0 && Ram8(Player(player) + kCharacter) != wanted[player] && !reported) {
            reported = true;
            fprintf(stderr, "swap test: player %d is %02X at %d, not %02X\n", player, Ram8(Player(player) + kCharacter), frame, wanted[player]);
        }
        if (since[player] > 0 && ++since[player] > 4 && !TaggedIn(player) && !reported) {
            reported = true;
            fprintf(stderr, "swap test: player %d not set up at %d\n", player, frame);
        }
        if (TaggedIn(player)) {
            since[player] = 0;
        }
    }
    int index = 0;
    for (const char *entry = swap; entry; entry = strchr(entry, ',') ? strchr(entry, ',') + 1 : nullptr, index++) {
        int at = 0, player = 0, character = 0;
        if (sscanf(entry, "%d:%d:%x", &at, &player, &character) < 3 || (done >> index) & 1) {
            continue;
        }
        if (free ? frame >= at && CanTag(player) : frame == at) {
            done |= 1 << index;
            fprintf(stderr, "swap at %d: player %d %02X -> %02X\n", frame, player, Ram8(Player(player) + kCharacter), character);
            TagIn(player, character, -1);
            wanted[player] = character;
            since[player] = 1;
            break;
        }
    }
}

void RegisterLiveTag() {
    AddHook(0x02140A, "TagEntrance", TagEntrance);
    AddHook(0x02145E, "TagSetUp", TagSetUp);
}

} // namespace cps2recomp
