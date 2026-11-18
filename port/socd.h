// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 cps2recomp contributors. Part of cps2recomp.
#pragma once

#include "../machine/machine.h"

namespace port {

// Clean the combined local controls before recording or sending them. No remembered direction or extra frame:
// releasing either of two opposing directions immediately leaves the one still held. Replays are already inputs.
inline uint16_t CleanSocd(uint16_t pad, bool neutral) {
    if (!neutral) {
        return pad;
    }
    const auto cancel = [&](uint16_t directions) {
        if ((pad & directions) == directions) {
            pad &= static_cast<uint16_t>(~directions);
        }
    };
    cancel(cps2recomp::kLeft | cps2recomp::kRight);
    cancel(cps2recomp::kUp | cps2recomp::kDown);
    return pad;
}

} // namespace port
