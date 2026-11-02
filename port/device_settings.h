// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Input Lab contributors
#pragma once
#include <SDL3/SDL.h>
#include "../recomp-common/folders.h"
#include <filesystem>
#include <cstdlib>
namespace port {
inline std::filesystem::path DeviceSettingsFolder(const std::string &settings, const std::string &explicitFolder,
                                                   bool collection) {
    if (!explicitFolder.empty()) return std::filesystem::path(explicitFolder);
    if (!collection) return std::filesystem::path(settings).parent_path();
    if (const char *home = std::getenv("CPS2RECOMP_HOME"); home && *home) return std::filesystem::path(home);
    const auto base = std::filesystem::path(SDL_GetBasePath());
    const auto portable = recomp::PortableFolder(base);
    if (!portable.empty()) return portable / recomp::FolderName("CPS-2 Recomp");
    char *pref = SDL_GetPrefPath("", "CPS-2 Recomp");
    const auto path = pref ? std::filesystem::path(pref) : base;
    SDL_free(pref); return path;
}
}
