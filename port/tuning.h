// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Input Lab contributors
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace port {
inline constexpr double kNativeRefreshHz = 8000000.0 / 512 / 262;
enum class Tune { MainCpu, AudioCpu, Refresh, Brightness, Contrast, Gamma, Count };
inline constexpr size_t kTuneCount = static_cast<size_t>(Tune::Count);
struct TuneInfo {
    const char *key, *title, *description, *suffix;
    double minimum, maximum, step, usual;
    int decimals;
};
inline constexpr std::array<TuneInfo, kTuneCount> kTuning = {{
    {"maincpu_percent", "Overclock CPU :maincpu", "Main processor clock (the board's native frequency at 100%). Changes CPU work available between video interrupts; it is separate from game speed.", "%", 25, 400, 0.1, 100, 1},
    {"audiocpu_percent", "Overclock CPU :audiocpu", "Audio processor clock (the board's native frequency at 100%). The QSound chip and audio sample rate keep their own clocks. Older driver-based save states need Reset game to apply this slider.", "%", 25, 400, 0.1, 100, 1},
    {"refresh_hz", "Screen Refresh Rate", "Video frequency, not the monitor's display mode. Changes time between video interrupts while CPU clocks stay independent. Effects depend on the game's timing.", " Hz", 40, 90, 0.1, kNativeRefreshHz, 3},
    {"brightness", "Screen Brightness", "Picture brightness. The menu remains readable even when the game picture is black.", "", 0, 2, 0.01, 1, 2},
    {"contrast", "Screen Contrast", "Picture contrast around middle grey; affects this game's picture only.", "", 0, 2, 0.01, 1, 2},
    {"gamma", "Screen Gamma", "Midtone correction. Values above 1 brighten midtones; black and white stay at the endpoints before brightness and contrast.", "", 0.1, 3, 0.01, 1, 2},
}};
struct Tuning {
    std::array<double, kTuneCount> values = {100, 100, kNativeRefreshHz, 1, 1, 1};
    double Get(Tune field) const { return values[static_cast<size_t>(field)]; }
};
Tuning LoadTuning(const std::string &path);
bool SaveTuning(const std::string &path, const Tuning &tuning);
Tuning &CurrentTuning();
void LoadCurrentTuning(const std::string &path);
bool SaveCurrentTuning();
bool SetCurrentTuning(size_t index, double value);
// A presentation-only lookup. Native video RAM, saved states and HUD/menu colours are unaffected.
class PictureTuning {
public:
    void Apply(uint32_t *pixels, size_t count, const Tuning &tuning);
private:
    std::array<double, 3> m_values = {-1, -1, -1};
    std::array<uint8_t, 256> m_table{};
};
} // namespace port
