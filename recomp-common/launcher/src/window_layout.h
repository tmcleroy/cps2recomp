// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Input Lab contributors
#pragma once
#include <SDL3/SDL.h>
#include <algorithm>
namespace launcher {
// Client area leaves room for the native title bar, taskbar and visible desktop margins.
inline SDL_Rect InitialLauncherBounds(SDL_Rect usable, float scale = 1.0f) {
    const int width = std::max(1, std::min(static_cast<int>(1440 * scale), static_cast<int>(usable.w * 0.88)));
    const int height = std::max(1, static_cast<int>(usable.h * 0.88));
    return {usable.x + (usable.w - width) / 2, usable.y + (usable.h - height) / 2, width, height};
}
}
