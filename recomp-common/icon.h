// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of recomp-common: https://github.com/tmcleroy/recomp-common
//
// A port's icon, from the PNG tools/make_icons.sh puts in the program (its icon_png.h: kIconPng, kIconPngSize):
// the window's icon (the task bar's, the dock's while it runs, a Linux desktop's), and the in-game menu's
// (MenuConfig::icon). docs/ICONS.md: how an icon is drawn, made and wired in.
#pragma once

#include "menu.h"

#include <SDL3/SDL.h>

#include <cstddef>
#include <cstdint>

namespace recomp::sdl {

// The PNG as an RGBA32 surface (the caller destroys it), or nullptr
inline SDL_Surface *LoadIcon(const unsigned char *png, size_t size) {
    if (!png || !size) {
        return nullptr;
    }
    SDL_Surface *loaded = SDL_LoadPNG_IO(SDL_IOFromConstMem(png, size), true);
    if (!loaded) {
        return nullptr;
    }
    SDL_Surface *rgba = SDL_ConvertSurface(loaded, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(loaded);
    return rgba;
}

inline void SetWindowIcon(SDL_Window *window, const unsigned char *png, size_t size) {
    if (SDL_Surface *icon = window ? LoadIcon(png, size) : nullptr) {
        SDL_SetWindowIcon(window, icon);
        SDL_DestroySurface(icon);
    }
}

// For MenuConfig::icon (0xAABBGGRR, as RGBA32's bytes read on a little-endian machine)
inline MenuIcon MenuIconOf(const unsigned char *png, size_t size) {
    MenuIcon icon;
    SDL_Surface *surface = LoadIcon(png, size);
    if (!surface) {
        return icon;
    }
    icon.width = surface->w;
    icon.height = surface->h;
    icon.pixels.resize(static_cast<size_t>(surface->w) * surface->h);
    for (int y = 0; y < surface->h; y++) {
        const uint8_t *row = static_cast<const uint8_t *>(surface->pixels) + static_cast<size_t>(y) * surface->pitch;
        for (int x = 0; x < surface->w; x++) {
            const uint8_t *p = row + x * 4;
            icon.pixels[static_cast<size_t>(y) * surface->w + x] =
                static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) | (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
        }
    }
    SDL_DestroySurface(surface);
    return icon;
}

} // namespace recomp::sdl
