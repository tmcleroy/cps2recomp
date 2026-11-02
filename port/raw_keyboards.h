// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026. Local Input Lab extension to cps2recomp.
#pragma once
#include "keyboards.h"
#include <memory>
namespace port {
// Own raw-input sink: SDL's combined key-up filtering loses an individual
// keyboard's release when both devices hold the same key.
class RawKeyboards {
public:
    RawKeyboards();
    ~RawKeyboards();
    bool Open(SDL_Window *owner);
    // A shader can recreate SDL's native window. Keep the device states and attach
    // the existing independent sink to its new owner instead of losing input focus.
    bool Retarget(SDL_Window *owner);
    void PollFocus();
    const std::string &Error() const;
    KeyboardState &State();
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace port
