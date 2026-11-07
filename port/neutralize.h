// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Input Lab contributors
#pragma once
#include <cstdint>
namespace port {
inline const char *NeutralizeFeatureName(int player) {
    static constexpr const char *names[] = {"neutralize_directions_1", "neutralize_directions_2",
        "neutralize_directions_3", "neutralize_directions_4"};
    return player >= 0 && player < 4 ? names[player] : "";
}
// Each opposing axis pair cancels independently; all attack/start/coin bits survive.
inline uint16_t NeutralizeOpposingDirections(uint16_t buttons) {
    if ((buttons & 3u) == 3u) buttons &= ~uint16_t{3};
    if ((buttons & 12u) == 12u) buttons &= ~uint16_t{12};
    return buttons;
}
}
