// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Input Lab contributors
#pragma once
namespace inputlab {
inline int ToggleAlpha2(int character) {
    constexpr int pairs[][2] = {{0,23},{2,20},{4,18},{15,22},{16,21}};
    for (const auto &pair : pairs) {
        if (character == pair[0]) return pair[1];
        if (character == pair[1]) return pair[0];
    }
    return character;
}
inline int Alpha2Base(int character) {
    const int other = ToggleAlpha2(character);
    return character >= 18 && other != character ? other : character;
}
}
