// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Input Lab contributors
#pragma once
namespace inputlab {
inline int ToggleVampire(int cursor, int character) {
    if (cursor == 2) return character == 0x12 ? 2 : 0x12;
    if (cursor == 8) return character == 0x18 ? 8 : 0x18;
    return character;
}
}
