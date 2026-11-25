// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Cyberbonzo
#include "punisher.h"

namespace cps2recomp {
namespace {
bool BoneBreakerBodyInView(M68K &cpu) {
    if (!FeatureOn("widescreen")) return false;

    // Bone Breaker tests his body separately from his wheels and gun. D0.W is
    // world X - camera X + 80. The original BHI rejects values outside 0..544.
    // Allow for the two side strips even when the camera shifts towards one edge
    // of a room. The sprite writer still clips individual groups to the viewport.
    constexpr int margin = 2 * Video::kWideSide;
    const int x = static_cast<int16_t>(cpu.d[0]);
    if ((x < 0 && x >= -margin) || (x > 544 && x <= 544 + margin)) {
        // Fall through to the unmodified vertical test at 06FC7A. Its ADDI
        // overwrites Z before any other instruction can read it.
        cpu.flagZ = true;
    }
    return false;
}
} // namespace

void RegisterPunisherWideBone() {
    AddHook(HookAt(0x06FC76), "WideBoneBreakerBody", BoneBreakerBodyInView);
}
} // namespace cps2recomp
