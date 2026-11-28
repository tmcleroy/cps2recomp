// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of recomp-common: https://github.com/tmcleroy/recomp-common
#pragma once
// The Display switches a game changed in its own menu while it ran (launcher.h, Toggle::global): the launcher keeps
// launcher.ini in memory, so it reads these back from the file, or its next save would put the old values over them.

#include <string>
#include <vector>

#include "launcher.h"
#include "settings.h"

namespace launcher {

// Each switch's value, read from `prefs`' own file into `prefs` (a global one from [video], another from
// `video_section`); nothing else of `prefs` is touched. Returns whether any changed.
inline bool reread_switches(recomp::Ini& prefs, const std::vector<Toggle>& display, const std::string& video_section) {
    recomp::Ini disk(prefs.path());
    disk.load();
    bool changed = false;
    for (const Toggle& toggle : display) {
        const std::string section = toggle.global ? std::string("video") : video_section;
        const std::string value = disk.get(section, toggle.key);
        if (!value.empty() && value != prefs.get(section, toggle.key)) {
            prefs.set(section, toggle.key, value);
            changed = true;
        }
    }
    return changed;
}

}  // namespace launcher
