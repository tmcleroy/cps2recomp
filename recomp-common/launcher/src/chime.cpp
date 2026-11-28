// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of recomp-common: https://github.com/tmcleroy/recomp-common
#include "chime.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>

namespace launcher {

namespace {

// One bell-like tone added at `start` seconds: a sine and its octave, a short soft attack and a ringing decay
void add_tone(std::vector<float>& out, int rate, double start, double length, double hertz, double level) {
    const double pi = 3.14159265358979323846;
    const size_t first = static_cast<size_t>(start * rate);
    const size_t count = static_cast<size_t>(length * rate);
    for (size_t i = 0; i < count && first + i < out.size(); i++) {
        const double t = static_cast<double>(i) / rate;
        const double attack = std::min(1.0, t / 0.006);
        const double decay = std::exp(-t * 7.0);
        const double wave = std::sin(2 * pi * hertz * t) + 0.25 * std::sin(2 * pi * hertz * 2 * t) * std::exp(-t * 14.0);
        out[first + i] += static_cast<float>(wave * attack * decay * level);
    }
}

}  // namespace

std::vector<float> chime_samples(int rate, float volume) {
    std::vector<float> out(static_cast<size_t>(rate * 0.62), 0.0f);
    add_tone(out, rate, 0.00, 0.50, 880.00, 1.0);   // A5
    add_tone(out, rate, 0.13, 0.49, 1318.51, 1.0);  // E6
    float loudest = 0.0f;
    for (float sample : out) {
        loudest = std::max(loudest, std::fabs(sample));
    }
    const float scale = loudest > 0.0f ? volume / loudest : 0.0f;
    for (float& sample : out) {
        sample *= scale;
    }
    // (the last few milliseconds fade, so that the end is not a click)
    const size_t fade = std::min(out.size(), static_cast<size_t>(rate * 0.02));
    for (size_t i = 0; i < fade; i++) {
        out[out.size() - 1 - i] *= static_cast<float>(i) / static_cast<float>(fade);
    }
    return out;
}

void play_chime() {
    // (kept for the next one: opened the first time, and the same stream is given the samples again)
    static SDL_AudioStream* stream = nullptr;
    static bool tried = false;
    constexpr int kRate = 44100;
    if (!tried) {
        tried = true;
        if (SDL_InitSubSystem(SDL_INIT_AUDIO)) {
            SDL_AudioSpec spec{};
            spec.format = SDL_AUDIO_F32;
            spec.channels = 1;
            spec.freq = kRate;
            stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
        }
    }
    if (!stream) {
        return;
    }
    static const std::vector<float> samples = chime_samples(kRate, 0.35f);
    SDL_ClearAudioStream(stream);
    SDL_PutAudioStreamData(stream, samples.data(), static_cast<int>(samples.size() * sizeof(float)));
    SDL_ResumeAudioStreamDevice(stream);
}

}  // namespace launcher
