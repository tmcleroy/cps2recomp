// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Input Lab contributors
#include "../port/neutralize.h"
#include <cstdio>
int main() {
    for (unsigned bits = 0; bits < 65536; ++bits) {
        const auto result = port::NeutralizeOpposingDirections(bits);
        const unsigned horizontal = (bits & 3) == 3 ? 0 : bits & 3;
        const unsigned vertical = (bits & 12) == 12 ? 0 : bits & 12;
        if (result != ((bits & ~15u) | horizontal | vertical)) return 1;
        if (port::NeutralizeOpposingDirections(result) != result) return 2;
    }
    std::puts("65536 input masks: independent axis cancellation, other bits preserved, idempotent");
}
