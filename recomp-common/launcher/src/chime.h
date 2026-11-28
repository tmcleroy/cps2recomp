// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of recomp-common: https://github.com/tmcleroy/recomp-common
#pragma once
// A short notification chime, made in code (two soft bell tones, going up): no sound file ships with the launcher.

#include <vector>

namespace launcher {

// The chime's samples: mono, 32-bit float at `rate` Hz, about 0.6 seconds, never louder than `volume` (0..1).
// (apart from the sound device so that a test can look at them)
std::vector<float> chime_samples(int rate, float volume);

// Plays it on the system's default sound device, and returns at once. Does nothing, quietly, where there is no
// device. Called from the launcher's one thread.
void play_chime();

}  // namespace launcher
