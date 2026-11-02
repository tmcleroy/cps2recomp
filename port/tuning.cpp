// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Input Lab contributors
#include "tuning.h"
#include "../recomp-common/settings.h"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <iomanip>
#include <locale>
#include <sstream>

namespace port {
namespace {
    Tuning current;
    std::string currentPath;
    double Bounded(size_t index, double value) {
        const auto &info = kTuning[index];
        if (!std::isfinite(value)) return info.usual;
        value = std::clamp(value, info.minimum, info.maximum);
        if (std::abs(value - info.usual) < 1e-9) return info.usual;
        return index < 2 ? std::round(value * 10) / 10 : value;
    }
}
Tuning LoadTuning(const std::string &path) {
    Tuning result;
    recomp::Ini ini(path); ini.load();
    for (size_t i = 0; i < kTuneCount; ++i) {
        const auto text = ini.get("sliders", kTuning[i].key);
        double value = kTuning[i].usual;
        const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
        if (parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size()) result.values[i] = Bounded(i, value);
    }
    return result;
}
bool SaveTuning(const std::string &path, const Tuning &tuning) {
    if (path.empty()) return false;
    recomp::Ini ini(path); ini.load();
    for (size_t i = 0; i < kTuneCount; ++i) {
        std::ostringstream text;
        text.imbue(std::locale::classic());
        text << std::setprecision(15) << Bounded(i, tuning.values[i]);
        ini.set("sliders", kTuning[i].key, text.str());
    }
    return ini.save();
}
Tuning &CurrentTuning() { return current; }
void LoadCurrentTuning(const std::string &path) { currentPath = path; current = LoadTuning(path); }
bool SaveCurrentTuning() { return SaveTuning(currentPath, current); }
bool SetCurrentTuning(size_t index, double value) {
    if (index >= kTuneCount) return false;
    current.values[index] = Bounded(index, value);
    return SaveCurrentTuning();
}
void PictureTuning::Apply(uint32_t *pixels, size_t count, const Tuning &tuning) {
    const double brightness = Bounded(3, tuning.Get(Tune::Brightness));
    const double contrast = Bounded(4, tuning.Get(Tune::Contrast));
    const double gamma = Bounded(5, tuning.Get(Tune::Gamma));
    if (brightness == 1 && contrast == 1 && gamma == 1) return;
    const std::array<double, 3> values{brightness, contrast, gamma};
    if (values != m_values) {
        m_values = values;
        for (size_t i = 0; i < m_table.size(); ++i) {
            const double corrected = ((std::pow(i / 255.0, 1.0 / gamma) - 0.5) * contrast + 0.5) * brightness;
            m_table[i] = static_cast<uint8_t>(std::lround(std::clamp(corrected, 0.0, 1.0) * 255));
        }
    }
    for (size_t i = 0; i < count; ++i) {
        const uint32_t rgb = pixels[i];
        pixels[i] = (rgb & 0xFF000000u) | m_table[rgb & 255] | (uint32_t{m_table[(rgb >> 8) & 255]} << 8) |
                    (uint32_t{m_table[(rgb >> 16) & 255]} << 16);
    }
}
} // namespace port
