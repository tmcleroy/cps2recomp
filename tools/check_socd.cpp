// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 cps2recomp contributors. Part of cps2recomp.
#include "../port/socd.h"

#include <array>
#include <cstdio>
#include <cstdlib>

using namespace cps2recomp;

static void Check(uint16_t input, uint16_t expected, bool neutral = true) {
    if (port::CleanSocd(input, neutral) != expected) {
        std::fprintf(stderr, "SOCD failed: input %04x, expected %04x, neutral %d\n", input, expected, neutral);
        std::exit(1);
    }
}

int main() {
    // All direction combinations, independently specified. Upper bits (buttons, start, coin, tag and spare)
    // must survive unchanged; Off must preserve even opposing directions for every possible input word.
    constexpr std::array<uint16_t, 16> expected = {
        0, kRight, kLeft, 0, kDown, kDown | kRight, kDown | kLeft, kDown,
        kUp, kUp | kRight, kUp | kLeft, kUp, 0, kRight, kLeft, 0};
    for (unsigned input = 0; input <= UINT16_MAX; ++input) {
        Check(input, (input & 0xfff0) | expected[input & 15]);
        Check(input, input, false);
    }
    // Overlap/release in either order, and a half circle whose left/right keys overlap while down stays held.
    for (const auto &motion : std::array<std::array<uint16_t, 5>, 3>{{
             {kLeft, kLeft | kRight, kRight, 0, kLeft},
             {kRight, kLeft | kRight, kLeft, 0, kRight},
             {kLeft, kLeft | kDown, kLeft | kRight | kDown, kRight | kDown, kRight}}}) {
        for (const auto input : motion) {
            Check(input | kKick3, expected[input] | kKick3);
        }
    }
    // Directions held on different devices cancel after the frontend combines those devices for one player.
    Check(kLeft | (kRight | kPunch1), kPunch1);
    std::puts("ok: SOCD neutral directions, button preservation, overlap/release and Off for all input words");
}
