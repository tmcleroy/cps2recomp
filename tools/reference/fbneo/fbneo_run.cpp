// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
//
// fbneo_run: FinalBurn Neo's libretro core run headless for the cps2recomp timing comparison
// (docs/ARCADE_ACCURACY.md): a cps2recomp input movie played into it, work RAM (FF0000-FFFFFF, 64 KB, in address
// order) appended to a file at the end of every frame, and work RAM bytes set at a chosen frame (the reference's state).
//
//   fbneo_run CORE.dylib ROMZIP FRAMES RAMLOG [MOVIE] [POKEFRAME:ADDR=BYTE,ADDR=BYTE...]
//   (FBNEO_SYSTEM: the system and save folder; FBNEO_SHOTS=prefix: each frame's picture as raw RGB565)
#include <dlfcn.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>
#include "libretro.h"

static std::string g_system;
static std::vector<retro_input_descriptor> g_descriptors;
static std::vector<std::string> g_descNames;
static uint16_t g_pad[4];  // cps2recomp's bits for each player this frame
static FILE *g_shots = nullptr;

// cps2recomp's controls bits (machine/machine.h): right, left, down, up, 3 punches, 3 kicks, Start, Coin
static int BitFor(const std::string &d) {
    struct { const char *name; int bit; } table[] = {
        {"Right", 0}, {"Left", 1}, {"Down", 2}, {"Up", 3},
        {"Weak Punch", 4}, {"Medium Punch", 5}, {"Strong Punch", 6},
        {"Weak Kick", 7}, {"Medium Kick", 8}, {"Strong Kick", 9},
        {"Start", 10}, {"Coin", 11},
    };
    for (auto &t : table) {
        if (d.size() >= strlen(t.name) && d.compare(d.size() - strlen(t.name), strlen(t.name), t.name) == 0) return t.bit;
    }
    return -1;
}
static int PlayerOf(const std::string &d) {
    if (d.rfind("P1 ", 0) == 0) return 0;
    if (d.rfind("P2 ", 0) == 0) return 1;
    return -1;
}
static std::map<std::pair<unsigned, unsigned>, std::pair<int, int>> g_map; // (port, id) -> (player, bit)

static bool Environment(unsigned cmd, void *data) {
    switch (cmd) {
    case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
    case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
        *static_cast<const char **>(data) = g_system.c_str();
        return true;
    case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
        return true;
    case RETRO_ENVIRONMENT_GET_CAN_DUPE:
        *static_cast<bool *>(data) = true;
        return true;
    case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS: {
        g_map.clear();
        for (auto *d = static_cast<const retro_input_descriptor *>(data); d->description; d++) {
            const std::string name = d->description;
            const int player = PlayerOf(name) >= 0 ? PlayerOf(name) : static_cast<int>(d->port), bit = BitFor(name);
            if (d->device == RETRO_DEVICE_JOYPAD && player >= 0 && bit >= 0) g_map[{d->port, d->id}] = {player, bit};
            else if (d->device == RETRO_DEVICE_JOYPAD && bit == 11) g_map[{d->port, d->id}] = {static_cast<int>(d->port), bit};
            if (getenv("FBNEO_VERBOSE")) fprintf(stderr, "input: port %u id %u %s\n", d->port, d->id, d->description);
        }
        return true;
    }
    default:
        return false;
    }
}
static void Log(enum retro_log_level, const char *fmt, ...) {}
static void Video(const void *data, unsigned w, unsigned h, size_t pitch) {
    if (g_shots && data) {
        for (unsigned y = 0; y < h; y++) fwrite(static_cast<const uint8_t *>(data) + y * pitch, 2, w, g_shots);
    }
}
static void Audio(int16_t, int16_t) {}
static size_t AudioBatch(const int16_t *, size_t frames) { return frames; }
static void Poll() {}
static int16_t State(unsigned port, unsigned device, unsigned index, unsigned id) {
    if (device != RETRO_DEVICE_JOYPAD) return 0;
    auto it = g_map.find({port, id});
    if (it == g_map.end()) return 0;
    return (g_pad[it->second.first] >> it->second.second) & 1;
}

int main(int argc, char **argv) {
    if (argc < 5) { fprintf(stderr, "usage: fbneo_run CORE ROMZIP FRAMES RAMLOG [MOVIE] [POKE]\n"); return 2; }
    void *core = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!core) { fprintf(stderr, "%s\n", dlerror()); return 1; }
#define SYM(n) auto n = reinterpret_cast<decltype(&::n)>(dlsym(core, #n))
    SYM(retro_set_environment); SYM(retro_set_video_refresh); SYM(retro_set_audio_sample);
    SYM(retro_set_audio_sample_batch); SYM(retro_set_input_poll); SYM(retro_set_input_state); SYM(retro_init);
    SYM(retro_load_game); SYM(retro_run); SYM(retro_get_memory_data); SYM(retro_get_memory_size);
    SYM(retro_set_controller_port_device);
    g_system = getenv("FBNEO_SYSTEM") ? getenv("FBNEO_SYSTEM") : ".";
    if (getenv("FBNEO_SHOTS")) g_shots = fopen(getenv("FBNEO_SHOTS"), "wb");
    retro_set_environment(Environment);
    retro_set_video_refresh(Video);
    retro_set_audio_sample(Audio);
    retro_set_audio_sample_batch(AudioBatch);
    retro_set_input_poll(Poll);
    retro_set_input_state(State);
    retro_init();
    retro_game_info info{argv[2], nullptr, 0, nullptr};
    if (!retro_load_game(&info)) { fprintf(stderr, "load failed: %s\n", argv[2]); return 1; }
    for (unsigned p = 0; p < 2; p++) retro_set_controller_port_device(p, RETRO_DEVICE_JOYPAD);
    const long frames = atol(argv[3]);
    FILE *ram = fopen(argv[4], "wb");
    // the movie: "frame p1 p2" (hex bits), each held until the next line
    std::vector<std::pair<long, std::pair<unsigned, unsigned>>> movie;
    if (argc > 5 && argv[5][0]) {
        FILE *m = fopen(argv[5], "r");
        char line[256];
        while (m && fgets(line, sizeof line, m)) {
            if (line[0] == '#') continue;
            long f; unsigned a, b;
            if (sscanf(line, "%ld %x %x", &f, &a, &b) == 3) movie.push_back({f, {a, b}});
        }
        if (m) fclose(m);
    }
    long pokeAt = -1;
    std::vector<std::pair<uint32_t, uint8_t>> pokes;
    if (argc > 6) {
        char *q = nullptr;
        pokeAt = strtol(argv[6], &q, 10);
        while (*q == ':' || *q == ',') {
            const uint32_t a = static_cast<uint32_t>(strtoul(q + 1, &q, 16));
            if (*q == '=') pokes.push_back({a & 0xFFFF, static_cast<uint8_t>(strtoul(q + 1, &q, 16))});
        }
    }
    size_t next = 0;
    std::vector<uint8_t> out(0x10000);
    for (long f = 0; f < frames; f++) {
        while (next < movie.size() && movie[next].first <= f) {
            g_pad[0] = static_cast<uint16_t>(movie[next].second.first);
            g_pad[1] = static_cast<uint16_t>(movie[next].second.second);
            next++;
        }
        auto *r = static_cast<uint8_t *>(retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM));
        const size_t n = retro_get_memory_size(RETRO_MEMORY_SYSTEM_RAM);
        if (f == pokeAt && r) {
            for (auto &[a, v] : pokes) if (a < n) r[a ^ 1] = v; // (the 68000's words kept byte-swapped)
        }
        retro_run();
        r = static_cast<uint8_t *>(retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM));
        if (r && n >= 0x10000) {
            for (size_t k = 0; k < 0x10000; k++) out[k] = r[k ^ 1];
            fwrite(out.data(), 1, out.size(), ram);
        }
    }
    fclose(ram);
    if (g_shots) fclose(g_shots);
    return 0;
}
