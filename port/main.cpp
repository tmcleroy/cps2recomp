// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
//
// The port's entry point: reads the ROM set, runs the game, shows and plays what it makes.
#include "game_info.h"

#include "../runtime/game.h"
#include "../runtime/lift.h"
#include "../runtime/native.h"
#include "../runtime/native_z80.h"
#include "../runtime/state.h"
#include "hud.h"
#include "i18n.h"
#include "../runtime/board.h"
#include "../fight/alternative.h"
#include "../fight/battle.h"
#include "../fight/tag.h"
#include "../fight/guest_stages.h"
#include "../fight/session.h"
#include "png_write.h"
#include "../model/image_files.h"
#include "present.h"
#include "gpu_picture.h"
#include "../runtime/spare.h"
#include "../runtime/guest.h"
#include "port.h"
#include "../recomp-common/arcade_screen.h"
#include "../recomp-common/folders.h"
#include "../recomp-common/menu.h"
#include "../recomp-common/proxy.h"
#include "../recomp-common/settings.h"
#include "../recomp-common/text.h"

#include <SDL3/SDL_main.h> // (on Windows the game is a windowed program: its main is called from WinMain)

#include <zlib.h>

#include <algorithm>
#include <iterator>
#include <chrono>
#include <cmath>
#include <map>
#include <random>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <deque>
#include <filesystem>
#include <set>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#endif

using namespace cps2recomp;

// (what the scene export says, in the language chosen: its words, then two spaces and the folder)
static std::string SaidOfScene(const std::string &said) {
    const size_t at = said.find("  ");
    return at == std::string::npos ? recomp::Tr(said) : recomp::Tr(said.substr(0, at)) + said.substr(at);
}

namespace port {

namespace {
    // Persistent online labels keep brighter ink than their backplates on busy stages.
    constexpr HudOpacity kOnlineHudOpacity[] = {{112, 232}, {64, 190}, {220, 255}};
    constexpr HudOpacity kSpectatingOpacity = {48, 110}; // (the spectator's label: there, but out of the way)
    // The modes played online (battle::StartOnline; 3 the training session, session::StartSession), by number, and
    // their pages' names
    constexpr const char *kOnlineModes[] = {"Versus", "Ball battle", "Tag team battle", "Training"};
    constexpr int kOnlineTraining = 3;
    const Feature *FindFeature(const std::string &name) {
        for (const Feature &feature : TheGame().features) {
            if (name == feature.name) {
                return &feature;
            }
        }
        return nullptr;
    }
    bool OnlineModeHere(int mode) {
        return mode == 0 || (mode == 1 && FindFeature("court_points")) || (mode == 2 && FindFeature("team_size")) ||
               (mode == kOnlineTraining && session::Described());
    }
    // A player's own fighters' rows on the modes' pages (the board's player 1's, or player 2's: the host's side, or the
    // other side's, unless the host has swapped the sides)
    bool OwnRow(int player, const std::string &row) {
        return player == 0 ? row == "court_one" || row.rfind("team_1_", 0) == 0 : row == "court_two" || row.rfind("team_2_", 0) == 0;
    }
    // A game with a mode to start online (the host's pause and the other player's Ready are for them)
    bool ModesOnline() {
        return OnlineModeHere(1) || OnlineModeHere(2);
    }
    // The board's player at a side's first or second place in a match of `players` (-1: none; a side of one has no
    // second): the game's own teams (TheGame().matchOwner: 2 vs 2 in either team order, 2 vs 1, 1 vs 2), or players 1
    // and 3 against 2 and 4; in a match of two, player 1 against player 2
    int SidePlayer(int side, int place, int players) {
        const auto owner = TheGame().matchOwner;
        const int player = players == 2 ? (place == 0 ? side : -1) : owner ? owner(side, place) : side + 2 * place;
        if (player < 0 || player >= players || (place == 1 && player == SidePlayer(side, 0, players))) return -1;
        return player;
    }
    // (the Ready row's place in Port::onlineRows: not one of the game's features)
    constexpr const char *kReadyRow = "#ready";
} // namespace

namespace {
    port::GpuPicture *g_gpuPicture = nullptr; // (--video gpu-check)
} // namespace

namespace {
    constexpr uint32_t kStateVersion = 16; // (5: the 68000's wait kept; 6: the custom soundtrack's place; 7: the board timing's palette copy;
                                          // 8: widescreen's camera a frame ago; 9: a guest board's, runtime/guest.h;
                                          // 10: classic CPS-1 sound, machine/cps1_sound.h; 11: live tag swaps (upstream),
                                          // or MVC starting order/Cross Fever clock (the four-player preview);
                                          // 12: both branches combined; legacy MVC 11 parts are identified by their marks;
                                          // 13: either board DMA timing (upstream) or MVC partner-tag input edges (preview);
                                          // 14: both layouts combined; 15: Marvel vs. Capcom's ball battle;
                                          // 16: the camera and match the alternative modes want, fight/alternative.cpp)

    // (a path is UTF-8 text: by its wide name on Windows, where a folder with an accent is not in the system's code page)
    FILE *OpenFile(const std::string &path, const wchar_t *wideMode, const char *mode) {
#ifdef _WIN32
        (void)mode;
        return _wfopen(TextPath(path).wstring().c_str(), wideMode);
#else
        (void)wideMode;
        return fopen(path.c_str(), mode);
#endif
    }
    bool ReadFile(const std::string &path, std::vector<uint8_t> &data) {
        FILE *f = OpenFile(path, L"rb", "rb");
        if (!f) {
            return false;
        }
        fseek(f, 0, SEEK_END);
        data.resize(static_cast<size_t>(ftell(f)));
        fseek(f, 0, SEEK_SET);
        const bool ok = fread(data.data(), 1, data.size(), f) == data.size();
        fclose(f);
        return ok;
    }
    bool WriteFile(const std::string &path, const void *data, size_t size) {
        FILE *f = OpenFile(path, L"wb", "wb");
        if (!f) {
            return false;
        }
        const bool ok = fwrite(data, 1, size, f) == size;
        fclose(f);
        return ok;
    }
    // "FRAME:path"
    bool AtFrame(const std::string &spec, uint64_t frame, std::string &path) {
        const size_t colon = spec.find(':');
        if (spec.empty() || colon == std::string::npos || strtoull(spec.c_str(), nullptr, 10) != frame) {
            return false;
        }
        path = spec.substr(colon + 1);
        return true;
    }
    uint64_t Random(uint64_t &state) {
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
        return state;
    }
} // namespace

// The board's settings that game.cfg declares (board_option_1 to board_option_12): choices of the board's DIP switches,
// on a "Board" page of the menu and the launcher. Each line is
//   name | title | bank:low-high | description | choice=value | choice=value ...
// (the bank A, B or C, the bits low to high, each choice's value as the bits hold it). "As the board is set", the
// first choice, leaves the bits as game.cfg's dip has them.
namespace {
    struct BoardOption {
        std::string name, title, description;
        int bank = 0, low = 0, high = 0;
        std::vector<std::string> names;
        std::vector<uint8_t> values;
        std::vector<const char *> choices; // ("As the board is set", then the names)
    };
    // (a deque: Feature keeps pointers into these strings, which must not move)
    std::deque<BoardOption> g_boardOptions;

    std::string Trimmed(const std::string &s) {
        const size_t a = s.find_first_not_of(" \t"), b = s.find_last_not_of(" \t");
        return a == std::string::npos ? "" : s.substr(a, b - a + 1);
    }

    bool ParseBoardOption(const char *text, BoardOption &option, std::string &error) {
        std::vector<std::string> fields;
        std::string line = text;
        for (size_t at = 0; at <= line.size();) {
            const size_t bar = std::min(line.find('|', at), line.size());
            fields.push_back(Trimmed(line.substr(at, bar - at)));
            at = bar + 1;
        }
        option.name = fields[0];
        const std::string what = "game.cfg's board option " + (option.name.empty() ? std::string("\"") + text + "\"" : option.name);
        if (fields.size() < 5 || option.name.empty()) {
            error = what + ": name | title | bank:low-high | description | at least one choice=value";
            return false;
        }
        option.title = fields[1];
        option.description = fields[3];
        char bank = 0;
        int consumed = 0;
        const int got = sscanf(fields[2].c_str(), "%c:%d-%d%n", &bank, &option.low, &option.high, &consumed);
        if (got != 3) {
            consumed = 0;
            if (sscanf(fields[2].c_str(), "%c:%d%n", &bank, &option.low, &consumed) != 2) consumed = 0;
            option.high = option.low;
        }
        if (consumed != static_cast<int>(fields[2].size()) || bank < 'A' || bank > 'C' ||
            option.low < 0 || option.high > 7 || option.low > option.high) {
            error = what + ": bits must be A:0-2 or A:6, with no trailing characters";
            return false;
        }
        option.bank = bank - 'A';
        const unsigned limit = 1u << (option.high - option.low + 1);
        for (size_t n = 4; n < fields.size(); n++) {
            const size_t eq = fields[n].rfind('=');
            char *end = nullptr;
            const unsigned long value = eq == std::string::npos ? 0 : strtoul(fields[n].c_str() + eq + 1, &end, 0);
            if (eq == std::string::npos || eq == 0 || end == fields[n].c_str() + eq + 1 || *end || value >= limit) {
                error = what + ": the choice \"" + fields[n] + "\" is not name=value, with a value its bits can hold";
                return false;
            }
            option.names.push_back(Trimmed(fields[n].substr(0, eq)));
            option.values.push_back(static_cast<uint8_t>(value));
        }
        return true;
    }
} // namespace

// Reads game.cfg's board options (none: no Board page); false, with what is wrong, if one cannot be read
bool ReadBoardOptions(std::string &error) {
    static const char *const kTexts[] = {
#ifdef GAME_BOARD_OPTION_1
        GAME_BOARD_OPTION_1,
#endif
#ifdef GAME_BOARD_OPTION_2
        GAME_BOARD_OPTION_2,
#endif
#ifdef GAME_BOARD_OPTION_3
        GAME_BOARD_OPTION_3,
#endif
#ifdef GAME_BOARD_OPTION_4
        GAME_BOARD_OPTION_4,
#endif
#ifdef GAME_BOARD_OPTION_5
        GAME_BOARD_OPTION_5,
#endif
#ifdef GAME_BOARD_OPTION_6
        GAME_BOARD_OPTION_6,
#endif
#ifdef GAME_BOARD_OPTION_7
        GAME_BOARD_OPTION_7,
#endif
#ifdef GAME_BOARD_OPTION_8
        GAME_BOARD_OPTION_8,
#endif
#ifdef GAME_BOARD_OPTION_9
        GAME_BOARD_OPTION_9,
#endif
#ifdef GAME_BOARD_OPTION_10
        GAME_BOARD_OPTION_10,
#endif
#ifdef GAME_BOARD_OPTION_11
        GAME_BOARD_OPTION_11,
#endif
#ifdef GAME_BOARD_OPTION_12
        GAME_BOARD_OPTION_12,
#endif
        nullptr};
    g_boardOptions.clear();
    for (const char *const *text = kTexts; *text; text++) {
        BoardOption &option = g_boardOptions.emplace_back();
        if (!ParseBoardOption(*text, option, error)) {
            g_boardOptions.clear();
            return false;
        }
        option.choices.push_back("As the board is set");
        for (const std::string &name : option.names) {
            option.choices.push_back(name.c_str());
        }
    }
    return true;
}

// The DIP switches as the Board page's choices have them: the board's, with each chosen option's bits put in
void ApplyBoardOptions(Machine &machine) {
    uint8_t dip[3];
    std::copy(machine.DipAsSet(), machine.DipAsSet() + 3, dip);
    for (const BoardOption &option : g_boardOptions) {
        const int choice = FeatureValue(option.name.c_str());
        if (choice <= 0 || choice > static_cast<int>(option.values.size())) {
            continue;
        }
        const uint8_t mask = static_cast<uint8_t>(((1u << (option.high - option.low + 1)) - 1) << option.low);
        dip[option.bank] = static_cast<uint8_t>((dip[option.bank] & ~mask) | ((option.values[choice - 1] << option.low) & mask));
    }
    std::copy(dip, dip + 3, machine.dip);
}

// The cheats that game.cfg declares (cheat_1 to cheat_16): writes to the work RAM, made before every frame while the
// cheat is on, on a "Cheats" page of the menu and the launcher. Each line is
//   name | title | description | [when ADDR.s=VAL | ] choice: ADDR.s=VAL ADDR.s=VAL ... | choice: ...
// (hex; s is b, w or l; "when ADDR.s!=VAL" too). A cheat with one choice (On) is switched on and off.
namespace {
    struct RamWrite {
        uint32_t address = 0;
        char size = 'b';
        uint32_t value = 0;
    };
    struct Cheat {
        std::string name, title, description;
        bool when = false, whenNot = false;
        RamWrite condition;
        std::vector<std::string> names;
        std::vector<std::vector<RamWrite>> writes;
        std::vector<const char *> choices; // ("Off", then the names)
    };
    std::deque<Cheat> g_cheats; // (a deque: Feature keeps pointers into these strings)

    // "ADDR.s=VAL" (or "!=" when `notEqual` may be given); false if it is not that, or not in the work RAM
    bool ParseRamWrite(const std::string &text, RamWrite &write, bool *notEqual = nullptr) {
        char *end;
        write.address = static_cast<uint32_t>(strtoul(text.c_str(), &end, 16));
        if (end == text.c_str() || end[0] != '.' || (end[1] != 'b' && end[1] != 'w' && end[1] != 'l')) {
            return false;
        }
        write.size = end[1];
        const char *rest = end + 2;
        if (notEqual) {
            *notEqual = rest[0] == '!';
            if (*notEqual) {
                rest++;
            }
        }
        if (rest[0] != '=' || !rest[1]) {
            return false;
        }
        const unsigned long long value = strtoull(rest + 1, &end, 16);
        const unsigned long long limit = write.size == 'b' ? 0xFF : write.size == 'w' ? 0xFFFF : 0xFFFFFFFFull;
        const int bytes = write.size == 'b' ? 1 : write.size == 'w' ? 2 : 4;
        if (*end || value > limit || write.address < 0xFF0000 || write.address + bytes - 1 > 0xFFFFFF ||
            (bytes > 1 && (write.address & 1))) {
            return false;
        }
        write.value = static_cast<uint32_t>(value);
        return true;
    }

    bool ParseCheat(const char *text, Cheat &cheat, std::string &error) {
        std::vector<std::string> fields;
        const std::string line = text;
        for (size_t at = 0; at <= line.size();) {
            const size_t bar = std::min(line.find('|', at), line.size());
            fields.push_back(Trimmed(line.substr(at, bar - at)));
            at = bar + 1;
        }
        cheat.name = fields[0];
        const std::string what = "game.cfg's cheat " + (cheat.name.empty() ? std::string("\"") + text + "\"" : cheat.name);
        if (fields.size() < 4 || cheat.name.empty()) {
            error = what + ": name | title | description | [when ADDR.s=VAL |] choice: ADDR.s=VAL ...";
            return false;
        }
        cheat.title = fields[1];
        cheat.description = fields[2];
        size_t n = 3;
        if (fields[n].rfind("when ", 0) == 0) {
            cheat.when = true;
            if (!ParseRamWrite(Trimmed(fields[n].substr(5)), cheat.condition, &cheat.whenNot)) {
                error = what + ": the condition \"" + fields[n] + "\" is not when ADDR.s=VAL (or !=) in the work RAM, FF0000-FFFFFF";
                return false;
            }
            n++;
        }
        if (n >= fields.size()) {
            error = what + ": it has no choice (choice: ADDR.s=VAL ...)";
            return false;
        }
        for (; n < fields.size(); n++) {
            const size_t colon = fields[n].find(':');
            std::vector<RamWrite> writes;
            std::istringstream words(colon == std::string::npos ? std::string() : fields[n].substr(colon + 1));
            std::string word;
            while (words >> word) {
                RamWrite write;
                if (!ParseRamWrite(word, write)) {
                    error = what + ": \"" + word + "\" is not ADDR.s=VAL (s: b, w or l; a word or long at an even address) in the work RAM, FF0000-FFFFFF";
                    return false;
                }
                writes.push_back(write);
            }
            if (colon == std::string::npos || colon == 0 || writes.empty()) {
                error = what + ": the choice \"" + fields[n] + "\" is not name: ADDR.s=VAL ...";
                return false;
            }
            cheat.names.push_back(Trimmed(fields[n].substr(0, colon)));
            cheat.writes.push_back(writes);
        }
        return true;
    }

    uint32_t RamValue(const RamWrite &at) {
        return at.size == 'b' ? Ram8(at.address) : at.size == 'w' ? Ram16(at.address) : Ram32(at.address);
    }
} // namespace

// Reads game.cfg's cheats (none: no Cheats page); false, with what is wrong, if one cannot be read
bool ReadCheats(std::string &error) {
    static const char *const kTexts[] = {
#ifdef GAME_CHEAT_1
        GAME_CHEAT_1,
#endif
#ifdef GAME_CHEAT_2
        GAME_CHEAT_2,
#endif
#ifdef GAME_CHEAT_3
        GAME_CHEAT_3,
#endif
#ifdef GAME_CHEAT_4
        GAME_CHEAT_4,
#endif
#ifdef GAME_CHEAT_5
        GAME_CHEAT_5,
#endif
#ifdef GAME_CHEAT_6
        GAME_CHEAT_6,
#endif
#ifdef GAME_CHEAT_7
        GAME_CHEAT_7,
#endif
#ifdef GAME_CHEAT_8
        GAME_CHEAT_8,
#endif
#ifdef GAME_CHEAT_9
        GAME_CHEAT_9,
#endif
#ifdef GAME_CHEAT_10
        GAME_CHEAT_10,
#endif
#ifdef GAME_CHEAT_11
        GAME_CHEAT_11,
#endif
#ifdef GAME_CHEAT_12
        GAME_CHEAT_12,
#endif
#ifdef GAME_CHEAT_13
        GAME_CHEAT_13,
#endif
#ifdef GAME_CHEAT_14
        GAME_CHEAT_14,
#endif
#ifdef GAME_CHEAT_15
        GAME_CHEAT_15,
#endif
#ifdef GAME_CHEAT_16
        GAME_CHEAT_16,
#endif
        nullptr};
    g_cheats.clear();
    for (const char *const *text = kTexts; *text; text++) {
        Cheat &cheat = g_cheats.emplace_back();
        if (!ParseCheat(*text, cheat, error)) {
            g_cheats.clear();
            return false;
        }
        cheat.choices.push_back("Off");
        for (const std::string &name : cheat.names) {
            cheat.choices.push_back(name.c_str());
        }
    }
    return true;
}

// The cheats that are on, their writes made (while their condition holds): before every frame the game runs, run-ahead's
// too
void ApplyCheats() {
    for (const Cheat &cheat : g_cheats) {
        const int choice = FeatureValue(cheat.name.c_str());
        if (choice <= 0 || choice > static_cast<int>(cheat.writes.size())) {
            continue;
        }
        if (cheat.when && (RamValue(cheat.condition) == cheat.condition.value) == cheat.whenNot) {
            continue;
        }
        for (const RamWrite &write : cheat.writes[choice - 1]) {
            if (write.size == 'b') {
                SetRam8(write.address, static_cast<uint8_t>(write.value));
            } else if (write.size == 'w') {
                SetRam16(write.address, static_cast<uint16_t>(write.value));
            } else {
                SetRam32(write.address, write.value);
            }
        }
    }
}

// Configuration declarations use the existing feature registry; duplicate names are errors, not a second mechanism.
bool ReadDeclaredFeatures(std::string& error) {
    if (!ReadBoardOptions(error) || !ReadCheats(error)) return false;
    std::set<std::string> names;
    for (const auto& feature : TheGame().features) names.insert(feature.name);
    for (const auto& option : g_boardOptions) {
        if (!names.insert(option.name).second) { error = "duplicate game.cfg feature: " + option.name; return false; }
    }
    for (const auto& cheat : g_cheats) {
        if (!names.insert(cheat.name).second) { error = "duplicate game.cfg feature: " + cheat.name; return false; }
    }
    return true;
}

// Every game's features that come from the port itself (after the game's own: RegisterGame), for the game and
// for the list the launcher asks for (--list-features)
// The board's timing: the port's slowdown feature ("Arcade perfect", the first choice, unless another is chosen), or
// what the game's sources say (Game::boardTiming)
bool BoardTimingWanted() {
    return TheGame().boardTiming ? TheGame().boardTiming() : FeatureValue("slowdown") == 0;
}

void RegisterPortFeatures() {
    // (the board's settings from game.cfg: ReadBoardOptions)
    if (!g_boardOptions.empty()) {
        if (std::none_of(TheGame().pages.begin(), TheGame().pages.end(), [](const Page &page) { return !strcmp(page.name, "Board"); })) {
            TheGame().pages.push_back({"Board", "", "The board's own settings, its DIP switches: each as the board is set unless chosen here."});
        }
        for (const BoardOption &option : g_boardOptions) {
            TheGame().features.push_back({option.name.c_str(), "Board", option.title.c_str(), option.description.c_str(), kFeatureOwn, option.choices});
        }
    }
    // (the cheats from game.cfg: ReadCheats; on the game's own Cheats page, if its sources have one)
    if (!g_cheats.empty()) {
        Game &game = TheGame();
        if (std::none_of(game.pages.begin(), game.pages.end(), [](const Page &page) { return !strcmp(page.name, "Cheats"); })) {
            game.pages.push_back({"Cheats", "", "Changes to the game's memory, for trying things out: each off unless chosen, and they can be switched any time."});
        }
        for (const Cheat &cheat : g_cheats) {
            game.features.push_back({cheat.name.c_str(), "Cheats", cheat.title.c_str(), cheat.description.c_str(), kFeatureOwn, cheat.choices});
        }
    }

    // (run_ahead: Port::RunAhead)
    TheGame().features.push_back({"run_ahead", "Game", "Run-ahead",
                                  "Shows the game a frame or two further on than it is, so that what you press is seen sooner than "
                                  "on the board (input lag taken away). The game runs as it does without it; it takes two or three "
                                  "times the processor. Off by itself in the game's own modes beyond fights and training. Online it is each "
                                  "player's own: the other player's moves in the frames ahead are guessed, and put right as they come.",
                                  0, {"Off", "1 frame", "2 frames"}});
    // (rewind, practice_point: Port::RewindTick, PracticeSet; docs/PRACTICE.md)
    TheGame().features.push_back({"rewind", "Game", "Rewind",
                                  "Hold R (a gamepad's left trigger) and the game runs backwards, three times as fast as play, "
                                  "through its last half a minute or so, unheard; let go and play on from there. Kept as a "
                                  "state every few frames, a few dozen megabytes of memory. With it on, the left trigger "
                                  "rewinds in place of a tag game's Tag. Not online.",
                                  0});
    TheGame().features.push_back({"practice_point", "Game", "Practice point",
                                  "] (a gamepad's left shoulder held half a second) keeps the game as it is that instant; "
                                  "Backspace (the shoulder tapped) puts it back, at once and as often as you like; Backslash "
                                  "(the shoulder with the left trigger held) shows your last attempt from it again, the "
                                  "controls as you pressed them, until you press a button. Until a point is set, those keys "
                                  "do what they do without it (a training mode's Start over and Replay). Not online.",
                                  0});
    // (slowdown: machine.h, SetBoardTiming; docs/TIMING.md. Not for a game whose sources decide it: Game::boardTiming,
    // a shooting game's own Slowdown row)
    if (!TheGame().boardTiming) {
        TheGame().features.push_back({"slowdown", "Game", "Slowdown",
                                      "Arcade perfect: the processor runs as on the arcade board, waiting for its memory and giving "
                                      "the bus to the video chip, so the game runs at the board's speed and slows down where and as "
                                      "much as the board does (big supers, busy screens); checked frame by frame against a "
                                      "cycle-exact model of the board. None: the processor has every clock, so the game seldom "
                                      "slows, and a game that fits in more of its work when it has time (Street Fighter II's turbo "
                                      "speeds) runs faster than on the board. In training mode, the game's other modes, widescreen "
                                      "and with cheats on, the port's own code for some routines runs, and the timing there is "
                                      "close but not exact.",
                                      0, {"Arcade perfect", "None"}});
    }
    TheGame().features.push_back({"keyboard_only", "Game", "Keyboard only",
                                  "For JoyToKey and other keyboard mappers. Ignore native gamepad buttons, sticks and shortcuts, "
                                  "including the chat button. Your keyboard bindings still work."});
    // (socd: port/socd.h, in the window's reading of the controls, so online the other side gets them cleaned)
    TheGame().features.push_back({"socd", "Game", "SOCD cleaner",
                                  "What the game sees when you hold two opposite directions at once, as a hit box or a keyboard "
                                  "can (the board passes both on, and games do odd things with them). Neutral: up and down is "
                                  "neither, left and right is neither. Hit box: up and down is up, left and right is neither. "
                                  "4-way: as Neutral, and no diagonals, the direction pressed last winning. Your own: online "
                                  "the other player's choice is theirs.",
                                  0, {"Off", "Neutral (U+D=N, L+R=N)", "Hit box (U+D=U, L+R=N)", "4-way, no diagonals"}});
    // (rapid_fire: Port::RapidFire)
    TheGame().features.push_back({"rapid_fire", "Game", "Rapid fire",
                                  "A button held down is pressed again and again (fifteen times a second), for the first button or "
                                  "for all of them. The first press is seen at once, as without it.",
                                  0, {"Off", "First button", "Every button"}});
    // (display_*: how the picture is fitted to the window, present.h; the first of each is how it always was)
    TheGame().features.push_back({"display_scaling", "Display", "Scaling",
                                  "Sharp: as large as fits, each pixel a crisp block. Whole pixels: every pixel the same whole "
                                  "number of the screen's pixels tall (a border round it). Smooth: softened all over.",
                                  0, {"Sharp", "Whole pixels", "Smooth"}});
    TheGame().features.push_back({"display_shape", "Display", "Picture shape",
                                  "4:3: as the arcade's monitor showed it. Square pixels: each pixel as wide as it is tall, "
                                  "a little wider than 4:3. Fill: the whole window or screen, stretched to it.",
                                  0, {"4:3", "Square pixels", "Fill"}});
    // (display_turn: a vertical game only, Turned)
    if (TheGame().vertical) {
        TheGame().features.push_back({"display_turn", "Display", "Picture turned",
                                      "The game was made for a monitor stood on end. Upright: its picture stood up in the window, as "
                                      "the cabinet showed it. On its side: as the board draws it, for a screen turned on its side.",
                                      0, {"Upright", "On its side"}});
    }
    // (display_shader: Port::ShaderPreset, shading.h; the presets are in shaders/ beside the program, kShaderPresets.
    // librashader is in each release, tools/librashader.sh; on Android the presets are unpacked by GameActivity.java)
    TheGame().features.push_back({"display_shader", "Display", "Shader",
                                  "The picture drawn through one of RetroArch's shaders. CRT: a curved arcade monitor. CRT "
                                  "Lottes and CRT Guest: flat monitors, sharp and bright. CRT Royale: the closest to a real "
                                  "monitor's glass and the slowest. CRT Light: lines and a faint mask, for a slow computer. "
                                  "Scanlines: the lines alone, light, strong, as an OSSC draws them, or dark (a black line "
                                  "between each two, best with Whole pixels). xBRZ and ScaleFX: the pixels' edges made smooth "
                                  "lines and curves. Your own: the first .slangp preset in the shaders folder beside the "
                                  "game's settings. The launcher chooses which of these the menu goes through. The picture's "
                                  "shape and size stay as set; the shader draws its pixels in place of Scaling.",
                                  kFeatureMenuChoices,
                                  {"Off", "CRT", "CRT Lottes", "CRT Royale", "CRT Guest", "CRT Light", "Scanlines: light",
                                   "Scanlines: strong", "Scanlines: OSSC", "Scanlines: dark", "xBRZ", "ScaleFX", "Your own"}});
    // (zoom: the camera, port/zoom.h, from the menu as well as the zoom's buttons; for this run only, as they are)
    if (Zoom::Possible()) {
        TheGame().features.push_back({"zoom", "Display", "Zoom",
                                      "The picture shown larger about a fighter, following it: player 1's or player 2's (1.5x; the "
                                      "zoom's buttons change the size), or both fighters', as close as keeps them both in the "
                                      "picture. Or by distance: a little closer as the fighters come together, out to all of the "
                                      "picture as they part, with the health bars and clock left as they are. Also the left "
                                      "stick's click held with a shoulder (with the right stick's: both), Left Shift with Q, E or "
                                      "W, or a touch screen's Zoom button (both).",
                                      kFeatureSession, {"Off", "Player 1", "Player 2", "Both fighters", "By distance"}});
    }
    TheGame().features.push_back({"input_lag", "Display", "Input lag readout",
                                  "At the bottom of the picture: how many frames the game itself takes to show a press (found in a "
                                  "fight, by running the next frames with and without one), online the match's input delay, what "
                                  "run-ahead takes off, the port's own time from the press to handing that frame to the screen, and "
                                  "the sum. Your screen's own delay comes on top of it."});
    // (online play's readout, each player's own: Port::Shown)
    TheGame().features.push_back({"online_ping", "Online", "Ping",
                                  "The host sees its highest estimated ping to a guest. A guest sees the estimate to the farthest player through the host.",
                                  kFeatureEnhanced});
    TheGame().features.push_back({"online_hud_opacity", "Online", "Online HUD opacity",
                                  "Fade the names, point markers, streak, set score and network readings on your screen.",
                                  0, {"Soft", "Faint", "High contrast"}});
    TheGame().features.push_back({"online_stats", "Online", "Network stats",
                                  "During an online match, in small letters at the top right of the picture: the ping and its "
                                  "jitter (how much it wavers), each player's input delay and run-ahead, and the rollbacks of the "
                                  "last second (how many, and the most frames one went back).",
                                  kFeatureEnhanced});
    TheGame().features.push_back({"online_link_rtt", "Online", "Show direct link RTT",
                                  "Show the measured round-trip time to each directly connected machine, labelled by player number. Guests see their link to the host.",
                                  0});
    // (the scene export: Port::SaveScene, SceneClip; scene.h)
    TheGame().pages.push_back({"Scene export", "Game",
                               "The picture taken apart: each of the board's layers by itself, as pictures with nothing where a "
                               "layer has nothing, for one frame or a few seconds."});
    TheGame().commands.push_back({"scene_save", "Scene export", "Save scene",
                                  "Saves the picture as it is now, and each of the board's layers by itself (its three scroll "
                                  "screens and its objects, which hold the fighters, the panel and parts of stages) with nothing "
                                  "where the layer has nothing, in a folder of its own in exports/scenes beside your saves, with a "
                                  "scene.json of the layers' order, back to front. A fight on another game's stage has that "
                                  "stage's layers too. Also Shift+F12.",
                                  nullptr});
    TheGame().commands.push_back({"scene_clip", "Scene export", "Record clip",
                                  "Saves the same for each frame from now, for as long as Clip length says, each layer's frames in "
                                  "a folder of their own (this row again, or Ctrl+F12, stops sooner). The game goes on as it does.",
                                  nullptr});
    TheGame().features.push_back({"scene_clip_length", "Scene export", "Clip length", "How long Record clip records for.", 0,
                                  {"3 seconds", "5 seconds", "10 seconds"}});
    TheGame().commands.push_back({"scene_show_folder", "Scene export", "Show scenes folder",
                                  "Opens exports/scenes beside your saves, where Save scene and Record clip write, in your file "
                                  "manager.",
                                  nullptr});
    // (SVG beside the PNGs: issue #368, model/image_files.h's WriteSvg)
    TheGame().features.push_back({"export_svg", "Game", "SVG pictures too",
                                  "Screenshots (F12), Save scene, the stage viewer's Save whole stage and the model viewer's "
                                  "saves also write SVG pictures beside their PNGs: each pixel a square, so they scale to "
                                  "any size exactly. A scene's and a whole stage's has each layer as a layer of its own "
                                  "(scene.svg, parts.svg, stage.svg), for drawing programs. Not for clips.",
                                  0});
    // (the video recording: Port::RecordVideo, video_record.h)
    TheGame().pages.push_back({"Video", "Game", "What you play and hear, recorded as an MP4 video that plays anywhere."});
    TheGame().commands.push_back({"video_record", "Video", "Record video",
                                  "Records the game as you play it, with its sound (the custom soundtrack's too), as an MP4 "
                                  "video in videos beside your saves, until this row again (Stop recording) or Alt+F12. "
                                  "The picture is as screenshots take it, readouts and all, without the lines at the bottom; "
                                  "the video's time is the game's: stopped or in this menu nothing is recorded, and slowed "
                                  "or sped up it plays at the game's own speed.",
                                  nullptr});
    TheGame().features.push_back({"video_size", "Video", "Video size",
                                  "How large Record video makes the picture: so many times the board's lines, each of its "
                                  "pixels a sharp block, in the shape the Display page shows it in.",
                                  0,
                                  {"3 times (672 lines)", "2 times (448 lines)", "4 times (896 lines)", "1080 lines"}});
    TheGame().commands.push_back({"video_replay_save", "Video", "Save replay",
                                  "Saves what was just played, as long as Replay length says, as a video in videos (as Record "
                                  "video's, its sound too): kept all the while, so it needs no recording started. Made beside "
                                  "the game in a few seconds, which goes on meanwhile. Also Alt+Shift+F12.",
                                  nullptr});
    TheGame().features.push_back({"video_replay", "Video", "Replay length",
                                  "How much of what was just played Save replay saves. Kept as a save state every second and "
                                  "the controls of every frame, so a minute of it is a few dozen megabytes of memory. Not "
                                  "online.",
                                  0,
                                  {"30 seconds", "60 seconds", "10 seconds", "Off"}});
    TheGame().commands.push_back({"video_show_folder", "Video", "Show videos folder",
                                  "Opens videos beside your saves, where Record video writes, in your file manager.", nullptr});
    TheGame().commands.push_back({"online_host", "Online", "Host a match",
                                  "Others can join you online while you play on as you like (modes, the menu, save states): "
                                  "the match's code is at the bottom of the screen to give them, and with your name from the "
                                  "launcher's Online page it's in the lobby too. When someone joins, the game starts over "
                                  "for the match. Choose Unlisted match to share the code privately. This row again stops hosting.",
                                  nullptr});
    // (names: shown unless hidden, whatever the preset, so a switch that hides them)
    TheGame().features.push_back({"online_hide_names", "Online", "Hide names",
                                  "During an online match, each player's name is under their life bar (the name each gave on "
                                  "the launcher's Online page), and under it a win streak of two or more: this hides them.",
                                  0});
    // (issue #363: under the life bars they can cover a game's second bars, Marvel vs. Capcom's partners)
    TheGame().features.push_back({"online_names_place", "Online", "Names shown",
                                  "Where the players' names and win streaks are during an online match: under each side's "
                                  "life bar, or along the very top of the picture, each side's on one line, out of the way "
                                  "of the bars and the score. In a fighting game's match of two the names are in the "
                                  "score's strip at the top either way.",
                                  0, {"Under the life bars", "At the top"}});
    TheGame().features.push_back({"online_delay", "Online", "Input delay",
                                  "During an online match, how many frames your own controls are held back, at the bottom of the "
                                  "picture (\"DELAY 2\"), and the other player's when theirs is not the same: each player chooses their own.",
                                  0});
    TheGame().features.push_back({"online_hide_streak", "Online", "Hide online win streak",
                                  "Hide the win streak under the player names on this screen.", 0});
    TheGame().features.push_back({"online_wait_input", "Online", "Input during network waits",
                                  "Retain short taps plays recent changes in order when frames resume. Current state uses only what you hold when a frame advances. This choice applies to your controllers.",
                                  0, {"Retain short taps", "Current state"}});
    TheGame().features.push_back({"online_host_delay", "Online", "Your input delay",
                                  "Frames your own controls wait before they count, in every match you host or join; the other player "
                                  "chooses theirs. Fewer feel quicker, but the other player sees more of your moves corrected on a "
                                  "slow connection. Use launcher setting follows the launcher's (0 unless chosen). From the next match.",
                                  0, {"Use launcher setting", "0 frames", "1 frame", "2 frames", "3 frames", "4 frames",
                                      "5 frames", "6 frames", "7 frames", "8 frames"}});
    TheGame().features.push_back({"online_unlisted", "Online", "Unlisted match",
                                  "Keep matches you host out of the public lobby, even with a player name. Share the code or "
                                  "host address with the people you want to join. Anyone who has the code can use it.", 0});
    // How many physical controllers at this machine will play in the match.
    if (std::any_of(TheGame().features.begin(), TheGame().features.end(), [](const Feature &f) { return !strcmp(f.name, tag::kFeature); })) {
        TheGame().features.push_back({"online_people", "Online", "People at this machine",
                                      "Choose 1 for your own controller, or 2 for two people sharing this machine in a tag match. "
                                      "The host sets the number of people on each team under Tag team. With three or four machines "
                                      "and one controller each, everyone chooses 1.",
                                      0, {"1", "2"}});
    }
    // (display_picture: the GPU's picture, --video gpu, unless the CPU's is chosen, --video native; docs/NATIVE_VIDEO.md; read
    // as the game starts. It was display_gpu, off by default, while the GPU's was a test.)
    TheGame().features.push_back({"display_picture", "Display", "Picture drawn by",
                                  "The GPU: the game's picture drawn on the graphics card from what the game's code hands over (its "
                                  "sprites, its scroll layers and palettes), the same picture pixel for pixel; frames it cannot draw "
                                  "yet (a shader preset, the modes that put two pictures together) are drawn by the CPU, and so is "
                                  "everything on a machine whose graphics card cannot be used. The CPU: every frame drawn by the "
                                  "processor, as before. Takes effect when the game starts.",
                                  0, {"The GPU", "The CPU"}});
    // (display_vsync: Presenter::SetVsync, issue #185)
    TheGame().features.push_back({"display_vsync", "Display", "VSync",
                                  "Each picture shown when the screen refreshes, so the picture never tears. The game keeps the "
                                  "board's pace (59.64 frames a second), so now and then a frame stays up for one more of the "
                                  "screen's refreshes (on a 60 Hz screen, about every three seconds), and the controls can be a "
                                  "frame later; on a screen slower than 60 Hz the game slows to it. Off: each picture shown as soon "
                                  "as it's made.",
                                  0});
    TheGame().features.push_back({"window_size", "Display", "Window size",
                                  "How large the window opens, in times the board's 224 lines (it can be made any size by "
                                  "dragging its edges; full screen is on the launcher's first page and F11).",
                                  0, {"3x", "2x", "4x", "5x", "6x"}});
    // (crt_*: an arcade monitor. Resolution and Screen: frontend_sdl.cpp, set on the launcher (kFeatureLauncherOnly), so
    // a mode the monitor may not take is never chosen in the menu while playing; Compact menu: recomp-common's
    // Menu::SetCompact)
    TheGame().features.push_back({"crt_resolution", "Arcade monitor", "Full screen resolution",
                                  "For an arcade monitor (CRT Emudriver or a custom resolution). 384 x 224: full screen switches to "
                                  "the board's own resolution, pixel for pixel. Super resolution: to one of its 224 lines and "
                                  "1920 or more columns (2560 x 224, say), the picture stretched across; with none, one of 240 "
                                  "lines (2560 x 240), the 224 in its middle. Only such a mode near "
                                  "59.64 Hz is used; if there is none, full screen stays as it was. Set on the launcher.",
                                  kFeatureLauncherOnly, {"Off", "384 x 224", "Super resolution"}});
    TheGame().features.push_back({"crt_screen", "Arcade monitor", "Screen",
                                  "Which screen the game opens on and goes full screen on, where there are several: the main "
                                  "one or another, in the order the system lists them. One that isn't connected: the main. "
                                  "Set on the launcher; the in-game menu can only put it back to the main one.",
                                  kFeatureLauncherOnly, {"Main", "Second", "Third", "Fourth"}});
    // (crt_switchres, crt_monitor: frontend_sdl.cpp's Arcade(), recomp-common's arcade_screen.h; only in a build with
    // Switchres, Windows. Set on the launcher, as the resolution is: the monitor's ranges decide what modes it makes)
    if (recomp::sdl::SwitchresBuilt()) {
        TheGame().features.push_back({"crt_switchres", "Arcade monitor", "Make the mode at the board's rate",
                                      "With 384 x 224 or Super resolution: full screen makes that mode at exactly the board's "
                                      "59.637 Hz with Switchres (CRT Emudriver, as GroovyMAME does) and switches the screen to it, "
                                      "so a game on the board's pace never shows a frame twice, and synced to the screen it plays "
                                      "at the board's own speed. Back to the desktop's mode when the game ends. If it can't, a mode "
                                      "the system lists, as without it. Set on the launcher.",
                                      kFeatureLauncherOnly});
        TheGame().features.push_back({"crt_monitor", "Arcade monitor", "Monitor",
                                      "The monitor whose ranges Switchres keeps to in making a mode (its presets): Arcade 15 kHz "
                                      "for a standard-resolution arcade monitor; Generic 15 kHz for a TV or a consumer CRT; NTSC "
                                      "and PAL TVs; the 15/25 kHz and 15/25/31 kHz tri-sync arcade monitors; 31 kHz VGA arcade "
                                      "monitors. Choose the one the monitor is: a mode outside its ranges can harm it. Set on the "
                                      "launcher.",
                                      kFeatureLauncherOnly,
                                      {"Arcade 15 kHz", "Generic 15 kHz", "NTSC TV", "PAL TV", "Arcade 15/25 kHz", "Arcade 15/25/31 kHz",
                                       "Arcade 31 kHz"}});
    }
    // (crt_board_pace: frontend_sdl.cpp. Off, the default: in an arcade mode the frames are paced by the screen's refresh,
    // as a mode the system lists is seldom exactly the board's rate; on: by the board's clock, as in a window)
    TheGame().features.push_back({"crt_board_pace", "Arcade monitor", "Keep the board's pace",
                                  "In full screen at an arcade resolution the game is synced to the screen: one frame for each "
                                  "of its refreshes, each shown as it refreshes, so a mode that isn't exactly the board's 59.64 Hz "
                                  "(most run at 59.98 or 60) never shows a frame twice or tears; the game then runs at the "
                                  "screen's rate (0.6% faster at 60 Hz, too little to notice). On: the board's own clock, each "
                                  "picture shown as soon as it's made, as GroovyMAME does with a mode made at the exact rate. "
                                  "Online, always the board's pace.",
                                  0});
    TheGame().features.push_back({"crt_compact_menu", "Arcade monitor", "Compact menu",
                                  "The in-game menu in small letters with its rows close together, as suits a screen of few "
                                  "lines (a small window, an arcade monitor): a page's rows all shown at once, none running "
                                  "into the next. Off: compact only in full screen at an arcade resolution.",
                                  0});
    // (sound_filter: port/sound_filter.h, in the frontend; only what is heard, so it is each player's own online)
    TheGame().features.push_back({"sound_filter", "Sound", "Sound filter",
                                  "Softens the board's sound: the sound chip's samples are played without smoothing, which "
                                  "leaves a hiss and grit in the treble that headphones and good speakers bring out (the "
                                  "board's own amplifier took some of it away). Soft takes off the harshest of it; Softer, "
                                  "more, for a warmer sound. Off: the sound as the chip makes it.",
                                  0, {"Off", "Soft", "Softer"}});
    // (custom_soundtrack: soundtrack.h; for a game whose sources say where its sound program's songs are)
    if (TheGame().songCount) {
        TheGame().features.push_back({"custom_soundtrack", "Sound", "Custom soundtrack",
                                      "Plays your own music files in place of the board's music; the sound effects and voices are "
                                      "the board's. Files named for the game's songs (songs.txt in the folder lists them) replace "
                                      "those songs; a folder of any other music plays as a playlist, one file after another. The "
                                      "folder is chosen on the launcher's game page. Shuffled: the playlist in a random order.",
                                      kFeatureEnhanced, {"Off", "On", "On, shuffled"}});
    }
}

Scaling DisplayScaling() {
    return static_cast<Scaling>(std::clamp(FeatureValue("display_scaling"), 0, 2));
}
Shape DisplayShape() {
    return static_cast<Shape>(std::clamp(FeatureValue("display_shape"), 0, 2));
}
bool Turned() {
    // (not on an arcade monitor: the picture as the board sends it, the monitor turned as a cabinet's is)
    return TheGame().vertical && FeatureValue("display_turn") == 0 && ArcadeResolution() == ArcadeMode::Off;
}
ArcadeMode ArcadeResolution() {
    return static_cast<ArcadeMode>(std::clamp(FeatureValue("crt_resolution"), 0, 2));
}
int DisplayScreen() {
    return std::clamp(FeatureValue("crt_screen"), 0, 3);
}
bool SwitchresChosen() {
    return recomp::sdl::SwitchresBuilt() && FeatureValue("crt_switchres") != 0;
}
int ArcadeMonitor() {
    return std::clamp(FeatureValue("crt_monitor"), 0, recomp::sdl::kMonitorPresetCount - 1);
}
bool ArcadeSyncChosen() {
    return FeatureValue("crt_board_pace") == 0;
}
bool CompactMenuChosen() {
    return FeatureValue("crt_compact_menu") != 0;
}
void TurnPicture(const uint32_t *pixels, int across, std::vector<uint32_t> &out) {
    // (a quarter to the left: the picture's right edge becomes its top)
    const int rows = Video::kHeight;
    out.resize(static_cast<size_t>(across) * rows);
    for (int y = 0; y < rows; y++) {
        const uint32_t *from = pixels + static_cast<size_t>(y) * across;
        for (int x = 0; x < across; x++) {
            out[static_cast<size_t>(across - 1 - x) * rows + y] = from[x];
        }
    }
}
int WindowSize() {
    static const int kTimes[] = {3, 2, 4, 5, 6};
    return kTimes[std::clamp(FeatureValue("window_size"), 0, 4)];
}

#ifdef GAME_REGION_AT
namespace {
    // Where the region is in the code, not the header (game.cfg's region_at: an instruction `move.w #region,d16(a5)`):
    // the translated code has the game's own set's region in it, so once the instruction is past, its variable is
    // given the one the program as it is now has there (another region's set, or online the host's: Netplay)
    uint32_t g_regionAt = 0;
    bool AfterRegion(M68K &c) {
        const uint8_t *op = ProgramOpcodes(g_regionAt);
        const int16_t variable = static_cast<int16_t>((op[4] << 8) | op[5]);
        SetRam16((c.a[5] + variable) & 0xFFFFFF, static_cast<uint16_t>((op[2] << 8) | op[3]));
        return false;
    }
    void RegisterRegion(uint32_t at) {
        const uint8_t *op = ProgramOpcodes(at);
        if (((op[0] << 8) | op[1]) != 0x3B7C) {
            fprintf(stderr, "region_at %06X: not move.w #region,d16(a5)\n", at);
            return;
        }
        g_regionAt = at;
        AddHook(at + 6, "Region", AfterRegion);
    }
} // namespace
#endif

bool Port::Init(std::string &error) {
#ifndef GAME_BOARD
#define GAME_BOARD ""
#endif
#ifndef GAME_CPS_B
#define GAME_CPS_B ""
#endif
#ifndef GAME_KEY
#define GAME_KEY ""
#endif
#ifndef GAME_OTHER_SET
#define GAME_OTHER_SET ""
#endif
#ifndef GAME_GFX_MAP
#define GAME_GFX_MAP ""
#endif
    RomList list = ParseRomList(GAME_PROGRAM, GAME_GRAPHICS, GAME_SOUND_PROGRAM, GAME_SAMPLES, GAME_KEY, GAME_ROM_SET, GAME_ROM_PARENT,
                                GAME_BOARD, GAME_CPS_B, GAME_OTHER_SET);
    if (!ParseGfxMap(GAME_GFX_MAP, list.board)) {
        error = "game.cfg's gfx_map cannot be read";
        return false;
    }
#ifdef GAME_CPU_CLOCK
    char* clockEnd = nullptr;
    list.board.cpuClock = static_cast<int>(strtol(GAME_CPU_CLOCK, &clockEnd, 10));
    if (!*GAME_CPU_CLOCK || *clockEnd || (list.board.cpuClock != 10 && list.board.cpuClock != 12)) {
        error = "game.cfg's cpu_clock is 10 or 12 (MHz)";
        return false;
    }
#endif
    if (opt.cpuClock) { // (--cpu-clock: another board's 68000, for comparing with a model of the board that has it)
        list.board.cpuClock = opt.cpuClock;
    }
#ifdef GAME_DIP
    {
        unsigned a, b, c;
        if (sscanf(GAME_DIP, "%x %x %x", &a, &b, &c) != 3 || a > 0xFF || b > 0xFF || c > 0xFF) {
            error = "game.cfg's dip is three hex bytes (banks A, B and C)";
            return false;
        }
        list.board.dip[0] = static_cast<uint8_t>(a);
        list.board.dip[1] = static_cast<uint8_t>(b);
        list.board.dip[2] = static_cast<uint8_t>(c);
    }
#endif
    if (!LoadRomSet(opt.romPath, list, roms, error)) {
        return false;
    }
    if (!roms.name.empty()) {
        fprintf(stderr, "%s: the set %s (%s)\n", GAME_NAME, roms.set.c_str(), roms.name.c_str());
    }
#ifdef GAME_EXTRA_SET
    // Another set the game reads (another game made from the same program, say): from --rom2, or beside --rom
    {
        const RomList extra = ParseRomList(GAME_EXTRA_PROGRAM, GAME_EXTRA_GRAPHICS, GAME_EXTRA_SOUND_PROGRAM, GAME_EXTRA_SAMPLES,
                                           GAME_EXTRA_KEY, GAME_EXTRA_SET, GAME_EXTRA_SET);
        std::string path = opt.extraRomPath;
        if (path.empty()) {
            std::string folder = opt.romPath;
            while (!folder.empty() && folder.back() == '/') {
                folder.pop_back();
            }
            const size_t slash = folder.find_last_of('/');
            folder = slash == std::string::npos ? "." : folder.substr(0, slash);
            path = folder + "/" + GAME_EXTRA_SET;
        }
        std::string extraError;
        if (!LoadRomSet(path, extra, extraRoms, extraError) && !LoadRomSet(path + ".zip", extra, extraRoms, extraError)) {
            error = std::string("the ROM set ") + GAME_EXTRA_SET + " is needed too: " + extraError;
            return false;
        }
    }
#endif
#ifdef GAME_GUEST_TABLE
    // Boards of other games run beside this one (runtime/guest.h): each from --guest ID=PATH, or beside --rom (by the
    // game's id, or its set's name). None is needed: without one, the game's sources leave out what they would show of it.
    {
        std::string folder = opt.romPath;
        while (!folder.empty() && folder.back() == '/') {
            folder.pop_back();
        }
        const size_t slash = folder.find_last_of('/');
        folder = slash == std::string::npos ? "." : folder.substr(0, slash);
        static const GuestGame kGuests[] = GAME_GUEST_TABLE;
        for (const GuestGame &guest : kGuests) {
            const auto given = opt.guestRomPaths.find(guest.id);
            std::string guestError;
            const bool loaded = given != opt.guestRomPaths.end()
                                    ? LoadGuest(guest, given->second, guestError)
                                    : LoadGuest(guest, folder + "/" + guest.id, guestError) || LoadGuest(guest, folder + "/" + guest.set, guestError);
            if (loaded) {
                fprintf(stderr, "%s: the set %s too (%s)\n", GAME_NAME, guest.set, guest.name);
#ifdef GAME_GUEST_VIDEO
                // (its board's own records: its game's instructions that write the video's registers, watched)
                struct GuestVideo {
                    const char *id, *writes;
                };
                static const GuestVideo kVideo[] = GAME_GUEST_VIDEO;
                for (const GuestVideo &v : kVideo) {
                    Machine *board = cps2recomp::Guest(guest.id);
                    if (board && std::string(v.id) == guest.id) {
                        RegisterGuestVideoLifts(guest.id, v.writes, board->cpu.opcodes, board->cpu.romSize);
                        board->nativeVideo.checkName = std::string("video check, ") + guest.id;
                    }
                }
#endif
            } else {
                fprintf(stderr, "%s: without %s (%s): %s\n", GAME_NAME, guest.set, guest.name, guestError.c_str());
            }
        }
    }
#endif
    machine = std::make_unique<Machine>(roms);
    machine->cpuSpeed = opt.cpuSpeed > 0 ? opt.cpuSpeed : 1;
    if (!opt.ramLog.empty()) {
        machine->ramLog = OpenFile(opt.ramLog, L"wb", "wb");
        if (!machine->ramLog) {
            error = "cannot write " + opt.ramLog;
            return false;
        }
    }
    SetRam(machine->ram.data(), machine->videoRam.data());
    SetProgram(roms.program.data(), static_cast<uint32_t>(roms.program.size()));
    SetProgramOpcodes(roms.opcodes.data());
#ifdef GAME_EXTRA_SET
    {
        const uint32_t base = static_cast<uint32_t>(strtoul(GAME_EXTRA_BASE, nullptr, 16));
        machine->SetExtraProgram(&extraRoms, base);
        SetExtraProgram(extraRoms.program.data(), extraRoms.opcodes.data(), static_cast<uint32_t>(extraRoms.program.size()), base);
        SetExtraSound(machine->extraSound.get());
    }
#endif
    SetEeprom(&machine->eeprom);
    RegisterGame();
#ifdef GAME_REGION_AT
    RegisterRegion(static_cast<uint32_t>(strtoul(GAME_REGION_AT, nullptr, 16)));
#endif
    machine->cpu.pointerFrom = PointerLoads(machine->cpu.extraLow, machine->cpu.extraHigh);
    machine->cpu.keepHistory = getenv("CPS2RECOMP_PC_HISTORY") != nullptr;
    std::string definitionError;
    if (!ReadDeclaredFeatures(definitionError)) {
        fprintf(stderr, "%s\n", definitionError.c_str());
        error = definitionError;
        return false;
    }
    RegisterPortFeatures();
    LoadFeatures(opt.settingsPath, opt.features);
    // (during an online match, the menu leaves alone what the match decides for both players: Netplay::MatchDecides)
    OnlinePlayFrom([this] { return net != nullptr; });
    menu.matchDecides = [this](size_t feature) {
        return net && !connecting && feature < TheGame().features.size() && Netplay::MatchDecides(TheGame().features[feature]);
    };
    // The 68000 and the Z80 are run the same way; each falls back to its interpreter if the build has no
    // translated code for it
    if (opt.cpu.empty()) {
        opt.cpu = HasNativeCode() || HasZ80Code() ? "native" : "interp";
    }
    if (opt.cpu == "native") {
        machine->cpuMode = M68K::Mode::Native;
        machine->soundMode = Z80::Mode::Native;
    } else if (opt.cpu == "blocks") {
        machine->cpuMode = M68K::Mode::Blocks;
        machine->soundMode = Z80::Mode::Blocks;
    } else if (opt.cpu == "interp") {
        machine->cpuMode = M68K::Mode::Interpreter;
        machine->soundMode = Z80::Mode::Interpreter;
    } else if (opt.cpu == "check") {
        machine->cpuMode = M68K::Mode::Check;
        machine->soundMode = Z80::Mode::Check;
    } else {
        error = "--cpu: native, blocks, interp or check";
        return false;
    }
    // The sound program as ordinary code where there is one for it, unless the Z80's code is what is being
    // looked at
    if (opt.sound.empty()) {
        opt.sound = machine->soundDriver && opt.cpu == "native" && opt.recordEntries.empty() ? "driver" : "z80";
    }
    if (opt.sound == "z80") {
        machine->soundSide = Machine::SoundSide::Z80;
    } else if ((opt.sound == "driver" || opt.sound == "check") && machine->soundDriver) {
        machine->soundSide = opt.sound == "driver" ? Machine::SoundSide::Driver : Machine::SoundSide::Check;
    } else {
        error = machine->soundDriver ? "--sound: driver, z80 or check" : "--sound: this game's sound program is not one there is a driver for";
        return false;
    }
    machine->soundCpu.recording = !opt.recordEntries.empty();
    SetRewrites(opt.checkRewrites, opt.noRewrites);
    // The picture at the game's level (docs/NATIVE_VIDEO.md): the shared rewrites of Capcom's video library
    if (opt.video == "check" || opt.video == "gpu-check") {
        machine->nativeVideo.checking = true;
#ifdef GAME_GUEST_TABLE
        // (the guests' boards' records checked too)
        static const GuestGame kCheckedGuests[] = GAME_GUEST_TABLE;
        for (const GuestGame &guest : kCheckedGuests) {
            if (Machine *board = cps2recomp::Guest(guest.id)) {
                board->nativeVideo.checking = true;
            }
        }
#endif
    }
    // The machine's picture drawn from the game's side's records, a line at a time, not by video.cpp, unless the board's
    // is asked for (--video board) or the checks compare the two
    // (the GPU's picture unless the CPU's is asked for: --video native, cpu or board, or the Display page's choice)
    if (opt.video == "cpu") {
        opt.video = "native";
    }
    const bool gpu = opt.video == "gpu" || (opt.video.empty() && FeatureValue("display_picture") == 0);
    if (opt.video == "native" || gpu || opt.video.empty()) {
        machine->nativeVideo.recording = true;
        machine->nativeVideo.drawsPicture = true;
#ifdef GAME_GUEST_TABLE
        // (the guests' boards' pictures too, from their own records)
        static const GuestGame kNativeGuests[] = GAME_GUEST_TABLE;
        for (const GuestGame &guest : kNativeGuests) {
            if (Machine *board = cps2recomp::Guest(guest.id)) {
                board->nativeVideo.recording = true;
                board->nativeVideo.drawsPicture = true;
            }
        }
#endif
    }
    if (gpu) {
        // (the window's picture drawn on the GPU from the native records, the frames the GPU draws: frontend_sdl.cpp)
        machine->nativeVideo.recording = true;
        gpuWanted = true;
        machine->nativeVideo.frameDrawn = [](const NativeVideo::FrameData &frame, void *user) {
            static_cast<port::GpuPicture *>(user)->Keep(frame);
        };
        machine->nativeVideo.frameUser = &gpuPicture;
        gpuPicture.guestBehind = &machine->guestBehind;
        gpuPicture.guestShift = &machine->guestShift;
#ifdef GAME_GUEST_TABLE
        // (the guests' boards' frames too, for their stages drawn under the game's objects)
        static const GuestGame kDrawnGuests[] = GAME_GUEST_TABLE;
        for (const GuestGame &guest : kDrawnGuests) {
            if (Machine *board = cps2recomp::Guest(guest.id)) {
                GuestPicture g;
                g.picture = std::make_unique<GpuPicture>();
                g.roms = cps2recomp::GuestRoms(guest.id);
                board->nativeVideo.recording = true;
                board->nativeVideo.frameDrawn = [](const NativeVideo::FrameData &frame, void *user) {
                    static_cast<port::GpuPicture *>(user)->Keep(frame);
                };
                board->nativeVideo.frameUser = g.picture.get();
                gpuPicture.AddGuest(g.picture.get());
                guestPictures.push_back(std::move(g));
            }
        }
#endif
    }
    if (!opt.video.empty() && opt.video != "board" && opt.video != "native" && opt.video != "gpu" && opt.video != "check" &&
        opt.video != "gpu-check") {
        error = "--video: gpu, native (or cpu), board, check or gpu-check";
        return false;
    }
    RegisterVideoLifts();
    if (opt.video == "gpu-check") {
        // (each frame drawn on the GPU as well, on a device of the check's own, no window: port/gpu_picture.h)
        static port::GpuPicture checkPicture;
        std::string gpuError;
        if (!checkPicture.Open(nullptr, roms.graphics.data(), roms.graphics.size(), gpuError) ||
            (!extraRoms.graphics.empty() && !checkPicture.SetExtraGraphics(extraRoms.graphics.data(), extraRoms.graphics.size(), gpuError))) {
            error = "--video gpu-check: " + gpuError;
            return false;
        }
        g_gpuPicture = &checkPicture;
        checkPicture.guestBehind = &machine->guestBehind;
        checkPicture.guestShift = &machine->guestShift;
#ifdef GAME_GUEST_TABLE
        static const GuestGame kCheckedGpuGuests[] = GAME_GUEST_TABLE;
        for (const GuestGame &guest : kCheckedGpuGuests) {
            Machine *board = cps2recomp::Guest(guest.id);
            const RomSet *guestRoms = cps2recomp::GuestRoms(guest.id);
            if (!board || !guestRoms) {
                continue;
            }
            GuestPicture g;
            g.picture = std::make_unique<GpuPicture>();
            g.roms = guestRoms;
            if (!g.picture->Open(checkPicture.Device(), guestRoms->graphics.data(), guestRoms->graphics.size(), gpuError)) {
                error = std::string("--video gpu-check: ") + guest.id + ": " + gpuError;
                return false;
            }
            g.picture->checkName = std::string("video gpu-check, ") + guest.id;
            board->nativeVideo.frameDrawn = [](const NativeVideo::FrameData &frame, void *user) {
                static_cast<port::GpuPicture *>(user)->Check(frame);
                static_cast<port::GpuPicture *>(user)->Keep(frame);
            };
            board->nativeVideo.frameUser = g.picture.get();
            checkPicture.AddGuest(g.picture.get());
            guestPictures.push_back(std::move(g));
        }
#endif
        machine->nativeVideo.frameDrawn = [](const NativeVideo::FrameData &frame, void *user) {
            static_cast<port::GpuPicture *>(user)->Check(frame);
            static_cast<port::GpuPicture *>(user)->Keep(frame);
        };
        machine->nativeVideo.frameUser = &checkPicture;
    }
    machine->cpu.watchLow = opt.watchLow;
    machine->cpu.watchHigh = opt.watchHigh;
    machine->cpu.recording = !opt.recordEntries.empty();
    cps2recomp::SetPlayerFolder(PlayerFolder());
    if (!opt.eepromPath.empty()) {
        std::vector<uint8_t> data;
        if (ReadFile(opt.eepromPath, data) && data.size() == sizeof(machine->eeprom.words)) {
            memcpy(machine->eeprom.words, data.data(), data.size());
        }
    }
    if (!opt.moviePlay.empty() && !playMovie.Load(opt.moviePlay)) {
        error = "cannot read the movie " + opt.moviePlay;
        return false;
    }
    if (!opt.hashLog.empty()) {
        hashFile = fopen(opt.hashLog.c_str(), "w");
    }
    autoState = opt.autoSeed * 0x9E3779B97F4A7C15ull + 0x1234567ull;
    rollbackTest.random = opt.rollbackTest * 0x9E3779B97F4A7C15ull + 0x89ABCDEull;
    if (TheGame().songCount) {
        machine->soundtrack.FindMusic(roms.soundProgram, TheGame().songTable, TheGame().songCount, TheGame().musicBelow,
                                            TheGame().songAddresses, TheGame().songEntry);
    }
    SoundtrackFollow();
    machine->SetBoardTiming(BoardTimingWanted());
    return true;
}

// The custom soundtrack's folder: the one the launcher chose, or "soundtrack" beside settings.ini
// The Display page's Shader's presets, in the order of its choices after Off (the folders and files of RetroArch's
// slang-shaders, copied to shaders/ beside the program by tools/shaders.py), and before them what Your own is
namespace {
    const char *const kShaderPresets[] = {
        "crt/crt-geom-mini.slangp", "crt/crt-lottes.slangp", "crt/crt-royale.slangp", "crt/crt-guest-advanced-fast.slangp",
        "crt/zfast-crt.slangp", "scanlines/scanline-fract.slangp", "pixel-art-scaling/sharp-bilinear-scanlines.slangp",
        "scanlines/ossc.slangp", "scanlines/integer-scaling-scanlines.slangp",
        "edge-smoothing/xbrz/xbrz-freescale-multipass.slangp", "edge-smoothing/scalefx/scalefx.slangp",
    };
    constexpr int kYourOwnShader = 1 + static_cast<int>(std::size(kShaderPresets));
} // namespace

std::string Port::ShaderPreset() const {
    if (!opt.shader.empty()) {
        return opt.shader;
    }
    const int value = FeatureValue("display_shader");
    std::error_code error;
    if (value == kYourOwnShader) {
        // (the first .slangp, by name, in shaders/ beside settings.ini)
        if (opt.settingsPath.empty()) {
            return "";
        }
        std::vector<std::filesystem::path> found;
        for (const auto &entry : std::filesystem::directory_iterator(TextPath(opt.settingsPath).parent_path() / "shaders", error)) {
            if (entry.path().extension() == ".slangp") {
                found.push_back(entry.path());
            }
        }
        std::sort(found.begin(), found.end());
        return found.empty() ? "" : PathText(found.front());
    }
    if (value < 1 || value >= kYourOwnShader) {
        return "";
    }
    // (beside the program, or in a macOS app its Resources; CPS2RECOMP_SHADERS for another folder)
    if (const char *folder = getenv("CPS2RECOMP_SHADERS")) {
        return PathText(TextPath(folder) / kShaderPresets[value - 1]);
    }
    const char *base = SDL_GetBasePath();
    const std::filesystem::path program = base ? TextPath(base) : std::filesystem::path(".");
    for (const std::filesystem::path &at : {program / "shaders", program / ".." / "Resources" / "shaders"}) {
        if (std::filesystem::exists(at / kShaderPresets[value - 1], error)) {
            return PathText(at / kShaderPresets[value - 1]);
        }
    }
    return PathText(program / "shaders" / kShaderPresets[value - 1]);
}

std::string Port::SoundtrackFolder() const {
    if (!opt.soundtrack.empty()) {
        return opt.soundtrack;
    }
    if (opt.settingsPath.empty()) {
        return "";
    }
    return PathText(TextPath(opt.settingsPath).parent_path() / "soundtrack");
}

void Port::SoundtrackFollow() {
    const int value = TheGame().songCount ? FeatureValue("custom_soundtrack") : 0;
    const bool on = value > 0;
    if (value == soundtrackValue) {
        return;
    }
    soundtrackValue = value;
    soundtrackOn = on;
    if (!on) {
        machine->soundtrack.Replace({});
        machine->soundtrack.state.song = -1; // (the board's music again from the next piece the game asks for)
        return;
    }
    const std::string folder = SoundtrackFolder();
    if (folder.empty()) {
        return;
    }
    // (the folder that is the game's own is made, with the list of the songs' names in it)
    std::error_code error;
    const std::filesystem::path dir = TextPath(folder);
    if (opt.soundtrack.empty()) {
        std::filesystem::create_directories(dir, error);
    }
    if (std::filesystem::is_directory(dir, error)) {
        const std::string list = "The songs of " GAME_NAME " a custom soundtrack can play: name the file as the first column (or\n"
                                 "the number, the second), with .ogg, .flac, .mp3 or .wav; or name it in soundtrack.ini.\n"
                                 "(With no file named for a song, the folder's files play as a playlist instead.)\n\n" +
                                 SongList(machine->soundtrack);
        std::vector<uint8_t> have;
        if (!ReadFile(PathText(dir / "songs.txt"), have) || std::string(have.begin(), have.end()) != list) {
            WriteFile(PathText(dir / "songs.txt"), list.data(), list.size());
        }
    }
    const int found = soundtrack.Open(folder, machine->soundtrack, value == 2);
    if (opt.stats || getenv("CPS2RECOMP_SOUNDTRACK_LOG")) {
        fprintf(stderr, "soundtrack: %d %s from %s\n", found, soundtrack.IsPlaylist() ? "files as a playlist" : "songs", folder.c_str());
    }
}

// What the game's state comes to, for comparing runs: the CPU, its memory, the video and object memory, the
// registers and the picture
uint32_t Port::StateHash() const {
    const Machine &m = *machine;
    uLong h = crc32(0, nullptr, 0);
    auto add = [&](const void *p, size_t size) { h = crc32(h, static_cast<const Bytef *>(p), static_cast<uInt>(size)); };
    if (opt.hashPicture) {
        add(m.Picture(), sizeof(uint32_t) * m.PictureWidth() * Video::kHeight);
        return static_cast<uint32_t>(h);
    }
    add(m.cpu.d, sizeof(m.cpu.d));
    add(m.cpu.a, sizeof(m.cpu.a));
    add(&m.cpu.pc, sizeof(m.cpu.pc));
    const uint16_t sr = m.cpu.SR();
    add(&sr, sizeof(sr));
    add(&m.cpu.cycles, sizeof(m.cpu.cycles));
    add(m.ram.data(), m.ram.size());
    add(m.videoRam.data(), m.videoRam.size());
    add(m.objectRam.data(), m.objectRam.size());
    if (net || opt.hashNoSound) {
        // (online: the sound side left out, as each player's custom soundtrack changes what it is asked, and the game
        // never reads back what it does)
    } else if (!opt.hashGame) {
        add(m.soundRam, sizeof(m.soundRam));
    } else {
        // (byte 5 is the sound program's note of the last place it gave a sound, which it writes when it
        // comes to it; the game does not read it)
        add(m.soundRam, 5);
        add(m.soundRam + 6, sizeof(m.soundRam) - 6);
    }
    if (!opt.hashGame && !net && !opt.hashNoSound) {
        add(m.soundRam2, sizeof(m.soundRam2));
        add(m.soundCpu.r, sizeof(m.soundCpu.r));
        add(m.soundCpu.r2, sizeof(m.soundCpu.r2));
        const uint16_t sound[] = {m.soundCpu.pc, m.soundCpu.sp, m.soundCpu.ix, m.soundCpu.iy, m.soundCpu.f, m.soundCpu.f2,
                                  m.soundCpu.refresh};
        add(sound, sizeof(sound));
        add(&m.soundCpu.cycles, sizeof(m.soundCpu.cycles));
    }
    add(m.cpsA, sizeof(m.cpsA));
    add(m.cpsB, sizeof(m.cpsB));
    add(m.objectRegs, sizeof(m.objectRegs));
    if (!opt.hashNoPicture) {
        add(m.Picture(), sizeof(uint32_t) * m.PictureWidth() * Video::kHeight);
    }
    return static_cast<uint32_t>(h);
}

void Port::MenuAct(Menu::Action action) {
    std::string error;
    const std::string path = StatePath(menu.Slot());
    switch (action) {
    case Menu::Action::None: return;
    case Menu::Action::Coin:
        coinFrames = 6;
        menu.Close();
        return;
    case Menu::Action::Start2:
        start2Frames = 6;
        menu.Close();
        return;
    case Menu::Action::Test:
        testFrames = 6;
        menu.Close();
        return;
    case Menu::Action::ResetOptions:
        ResetFeatures();
        menu.SetFullSpeed();
        menu.Note("Every option is back as it first was.");
        return;
    case Menu::Action::Reset:
        EndSessionFeatures();
        machine->Reset();
        menu.Close();
        return;
    case Menu::Action::Quit:
        KeepPicture(); // (before the menu goes: the game's picture, not the menu's)
        quit = true;
        menu.Close();
        return;
    case Menu::Action::Command:
        RunCommand(menu.Command());
        menu.Close();
        return;
    case Menu::Action::OnlineCommand:
        OnlineCommand(menu.Command());
        return;
    case Menu::Action::Leave:
        menu.Close();
        LeaveOnline();
        return;
    case Menu::Action::Save:
        if (SaveState(path, error)) {
            menu.Close();
            return;
        }
        break;
    case Menu::Action::Load:
        if (LoadState(path, error)) {
            menu.Close();
            return;
        }
        error = recomp::i18n::TrFormat("Nothing is kept in slot {}.", {std::to_string(menu.Slot())});
        break;
    }
    menu.Note(error);
}

void Port::RunCommand(size_t index) {
    const auto &commands = TheGame().commands;
    if (index < commands.size() && !strcmp(commands[index].name, "online_host")) {
        HostFromMenu();
        return;
    }
    if (index < commands.size() && !strcmp(commands[index].name, "scene_save")) {
        SaveScene();
        return;
    }
    if (index < commands.size() && !strcmp(commands[index].name, "scene_clip")) {
        SceneClip();
        return;
    }
    if (index < commands.size() && !strcmp(commands[index].name, "video_record")) {
        RecordVideo();
        return;
    }
    if (index < commands.size() && !strcmp(commands[index].name, "video_replay_save")) {
        SaveReplay();
        return;
    }
    if (index < commands.size() && !strcmp(commands[index].name, "video_show_folder")) {
        const std::string folder = PlayerFolder() + "videos";
        std::error_code error;
        std::filesystem::create_directories(std::filesystem::path(std::u8string(folder.begin(), folder.end())), error);
        OpenFolder(folder);
        return;
    }
    if (index < commands.size() && !strcmp(commands[index].name, "scene_show_folder")) {
        const std::string folder = PlayerFolder() + "exports/scenes";
        std::error_code error;
        std::filesystem::create_directories(std::filesystem::path(std::u8string(folder.begin(), folder.end())), error);
        OpenFolder(folder);
        return;
    }
    if (index < commands.size() && commands[index].run) {
        commands[index].run(*machine);
    }
}

void Port::Key(char key, unsigned n) {
    if (net && key == 'k' && n == kKeyRestart && session::InSession() && !net->Watching()) {
        // (online, in a training session: Start over for both, at the place this player's stick chooses)
        const int side = net->Player(net->Local(), 0);
        net->Command("restart=" + std::to_string(session::RestartPlace(machine->players[side] & fight::kSticks)));
        return;
    }
    if (net && key != 'z' && key != 'm' && key != 'l' && key != 'y') {
        return; // (online: only the leave question, the zoom's, which does nothing: Zoom::Forbid, and training's Start over)
    }
    switch (key) {
    case 'p':
        paused = !paused;
        steps = 0;
        break;
    case 'o': // (stops the game if it is running)
        if (paused) {
            steps++;
        }
        paused = true;
        break;
    case '-': menu.Faster(-1); break;
    case '=': menu.Faster(1); break;
    case 'h': hurry = !hurry; break; // (tests: the frontend sets it while F is held)
    case 'l': // (tests: online, Esc: the match's menu, or the question whether to leave)
        if (net && (menu.IsOpen() || leaving)) {
            menu.Close();
            leaving = false;
        } else if (net) {
            OpenOnlineMenu();
        }
        break;
    case 'y': // (tests: ... the match left)
        if (net && (leaving || menu.OnlineOpen())) {
            menu.Close();
            LeaveOnline();
        }
        break;
    case 'z': // (tests: the zoom pressed for player n + 1; 2: for both; 3: by distance, the menu's choice)
        if (n == Zoom::kDistance) {
            zoom.Choose(4);
        } else if (n == Zoom::kBoth) {
            zoom.Both();
        } else {
            zoom.Press(static_cast<int>(n));
        }
        break;
    case 'm': // (tests: the zoom's camera moved a step: 1 left, 2 right, 3 up, 4 down)
        zoom.Pan(n == 1 ? -1 : n == 2 ? 1 : 0, n == 3 ? -1 : n == 4 ? 1 : 0);
        break;
    case 'n':
        if (!MultiGame().empty() && NextGameAsked() < 0) {
            AskNextGame(static_cast<int64_t>(tick));
        }
        break;
    case ']': PracticeSet(); break;
    case 'w': rewindHeld = !rewindHeld; break; // (tests: the frontend sets it while R or the left trigger is held)
    case 'k':
        // (with a practice point set, Backspace and Backslash are its: port.h, Practice)
        if ((n == kKeyRestart && PracticeBack()) || (n == kKeyExtra && PracticeReplay())) {
            break;
        }
        for (size_t i = 0; i < TheGame().commands.size(); i++) {
            if (TheGame().commands[i].key == n && n != kKeyNone) {
                RunCommand(i);
            }
        }
        break;
    default: break;
    }
}

namespace {
constexpr uint64_t kNextGameWait = 150; // ticks (so stopped too)

// (hosting a player invited from the launcher's waiting room: whether they have been asked, or said no)
std::string InvitationLine(const Netplay &net) {
    return net.Invitation() == "declined" ? net.Invited() + " declined"
           : net.Invitation() == "away"   ? net.Invited() + " couldn't be invited"
                                          : "Invited " + net.Invited();
}

// (online) the side of the screen a board player (0-3) plays for, -1 for none: as tag team's mode has the people
// (fight/tag.h), or the board's own two
int ScreenSide(int player) {
    for (int side = 0; side < 2; side++) {
        for (int pick = 0; pick < 2; pick++) {
            if (tag::Person(side, pick) == player) {
                return side;
            }
        }
    }
    return -1;
}
} // namespace

bool Port::Tick() {
    if (ProgramEnded() >= 0) {
        quit = true; // (the game's sources are done: runtime/game.h, EndProgram)
        return false;
    }
    // (tests: --keys TICK:KEY,...: keys pressed at those ticks; a digit after 'k' is the command key's number)
    for (size_t at = 0; at < opt.keys.size();) {
        const size_t colon = opt.keys.find(':', at);
        if (colon == std::string::npos) {
            break;
        }
        if (strtoull(opt.keys.c_str() + at, nullptr, 10) == tick && colon + 1 < opt.keys.size()) {
            const char key = opt.keys[colon + 1];
            const size_t end = std::min(opt.keys.find(',', colon), opt.keys.size());
            if (key == 'c') { // (c and a command's name)
                const std::string name = opt.keys.substr(colon + 2, end - colon - 2);
                for (size_t i = 0; i < TheGame().commands.size(); i++) {
                    if (name == TheGame().commands[i].name) {
                        RunCommand(i);
                    }
                }
            } else if (key == 'f') { // (f and NAME=N: a feature set)
                const std::string what = opt.keys.substr(colon + 2, end - colon - 2);
                const size_t eq = what.find('=');
                if (eq != std::string::npos) {
                    SetFeatureValue(what.substr(0, eq).c_str(), atoi(what.c_str() + eq + 1));
                }
            } else {
                Key(key, key == 'k' || key == 'z' || key == 'm' ? static_cast<unsigned>(atoi(opt.keys.c_str() + colon + 2)) : 0);
            }
        }
        const size_t comma = opt.keys.find(',', colon);
        at = comma == std::string::npos ? opt.keys.size() : comma + 1;
    }
    tick++;
    // (headless, nothing draws the window's picture, where the notices at the bottom count down (Framed): here, so
    // that a notice goes as it would in the window)
    if (opt.headless && !opt.presentEvery && shotTicks > 0 && shotTicks <= kSpeedTicks) {
        shotTicks--;
    }
    if (opt.online && !net && !rejoin.empty()) {
        // (joining again: a moment between tries)
        SetHud({{0.5f, 0.36f, 0, recomp::Tr(rejoin)}, {0.5f, 0.48f, 0, "Joining again in a moment"}, {0.5f, 0.6f, 0, "Esc: quit"}});
        if (tick >= againAt) {
            Again(rejoin);
        }
        return false;
    }
    if (net && connecting) {
        // (the other player not here yet: said on the screen, as the readout, over the board's black picture)
        std::string error;
        const int state = net->Waiting(*this, error);
        if (state < 0) {
            if (!rejoin.empty()) {
                // (joining again: the host may not be back yet; tried every few seconds until Esc)
                fprintf(stderr, "online: %s\n", error.c_str());
                net.reset();
                againAt = tick + 180;
                return false;
            }
            if (hostedFromMenu) {
                SetHud({});
                BackToSolo(false);
                hostNote = recomp::i18n::TrFormat("Stopped hosting: {}", {recomp::Tr(error)});
                hostNoteUntil = tick + 300;
                return false;
            }
            net->ended = error;
            net->failed = true;
            quit = true;
            SetHud({});
            return false;
        }
        if (state == 0 && hostedFromMenu && !net->Found()) {
            // (the one who came went quiet before the match began: the player's own game again, still hosted)
            SetHud({});
            BackToSolo(true);
            return false;
        }
        if (state == 0 && net->Arcade()) {
            // (--net-arcade: the game played meanwhile, as offline but with the match's features, the wait said at
            // the bottom (Shown); until someone has come, who is then sent the game as it is, and it waits for them)
            SetHud({});
            if (net->Found() || leaving) {
                return false;
            }
            ArcadeFrame();
            return true;
        }
        if (state == 0) {
            // (short lines, each its own panel: one wider than the picture is not drawn; the dots padded so that the
            // line keeps its width; the code in green)
            static const char *const kDots[] = {"   ", ".  ", ".. ", "..."};
            std::vector<HudLine> lines;
            if (!rejoin.empty()) {
                lines.push_back({0.5f, 0, 0, recomp::Tr(rejoin)});
            }
            lines.push_back({0.5f, 0, 0, recomp::Tr(net->waiting) + kDots[(tick / 30) % 4]});
            if (!net->code.empty()) {
                lines.push_back({0.5f, 0, 0, recomp::i18n::TrFormat("Match code {}", {net->code}), 96, 230, 120});
                if (net->Listed()) {
                    lines.push_back({0.5f, 0, 0, "Listed in the lobby"});
                }
                if (!net->Invited().empty()) {
                    lines.push_back({0.5f, 0, 0, InvitationLine(*net)});
                }
            }
            lines.push_back({0.5f, 0, 0, rejoin.empty() ? "Esc: give up" : "Esc: quit"});
            for (size_t n = 0; n < lines.size(); n++) {
                lines[n].y = 0.5f + (static_cast<float>(n) - (lines.size() - 1) / 2.0f) * 0.1f;
            }
            SetHud(lines);
            return false;
        }
        SetHud({});
        Matched();
    }
    if (net) {
        // Online: a frame a tick when the other side's controls allow (and as fast as it goes while the game's
        // sources take it somewhere); no stopping, slowing or speeding up
        const bool ready = net->Ready(frame);
        HearChat();
        HearEvents();
        OnlineSetupFrame();
        if (!opt.netTyping.empty()) {
            typing = true;
            typed = opt.netTyping;
        }
        for (auto it = opt.netCommands.begin(); it != opt.netCommands.end();) {
            if (frame >= it->first) {
                net->Command(it->second);
                it = opt.netCommands.erase(it);
            } else {
                ++it;
            }
        }
        if (!opt.netSay.empty() && frame >= opt.netSayAt) {
            typed = opt.netSay;
            opt.netSay.clear();
            Say();
        }
        if (!ready) {
            if (!net->ended.empty()) {
                // (the other side gone: a window waits for them again; the host's leaving ends the joining side's)
                const bool again = !opt.headless && !net->Watching() && (net->Hosting() ? net->left || net->lost : net->lost);
                if (net->SetFinished()) {
                    quit = true;
                } else if (hostedFromMenu) {
                    BackToSolo(again); // (the other side gone: still hosted, the player's own game again)
                } else if (again) {
                    Again(net->ended);
                } else {
                    quit = true;
                }
            }
            return false;
        }
        RunFrame();
        for (int n = 0; n < 120 && FastForward() && net->Ready(frame); n++) {
            machine->audio.clear();
            RunFrame();
        }
        return true;
    }
    // (the next game asked for: the game's sources have had their time to end it their own way)
    if (NextGameAsked() >= 0 && tick >= static_cast<uint64_t>(NextGameAsked()) + kNextGameWait) {
        EndProgram(kNextGame);
    }
    // Rewind held (port.h, Rewind): a kept state back each tick, whatever the speed; not while the game's sources take
    // it somewhere, nor stopped (there the left trigger steps on)
    const bool rewinding = rewindHeld && RewindOn() && !paused && !FastForward() && !menu.IsOpen();
    if (rewinding != rewind.was) {
        rewind.was = rewinding;
        rewind.ticks = 0;
        if (!rewinding && shotSaid == recomp::Tr("REWINDING")) {
            shotTicks = 0; // (let go: the notice goes with it)
        }
    }
    if (rewinding) {
        rewind.ticks++;
        if (rewind.ticks == 1 || rewind.ticks % 30 == 0) {
            Said("REWINDING");
        }
        return RewindTick();
    }
    if (FastForward()) {
        // The game's sources are taking the game somewhere: as fast as it goes, a slice at a time
        // (no further than --frames asks)
        for (int n = 0; n < 120 && FastForward() && (opt.frames == 0 || frame < opt.frames); n++) {
            RunFrame();
            machine->audio.clear();
        }
        return true;
    }
    int frames = 1;
    if (paused) {
        if (steps == 0) {
            return false;
        }
        steps--;
    } else {
        const Menu::Rate rate = menu.GameRate();
        owed += rate.frames;
        frames = owed / rate.ticks;
        owed %= rate.ticks;
        if (frames == 0) {
            return false;
        }
    }
    if (hurry && !paused && !MultiGame().empty()) {
        // A Multi game run's fast forward: kHurry frames, unheard
        for (int n = 0; n < kHurry; n++) {
            RunFrame();
            machine->audio.clear();
        }
        return true;
    }
    // (faster than the board: those frames and all their sound, which the frontend plays faster by as much)
    for (int n = 0; n < frames; n++) {
        RunFrame();
    }
    return true;
}

void Port::Shown(std::vector<uint32_t> &pixels, std::vector<SDL_Rect> *panels, bool over, uint32_t base, bool boxes, bool notices) const {
    const int across = ShownWidth();
    if (over) {
        pixels.assign(static_cast<size_t>(across) * Video::kHeight, base);
    } else {
        pixels.assign(machine->Picture(), machine->Picture() + across * Video::kHeight);
    }
    // (a vertical game's picture turned upright, port::Turned: its boxes drawn on the board's picture, as they belong to the
    // game's, and the text upright on the picture turned, which is turned back at the end, to be turned again as it is shown)
    const bool turned = port::Turned();
    std::vector<uint32_t> upright;
    const unsigned boxParts = boxes ? +port::kHudBoxes : 0u;
    if (turned) {
        DrawHud(pixels.data(), across, Video::kHeight, nullptr, boxParts);
        port::TurnPicture(pixels.data(), across, upright);
    }
    uint32_t *const px = turned ? upright.data() : pixels.data();
    const int W = turned ? +Video::kHeight : across, H = turned ? across : +Video::kHeight;
    Overlays(px, W, H, panels, notices, turned ? +port::kHudText : boxParts | port::kHudText, net && !connecting && net->Silent() > 1000);
    if (turned) {
        // (back as the board draws it: a quarter to the right)
        for (int y = 0; y < Video::kHeight; y++) {
            for (int x = 0; x < across; x++) {
                pixels[static_cast<size_t>(y) * across + x] = upright[static_cast<size_t>(across - 1 - x) * Video::kHeight + y];
            }
        }
    }
}

void Port::Overlays(uint32_t *px, int W, int H, std::vector<SDL_Rect> *panels, bool notices, unsigned hudParts, bool waiting) const {
    const bool liveOnline = net && !connecting;
    const HudOpacity &onlineOpacity = kOnlineHudOpacity[liveOnline ? std::clamp(FeatureValue("online_hud_opacity"), 0, 2) : 0];
    DrawHud(px, W, H, panels, hudParts, liveOnline ? &onlineOpacity : nullptr);
    const int bottomInset = liveOnline ? TheGame().onlineBottomInset : 0;
    // A line above the native gauges, in the middle (up pixels higher).
    // (small: the smaller font, kSmallFont, in a lower box at the bottom of the line's place)
    auto status = [&](const char *text, int up, HudOpacity opacity = {220, 255}, bool small = false) {
        if (!notices) {
            return; // (a video's picture: Port::VideoFrame)
        }
        const recomp::text::Canvas canvas{px, W, H};
        const float width = small ? recomp::text::SmallTextWidth(text, 1) - 1 : recomp::text::TextWidth(text, 1);
        const float x = (W - width) / 2, y = static_cast<float>(H - 11 - bottomInset - up);
        if (small) {
            canvas.Fill(x - 2, y + 3, width + 4, 7, {20, 22, 30, opacity.panel});
            canvas.SmallText(x, y + 4, 1, {255, 255, 255, opacity.ink}, text);
            if (panels) {
                panels->push_back({static_cast<int>(x) - 2, static_cast<int>(y) + 3, static_cast<int>(width) + 5, 7});
            }
            return;
        }
        canvas.Fill(x - 3, y - 2, width + 6, 12, {20, 22, 30, opacity.panel});
        canvas.Text(x, y, 1, 0, {255, 255, 255, opacity.ink}, text);
        if (panels) {
            panels->push_back({static_cast<int>(x) - 3, static_cast<int>(y) - 2, static_cast<int>(width) + 7, 12});
        }
    };
    // Stopped or fast-forwarded: said at the bottom all the while (stopped, with the frame's number, for counting
    // frames by stepping); slowed or sped up, for a few seconds after the speed changes (speedTicks)
    const bool hurried = hurry && !paused && !MultiGame().empty();
    const bool told = paused || hurried || (!menu.FullSpeed() && speedTicks > 0);
    if (told) {
        char text[64];
        if (paused) {
            snprintf(text, sizeof(text), "STOPPED  frame %llu", static_cast<unsigned long long>(frame));
        } else if (hurried) {
            snprintf(text, sizeof(text), "FAST FORWARD x%d", kHurry);
        } else {
            const Menu::Rate rate = menu.GameRate();
            if (rate.frames < rate.ticks) {
                snprintf(text, sizeof(text), "SPEED %d/%d", rate.frames, rate.ticks);
            } else {
                snprintf(text, sizeof(text), "SPEED x%g", static_cast<double>(rate.frames) / rate.ticks);
            }
        }
        status(text, 0);
    }
    // (online: the ping and the input delay, as the Display page has them, or that the other side has gone quiet; and
    // above, until this side's player has pressed Start, that it lets them in)
    int said = told ? 1 : 0;
    if (const std::string line = LagLine(); !line.empty()) {
        status(line.c_str(), said * 14, {220, 255}, true);
        said++;
    }
    // (hosting from the in-game menu, the game the player's own meanwhile: the match's code; or why it stopped)
    if (waitingHost) {
        const std::string code = waitingHost->code;
        status(code.empty() ? "HOSTING" : ("HOSTING  CODE " + code + (waitingHost->Listed() ? "  IN THE LOBBY" : "")).c_str(), said++ * 14);
    } else if (tick < hostNoteUntil) {
        status(hostNote.c_str(), said++ * 14);
    }
    if (net && !connecting) {
        char text[64] = "";
        if (waiting) {
            snprintf(text, sizeof(text), net->Machines() > 2 ? "WAITING FOR THE OTHER PLAYERS" : "WAITING FOR THE OTHER PLAYER");
        } else if (net->Watching()) {
            // (a spectator: no ping or delay of its own to care about; the label, dim, where they would be)
            status("SPECTATING", said++ * 14, kSpectatingOpacity);
        } else {
            // (the delay: this machine's, and the others' after it where any is not the same: "DELAY 0 (2)")
            std::string delays = "DELAY " + std::to_string(net->Delay());
            std::string others;
            bool differ = false;
            for (int m = 0; m < net->Machines(); m++) {
                if (m != net->Local()) {
                    others += (others.empty() ? "" : " ") + std::to_string(net->DelayOf(m));
                    differ = differ || net->DelayOf(m) != net->Delay();
                }
            }
            if (differ && !net->Watching()) delays += " (" + others + ")";
            // (and RELAY, while a machine in the match is reached through the relay: a little slower than directly)
            const char *relay = net->Relayed() ? "   RELAY" : "";
            if (FeatureOn("online_ping") && FeatureOn("online_delay")) {
                snprintf(text, sizeof(text), "PING %d ms   %s%s", net->Ping(), delays.c_str(), relay);
            } else if (FeatureOn("online_ping")) {
                snprintf(text, sizeof(text), "PING %d ms%s", net->Ping(), relay);
            } else if (FeatureOn("online_delay")) {
                snprintf(text, sizeof(text), "%s%s", delays.c_str(), relay);
            } else if (*relay) {
                snprintf(text, sizeof(text), "RELAY");
            }
        }
        if (*text) {
            status(text, said++ * 14, waiting ? HudOpacity{220, 255} : onlineOpacity);
        }
        if (notices && !net->Watching() && FeatureOn("online_link_rtt")) {
            std::string routes = "RTT";
            for (int m = 0; m < net->Machines(); ++m) {
                if (m == net->Local() || (!net->Hosting() && m != 0)) continue;
                routes += "   ";
                for (int p = 0; p < net->People(m); ++p) {
                    if (p) routes += "+";
                    routes += "P" + std::to_string(net->Player(m, p) + 1);
                }
                const int rtt = net->DirectRtt(m);
                routes += rtt < 0 ? " --" : " " + std::to_string(rtt) + " ms";
            }
            status(routes.c_str(), said++ * 14, onlineOpacity);
        }
        if (!cameIn) {
            status("PRESS START TO JOIN", said++ * 14);
        }
        if (net->Set().Winner() >= 0) {
            const std::string result = std::string(tag::Chosen() == tag::kOff ? "PLAYER " : "TEAM ") + std::to_string(net->Set().Winner() + 1) + " WINS THE FT" +
                                       std::to_string(net->Set().Target()) + " SET";
            status(result.c_str(), said++ * 14);
        }
    }
    // (--net-arcade: the host playing while it waits; the wait, and the match's code to give, at the bottom)
    if (net && connecting && net->Arcade()) {
        if (net->Found()) {
            status("A CHALLENGER IS COMING", said++ * 14);
        } else {
            static const char *const kDots[] = {"   ", ".  ", ".. ", "..."};
            const std::string waiting = std::string("WAITING FOR A CHALLENGER") + kDots[(tick / 30) % 4];
            status(waiting.c_str(), said++ * 14);
            if (!net->code.empty()) {
                const std::string code = "MATCH CODE " + net->code + (net->Listed() ? "   IN THE LOBBY" : "");
                status(code.c_str(), said++ * 14);
                if (!net->Invited().empty()) {
                    std::string invited = InvitationLine(*net);
                    for (char &c : invited) {
                        c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
                    }
                    status(invited.c_str(), said++ * 14);
                }
            }
        }
    }
    // (what the camera's zoom was set to, for a few seconds: above that if both are said)
    if (*zoom.Label()) {
        status(zoom.Label(), said++ * 14);
    }
    if (shotTicks > 0) {
        status(shotSaid.c_str(), said++ * 14);
    }
    // (a video being recorded: how long so far, all the while, in the window only)
    if (recorder.Recording() && !shooting) {
        const int seconds = static_cast<int>(recorder.Seconds());
        char text[64];
        snprintf(text, sizeof(text), "%s  %d:%02d", recomp::Tr("RECORDING VIDEO"), seconds / 60, seconds % 60);
        status(text, said * 14);
    }
    // (online: each player's name under their side's life bar, and under it a win streak of two or more)
    if (net && !connecting) {
        const recomp::text::Canvas canvas{px, W, H};
        auto under = [&](int side, float y, const std::string &text, recomp::text::Color colour) {
            const float width = recomp::text::TextWidth(text.c_str(), 1);
            const float x = side == 0 ? 10.0f : W - 10.0f - width;
            canvas.Fill(x - 3, y - 2, width + 6, 12, {20, 22, 30, onlineOpacity.panel});
            colour.a = onlineOpacity.ink;
            canvas.Text(x, y, 1, 0, colour, text.c_str());
            if (panels) {
                panels->push_back({static_cast<int>(x) - 3, static_cast<int>(y) - 2, static_cast<int>(width) + 7, 12});
            }
        };
        // Each teammate keeps their player number; first-to scores remain team scores.
        const bool teams = tag::Chosen() != tag::kOff;
        const bool hidden = FeatureOn("online_hide_names");
        int room[2] = {W / 2, W - W / 2};
        // (the score: a set's, or in Casual the games each side has won, in a fighting game: FirstTo::Counting)
        const bool strip = net->Set().Counting();
        if (strip) {
            // Keep the score centred even when the players' names have different lengths.
            constexpr float size = 1;
            // Whole font dots stay sharp; compact padding keeps the strip clear of the life bars.
            auto smallText = [&](float x, const std::string &text, recomp::text::Color colour) {
                colour.a = onlineOpacity.ink;
                canvas.Text(std::floor(x), 2, size, 0, colour, text.c_str());
            };
            const std::string left = std::to_string(net->Set().Score(0));
            const std::string format = net->Set().Target() ? "FT" + std::to_string(net->Set().Target()) : "-";
            const std::string right = std::to_string(net->Set().Score(1));
            const float leftWidth = recomp::text::TextWidth(left.c_str(), size);
            const float formatWidth = recomp::text::TextWidth(format.c_str(), size);
            const float rightWidth = recomp::text::TextWidth(right.c_str(), size);
            const float centre = (W - formatWidth) / 2.0f;
            const float leftAt = centre - 6 - leftWidth;
            const float rightAt = centre + formatWidth + 6;
            float stripLeft = leftAt, stripRight = rightAt + rightWidth;
            std::string names[2];
            float nameX[2] = {};
            if (!teams && !hidden) {
                for (int side = 0; side < 2; ++side) {
                    names[side] = net->Name(side);
                    if (names[side].empty()) names[side] = "Player " + std::to_string(side + 1);
                    const float space = side == 0 ? leftAt - 12 : W - rightAt - rightWidth - 12;
                    names[side].resize(recomp::text::Bytes(names[side].data(), names[side].size(),
                            static_cast<size_t>(std::max(0.0f, space) / recomp::text::kCharWidth)));
                    const float width = recomp::text::TextWidth(names[side].c_str(), size);
                    nameX[side] = side == 0 ? leftAt - 9 - width : rightAt + rightWidth + 9;
                    stripLeft = std::min(stripLeft, nameX[side]);
                    stripRight = std::max(stripRight, nameX[side] + width);
                }
            }
            const int x0 = std::max(0, static_cast<int>(std::floor(stripLeft)) - 3);
            const int x1 = std::min(W, static_cast<int>(std::ceil(stripRight)) + 3);
            room[0] = x0;
            room[1] = W - x1;
            canvas.Fill(x0, 0, x1 - x0, 11, {20, 22, 30, onlineOpacity.panel});
            smallText(leftAt, left, {0, 220, 220, 255});
            smallText(centre, format, {255, 210, 80, 255});
            smallText(rightAt, right, {0, 220, 220, 255});
            for (int side = 0; side < 2; ++side) {
                if (!names[side].empty()) smallText(nameX[side], names[side], {255, 255, 255, 255});
            }
            if (panels) panels->push_back({x0, 0, x1 - x0, 11});
        }
        const bool atTop = FeatureValue("online_names_place") == 1;
        // (each side's names, where the score's strip hasn't them, and its win streak)
        for (int side = 0; side < 2 && !hidden; side++) {
            float y = static_cast<float>(TheGame().onlineNamesY);
            const bool showStreak = streak[side] >= 2 && !FeatureOn("online_hide_streak");
            std::vector<std::pair<std::string, bool>> lines; // (each line, and whether it is drawn bright)
            bool named = false;
            const int point = tag::OnPoint(side);
            for (int player = 0; player < Machine::kMaxPlayers && (!strip || teams); player++) {
                if (ScreenSide(player) != side || net->MachineOf(player) < 0) {
                    continue;
                }
                const std::string name = net->Name(player);
                named = named || !name.empty();
                if (teams) {
                    lines.push_back({"P" + std::to_string(player + 1) + (name.empty() ? "" : " " + name), point < 0 || point == player});
                } else if (!name.empty()) {
                    lines.push_back({name, true});
                }
            }
            if (!named) {
                lines.clear();
            }
            if (showStreak) {
                lines.push_back({"STREAK " + std::to_string(streak[side]), true});
            }
            if (atTop) {
                // One line a side along the top edge (in the first-to strip when there is one), each part in its colour
                const size_t originalSize = lines.size();
                const float gap = static_cast<float>(FitHudNames(lines, showStreak ? streak[side] : 0, room[side] - 14));
                const bool streakBelow = lines.size() < originalSize;
                if (streakBelow) under(side, 14, "W" + std::to_string(streak[side]), {255, 210, 80, 255});
                float width = 0;
                for (size_t n = 0; n < lines.size(); n++) {
                    width += (n ? gap : 0) + recomp::text::TextWidth(lines[n].first.c_str(), 1);
                }
                if (lines.empty()) {
                    continue;
                }
                float x = side == 0 ? 10.0f : W - 10.0f - width;
                const float top = 2.0f;
                canvas.Fill(x - 3, top - 2, width + 6, 12, {20, 22, 30, onlineOpacity.panel});
                if (panels) {
                    panels->push_back({static_cast<int>(x) - 3, static_cast<int>(top) - 2, static_cast<int>(width) + 7, 12});
                }
                for (size_t n = 0; n < lines.size(); n++) {
                    const bool isStreak = showStreak && !streakBelow && n + 1 == lines.size();
                    recomp::text::Color colour = isStreak ? recomp::text::Color{255, 210, 80, 255}
                                                       : lines[n].second ? recomp::text::Color{255, 255, 255, 255}
                                                                         : recomp::text::Color{150, 150, 160, 255};
                    colour.a = onlineOpacity.ink;
                    canvas.Text(x, top, 1, 0, colour, lines[n].first.c_str());
                    x += recomp::text::TextWidth(lines[n].first.c_str(), 1) + gap;
                }
                continue;
            }
            for (size_t n = 0; n < lines.size(); n++) {
                const bool isStreak = showStreak && n + 1 == lines.size();
                under(side, y, lines[n].first, isStreak ? recomp::text::Color{255, 210, 80, 255}
                                               : lines[n].second ? recomp::text::Color{255, 255, 255, 255}
                                                                 : recomp::text::Color{150, 150, 160, 255});
                y += 12;
            }
        }
        // The network readout (online_stats), at the top right under the score's strip, in the small letters: the ping and
        // its jitter, each machine's players with its input delay and run-ahead, and the last second's rollbacks
        if (notices && FeatureOn("online_stats") && !net->Watching() && !waiting) {
            std::vector<std::string> lines;
            lines.push_back("PING " + std::to_string(net->Ping()) + " MS  JITTER " + std::to_string(net->Jitter()) + " MS" +
                            (net->Relayed() ? "  RELAY" : ""));
            for (int m = 0; m < net->Machines(); ++m) {
                std::string who;
                for (int person = 0; person < net->People(m); ++person) {
                    who += (person ? "+P" : "P") + std::to_string(net->Player(m, person) + 1);
                }
                const int ahead = net->AheadOf(m);
                lines.push_back(who + (m == net->Local() ? " YOU" : "") + "  DELAY " + std::to_string(net->DelayOf(m)) + "  AHEAD " +
                                (ahead < 0 ? std::string("?") : std::to_string(ahead)));
            }
            int count = 0, most = 0;
            net->Rollbacks(count, most);
            lines.push_back("ROLLBACK " + std::to_string(most) + "F  " + std::to_string(count) + "/S");
            float y = strip ? 12.0f : 1.0f;
            for (const std::string &text : lines) {
                const float width = recomp::text::SmallTextWidth(text.c_str(), 1) - 1;
                const float x = W - 3 - width;
                canvas.Fill(x - 2, y, width + 4, 7, {20, 22, 30, onlineOpacity.panel});
                canvas.SmallText(x, y + 1, 1, {255, 255, 255, onlineOpacity.ink}, text.c_str());
                if (panels) {
                    panels->push_back({static_cast<int>(x) - 2, static_cast<int>(y), static_cast<int>(width) + 5, 7});
                }
                y += 7;
            }
        }
        // The chat: its lines at the left, above the super gauges, the newest lowest, and the line being typed under them
        float y = H - 44.0f;
        if (bottomInset) y = std::min(y, static_cast<float>(H - 11 - bottomInset - said * 14));
        auto line = [&](const std::string &text, recomp::text::Color colour) {
            const float width = recomp::text::TextWidth(text.c_str(), 1);
            canvas.Fill(7, y - 2, width + 6, 12, {20, 22, 30, 200});
            canvas.Text(10, y, 1, 0, colour, text.c_str());
            if (panels) {
                panels->push_back({7, static_cast<int>(y) - 2, static_cast<int>(width) + 7, 12});
            }
            y -= 12;
        };
        // (and under them, what the keys and buttons are: while a line is typed, and for a while as a match begins)
#ifdef __ANDROID__
        const char *open = FeatureOn("keyboard_only") ? "Chat: tap CHAT" : "Chat: tap CHAT (or a gamepad's right stick click)";
        const char *keys = "Enter: send   CHAT or Back: cancel";
#else
        const bool keysOnly = FeatureOn("keyboard_only");
        const char *open = keysOnly ? "Chat: T" : "Chat: T, or a gamepad's right stick click";
        const char *keys = keysOnly ? "Enter: send   Esc: cancel" : "Enter: send   Esc or right stick click: cancel";
#endif
        if (typing) {
            line(keys, {170, 170, 170, 255});
            line("Say: " + typed + ((tick / 20) % 2 ? "_" : " "), {255, 210, 80, 255});
        } else if (tick < chatHintUntil) {
            line(open, {170, 170, 170, 255});
        }
        for (auto it = chat.rbegin(); it != chat.rend(); ++it) {
            line(it->text, it->own ? recomp::text::Color{170, 200, 255, 255} : recomp::text::Color{255, 255, 255, 255});
        }
    }
    // (online: the menu (Port::OpenOnlineMenu), in the middle, over the game, which goes on; the port's own, not the
    // readout, which the game's sources set every frame. Esc's question when there is no match to choose for (watching,
    // --net-arcade's wait: whether to stop hosting, the game stopped meanwhile)
    if (net && menu.OnlineOpen() && notices) {
        menu.Draw(px, W, H);
        if (panels) {
            panels->push_back({0, 0, W, H});
        }
    } else if (net && leaving) {
        const recomp::text::Canvas canvas{px, W, H};
        const bool hosting = connecting;
        const char *question = net->Watching() ? "Stop watching?" : hostedFromMenu ? "Leave the match?" : hosting ? "Stop hosting and quit?" : "Leave the match?";
        const char *confirm = hostedFromMenu ? "Confirm (A, Enter): play alone" : hosting ? "Confirm (A, Enter): quit" : "Confirm (A, Enter): leave";
        const struct {
            const char *text;
            recomp::text::Color colour;
        } lines[] = {{question, {255, 255, 255, 255}},
                     {confirm, {255, 130, 120, 255}},
                     {"Back (B, Backspace, Esc): stay", {255, 255, 255, 255}}};
        float y = H / 2.0f - 30;
        for (const auto &line : lines) {
            const float width = recomp::text::TextWidth(line.text, 1);
            const float x = (W - width) / 2;
            canvas.Fill(x - 6, y - 4, width + 12, 16, {20, 22, 30, 235});
            canvas.Text(x, y, 1, 0, line.colour, line.text);
            if (panels) {
                panels->push_back({static_cast<int>(x) - 6, static_cast<int>(y) - 4, static_cast<int>(width) + 13, 16});
            }
            y += 22;
        }
    }
}

void Port::Readouts(std::vector<uint32_t> &rgba, std::vector<SDL_Rect> *panels) const {
    std::vector<uint32_t> white;
    Shown(rgba, panels, true, 0xFF000000u);
    Shown(white, nullptr, true, 0xFFFFFFFFu);
    for (size_t n = 0; n < rgba.size(); n++) {
        // (over black, what is left is the readouts' colour times its alpha; over white, that and what they leave)
        const uint32_t black = rgba[n], left = (white[n] & 0xFF) - (black & 0xFF);
        rgba[n] = (black & 0xFFFFFF) | ((255 - std::min<uint32_t>(left, 255)) << 24);
    }
}

int Port::FramedColumns() const {
    const int columns = TheGame().framedColumns && !port::Turned() ? TheGame().framedColumns() : 0;
    return columns > 0 && columns < ShownWidth() && machine->Unpanelled() ? columns : 0;
}

void Port::Framed(std::vector<uint32_t> &pixels, Zoomed &zoomed) {
    const int across = ShownWidth(), columns = FramedColumns();
    constexpr int kBy = 3, kLines = Video::kHeight;
    const int outWidth = columns * kBy, outHeight = kLines * kBy;
    zoomed.width = outWidth;
    zoomed.height = outHeight;
    zoomed.columns = columns;
    // The camera: as close as shows both fighters and what else is kept in (a ball), to kFramedMost of the shape; out to
    // all of the picture's width (above it then, black); eased
    constexpr float kFramedMost = 1.3f, kEase = 0.06f;
    constexpr float kRoomAcross = 0.62f, kRoomTop = 8.0f, kRoomFeet = 14.0f;
    const float least = static_cast<float>(columns) / static_cast<float>(across);
    Game &game = TheGame();
    float wantedSize = least, wantedX = across / 2.0f, wantedY = kLines / 2.0f;
    int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    if (game.framedFollows && game.framedFollows() && game.focus && game.focus(0, x0, y0) && game.focus(1, x1, y1)) {
        const float roomAcross = game.focusTall * kRoomAcross;
        float left = std::min(x0, x1) - roomAcross, right = std::max(x0, x1) + roomAcross;
        float top = std::min(y0, y1) - (game.focusTall - game.focusLift) - kRoomTop, bottom = std::max(y0, y1) + game.focusLift + kRoomFeet;
        int ax = 0, ay = 0, reach = 0;
        if (game.focusAlso && game.focusAlso(ax, ay, reach)) {
            left = std::min(left, static_cast<float>(ax - reach));
            right = std::max(right, static_cast<float>(ax + reach));
            top = std::min(top, static_cast<float>(ay - reach));
            bottom = std::max(bottom, static_cast<float>(ay + reach));
        }
        wantedSize = std::clamp(std::min(columns / (right - left), kLines / (bottom - top)), least, kFramedMost);
        wantedX = (left + right) / 2;
        wantedY = (top + bottom) / 2;
    }
    FrameCamera &cam = frameCamera;
    if (!cam.settled) {
        cam = {wantedX, wantedY, wantedSize, true};
    }
    cam.size += (wantedSize - cam.size) * kEase;
    cam.x += (wantedX - cam.x) * kEase;
    cam.y += (wantedY - cam.y) * kEase;
    const float viewWidth = columns / cam.size, viewHeight = kLines / cam.size;
    float viewX = viewWidth >= across ? (across - viewWidth) / 2 : std::clamp(cam.x - viewWidth / 2, 0.0f, across - viewWidth);
    // (taller than the picture: the black above, where the panel goes, and as far as there is room the picture raised clear
    // of the panel's bottom lines (the gauges), which would else cover the fighters' feet)
    constexpr float kBelow = 26.0f;
    float viewY = viewHeight >= kLines ? kLines - viewHeight + std::min(kBelow * viewHeight / kLines, viewHeight - kLines)
                                       : std::clamp(cam.y - viewHeight / 2, 0.0f, kLines - viewHeight);

    // The game's picture without its panel, with the boxes the sources draw over the fight (the ball), through the view:
    // each pixel of the frame the nearest of the picture's (tables of which column and row, made once a frame: a frame
    // every sixtieth of a second on a handheld's processor too)
    std::vector<uint32_t> &fight = framedFight;
    fight.assign(machine->Unpanelled(), machine->Unpanelled() + static_cast<size_t>(across) * kLines);
    port::DrawHud(fight.data(), across, kLines, nullptr, port::kHudBoxes);
    pixels.resize(static_cast<size_t>(outWidth) * outHeight);
    std::vector<int> &columnOf = framedColumnOf;
    columnOf.resize(static_cast<size_t>(outWidth));
    for (int x = 0; x < outWidth; x++) {
        const int column = static_cast<int>(std::floor(viewX + (x + 0.5f) * viewWidth / outWidth));
        columnOf[static_cast<size_t>(x)] = column >= 0 && column < across ? column : -1;
    }
    for (int y = 0; y < outHeight; y++) {
        const int row = static_cast<int>(std::floor(viewY + (y + 0.5f) * viewHeight / outHeight));
        uint32_t *line = &pixels[static_cast<size_t>(y) * outWidth];
        if (row < 0 || row >= kLines) {
            std::fill(line, line + outWidth, 0xFF000000u);
            continue;
        }
        const uint32_t *from = &fight[static_cast<size_t>(row) * across];
        for (int x = 0; x < outWidth; x++) {
            const int column = columnOf[static_cast<size_t>(x)];
            line[x] = column < 0 ? 0xFF000000u : from[column] | 0xFF000000u;
        }
    }
    // Over it, as a picture of the shape's columns shows them: the game's panel (in the middle of the picture, as the
    // board's own), and the readout's text, each of their pixels kBy x kBy of the frame's
    std::vector<uint32_t> &laid = framedLaid;
    laid.assign(static_cast<size_t>(columns) * kLines, 0);
    // Canvas blends onto its destination. Over black and white we can recover how much of each
    // magnified court pixel should show through online text and panels, as Readouts does for the GPU.
    const bool liveOnline = net && !connecting;
    const bool waiting = liveOnline && net->Silent() > 1000;
    if (liveOnline) {
        framedWhite.assign(laid.size(), 0xffffffffu);
    }
    const uint8_t *panel = machine->PanelPixels();
    const uint32_t *picture = machine->Picture();
    const int from = (across - columns) / 2;
    if (panel) {
        for (int y = 0; y < kLines; y++) {
            for (int x = 0; x < columns; x++) {
                const size_t n = static_cast<size_t>(y) * across + x + from;
                if (panel[n]) {
                    const size_t at = static_cast<size_t>(y) * columns + x;
                    laid[at] = picture[n] | 0xff000000u;
                    if (liveOnline) framedWhite[at] = laid[at];
                }
            }
        }
    }
    Overlays(laid.data(), columns, kLines, nullptr, true, port::kHudText, waiting);
    if (liveOnline) Overlays(framedWhite.data(), columns, kLines, nullptr, true, port::kHudText, waiting);
    for (int y = 0; y < kLines; y++) {
        const uint32_t *row = &laid[static_cast<size_t>(y) * columns];
        for (int x = 0; x < columns; x++) {
            if (const uint32_t over = row[x]; over >> 24) {
                const uint32_t white = liveOnline ? framedWhite[static_cast<size_t>(y) * columns + x] : over;
                for (int dy = 0; dy < kBy; dy++) {
                    auto *dst = &pixels[static_cast<size_t>(y * kBy + dy) * outWidth + static_cast<size_t>(x * kBy)];
                    if (white == over) {
                        std::fill_n(dst, kBy, over);
                        continue;
                    }
                    for (int dx = 0; dx < kBy; dx++) {
                        uint32_t color = 0xff000000u;
                        for (unsigned shift = 0; shift < 24; shift += 8) {
                            const unsigned under = (dst[dx] >> shift) & 255;
                            const unsigned dark = (over >> shift) & 255, light = (white >> shift) & 255;
                            color |= ((dark * (255 - under) + light * under + 127) / 255) << shift;
                        }
                        dst[dx] = color;
                    }
                }
            }
        }
    }
}

bool Port::Windowed(std::vector<uint32_t> &pixels, Zoomed &zoomed, bool readouts) {
    // (a mode that frames a picture wider than it is shown: its own camera, not the zoom)
    if (FramedColumns() && !readouts) {
        zoomed.keep.clear();
        zoomed.panel = false;
        Framed(pixels, zoomed);
        return false;
    }
    frameCamera.settled = false;
    const int across = ShownWidth();
    // A mode that wants the zoom while it runs (Game::zoomWanted): its choice, and the player's put back after
    {
        static int wantedBefore = 0, choiceBefore = 0;
        const int wanted = TheGame().zoomWanted && Zoom::Possible() ? TheGame().zoomWanted() : 0;
        if (wanted != wantedBefore) {
            if (wantedBefore == 0) {
                choiceBefore = zoom.Choice();
            }
            zoom.Choose(wanted ? wanted : choiceBefore);
            wantedBefore = wanted;
        }
    }
    zoom.Follow(across, Video::kHeight);
    const Menu::Rate rate = menu.GameRate();
    if (rate.frames != saidRate.frames || rate.ticks != saidRate.ticks) {
        saidRate = rate;
        speedTicks = kSpeedTicks;
    } else if (speedTicks > 0) {
        speedTicks--;
    }
    if (shotTicks > 0) {
        shotTicks--;
    }
    // (a scene clip written, or not: said as a screenshot is)
    if (std::string said; scene.TakeSaid(said)) {
        shotSaid = SaidOfScene(said);
        shotTicks = kSpeedTicks;
    }
    zoomed.keep.clear();
    zoomed.panel = false;
    if (!zoom.View(across, Video::kHeight, zoomed.view)) {
        if (readouts) {
            Readouts(pixels);
        } else {
            Shown(pixels);
        }
        return false;
    }
    if (readouts) {
        Readouts(pixels, &zoomed.keep);
        return true;
    }
    // (the game's own picture above, the one with the readout below)
    // (not where the game's sources put the picture together from other frames: the last frame run is not the one shown)
    const bool apart = zoom.KeepsPanel() && machine->Unpanelled() && machine->PictureWidth() == across && !cps2recomp::PictureMade();
    Shown(pixels, &zoomed.keep, false, 0, !apart);
    const uint32_t *above = machine->Picture();
    static std::vector<uint32_t> unpanelled;
    if (apart) {
        // The panel left where it is (zoom.h, by distance): above, the game's picture without it, with the boxes the game's
        // sources draw over the fight (zoomed with it: hit boxes, a mode's ball); below, of the one with the readout,
        // only the panel's pixels and the readout's panels, the rest clear
        unpanelled.assign(machine->Unpanelled(), machine->Unpanelled() + across * Video::kHeight);
        DrawHud(unpanelled.data(), across, Video::kHeight, nullptr, port::kHudBoxes);
        above = unpanelled.data();
        const uint8_t *panel = machine->PanelPixels();
        std::vector<uint8_t> kept(pixels.size(), 0);
        for (const SDL_Rect &keep : zoomed.keep) {
            for (int y = std::max(keep.y, 0); y < std::min(keep.y + keep.h, +Video::kHeight); y++) {
                for (int x = std::max(keep.x, 0); x < std::min(keep.x + keep.w, across); x++) {
                    kept[static_cast<size_t>(y) * across + x] = 1;
                }
            }
        }
        for (size_t n = 0; n < pixels.size(); n++) {
            pixels[n] = panel[n] || kept[n] ? pixels[n] | 0xFF000000u : 0;
        }
        zoomed.panel = true;
    }
    pixels.insert(pixels.begin(), above, above + across * Video::kHeight);
    return true;
}

void Port::MenuPicture(std::vector<uint32_t> &pixels, uint32_t &width, uint32_t &height, int wantWidth, int wantHeight) const {
    std::vector<uint32_t> shown;
    Shown(shown);
    const uint32_t across = static_cast<uint32_t>(ShownWidth());
    const bool want = wantWidth > 0 && wantHeight > 0;
    height = want ? static_cast<uint32_t>(wantHeight) : Video::kHeight * 3;
    width = want ? static_cast<uint32_t>(wantWidth) : height * 4 / 3 * across / Video::kWidth;
    if (const int columns = FramedColumns()) {
        // (a framed picture, Port::Framed: the menu in the shape of its columns, over all of the picture at the bottom of it,
        // as wide as that shape and as much shorter, black above)
        if (!want) {
            width = height * 4 / 3 * static_cast<uint32_t>(columns) / Video::kWidth;
        }
        pixels.assign(static_cast<size_t>(width) * height, 0xFF000000u);
        const uint32_t tall = height * static_cast<uint32_t>(columns) / across;
        for (uint32_t y = 0; y < tall; y++) {
            const uint32_t *from = &shown[static_cast<size_t>(y * Video::kHeight / tall) * across];
            uint32_t *to = &pixels[static_cast<size_t>(height - tall + y) * width];
            for (uint32_t x = 0; x < width; x++) {
                to[x] = from[x * across / width];
            }
        }
        return;
    }
    pixels.resize(static_cast<size_t>(width) * height);
    if (port::Turned()) {
        // (the menu stays as it is, over the picture stood up in its middle at its shape, black beside it)
        std::vector<uint32_t> turned;
        port::TurnPicture(shown.data(), static_cast<int>(across), turned);
        const uint32_t tall = height, wide = static_cast<uint32_t>(std::lround(tall * port::PictureShape(Video::kHeight, static_cast<int>(across), port::Shape::Arcade, 1.0f)));
        const uint32_t left = (width - std::min(wide, width)) / 2;
        std::fill(pixels.begin(), pixels.end(), 0xFF000000u);
        for (uint32_t y = 0; y < tall; y++) {
            const uint32_t *from = &turned[static_cast<size_t>(y * across / tall) * Video::kHeight];
            uint32_t *to = &pixels[static_cast<size_t>(y) * width + left];
            for (uint32_t x = 0; x < wide && left + x < width; x++) {
                to[x] = from[x * Video::kHeight / wide];
            }
        }
        return;
    }
    for (uint32_t y = 0; y < height; y++) {
        const uint32_t *from = &shown[static_cast<size_t>(y * Video::kHeight / height) * across];
        uint32_t *to = &pixels[static_cast<size_t>(y) * width];
        for (uint32_t x = 0; x < width; x++) {
            to[x] = from[x * across / width];
        }
    }
}

std::string Port::PlayerFolder() const {
    const size_t slash = opt.eepromPath.find_last_of('/');
    return slash == std::string::npos ? "" : opt.eepromPath.substr(0, slash + 1);
}

std::string Port::StatePath(int slot) const {
    return PlayerFolder() + "state" + std::to_string(slot) + ".bin";
}

void Port::KeepPicture() {
    // (a run of a few seconds at least, in a window: not a test's, nor a start that went straight back out)
    if (opt.headless || frame < 600) {
        return;
    }
    const int ticks = shotTicks;
    shotTicks = 0;
    shooting = true;
    std::vector<uint32_t> shown;
    Shown(shown);
    shooting = false;
    shotTicks = ticks;
    const std::string path = PlayerFolder() + "picture.png";
    if (!WritePng(path.c_str(), shown.data(), static_cast<uint32_t>(ShownWidth()), Video::kHeight)) {
        fprintf(stderr, "picture: could not write %s\n", path.c_str());
    }
}

bool Port::Screenshot() {
    const std::string folder = PlayerFolder() + "screenshots/";
    SDL_CreateDirectory(folder.c_str());
    char stamp[32] = "";
    const std::time_t now = std::time(nullptr);
    if (const std::tm *local = std::localtime(&now)) {
        std::strftime(stamp, sizeof(stamp), "%Y-%m-%d %H.%M.%S", local);
    }
    // (another in the same second: a number after it)
    std::string name = std::string(GAME_ID " ") + stamp;
    for (int n = 2; std::filesystem::exists(std::filesystem::u8path(folder + name + ".png")); n++) {
        name = std::string(GAME_ID " ") + stamp + " " + std::to_string(n);
    }
    const int ticks = shotTicks;
    shotTicks = 0; // (not the last one's line in this one, nor the recording's)
    shooting = true;
    std::vector<uint32_t> shown;
    Shown(shown);
    shooting = false;
    shotTicks = ticks;
    const std::string path = folder + name + ".png";
    if (!WritePng(path.c_str(), shown.data(), static_cast<uint32_t>(ShownWidth()), Video::kHeight)) {
        fprintf(stderr, "screenshot: could not write %s\n", path.c_str());
        shotSaid = "SCREENSHOT NOT SAVED";
        shotTicks = kSpeedTicks;
        return false;
    }
    fprintf(stderr, "screenshot: %s\n", path.c_str());
    shotSaid = "SCREENSHOT SAVED  screenshots/" + name + ".png";
    if (FeatureOn("export_svg")) {
        // (the same as an SVG: the Game page's SVG pictures too)
        cps2recomp::image_files::Picture picture;
        picture.width = ShownWidth();
        picture.height = Video::kHeight;
        picture.pixels.resize(shown.size());
        for (size_t n = 0; n < shown.size(); n++) {
            picture.pixels[n] = shown[n] | 0xFF000000u;
        }
        double width = 0, height = 0;
        port::SvgSize(picture.width, picture.height, width, height);
        if (cps2recomp::image_files::WriteSvg(folder + name + ".svg", picture, width, height)) {
            shotSaid += " .svg";
        }
    }
    shotTicks = kSpeedTicks;
    return true;
}

// The name a scene's folder is given: the game's and the time, as a screenshot's
static std::string SceneName() {
    char stamp[32] = "";
    const std::time_t now = std::time(nullptr);
    if (const std::tm *local = std::localtime(&now)) {
        std::strftime(stamp, sizeof(stamp), "%Y-%m-%d %H.%M.%S", local);
    }
    return std::string(GAME_ID " ") + stamp;
}

void Port::SaveScene() {
    shotSaid = SaidOfScene(scene.Save(*machine, PlayerFolder() + "exports/scenes/", SceneName()));
    shotTicks = kSpeedTicks;
}

void Port::SceneClip() {
    if (scene.Recording()) {
        scene.StopClip();
        shotSaid = recomp::Tr("SAVING SCENE CLIP...");
    } else {
        static const int kSeconds[] = {3, 5, 10};
        const int seconds = kSeconds[std::clamp(FeatureValue("scene_clip_length"), 0, 2)];
        shotSaid = SaidOfScene(scene.StartClip(PlayerFolder() + "exports/scenes/", SceneName() + " clip", seconds * 60));
    }
    shotTicks = kSpeedTicks;
    menu.recordingClip = scene.Recording();
}

// The video's size, from the Video page's Video size: so many times the picture's lines (or 1080 lines), in its
// shape as the Display page has it (the whole window's: the board's monitor's); the widest picture the game makes
// (its widescreen), so that a picture as wide as that fills it and a narrower one has black beside it
static void VideoSize(int shown, int &width, int &height) {
    static const int kTimes[] = {3, 2, 4, 0};
    const int choice = std::clamp(FeatureValue("video_size"), 0, 3);
    const int columns = TheGame().widestPicture ? std::clamp(TheGame().widestPicture(), +Video::kWidth, +Video::kMaxWidth) : shown;
    const bool turned = port::Turned();
    const int across = turned ? +Video::kHeight : columns, down = turned ? columns : +Video::kHeight;
    const port::Shape shape = port::DisplayShape() == port::Shape::Fill ? port::Shape::Arcade : port::DisplayShape();
    const int lines = kTimes[choice] ? down * kTimes[choice] : 1080;
    VideoRecorder::SizeFor(port::PictureShape(across, down, shape, 4.0f / 3.0f), lines, width, height);
}

bool Port::StartVideo(const std::string &path) {
    int width = 0, height = 0;
    VideoSize(ShownWidth(), width, height);
    std::string error;
    if (!recorder.Start(path, width, height, Machine::kFramesPerSecond, error)) {
        fprintf(stderr, "video: %s\n", error.c_str());
        return false;
    }
    fprintf(stderr, "video: recording %s (%d x %d)\n", path.c_str(), width, height);
    recorder.SetWaiting(opt.headless || opt.unthrottled); // (not in real time: no picture left out)
    videoFrame = frame;
    menu.recordingVideo = true;
    return true;
}

void Port::StopVideo() {
    if (!recorder.Recording()) {
        return;
    }
    const std::string path = recorder.Path();
    const double seconds = recorder.Seconds();
    const uint64_t dropped = recorder.Dropped();
    const bool saved = recorder.Stop();
    menu.recordingVideo = false;
    fprintf(stderr, "video: %s %s (%.1f s%s)\n", saved ? "saved" : "not saved", path.c_str(), seconds,
            dropped ? (", " + std::to_string(dropped) + " pictures left out").c_str() : "");
    const size_t slash = path.find_last_of('/');
    shotSaid = saved ? std::string(recomp::Tr("VIDEO SAVED")) + "  videos/" + (slash == std::string::npos ? path : path.substr(slash + 1))
                     : recomp::Tr("VIDEO NOT SAVED");
    shotTicks = kSpeedTicks;
}

void Port::RecordVideo() {
    if (recorder.Recording()) {
        StopVideo();
        return;
    }
    const std::string folder = PlayerFolder() + "videos/";
    std::error_code ignored;
    std::filesystem::create_directories(std::filesystem::path(std::u8string(folder.begin(), folder.end())), ignored);
    // (named as a screenshot is: the game and the time, a number after it for another in the same second)
    std::string name = SceneName();
    for (int n = 2; std::filesystem::exists(std::filesystem::u8path(folder + name + ".mp4")); n++) {
        name = SceneName() + " " + std::to_string(n);
    }
    if (!StartVideo(folder + name + ".mp4")) {
        shotSaid = recomp::Tr("VIDEO NOT SAVED");
        shotTicks = kSpeedTicks;
    }
}

void Port::VideoFrame(const int16_t *sound, size_t soundFrames, const float *music, size_t musicFrames, int musicRate) {
    if (!recorder.Recording()) {
        return;
    }
    recorder.Sound(sound, soundFrames, QSound::kSampleRate);
    if (music && musicFrames) {
        recorder.Music(music, musicFrames, musicRate);
    }
    const int frames = static_cast<int>(frame - videoFrame);
    if (frames <= 0) {
        return;
    }
    videoFrame = frame;
    static std::vector<uint32_t> pixels, upright;
    Shown(pixels, nullptr, false, 0, true, false);
    const uint32_t *picture = pixels.data();
    int across = ShownWidth(), down = Video::kHeight;
    if (port::Turned()) {
        port::TurnPicture(pixels.data(), across, upright);
        picture = upright.data();
        std::swap(across, down);
    }
    const port::Shape shape = port::DisplayShape() == port::Shape::Fill ? port::Shape::Arcade : port::DisplayShape();
    recorder.Picture(picture, across, down, port::PictureShape(across, down, shape, 4.0f / 3.0f), frames);
}

// The instant replay (port.h, Replay): how many seconds are kept (the Video page's Replay length; 0: none)
static int ReplaySeconds() {
    static const int kSeconds[] = {30, 60, 10, 0};
    return kSeconds[std::clamp(FeatureValue("video_replay"), 0, 3)];
}

void Port::ReplayKeep(const uint16_t *pressed) {
    const int seconds = ReplaySeconds();
    if (seconds == 0 || net) {
        replay.kept.clear();
        replay.inputs.clear();
        return;
    }
    constexpr uint64_t kEvery = 60; // (a state a second)
    if (replay.kept.empty() || frame % kEvery == 0) {
        Replay::Kept kept;
        kept.frame = frame;
        KeepState(kept.state);
        if (replay.kept.empty()) {
            replay.inputs.clear();
            replay.inputsFrom = frame;
        }
        replay.kept.push_back(std::move(kept));
    }
    Replay::Input input;
    memcpy(input.players, pressed, sizeof(input.players));
    input.service = machine->serviceButton;
    input.test = machine->testSwitch;
    replay.inputs.push_back(input);
    // (the oldest kept that is still `seconds` back or less, and the controls from it on)
    const uint64_t span = static_cast<uint64_t>(seconds) * 60;
    while (replay.kept.size() > 1 && frame + 1 - replay.kept[1].frame >= span) {
        replay.kept.pop_front();
    }
    while (!replay.inputs.empty() && replay.inputsFrom < replay.kept.front().frame) {
        replay.inputs.pop_front();
        replay.inputsFrom++;
    }
}

// Rewind and the practice point (port.h, docs/PRACTICE.md): never online, nor for a spectator
bool Port::RewindOn() const {
    return FeatureOn("rewind") && !net && !opt.rollbackTest;
}
bool Port::PracticeOn() const {
    return FeatureOn("practice_point") && !net && !opt.rollbackTest;
}

void Port::Said(const char *what) {
    shotSaid = recomp::Tr(what);
    shotTicks = kSpeedTicks;
}

void Port::RewindKeep(const uint16_t *pressed) {
    if (!RewindOn()) {
        if (!rewind.ring.Empty()) {
            rewind.ring.Clear();
        }
        return;
    }
    if (frame % kRewindEvery != 0 && !rewind.ring.Empty()) {
        return;
    }
    StateRing::Input input;
    memcpy(input.players, pressed, sizeof(input.players));
    input.service = machine->serviceButton;
    input.test = machine->testSwitch;
    const auto began = std::chrono::steady_clock::now();
    std::vector<uint8_t> state;
    KeepState(state);
    rewind.ring.Push(frame, input, std::move(state), kRewindMost, kRewindMostBytes);
    // (tests: CPS2RECOMP_REWIND_LOG, what it costs: every 10 seconds, the states kept, their memory, the last keep's time)
    if (static const bool log = getenv("CPS2RECOMP_REWIND_LOG") != nullptr; log && frame % 600 == 0) {
        fprintf(stderr, "rewind: frame %llu, %zu states, %.1f MB, keeping took %.3f ms\n", static_cast<unsigned long long>(frame),
                rewind.ring.Size(), rewind.ring.Bytes() / 1048576.0,
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count());
    }
}

// The newest kept state put back and its frame run again with its controls (drawn, not heard), so the picture is
// the frame after it; the frame count goes back with it. What came after it is gone: the instant replay's states and
// controls (kept again from here), and the practice attempt's controls past here.
bool Port::RewindTick() {
    StateRing::Kept kept;
    if (!rewind.ring.Pop(kept)) {
        Said("NO FURTHER BACK");
        return false;
    }
    PutBackState(kept.state);
    machine->serviceButton = kept.input.service;
    machine->testSwitch = kept.input.test;
    const size_t sound = machine->audio.size();
    Step(kept.input.players);
    machine->audio.resize(sound);
    frame = kept.frame + 1;
    replay.kept.clear();
    replay.inputs.clear();
    if (!practice.state.empty()) {
        if (frame <= practice.frame) {
            practice.attempt.clear();
        } else if (practice.attempt.size() > frame - practice.frame) {
            practice.attempt.resize(static_cast<size_t>(frame - practice.frame));
        }
        practice.replaying = false;
    }
    return true;
}

void Port::PracticeSet() {
    if (!PracticeOn()) {
        return;
    }
    KeepState(practice.state);
    practice.frame = frame;
    practice.attempt.clear();
    practice.last.clear();
    practice.replaying = false;
    Said("PRACTICE POINT SET");
}

bool Port::PracticeBack() {
    if (!PracticeOn() || practice.state.empty()) {
        return false;
    }
    PutBackState(practice.state);
    frame = practice.frame;
    if (!practice.attempt.empty()) {
        practice.last = std::move(practice.attempt);
    }
    practice.attempt.clear();
    practice.replaying = false;
    rewind.ring.DropFrom(frame);
    replay.kept.clear();
    replay.inputs.clear();
    Said("BACK TO THE PRACTICE POINT");
    return true;
}

bool Port::PracticeReplay() {
    if (!PracticeOn() || practice.state.empty()) {
        return false;
    }
    if (!practice.replaying) {
        practice.replay = !practice.attempt.empty() ? practice.attempt : practice.last;
    }
    if (practice.replay.empty()) {
        Said("NOTHING TO REPLAY YET");
        return true;
    }
    PutBackState(practice.state);
    frame = practice.frame;
    practice.last = practice.replay;
    practice.attempt.clear();
    practice.replaying = true;
    practice.replayAt = 0;
    memcpy(practice.heldAtReplay, pad, sizeof(practice.heldAtReplay));
    rewind.ring.DropFrom(frame);
    replay.kept.clear();
    replay.inputs.clear();
    Said("REPLAY: A BUTTON TAKES OVER");
    return true;
}

void Port::PracticeInput(uint16_t *pressed) {
    if (practice.state.empty()) {
        return;
    }
    if (!PracticeOn()) {
        practice = {};
        return;
    }
    if (practice.replaying) {
        // (a button pressed that was not held when the replay began: the player's again from here)
        bool taken = practice.replayAt >= practice.replay.size();
        for (int p = 0; p < Machine::kMaxPlayers && !taken; p++) {
            taken = (pad[p] & ~practice.heldAtReplay[p]) != 0;
        }
        for (int p = 0; p < Machine::kMaxPlayers; p++) {
            practice.heldAtReplay[p] &= pad[p]; // (let go, and pressed again, it counts)
        }
        if (taken) {
            practice.replaying = false;
        } else {
            const StateRing::Input &input = practice.replay[practice.replayAt++];
            memcpy(pressed, input.players, sizeof(input.players));
            machine->serviceButton = input.service;
            machine->testSwitch = input.test;
        }
    }
    constexpr size_t kMostFrames = 60 * 60 * 10; // (ten minutes of controls)
    if (practice.attempt.size() < kMostFrames) {
        StateRing::Input input;
        memcpy(input.players, pressed, sizeof(input.players));
        input.service = machine->serviceButton;
        input.test = machine->testSwitch;
        practice.attempt.push_back(input);
    }
}

void Port::SaveReplay() {
    if (replay.making) {
        shotSaid = recomp::Tr("A REPLAY IS STILL BEING SAVED");
        shotTicks = kSpeedTicks;
        return;
    }
    if (replay.kept.empty() || replay.inputs.size() < 2) {
        shotSaid = recomp::Tr(ReplaySeconds() ? "NOTHING TO REPLAY YET" : "REPLAY LENGTH IS OFF");
        shotTicks = kSpeedTicks;
        return;
    }
    const std::string folder = PlayerFolder() + "videos/";
    std::error_code ignored;
    std::filesystem::create_directories(std::filesystem::path(std::u8string(folder.begin(), folder.end())), ignored);
    std::string name = SceneName() + " replay";
    for (int n = 2; std::filesystem::exists(std::filesystem::u8path(folder + name + ".mp4")); n++) {
        name = SceneName() + " replay " + std::to_string(n);
    }
    int width = 0, height = 0;
    VideoSize(ShownWidth(), width, height);
    std::string error;
    if (!replay.recorder.Start(folder + name + ".mp4", width, height, Machine::kFramesPerSecond, error)) {
        fprintf(stderr, "replay: %s\n", error.c_str());
        shotSaid = recomp::Tr("REPLAY NOT SAVED");
        shotTicks = kSpeedTicks;
        return;
    }
    replay.from = replay.kept;
    replay.made.assign(replay.inputs.begin(), replay.inputs.end());
    replay.madeFrom = replay.inputsFrom;
    replay.at = replay.from.front().frame;
    replay.end = replay.madeFrom + replay.made.size();
    replay.state = std::move(replay.from.front().state);
    replay.from.pop_front();
    replay.making = true;
    fprintf(stderr, "replay: %s, frames %llu to %llu (%zu bytes a state)\n", (folder + name + ".mp4").c_str(),
            static_cast<unsigned long long>(replay.at), static_cast<unsigned long long>(replay.end), replay.state.size());
    shotSaid = recomp::Tr("SAVING REPLAY...");
    shotTicks = 1 << 30; // (until it is saved)
}

void Port::ReplayMake() {
    if (!replay.making || replay.recorder.Busy()) {
        return;
    }
    // A few of its frames, while the encoder keeps up (and no more than a few milliseconds of this tick's time), with
    // the replay's machine and game in place of the port's, which are put back as they were: their state, and what a
    // state does not hold (the picture, the controls, the video's switches), as for a spare (runtime/spare.h)
    const auto began = std::chrono::steady_clock::now();
    static std::vector<uint8_t> live;
    KeepState(live);
    uint16_t held[Machine::kMaxPlayers];
    memcpy(held, machine->players, sizeof(held));
    const bool service = machine->serviceButton, testOn = machine->testSwitch;
    const auto hide = machine->video.hideObject;
    const unsigned layers = machine->video.layerMask, markLayers = machine->video.markLayers;
    const bool mark = machine->video.markObjects;
    const auto wide = machine->video.wide;
    const size_t sound = machine->audio.size();
    const size_t pixels = static_cast<size_t>(Video::kMaxWidth) * Video::kHeight;
    static std::vector<uint32_t> picture, shown, upright;
    picture.assign(machine->Picture(), machine->Picture() + pixels);
    SetRunningAhead(true); // (as run-ahead's frames: no records counted, no loading's resets: runtime/game.h)
    PutBackState(replay.state);
    NativeVideo::inSpare = true; // (not the picture shown: video/native.h, ShownFrame)
    for (int n = 0; n < 8 && replay.at < replay.end && !replay.recorder.Busy(); n++) {
        if (!replay.from.empty() && replay.from.front().frame == replay.at) {
            PutBackState(replay.from.front().state); // (as it was then: what ran differently goes no further)
            replay.from.pop_front();
        }
        const Replay::Input &input = replay.made[replay.at - replay.madeFrom];
        machine->serviceButton = input.service;
        machine->testSwitch = input.test;
        const size_t before = machine->audio.size();
        Step(input.players);
        replay.at++;
        replay.recorder.Sound(machine->audio.data() + before, (machine->audio.size() - before) / 2, QSound::kSampleRate);
        machine->audio.resize(before);
        Shown(shown, nullptr, false, 0, true, false);
        const uint32_t *frameShown = shown.data();
        int across = ShownWidth(), down = Video::kHeight;
        if (port::Turned()) {
            port::TurnPicture(shown.data(), across, upright);
            frameShown = upright.data();
            std::swap(across, down);
        }
        const port::Shape shape = port::DisplayShape() == port::Shape::Fill ? port::Shape::Arcade : port::DisplayShape();
        replay.recorder.Picture(frameShown, across, down, port::PictureShape(across, down, shape, 4.0f / 3.0f), 1);
        if (std::chrono::steady_clock::now() - began > std::chrono::milliseconds(4)) {
            break;
        }
    }
    NativeVideo::inSpare = false;
    KeepState(replay.state);
    PutBackState(live);
    SetRunningAhead(false);
    memcpy(machine->Picture(), picture.data(), pixels * sizeof(uint32_t));
    memcpy(machine->players, held, sizeof(held));
    machine->serviceButton = service;
    machine->testSwitch = testOn;
    machine->video.hideObject = hide;
    machine->video.layerMask = layers;
    machine->video.markLayers = markLayers;
    machine->video.markObjects = mark;
    machine->video.wide = wide;
    machine->audio.resize(sound);
    MarkPictureMade(); // (the window draws the CPU's picture, the port's, this tick: gpu_picture.h)
    if (replay.at < replay.end) {
        return;
    }
    const std::string path = replay.recorder.Path();
    const double seconds = replay.recorder.Seconds();
    const bool saved = replay.recorder.Stop();
    replay.making = false;
    replay.from.clear();
    replay.made.clear();
    replay.state.clear();
    fprintf(stderr, "replay: %s %s (%.1f s)\n", saved ? "saved" : "not saved", path.c_str(), seconds);
    const size_t slash = path.find_last_of('/');
    shotSaid = saved ? std::string(recomp::Tr("REPLAY SAVED")) + "  videos/" + (slash == std::string::npos ? path : path.substr(slash + 1))
                     : recomp::Tr("REPLAY NOT SAVED");
    shotTicks = kSpeedTicks;
}

void Port::StageSweepFrame() {
    switch (modes::StageSweep()) {
    case modes::Sweep::None: break;
    case modes::Sweep::Settling:
        if (!stageStitch.Begun()) {
            stageStitch.Begin(modes::SweepCamera());
            shotSaid = recomp::Tr("SAVING WHOLE STAGE...");
            shotTicks = 1 << 30; // (until it is saved)
        }
        break;
    case modes::Sweep::Across: stageStitch.Add(*machine, modes::SweepCamera()); break;
    case modes::Sweep::Done: {
        const std::string stage = modes::SweepStageName();
        std::string name = std::string(GAME_ID " ") + stage;
        for (char &c : name) {
            if (strchr("/\\:*?\"<>|", c)) {
                c = ' ';
            }
        }
        stageStitch.svg = FeatureOn("export_svg"); // (stage.svg too: the Game page's SVG pictures too)
        port::SvgSize(1, 1, stageStitch.svgPixelWidth, stageStitch.svgPixelHeight);
        if (!scene.WriteStage(std::move(stageStitch), PlayerFolder() + "exports/stages/", name, stage)) {
            shotSaid = recomp::Tr("A STAGE IS STILL BEING SAVED");
            shotTicks = kSpeedTicks;
        }
        stageStitch = StageStitch();
        modes::EndStageSweep();
        break;
    }
    case modes::Sweep::Refused: {
        // (a stage begun and ended under it: a state loaded, the viewer left; or Infinite chosen)
        const bool begun = stageStitch.Begun();
        stageStitch.End();
        shotSaid = recomp::Tr(begun ? "WHOLE STAGE NOT SAVED" : "CHOOSE A STAGE (NOT INFINITE) TO SAVE");
        shotTicks = kSpeedTicks;
        modes::EndStageSweep();
        break;
    }
    }
}

bool Port::SaveState(const std::string &path, std::string &error) {
    std::vector<uint8_t> data;
    Archive ar(data, true);
    ar.Mark(0x43505332);
    uint32_t version = kStateVersion;
    ar.Pod(version);
    machine->Serialize(ar);
    if (TheGame().state) {
        TheGame().state(ar);
    }
    if (!WriteFile(path, data.data(), data.size())) {
        error = "cannot write " + path;
        return false;
    }
    return true;
}

bool Port::LoadState(const std::string &path, std::string &error) {
    std::vector<uint8_t> data;
    if (!ReadFile(path, data)) {
        error = "cannot read " + path;
        return false;
    }
    Archive ar(data, false);
    ar.Mark(0x43505332);
    uint32_t version = 0;
    ar.Pod(version);
    const bool known = version >= 5 && version <= kStateVersion;
    ar.version = version;
    if (ar.Ok() && known) {
        std::vector<uint8_t> previous;
        KeepState(previous);
        machine->Serialize(ar);
        if (ar.Ok() && TheGame().state) {
            TheGame().state(ar);
        }
        if (ar.Ok() && ar.AtEnd()) {
            return true;
        }
        PutBackState(previous);
    }
    error = path + " is not a state this build wrote";
    return false;
}

// --music-test: each piece of the game's music (machine/soundtrack.h) on the board in turn, opt.musicTest frames each,
// and how long it made a sound for: a song that ends of itself is quiet well before the next (a fanfare, a game over)
void Port::MusicTest() {
    uint8_t *shared = machine->soundRam;
    const std::vector<uint16_t> &songs = machine->soundtrack.MusicSongs();
    // (the sound made since the last frame: when it was last heard)
    int64_t loudest = 0;
    for (size_t n = 0; n < machine->audio.size(); n++) {
        loudest = std::max<int64_t>(loudest, std::abs(static_cast<int>(machine->audio[n])));
    }
    if (loudest > 64 && musicTestAt) {
        musicTestHeard = machine->frame;
    }
    if (shared[0xFFD] != 0x88 || shared[0xFFF] != 0xFF || machine->frame < 600 || shared[0x00F] != 0xFF ||
        (musicTestAt && machine->frame - musicTestAt < opt.musicTest)) {
        return;
    }
    if (musicTestAt) {
        const uint16_t song = songs[musicTestIndex - 1];
        const uint64_t heard = musicTestHeard >= musicTestAt ? musicTestHeard - musicTestAt : 0;
        printf("music %02X: sound for %.1f s of %.1f%s\n", song, heard / Machine::kFramesPerSecond, opt.musicTest / Machine::kFramesPerSecond,
               heard + 120 < opt.musicTest ? " (ends)" : "");
        fflush(stdout);
    }
    if (musicTestIndex >= songs.size()) {
        quit = true;
        return;
    }
    const uint16_t song = songs[musicTestIndex++];
    uint8_t command[16] = {static_cast<uint8_t>(song >> 8), static_cast<uint8_t>(song), 0, 0, 0, 0, 0, 0x10};
    memcpy(shared, command, 15);
    shared[0x00F] = 0;
    musicTestAt = machine->frame;
    musicTestHeard = 0;
}

// --sound-test: once the game has the sound side going, a command every so many frames in place of whatever
// the game sends: each song in turn with made-up placing, and now and then one of the other commands with
// made-up numbers. For --sound check: far more of the driver than a game's play reaches.
void Port::SoundTest() {
    uint8_t *shared = machine->soundRam;
    machine->monoForTest = opt.soundTest % 4 == 0; // (every fourth seed)
    // (not at once: the sound program takes the game's stereo setting in its first interrupts)
    if (shared[0xFFD] != 0x88 || shared[0xFFF] != 0xFF || machine->frame < 400 || machine->frame % opt.soundTestEvery ||
        shared[0x00F] != 0xFF) {
        return;
    }
    auto random = [&]() {
        soundTestState = soundTestState * 6364136223846793005ull + 1442695040888963407ull;
        return static_cast<uint32_t>(soundTestState >> 33);
    };
    if (!soundTestState) {
        soundTestState = opt.soundTest;
        // Songs of the kinds this game's sound ROM may have none of, made up and put in the ROM's empty end
        // under song numbers the game does not use: single samples, ADPCM samples, and sequences with the
        // commands sequences seldom have (the driver's layout: sound/driver.cpp)
        std::vector<uint8_t> &rom = roms.soundProgram;
        const uint32_t table = static_cast<uint32_t>(rom[0x3FE0] | (rom[0x3FE1] << 8));
        const unsigned songs = static_cast<unsigned>((rom[table] << 8) | rom[table + 1]);
        uint32_t at = 0x30000;
        unsigned slot = 0;
        auto add = [&](const std::vector<uint8_t> &song) {
            for (; slot < songs; slot++) {
                uint8_t *entry = &rom[table + 5 + slot * 4];
                if (!(entry[0] | entry[1] | entry[2]) && at + song.size() <= rom.size()) {
                    entry[0] = static_cast<uint8_t>(at >> 16), entry[1] = static_cast<uint8_t>(at >> 8), entry[2] = static_cast<uint8_t>(at);
                    memcpy(&rom[at], song.data(), song.size());
                    at += static_cast<uint32_t>(song.size());
                    slot++;
                    return;
                }
            }
        };
        auto byte = [&](uint32_t range) {
            return static_cast<uint8_t>(random() % range);
        };
        auto track = [&](bool music) {
            std::vector<uint8_t> t = {0xC1, 0x01, byte(256), 0xC2, 0x00, 0xC4, byte(60), 0xC6, static_cast<uint8_t>(0x40 + byte(0x40)),
                                      0xC8, static_cast<uint8_t>(0x40 + byte(0x40)), 0xDE, byte(256), 0xDF, byte(256), 0xDF, byte(256),
                                      0xDC, byte(256), 0xDD, byte(256), 0xDD, byte(256), 0xC3, byte(256), 0xE1, byte(128), 0xC5, byte(128),
                                      0xE2, byte(128), 0xE0, byte(2), 0xE3, byte(0x30), 0xC9, byte(128), 0xE6, byte(128)};
            if (music) {
                t.insert(t.end(), {0xC7, byte(128)});
            }
            auto note = [&](bool tied) {
                t.insert(t.end(), {static_cast<uint8_t>(0x80 + byte(0x40)), static_cast<uint8_t>(0x24 + byte(0x30) + (tied ? 0x80 : 0))});
                if (byte(4) == 0) {
                    t.push_back(static_cast<uint8_t>(0x81 + byte(3)));
                }
                t.push_back(byte(128));
                if (byte(5) == 0) {
                    t.push_back(1);
                }
                t.push_back(static_cast<uint8_t>(1 + byte(60)));
            };
            auto jumpTo = [&](size_t operand, size_t target) {
                const int offset = static_cast<int>(target) - static_cast<int>(operand + 2);
                t[operand] = static_cast<uint8_t>(offset >> 8), t[operand + 1] = static_cast<uint8_t>(offset);
            };
            t.insert(t.end(), {0xC0, 0xD3});
            note(false);
            const size_t out = t.size() + 1;
            t.insert(t.end(), {0xDB, 0, 0});
            note(true);
            note(false);
            t.insert(t.end(), {0xD7, static_cast<uint8_t>(byte(4))});
            if (t.back() == 0) {
                t.back() = 2;
            }
            jumpTo(out, t.size());
            const size_t second = t.size() + 1;
            t.insert(t.end(), {0xCD, 0, 0, 0xCB});
            note(false);
            t.push_back(0xCA);
            jumpTo(second, t.size());
            note(false);
            const size_t end = t.size() + 1;
            t.insert(t.end(), {0xCC, 0, 0});
            note(false);
            jumpTo(end, t.size());
            t.insert(t.end(), {0xCB, 0xFF});
            return t;
        };
        // (and of the first table's instruments, some whose notes start again after a sound effect and take
        // the game's pitch shift, and some that are silent once they have decayed)
        const uint32_t instruments = static_cast<uint32_t>(rom[0x3FE4] | (rom[0x3FE5] << 8));
        for (unsigned n = 40; n < 60; n++) {
            if (n < 50) {
                rom[instruments + n * 8 + 5] = 0;
            } else {
                rom[instruments + n * 8 + 1] |= 0x80;
            }
        }
        for (int n = 0; n < 150; n++) {
            const uint32_t kind = random() % 5;
            if (kind < 2) {
                const bool instrument = random() % 2;
                add({static_cast<uint8_t>(0x80 + byte(16)), static_cast<uint8_t>(byte(0x20) | (byte(6) ? 0 : 0x80)), 0,
                     static_cast<uint8_t>(1 + byte(120)), 0, byte(100), byte(128), static_cast<uint8_t>(instrument ? 0x80 + byte(2) : 0),
                     byte(60), static_cast<uint8_t>(0x30 + byte(0x30)), byte(256)});
            } else if (kind == 2) {
                add({static_cast<uint8_t>(0x90 + byte(3)), static_cast<uint8_t>(byte(0x20) | (byte(4) ? 0 : 0x80)), byte(128), 0, byte(60)});
            } else {
                const bool music = kind == 4 && random() % 3 == 0;
                std::vector<uint8_t> song(33, 0);
                song[0] = music ? 0 : static_cast<uint8_t>(1 + byte(0x30));
                for (unsigned voice = 0; voice < 16; voice++) {
                    if (random() % 4 == 0) {
                        song[1 + voice * 2] = static_cast<uint8_t>(song.size() >> 8), song[2 + voice * 2] = static_cast<uint8_t>(song.size());
                        const std::vector<uint8_t> t = track(music);
                        song.insert(song.end(), t.begin(), t.end());
                    }
                }
                add(song);
            }
        }
    }
    uint8_t command[16] = {};
    const uint32_t pick = random() % 16;
    if (pick < 11) {
        command[0] = static_cast<uint8_t>(soundTestSong >> 8);
        command[1] = static_cast<uint8_t>(soundTestSong);
        soundTestSong++;
        command[6] = static_cast<uint8_t>(random() % 3 ? random() % 8 : 0);
        command[7] = static_cast<uint8_t>(random() % 0x24);
        command[8] = static_cast<uint8_t>(random() % 0x24);
        command[9] = static_cast<uint8_t>(random() % 3);
        command[10] = static_cast<uint8_t>(random());
        command[11] = static_cast<uint8_t>(random() % 3);
        command[12] = static_cast<uint8_t>(random());
    } else {
        static const uint8_t others[] = {0, 1, 2, 2, 3, 4, 5, 6, 7, 6, 7, 8, 9, 12, 13, 12, 13, 8, 9};
        command[0] = 0xFF;
        command[1] = others[random() % sizeof(others)];
        command[2] = static_cast<uint8_t>(random());
        command[3] = static_cast<uint8_t>(random() % 2 ? 0xFF : random() % 4);
        command[4] = static_cast<uint8_t>(command[1] == 2 ? random() % 16 : command[1] == 4 ? random() % 3 : random());
    }
    memcpy(shared, command, 15);
    shared[0x00F] = 0;
}

void Port::KeepState(std::vector<uint8_t> &data) {
    data.clear();
    Archive ar(data, true);
    machine->Serialize(ar);
    if (TheGame().state) {
        TheGame().state(ar);
    }
    ar.Pod(onlineReselect);
}

void Port::PutBackState(const std::vector<uint8_t> &data) {
    std::vector<uint8_t> &from = const_cast<std::vector<uint8_t> &>(data); // (an Archive that reads does not change it)
    Archive ar(from, false);
    machine->Serialize(ar);
    if (TheGame().state) {
        TheGame().state(ar);
    }
    ar.Pod(onlineReselect);
}

void Port::Step(const uint16_t *players) {
    memcpy(machine->players, players, sizeof(machine->players));
    if (onlineReselect.boot >= 0) {
        OnlineReselectBoot();
    }
    ApplyBoardOptions(*machine);
    ApplyCheats();
    if (TheGame().beforeFrame) {
        TheGame().beforeFrame(*machine);
    }
    // (the zoom by distance leaves the game's panel where it is: the frame drawn with it apart, zoom.h)
    machine->panel = zoom.KeepsPanel() || (TheGame().framedColumns && TheGame().framedColumns() > 0)
                         ? Machine::Panel{TheGame().panel, TheGame().panelLayers}
                         : Machine::Panel{};
    machine->RunFrame();
    if (opt.musicTest) {
        MusicTest();
    }
    if (opt.soundTest) {
        SoundTest();
    }
    if (TheGame().frame) {
        TheGame().frame(*machine);
    }
}

// --rollback-test (port.h, RollbackTest): the frame run with the others' controls guessed where they have not
// come, then, as the others' controls come, the frames guessed wrong run again
void Port::RollbackFrame(const uint16_t *real) {
    RollbackTest &t = rollbackTest;
    using Clock = std::chrono::steady_clock;
    const auto seconds = [](Clock::time_point since) { return std::chrono::duration<double>(Clock::now() - since).count(); };
    const Clock::time_point began = Clock::now();
    const auto keep = [&](RollbackTest::Kept &k) {
        const Clock::time_point at = Clock::now();
        KeepState(k.state);
        k.sound = machine->audio.size();
        t.keepTime += seconds(at);
        t.keeps++;
        t.stateSize = k.state.size();
    };
    const auto step = [&](const Movie::Pads &used) {
        const Clock::time_point at = Clock::now();
        Step(used.data());
        t.stepTime += seconds(at);
        t.steps++;
    };
    const auto guessed = [&](const uint16_t *pads, uint64_t at) {
        Movie::Pads used;
        std::copy(pads, pads + Machine::kMaxPlayers, used.begin());
        if (at >= t.confirmed) {
            const Movie::Pads last = t.confirmed ? t.kept[(t.confirmed - 1) % RollbackTest::kKept].real : Movie::Pads{};
            std::copy(last.begin() + 1, last.end(), used.begin() + 1);
        }
        return used;
    };
    RollbackTest::Kept &now = t.kept[frame % RollbackTest::kKept];
    keep(now);
    std::copy(real, real + Machine::kMaxPlayers, now.real.begin());
    now.used = guessed(real, frame);
    step(now.used);
    now.hash = hashFile ? StateHash() : 0;
    // The others' controls come up to here (the lag changes now and then, and never takes back what has come)
    if (Random(t.random) % 20 == 0) {
        t.lag = static_cast<int>(Random(t.random) % (RollbackTest::kMostLag + 1));
    }
    if (opt.frames && frame + 1 >= opt.frames) {
        t.lag = 0; // (the run's last frame: everything has come, so that what it ends on can be compared)
    }
    const uint64_t upTo = std::max<uint64_t>(t.confirmed, frame + 1 >= static_cast<uint64_t>(t.lag) ? frame + 1 - t.lag : 0);
    uint64_t wrong = upTo;
    for (uint64_t at = t.confirmed; at < upTo; at++) {
        const RollbackTest::Kept &k = t.kept[at % RollbackTest::kKept];
        if (k.used != k.real) {
            wrong = at;
            break;
        }
    }
    // (the hash log has a frame once its controls are the real ones and it has been run with them)
    const auto logConfirmed = [&](uint64_t from) {
        for (uint64_t at = from; hashFile && at < upTo; at++) {
            fprintf(hashFile, "%llu %08x\n", static_cast<unsigned long long>(at + 1), t.kept[at % RollbackTest::kKept].hash);
        }
    };
    const uint64_t from = t.confirmed;
    t.confirmed = upTo;
    if (wrong == upTo) {
        logConfirmed(from);
        t.frameTimes.push_back(static_cast<float>(seconds(began)));
        return;
    }
    t.rollbacks++;
    t.rerun += frame + 1 - wrong;
    t.most = std::max<uint64_t>(t.most, frame + 1 - wrong);
    const Clock::time_point putting = Clock::now();
    PutBackState(t.kept[wrong % RollbackTest::kKept].state);
    t.putBackTime += seconds(putting);
    t.putBacks++;
    machine->audio.resize(t.kept[wrong % RollbackTest::kKept].sound);
    for (uint64_t at = wrong; at <= frame; at++) {
        RollbackTest::Kept &k = t.kept[at % RollbackTest::kKept];
        if (at != wrong) {
            keep(k);
        }
        k.used = guessed(k.real.data(), at);
        // (only the last frame's picture is seen; a hash log with the picture in it has them all drawn)
        machine->drawing = at == frame || (hashFile && !opt.hashNoPicture);
        step(k.used);
        k.hash = hashFile ? StateHash() : 0;
    }
    logConfirmed(from);
    t.frameTimes.push_back(static_cast<float>(seconds(began)));
}

// Run-ahead: after the frame, the frames after it are run with the controls as they are and the picture (and
// the readout) of the last is the one shown; then the machine and the game's sources are put back as they were
// after the frame, and its sound is the frame's own. So the game goes on exactly as without it, and the picture
// shows what a press does a frame or two sooner. Not done for a frame a spare machine ran in (runtime/spare.h).
int Port::RunsAhead() const {
    const int ahead = FeatureValue("run_ahead");
    // (nor while stopped, stepped a frame at a time or slowed: there the frame shown is the frame; online, nor for a
    // spectator, whose frames are all the players' real ones, or while the host's pause holds the game)
    if (ahead <= 0 || FastForward() || hurry || SpareRan() || paused || !menu.FullSpeed() || rewind.was ||
        (TheGame().noRunAhead && TheGame().noRunAhead()) || (net && (connecting || net->Watching()))) {
        return 0;
    }
    return ahead;
}

// (online: from the frame shown, its controls every player's, the others' as guessed where they have not come;
// rollback puts right what the frames ahead guessed wrong, the next frame)
void Port::RunAhead() {
    const int ahead = RunsAhead();
    if (!ahead) {
        return;
    }
    static std::vector<uint8_t> kept;
    KeepState(kept);
    const size_t sound = machine->audio.size();
    uint16_t held[Machine::kMaxPlayers];
    memcpy(held, machine->players, sizeof(held));
    SetRunningAhead(true);
    for (int n = 0; n < ahead; n++) {
        memcpy(machine->players, held, sizeof(held));
        if (TheGame().beforeFrame) {
            TheGame().beforeFrame(*machine);
        }
        machine->RunFrame();
        if (TheGame().frame) {
            TheGame().frame(*machine);
        }
    }
    NativeVideo::keepLastFrame = true; // (the frame shown is the last ahead: still the scene's)
    PutBackState(kept);
    NativeVideo::keepLastFrame = false;
    SetRunningAhead(false);
    machine->audio.resize(sound);
    memcpy(machine->players, held, sizeof(held));
}

// The input lag readout's game part (Port::Lag): a few times, a few seconds apart, in a fight, the next frames run from
// the frame just run, with nothing held and then with player 1's light punch held, until the pictures part; then the
// machine, the game's sources and what a state does not hold (the picture, the controls, the video's switches) put
// back as they were, as for a spare (runtime/spare.h). Its frames are not the picture shown, nor counted (records,
// loading's resets: runtime/game.h). Online the frames are this machine's alone, like run-ahead's.
void Port::LagMeasure() {
    const bool on = FeatureOn("input_lag") && !opt.rollbackTest && !(net && (connecting || net->Watching()));
    if (on != lag.wasOn) {
        lag = {};
        lag.wasOn = on;
    }
    const Game &game = TheGame();
    if (!on || lag.tries >= Lag::kTries || frame < lag.nextAt || FastForward() || hurry || paused || SpareRan() ||
        !game.fighting || !game.fighting()) {
        return;
    }
    lag.nextAt = frame + 240;
    static std::vector<uint8_t> kept;
    KeepState(kept);
    uint16_t held[Machine::kMaxPlayers];
    memcpy(held, machine->players, sizeof(held));
    const auto hide = machine->video.hideObject;
    const unsigned layers = machine->video.layerMask, markLayers = machine->video.markLayers;
    const bool mark = machine->video.markObjects, drawing = machine->drawing;
    const auto wide = machine->video.wide;
    const size_t sound = machine->audio.size();
    const size_t pixels = static_cast<size_t>(Video::kMaxWidth) * Video::kHeight;
    static std::vector<uint32_t> picture;
    picture.assign(machine->Picture(), machine->Picture() + pixels);
    SetRunningAhead(true);
    NativeVideo::inSpare = true; // (not the picture shown: video/native.h, ShownFrame)
    machine->drawing = true;
    uint32_t plain[Lag::kFrames] = {};
    int found = 0;
    for (int way = 0; way < 2 && !found; way++) {
        if (way) {
            PutBackState(kept);
        }
        for (int n = 0; n < Lag::kFrames; n++) {
            memset(machine->players, 0, sizeof(machine->players));
            machine->players[0] = way ? kPunch1 : 0;
            if (game.beforeFrame) {
                game.beforeFrame(*machine);
            }
            machine->RunFrame();
            if (game.frame) {
                game.frame(*machine);
            }
            const uint32_t at = static_cast<uint32_t>(
                crc32(0, reinterpret_cast<const Bytef *>(machine->Picture()),
                      static_cast<uInt>(sizeof(uint32_t) * machine->PictureWidth() * Video::kHeight)));
            if (!way) {
                plain[n] = at;
            } else if (at != plain[n]) {
                found = n + 1;
                break;
            }
        }
    }
    NativeVideo::keepLastFrame = true; // (the frame shown is still the one just run: video/native.h)
    PutBackState(kept);
    NativeVideo::keepLastFrame = false;
    NativeVideo::inSpare = false;
    SetRunningAhead(false);
    memcpy(machine->Picture(), picture.data(), pixels * sizeof(uint32_t));
    memcpy(machine->players, held, sizeof(held));
    machine->drawing = drawing;
    machine->video.hideObject = hide;
    machine->video.layerMask = layers;
    machine->video.markLayers = markLayers;
    machine->video.markObjects = mark;
    machine->video.wide = wide;
    machine->audio.resize(sound);
    MarkPictureMade(); // (the window draws the CPU's picture, the port's, this tick: gpu_picture.h)
    if (found) {
        lag.game = lag.game ? std::min(lag.game, found) : found;
        lag.tries++;
        fprintf(stderr, "input lag: the game shows a press %d frames on (try %d; the least so far %d)\n", found, lag.tries, lag.game);
    }
}

void Port::LagPressed(uint64_t ns) {
    if (!lag.pressedAt && !menu.IsOpen()) {
        lag.pressedAt = ns;
    }
}

void Port::LagShown(bool ran, uint64_t ns) {
    if (!lag.pressedAt || !ran) {
        return;
    }
    const double ms = static_cast<double>(ns - lag.pressedAt) / 1e6;
    lag.pressedAt = 0;
    if (ms < 0 || ms > 250) {
        return; // (a press while the game was stopped, or the like)
    }
    lag.portMs = lag.presses ? lag.portMs * 0.9 + ms * 0.1 : ms;
    lag.presses++;
}

std::string Port::LagLine() const {
    if (!FeatureOn("input_lag") || (net && net->Watching())) {
        return {};
    }
    const int ahead = RunsAhead();
    const int delay = net && !connecting ? net->Delay() : 0;
    const double frameMs = 1000.0 / Machine::kFramesPerSecond;
    const double port = lag.presses ? lag.portMs : frameMs;
    char text[128];
    if (!lag.game) {
        if (!TheGame().fighting) {
            snprintf(text, sizeof(text), "INPUT LAG  PORT %.0f MS", port);
        } else if (delay) {
            snprintf(text, sizeof(text), "INPUT LAG  GAME ? (IN A FIGHT)  DELAY %d  PORT %.0f MS", delay, port);
        } else {
            snprintf(text, sizeof(text), "INPUT LAG  GAME ? (IN A FIGHT)  PORT %.0f MS", port);
        }
        return text;
    }
    // (the frame the press is first in is the port's time; the game's other frames after it and the match's delay,
    // less run-ahead's)
    const int after = std::max(0, lag.game - 1 + delay - ahead);
    std::string line = "INPUT LAG  GAME " + std::to_string(lag.game) + "F";
    if (delay) {
        line += "  DELAY " + std::to_string(delay);
    }
    if (ahead) {
        line += "  RUN-AHEAD -" + std::to_string(ahead);
    }
    snprintf(text, sizeof(text), "  PORT %.0f MS  = %.0f MS", port, after * frameMs + port);
    return line + text;
}

// Online: a round's result, the frame it comes (once: a second's quiet after it before the next, so that a frame run
// again in a rollback is not counted twice), and a match won when a player has won its rounds
void Port::Streaks() {
    const int result = battle::RoundResultNow();
    if (result < 0) {
        return; // (not a fighting game)
    }
    if (result == 0) {
        if (++roundQuiet >= 60) {
            roundCounted = false;
        }
        return;
    }
    roundQuiet = 0;
    if (roundCounted) {
        return;
    }
    roundCounted = true;
    if (result == 3 || !cameIn || !otherIn) {
        return; // (a draw; or a fight against the computer, before both have come in)
    }
    const int winner = result - 1;
    if (++roundsWon[winner] >= battle::RoundsToWin()) {
        streak[winner]++;
        streak[1 - winner] = 0;
        roundsWon[0] = roundsWon[1] = 0;
        if (net->Players() > 2) {
            fprintf(stderr, "online: %s team won the match (%d in a row)\n", winner ? "right" : "left", streak[winner]);
        } else {
            fprintf(stderr, "online: player %d won the match (%d in a row)\n", winner + 1, streak[winner]);
        }
    }
}

// Online play (netplay.h): the other side found and agreed with before the first frame; with a window, a step a tick
// (Tick), the wait shown on the screen, and without, here
// The chat's lines, each said for eight seconds, five at most (the oldest go first)
namespace {
    constexpr uint64_t kChatTicks = 8 * 60;
    constexpr size_t kChatLines = 5;
}

void Port::Say() {
    std::string line = typed;
    typing = false;
    typed.clear();
    if (!net || connecting) {
        return;
    }
    net->Say(line);
    const std::string name = net->Watching() ? net->OwnName() : net->MachineName(net->Local());
    if (line.find_first_not_of(' ') != std::string::npos) {
        chat.push_back({(name.empty() ? "You" : name) + ": " + line.substr(0, Netplay::kChatMost), tick + kChatTicks, true});
    }
}

void Port::HearChat() {
    for (const auto &[machine, line] : net->TakeHeard()) {
        if (machine < 0) { // (a spectator's: its name and the line, as said)
            fprintf(stderr, "online: chat: %s\n", line.c_str());
            chat.push_back({line, tick + kChatTicks, false});
            continue;
        }
        const std::string name = net->MachineName(machine);
        fprintf(stderr, "online: chat: %s: %s\n", name.empty() ? "(no name)" : name.c_str(), line.c_str());
        chat.push_back({(name.empty() ? (net->Machines() > 2 ? "Player " + std::to_string(net->Player(machine, 0) + 1) : "Them") : name) + ": " + line,
                        tick + kChatTicks, false});
    }
    while (!chat.empty() && (chat.size() > kChatLines || chat.front().until <= tick)) {
        chat.pop_front();
    }
}

#ifdef _WIN32
static const char *const kExeSuffix = ".exe";
#else
static const char *const kExeSuffix = "";
#endif

// The launcher's player name ([online] name in its launcher.ini: the collection's, or the game's own launcher's),
// for a match hosted from the in-game menu
std::filesystem::path LauncherFolder() {
    const char *base = SDL_GetBasePath();
    if (!base) {
        return {};
    }
    std::error_code missing;
    const bool collection = !std::filesystem::exists(std::string(base) + GAME_ID "-launcher" + kExeSuffix, missing);
    const std::string name = recomp::FolderName(collection ? "CPS-2 Recomp" : GAME_NAME);  // (as the launcher names it)
    std::filesystem::path folder = recomp::PortableFolder(base);
    if (!folder.empty()) {
        folder /= std::filesystem::u8path(name);
    } else if (char *pref = SDL_GetPrefPath("", name.c_str())) {
        folder = pref;
        SDL_free(pref);
    }
    return folder;
}

namespace {
    // The online settings of a match this machine hosts or joins: its own input delay (the in-game menu's Your input
    // delay, else the launcher's, as said with --launcher-net-delay or kept in launcher.ini; --net-delay over both), and,
    // hosting, whether it is in the lobby and lets viewers watch (from the in-game menu: the launcher's name and choices)
    void ApplyOnlineSettings(Netplay::Settings &settings, const Options &options, bool fromMenu) {
        const int chosen = FeatureSetting("online_host_delay");
        int global = options.launcherNetDelay;
        const bool hosting = settings.hosting || fromMenu; // (from the in-game menu: hosting, always)
        if (hosting) {
            settings.spectators = options.netSpectatorsRequested;
        }
        if (fromMenu || (global < 0 && chosen == 0 && options.netDelayOverride < 0)) {
            const std::filesystem::path folder = LauncherFolder();
            if (!folder.empty()) {
                recomp::Ini prefs(folder / "launcher.ini");
                prefs.load();
                if (fromMenu) {
                    settings.name = prefs.get("online", "name");
                    settings.spectators = settings.spectators || prefs.get("online", "spectators") != "0"; // (on unless switched off)
                }
                const std::string saved = prefs.get("online", "delay");
                if (global < 0 && saved.size() == 1 && saved[0] >= '0' && saved[0] <= '8') global = saved[0] - '0';
            }
        }
        if (fromMenu) settings.listed = !settings.name.empty();
        settings.delay = global >= 0 ? global : Netplay::kDefaultDelay;
        if (chosen > 0) settings.delay = chosen - 1;
        if (options.netDelayOverride >= 0) settings.delay = options.netDelayOverride;
        settings.runAhead = std::max(0, FeatureValue("run_ahead")); // (said to the others, for their network readout)
        if (hosting && FeatureOn("online_unlisted")) settings.listed = false;
    }
}

void Port::HostFromMenu() {
    menu.Close();
    if (waitingHost) {
        StopHosting(); // (the row again: Stop hosting)
        return;
    }
    if (net || opt.headless) {
        return;
    }
    Netplay::Settings settings = opt.net;
    settings.hosting = true;
    settings.solo = true;
    settings.arcade = false;
    settings.port = Netplay::kDefaultPort;
    ApplyOnlineSettings(settings, opt, true);
    settings.renewCode.clear();
    settings.renewSecret.clear();
    waitingHost = std::make_unique<Netplay>(settings);
    std::string error;
    if (!waitingHost->Begin(*this, error)) {
        waitingHost.reset();
        fprintf(stderr, "online: %s\n", error.c_str());
        menu.Open(true);
        menu.Note(error);
        return;
    }
    opt.net = settings;
    hostedFromMenu = true;
    menu.hosting = true;
    fprintf(stderr, "online: hosting from the menu; playing alone until someone joins\n");
}

void Port::PumpHost() {
    menu.hosting = waitingHost != nullptr;
    if (!waitingHost) {
        return;
    }
    Netplay *host = waitingHost.get();
    std::string error;
    if (host->Waiting(*this, error) < 0 && waitingHost.get() == host) {
        fprintf(stderr, "online: stopped hosting: %s\n", error.c_str());
        hostNote = "Stopped hosting: " + error;
        hostNoteUntil = tick + 300;
        StopHosting();
    }
}

void Port::MatchComing() {
    fprintf(stderr, "online: someone joined: the match starts from the board's reset\n");
    ownFeatures.clear();
    for (const Feature &feature : TheGame().features) {
        ownFeatures[feature.name] = FeatureSetting(feature.name);
    }
    menu.Close();
    menu.SetFullSpeed();
    paused = false;
    steps = 0;
    typing = false;
    EndSessionFeatures();
    machine->Reset();
    machine->audio.clear();
    zoom.Forbid();
    net = std::move(waitingHost); // (Netplay::Waiting goes on, as the match's)
    menu.hosting = false;
    connecting = true;
    cameIn = false;
    frame = 0;
}

void Port::StopHosting() {
    waitingHost.reset();
    hostedFromMenu = false;
    menu.hosting = false;
    fprintf(stderr, "online: stopped hosting\n");
}

void Port::BackToSolo(bool stillHosting) {
    Netplay::Settings settings = opt.net;
    if (net) {
        if (!net->code.empty()) {
            settings.renewCode = net->code.substr(0, 3) + net->code.substr(4);
            settings.renewSecret = net->Secret();
        }
        if (!connecting) {
            net->Leave(stillHosting);
            net->Report(true);
        }
    }
    net.reset(); // (the socket closed: the port is free for hosting again)
    connecting = leaving = typing = false;
    rejoin.clear();
    chat.clear();
    for (const auto &[name, value] : ownFeatures) {
        SetFeatureForNow(name.c_str(), value);
    }
    ownFeatures.clear();
    zoom.Forbid(false);
    machine->SetBoardTiming(BoardTimingWanted());
    hostedFromMenu = false;
    if (!stillHosting) {
        fprintf(stderr, "online: playing alone again\n");
        return;
    }
    ApplyOnlineSettings(settings, opt, true);
    settings.invite.clear();
    settings.inviteName.clear();
    waitingHost = std::make_unique<Netplay>(settings);
    std::string error;
    if (!waitingHost->Begin(*this, error)) {
        waitingHost.reset();
        fprintf(stderr, "online: %s\n", error.c_str());
        return;
    }
    opt.net = settings;
    hostedFromMenu = true;
    fprintf(stderr, "online: playing alone again, still hosting\n");
}

void Port::LeaveOnline() {
    leaving = false;
    if (hostedFromMenu) {
        BackToSolo(false);
    } else {
        quit = true;
    }
}


// The pause and the other player's Ready after the events of the frames before `before`: in each frame the host's
// (its setup, then its mode, which starts if it is Versus or the other player is ready, ending the pause and the
// Ready), then the other player's
Port::OnlineSetupState Port::OnlineSetupScan(uint64_t before) const {
    std::set<uint64_t> frames;
    for (const auto *log : {&onlineSetupLog, &onlineModeLog, &onlineReadyLog}) {
        for (auto it = log->begin(); it != log->end() && it->first < before; ++it) {
            frames.insert(it->first);
        }
    }
    OnlineSetupState state;
    uint64_t last = 0;
    const bool modes = ModesOnline();
    for (const uint64_t at : frames) {
        if (state.paused) {
            state.pausedFrames += at - last;
        }
        last = at;
        if (const auto setup = onlineSetupLog.find(at); setup != onlineSetupLog.end()) {
            state.paused = setup->second != 0;
        }
        if (const auto mode = onlineModeLog.find(at); mode != onlineModeLog.end() && (mode->second == 0 || !modes || state.ready)) {
            state.paused = state.ready = false;
        }
        if (const auto ready = onlineReadyLog.find(at); ready != onlineReadyLog.end()) {
            state.ready = ready->second != 0;
        }
    }
    if (state.paused && before > last) {
        state.pausedFrames += before - last;
    }
    return state;
}

bool Port::OnlinePausedAt(uint64_t at) const {
    return OnlineSetupScan(at + 1).paused;
}

bool Port::OnlineOtherReady() const {
    return OnlineSetupScan(UINT64_MAX).ready;
}

// Each tick of a match, the host's side: its match menu open is the pause for both, and a Start that waits for the
// other player's Ready is sent once it has come (the menu closed gives it up)
void Port::OnlineSetupFrame() {
    if (!net || connecting || net->Watching() || !net->Hosting() || net->Players() != 2 || !ModesOnline()) {
        return;
    }
    if (!menu.OnlineOpen()) {
        onlinePendingMode = -1;
    }
    if (onlinePendingMode >= 0 && OnlineOtherReady()) {
        const uint32_t seed = static_cast<uint32_t>(std::random_device()()) | 1;
        net->Command("mode=" + std::to_string(onlinePendingMode) + ":" + std::to_string(seed));
        onlinePendingMode = -1;
        menu.Close();
    }
    const bool want = menu.OnlineOpen();
    if (want != onlineSetupSent) {
        net->Command(want ? "setup=1" : "setup=0");
        onlineSetupSent = want;
    }
}

bool Port::OnlineReselectHere() const {
    return net && !net->Watching() && net->Players() == 2 && modes::Described().playersIn != 0;
}

// The board started again for both players' character choice: credits, then the Starts, until both are choosing (the
// players' own controls left out meanwhile). As session.cpp's Boot, but at the game's own speed, a frame of the match.
void Port::OnlineReselectBoot() {
    constexpr int32_t kMost = 3600; // (frames: given up after a minute)
    const modes::Description &m = modes::Described();
    for (uint16_t &p : machine->players) p = 0;
    if (modes::Choosing() || Ram8(m.playersIn) == 3 || ++onlineReselect.boot > kMost) {
        onlineReselect.boot = -1;
        return;
    }
    const bool press = onlineReselect.boot % 16 < 6;
    uint16_t &one = machine->players[0], &two = machine->players[1];
    if (Ram8(m.credits) < 2 && !FeatureOn("free_play")) {
        one = press ? kCoin : 0;
    } else if (m.startsTogether) {
        one = two = press ? kStart : 0;
    } else if (!(Ram8(m.playersIn) & 1)) {
        one = press ? kStart : 0;
    } else {
        two = press ? kStart : 0;
    }
}

// (player: the event's sender's machine, 0 the host's; side: the board's player that machine plays at the event's frame,
// whose fighters' rows it picks: Swap sides moves them)
void Port::OnlineEvent(int player, int side, const std::string &text, uint64_t eventFrame) {
    const uint64_t at = eventFrame; // (the frame the event is for: the pause's and Ready's logs keep it)
    for (size_t place = 0; place < text.size();) {
        const OnlinePickHistory::Position position{eventFrame, player, place};
        const size_t comma = std::min(text.find(',', place), text.size());
        const std::string item = text.substr(place, comma - place);
        place = comma + 1;
        const size_t eq = item.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        const std::string name = item.substr(0, eq);
        if (name == "reselect" && (player == 0 || player == 1)) {
            onlineReselect.asked[player] = atoi(item.c_str() + eq + 1) != 0;
            if (onlineReselect.asked[0] && onlineReselect.asked[1] && modes::Described().playersIn) {
                fprintf(stderr, "online: both asked to pick characters again: the board starts again at frame %llu\n",
                        static_cast<unsigned long long>(at));
                battle::StartOnline(*machine, 0, 1); // (the mode ended, the board reset: Back to Versus)
                onlineReselect = {};
                onlineReselect.boot = 0;
            }
        } else if (name == "restart") {
            // (either player's Start over in a training session: both machines at the place its sender chose)
            const int place = atoi(item.c_str() + eq + 1);
            if (session::StartOverAt(*machine, place)) {
                fprintf(stderr, "online: training started over at frame %llu (place %d)\n", static_cast<unsigned long long>(at), place);
            }
        } else if (name == "setup" && player == 0) {
            onlineSetupLog[at] = atoi(item.c_str() + eq + 1) != 0;
        } else if (name == "ready" && player == 1) { // (the other player's: their Ready, Port::onlineReadyLog)
            onlineReadyLog[at] = atoi(item.c_str() + eq + 1) != 0;
            if (net && net->Local() == 1) {
                onlineReadyMine = -1; // (its event has run: the log says it now)
            }
        } else if (name == "mode" && player == 0) {
            const int mode = atoi(item.c_str() + eq + 1);
            if (!OnlineModeHere(mode)) continue;
            onlineModeLog[at] = mode;
            if (mode == kOnlineTraining) {
                // (nothing to pick first: each player chooses their character in the game's own choice)
                fprintf(stderr, "online: training session started at frame %llu\n", static_cast<unsigned long long>(at));
                session::StartSession(*machine);
                continue;
            }
            if (mode == 0) {
                SetFeature("training_mode", false); // (Back to Versus ends a training session too)
            }
            if (mode != 0 && ModesOnline() && !OnlineSetupScan(at).ready) {
                // (the other player not ready before this frame, on both sides alike: not started)
                fprintf(stderr, "online: mode %d not started at frame %llu: the other player is not ready\n", mode,
                        static_cast<unsigned long long>(at));
                continue;
            }
            const size_t colon = item.find(':', eq);
            const uint32_t seed = colon == std::string::npos ? 1 : static_cast<uint32_t>(strtoul(item.c_str() + colon + 1, nullptr, 10));
            for (const auto &[row, log] : onlinePickLog) {
                SetFeatureForNow(row.c_str(), log.Before(position));
            }
            battle::StartOnline(*machine, mode, seed);
        } else if (OwnRow(side, name)) {
            const Feature *feature = FindFeature(name);
            const int value = atoi(item.c_str() + eq + 1);
            if (feature && value >= 0 && value < static_cast<int>(std::max<size_t>(feature->choices.size(), 2))) {
                auto it = onlinePickLog.try_emplace(name, FeatureValue(feature->name)).first;
                it->second.Record(position, value);
                SetFeatureForNow(feature->name, value);
                if (player == 1) {
                    onlineReadyLog[at] = 0; // (a fighter changed: not ready until said again)
                }
            }
        }
    }
}

void Port::OpenOnlineMenu() {
    leaving = false;
    if (!net || connecting || net->Watching()) {
        // (watching, or a host waiting: no match to choose for, only Esc's question: Confirm leaves, Back stays)
        leaving = true;
        return;
    }
    const int me = net->Local();
    Menu::Online online;
    onlineModes.clear();
    onlineRows.clear();
    if (net->Players() == 2 && net->Hosting()) {
        // (the host's: each mode this game has, a command that starts it for both, and Versus that ends it)
        for (const int mode : {1, 2, kOnlineTraining, 0}) {
            if (!OnlineModeHere(mode) || (mode == 0 && onlineModes.empty())) {
                continue;
            }
            onlineModes.push_back(mode);
            online.commands.push_back({Menu::kOnlinePage, mode == 0 ? "Back to Versus" : std::string("Start ") + kOnlineModes[mode],
                                       mode == 0 ? "Ends the mode for both players: the board starts again, and each presses Start to "
                                                   "come back in."
                                       : mode == kOnlineTraining
                                           ? "Starts a training session for both players: each chooses their own character, then no "
                                             "clock and no knock-outs, with your Training mode settings from before the match "
                                             "(health and gauge refills, fixed damage). Each player shows their own hit boxes, frame "
                                             "data and readouts (below). Backspace or a gamepad's left shoulder starts the round "
                                             "over for both."
                                           : std::string("Starts ") + kOnlineModes[mode] + " for both players once the other player "
                                             "has said Ready (until then it waits, the game paused), with your settings of its page "
                                             "from before the match, and each player's own picks below."});
        }
    }
    onlineSeats.clear();
    if (net->CanSeat()) {
        // (the host's: the people moved between the sides and partners, issue #448). Each move swaps the people at two
        // places, or two pairs of places, of the game's teams (SidePlayer): in a match of four the sides swapped, or the
        // left's first with each of the right's; in 2 vs 1 or 1 vs 2, who plays alone (the side of two stays the side
        // of two: that is the Tag team setting) or which partner goes first; in a match of two, the sides
        const int players = net->Players();
        const int left = SidePlayer(0, 0, players), leftTwo = SidePlayer(0, 1, players);
        const int right = SidePlayer(1, 0, players), rightTwo = SidePlayer(1, 1, players);
        struct Move {
            const char *what;
            std::vector<std::pair<int, int>> swaps;
        };
        std::vector<Move> moves;
        if (players == 2) {
            moves = {{"Swap sides", {{left, right}}}};
        } else if (players == 4) {
            moves = {{"Swap sides", {{left, right}, {leftTwo, rightTwo}}},
                     {"Change partners", {{leftTwo, right}}},
                     {"Change partners", {{leftTwo, rightTwo}}}};
        } else if (players == 3) {
            const bool soloLeft = leftTwo < 0;
            const int solo = soloLeft ? left : right, first = soloLeft ? right : left, second = soloLeft ? rightTwo : leftTwo;
            moves = {{"Change teams", {{solo, first}}}, {"Change teams", {{solo, second}}}, {"Swap partners", {{first, second}}}};
        }
        const Netplay::Layout now = net->SeatLayout();
        const bool two = players == 2;
        for (const Move &move : moves) {
            int to[Machine::kMaxPlayers] = {0, 1, 2, 3};
            bool valid = true;
            for (const auto &[a, b] : move.swaps) {
                valid = valid && a >= 0 && b >= 0;
                if (valid) std::swap(to[a], to[b]);
            }
            if (!valid) continue;
            Netplay::Layout moved{};
            for (int m = 0; m < net->Machines(); ++m)
                for (int person = 0; person < net->People(m); ++person) moved[m][person] = to[now[m][person]];
            onlineSeats.push_back(moved);
            online.commands.push_back({Menu::kOnlinePage, std::string(move.what) + ": " + TeamsSaid(moved),
                                       two ? "Swaps who plays player 1 and player 2 now, without leaving the match: player 1 "
                                             "first. Each player keeps their own controller, and their fighters' picks "
                                             "are the side's. The set's score starts again from 0-0."
                                           : "Moves everyone now, without leaving the match: the left team, then the right. Each "
                                             "player keeps their own controller. The set's score starts again from 0-0."});
        }
    }
    if (OnlineReselectHere()) {
        // (either player's: both back to the character choice once the other asks too; index after the modes and seats)
        const bool mine = onlineReselect.asked[me] != 0, theirs = onlineReselect.asked[1 - me] != 0;
        online.commands.push_back({Menu::kOnlinePage,
                                   mine ? "Pick characters again: asked (choose to take it back)"
                                        : theirs ? "Pick characters again: the other player asked, choose to agree"
                                                 : "Pick characters again",
                                   "Both players back to the character select, the winner too: once the other player "
                                   "chooses this as well, the game starts again and both of you are put in to choose. "
                                   "The other player is told that you asked. The set's score stays as it is."});
    }
    auto pick = [&](const std::string &feature, const std::string &title, const std::string &description) {
        const Feature *found = FindFeature(feature);
        if (!found) {
            return;
        }
        onlineRows.push_back(feature);
        online.features.push_back({Menu::kOnlinePage, title, description, {found->choices.begin(), found->choices.end()}});
    };
    if (net->Players() == 2) {
        const int side = net->Player(me, 0); // (the board's player this machine plays: the host's 1 unless swapped)
        pick(side == 0 ? "court_one" : "court_two", "Ball battle: your fighter",
             "Who you play in the ball battle: someone at random, or this one. The other player sees your choice.");
        if (FindFeature("team_size")) {
            const int size = FeatureValue("team_size") == 1 ? 3 : 2;
            static const char *const kOrder[3] = {"first", "second", "third"};
            for (int n = 0; n < size; n++) {
                pick("team_" + std::to_string(side + 1) + "_" + std::to_string(n + 1), std::string("Tag team: your ") + kOrder[n],
                     std::string("Your team's ") + kOrder[n] + " fighter in the tag team battle: someone at random, or this one.");
            }
        }
        if (ModesOnline()) {
            // The other player's Ready: theirs to say (the host's row shows it; the game is paused while the host's menu is open)
            onlineRows.push_back(kReadyRow);
            if (net->Hosting()) {
                online.features.push_back({Menu::kOnlinePage, "The other player",
                                           "Whether the other player has chosen their fighters and said Ready. Start waits for it. "
                                           "The game stands still for both while this menu is open.",
                                           {"Choosing", "Ready"}});
            } else {
                online.features.push_back({Menu::kOnlinePage, "Ready",
                                           "Say Ready once your fighters are chosen: the host can start a mode only then. Changing a "
                                           "fighter takes it back.",
                                           {"Not yet", "Ready"}});
            }
        }
    }
    const size_t localBegin = onlineRows.size();
    for (const char *key : {"online_wait_input", "keyboard_only", "online_hud_opacity", "online_stats", "online_ping", "online_link_rtt",
                            "online_delay", "online_hide_names", "online_hide_streak", "online_names_place"}) {
        if (const Feature *feature = FindFeature(key)) pick(key, feature->title, feature->description);
    }
    if (OnlineModeHere(kOnlineTraining)) {
        // (training's display, each player's own: drawn over this machine's picture only, netplay.cpp's Shared)
        for (const char *key : {"hit_boxes", "frame_data", "frame_meter", "input_display", "damage_readout", "stun_readout"}) {
            if (const Feature *feature = FindFeature(key)) pick(key, std::string("Training: ") + feature->title, feature->description);
        }
    }
    const size_t rulesBegin = onlineRows.size();
    for (const char *key : {tag::kFeature, tag::kTeamOrder, "cross_fever", "cross_fever_auto_gauge"}) {
        if (const Feature *feature = FindFeature(key)) {
            const int value = FeatureValue(key);
            const char *choice = feature->choices.empty() ? (value ? "On" : "Off") : feature->choices[std::clamp(value, 0, static_cast<int>(feature->choices.size()) - 1)];
            onlineRows.push_back(key);
            online.features.push_back({Menu::kOnlinePage, feature->title, "The host selects this rule before the match.", {choice}});
        }
    }
    if (FindFeature("cross_fever")) {
        const char *duo = "Duo: point controls both fighters";
        if (FeatureOn("cross_fever")) {
            switch (tag::Chosen()) {
            case tag::kCoop: duo = "Duo: independent on the co-op team"; break;
            case tag::kVersus: duo = "Duo: independent on both teams"; break;
            case tag::kTwoVsOne: duo = "Duo: independent left / shared right"; break;
            case tag::kOneVsTwo: duo = "Duo: shared left / independent right"; break;
            default: break;
            }
        }
        online.features.push_back({Menu::kOnlinePage, "Duo controls", "The host selects this rule before the match.", {duo}});
    }
    online.value = [this, rulesBegin](size_t row) {
        if (row >= rulesBegin) return 0;
        if (onlineRows[row] == kReadyRow) {
            return onlineReadyMine >= 0 ? onlineReadyMine : OnlineOtherReady() ? 1 : 0;
        }
        const auto it = onlinePicks.find(onlineRows[row]);
        return it != onlinePicks.end() ? it->second : FeatureValue(onlineRows[row].c_str());
    };
    online.setValue = [this, localBegin, rulesBegin](size_t row, int value) {
        if (!net || row >= rulesBegin) return;
        const std::string &name = onlineRows[row];
        if (name == kReadyRow) {
            if (!net->Hosting()) { // (the host's row only shows it)
                onlineReadyMine = value;
                net->Command(std::string("ready=") + (value ? "1" : "0"));
            }
            return;
        }
        if (row < localBegin) {
            onlinePicks[name] = value;
            net->Command(name + "=" + std::to_string(value));
            if (!net->Hosting()) {
                onlineReadyMine = 0; // (a fighter changed takes Ready back: OnlineEvent)
            }
        } else {
            SetFeatureValue(name.c_str(), value);
            if (!ownFeatures.empty()) ownFeatures[name] = value;
        }
    };
    online.leave = net->Hosting() && hostedFromMenu ? "Leave the match (play alone)" : "Leave the match";
    online.leaveDescription = net->Hosting() && hostedFromMenu ? "Ends the match for both: your own game comes back, still hosted."
                                                               : "Ends the match for both players.";
    menu.OpenOnline(std::move(online));
}

// The teams of a layout, as the menu and the chat say them: the left's (its first, then its second), then the right's
// (SidePlayer: the game's teams); in a match of two, player 1 then player 2
std::string Port::TeamsSaid(const Netplay::Layout &layout) const {
    std::string who[Machine::kMaxPlayers];
    for (int m = 0; net && m < net->Machines(); ++m) {
        for (int person = 0; person < net->People(m); ++person) {
            const std::string name = net->MachineName(m);
            const int player = layout[m][person];
            if (player < 0 || player >= Machine::kMaxPlayers) continue;
            who[player] = name.empty() ? (m == net->Local() && !net->Watching() ? "You" : "Machine " + std::to_string(m + 1))
                                       : name;
            if (net->People(m) > 1) who[player] += " (" + std::to_string(person + 1) + ")";
        }
    }
    const int players = net ? net->Players() : 2;
    std::string said;
    for (int side = 0; side < 2; ++side) {
        if (side) said += " vs ";
        for (int place = 0; place < 2; ++place) {
            const int player = SidePlayer(side, place, players);
            if (player >= 0) said += (place ? " + " : "") + who[player];
        }
    }
    return said;
}

void Port::OnlineCommand(size_t index) {
    if (net && OnlineReselectHere() && index == onlineModes.size() + onlineSeats.size()) {
        const bool mine = onlineReselect.asked[net->Local()] != 0;
        net->Command(mine ? "reselect=0" : "reselect=1");
        menu.Close();
        return;
    }
    if (net && index >= onlineModes.size() && index - onlineModes.size() < onlineSeats.size()) {
        net->Seat(onlineSeats[index - onlineModes.size()]);
        menu.Close();
        return;
    }
    if (net && index < onlineModes.size() && onlineModes[index] != 0 && onlineModes[index] != kOnlineTraining && ModesOnline() &&
        !OnlineOtherReady()) {
        // (the other player still choosing: started when they say Ready, the menu left open and the game paused)
        onlinePendingMode = onlineModes[index];
        chat.push_back({"Starts when the other player is ready", tick + 8 * 60, true});
        return;
    }
    if (net && index < onlineModes.size()) {
        const uint32_t seed = static_cast<uint32_t>(std::random_device()()) | 1;
        net->Command("mode=" + std::to_string(onlineModes[index]) + ":" + std::to_string(seed));
    }
    menu.Close();
}

void Port::LeaveQuestion(unsigned pressed) {
    if (pressed & recomp::kMenuConfirm) {
        LeaveOnline();
    } else if (pressed & recomp::kMenuBack) {
        leaving = false;
    }
}

// What the events that have come or been made say, on the screen as the chat's lines do: the host's mode, a player's
// fighters
void Port::HearEvents() {
    for (const auto &[player, text] : net->TakeEvents()) {
        if (text.rfind("hello,", 0) == 0) {
            continue; // (a player's own fighters as they came in with: not news)
        }
        const std::string name = net->MachineName(player);
        const std::string who = !net->Watching() && player == net->Local() ? "You"
                                : name.empty() ? (player == 0 ? "The host" : "Player " + std::to_string(player + 1)) : name;
        for (size_t at = 0; at < text.size();) {
            const size_t comma = std::min(text.find(',', at), text.size());
            const std::string item = text.substr(at, comma - at);
            at = comma + 1;
            const size_t eq = item.find('=');
            if (eq == std::string::npos) {
                continue;
            }
            const std::string name = item.substr(0, eq);
            const int value = atoi(item.c_str() + eq + 1);
            std::string said;
            if (name == "setup" && player == 0) {
                if (!net->Hosting() && !net->Watching()) {
                    if (value && !menu.IsOpen()) {
                        OpenOnlineMenu(); // (the other player's own picks, to choose while paused)
                    } else if (!value && menu.OnlineOpen()) {
                        menu.Close();
                    }
                    said = value ? "The host paused the game: choose your fighters, then Ready" : "The game goes on";
                }
            } else if (name == "reselect" && (player == 0 || player == 1) && OnlineReselectHere()) {
                const bool own = !net->Watching() && player == net->Local();
                said = value ? who + (own ? " asked" : " asks") + " to pick characters again" +
                                   (own ? " (when the other player agrees)" : ": Esc, Pick characters again to agree")
                             : who + (own ? " took back" : " took back") + " the ask to pick characters again";
            } else if (name == "ready" && player == 1) {
                said = who + (!net->Watching() && player == net->Local() ? (value ? " are ready" : " are choosing")
                                                                          : (value ? " is ready" : " is choosing"));
            } else if (Netplay::Layout layout{}; name == "seats" && player == 0 && net->SeatEvent(item, layout)) {
                // (the host moved the people: the win streaks were the sides', and start again; the set's score does
                // too, Netplay::Frame)
                streak[0] = streak[1] = roundsWon[0] = roundsWon[1] = 0;
                said = "Teams: " + TeamsSaid(layout);
            } else if (name == "restart") {
                said = who + " started the round over";
            } else if (name == "mode" && player == 0 && OnlineModeHere(value)) {
                said = who + (value == 0 ? " went back to Versus" : std::string(" started ") + kOnlineModes[value]);
            } else if (const Feature *feature = FindFeature(name); feature && OwnRow(net->Player(player, 0), name) && value >= 0 &&
                                                                    value < static_cast<int>(feature->choices.size())) {
                said = who + (player == net->Local() ? " picked " : " picked ") + feature->choices[value] +
                       (name.rfind("court", 0) == 0 ? " (Ball battle)" : " (Tag team)");
            }
            if (!said.empty()) {
                fprintf(stderr, "online: %s\n", said.c_str());
                chat.push_back({said, tick + kChatTicks, !net->Watching() && player == net->Local()});
            }
        }
    }
}

bool Port::StartOnline(std::string &error) {
    if (opt.rollbackTest || !opt.moviePlay.empty() || !opt.stateLoad.empty()) {
        error = "online play: not with --rollback-test, --movie or --state-load";
        return false;
    }
    if (!opt.net.watching) ApplyOnlineSettings(opt.net, opt, false);
    net = std::make_unique<Netplay>(opt.net);
    zoom.Forbid(); // (both players see the whole picture, as the board shows it)
    if (opt.headless && !net->Arcade()) {
        if (!net->Connect(*this, error)) {
            net.reset();
            return false;
        }
        Matched();
        return true;
    }
    if (!net->Begin(*this, error)) {
        net.reset();
        return false;
    }
    connecting = true;
    return true;
}

void Port::Again(const std::string &why) {
    Netplay::Settings settings = opt.net;
    if (net && net->Hosting()) {
        // (the same code for them to come back with; and the game from the board's reset, which the other side is sent)
        if (!net->code.empty()) {
            settings.renewCode = net->code.substr(0, 3) + net->code.substr(4);
            settings.renewSecret = net->Secret();
        }
        // (the invited player is not invited again: they had the match, and know its code)
        settings.invite.clear();
        settings.inviteName.clear();
        opt.net = settings;
        EndSessionFeatures();
        machine->Reset();
        machine->audio.clear();
    }
    fprintf(stderr, "online: %s Waiting for the match again.\n", why.c_str());
    rejoin = why;
    cameIn = false;
    frame = 0;
    net.reset(); // (the socket closed: the host's port is free for the new one)
    net = std::make_unique<Netplay>(settings);
    connecting = true;
    std::string error;
    if (!net->Begin(*this, error)) {
        fprintf(stderr, "online: %s\n", error.c_str());
        net.reset();
        againAt = tick + 180;
    }
}

// --net-arcade: a frame of the host's own play while it waits, player 1's controls only (the other players' are the
// one who comes's); counted as the match's are, from the first frame of the play
void Port::ArcadeFrame() {
    for (int player = 1; player < Machine::kMaxPlayers; player++) {
        pad[player] = 0;
    }
    RunFrame();
}

void Port::Matched() {
    if (!opt.movieRecord.empty()) recMovie.BeginRecording();
    connecting = false;
    onlinePickLog.clear();
    onlinePicks.clear();
    onlineSetupLog.clear();
    onlineModeLog.clear();
    onlineReadyLog.clear();
    onlineReselect = {};
    onlineSetupSent = false;
    onlinePendingMode = -1;
    onlineReadyMine = -1;
    net->BeginSet();
    chatHintUntil = tick + kChatTicks;
    // (the host's player, playing while it waited, is in already; the joining side is told so in the welcome)
    cameIn = net->Watching() || (net->Arcade() && cameIn);
    otherIn = net->Watching() || (!net->Hosting() && net->HostIn());
    std::string names;
    for (int player = 0; player < Machine::kMaxPlayers; player++) {
        if (net->MachineOf(player) >= 0) {
            const std::string name = net->Name(player);
            names += (names.empty() ? "" : ", ") + std::string("player ") + std::to_string(player + 1) + " " + (name.empty() ? "(no name)" : name);
        }
    }
    fprintf(stderr, "online: %s\n", names.c_str());
    if (net->Arcade()) {
        fprintf(stderr, "online: the match begins after %llu frames of the host's play\n", static_cast<unsigned long long>(frame));
    }
    streak[0] = streak[1] = roundsWon[0] = roundsWon[1] = 0;
    roundCounted = false;
    rejoin.clear();
    frame = net->Watching() ? net->WatchStart() : 0; // viewers begin at a confirmed snapshot
    // (the hashes compared leave out the picture: frames run again are not drawn)
    opt.hashNoPicture = true;
    if (net->Local() != 0) {
        opt.eepromPath.clear(); // (the board settings are the host's, for this match: the player's own are left as they are)
    }
    machine->SetBoardTiming(BoardTimingWanted());
    SoundtrackFollow();
}

// The port's rapid fire: a button the player holds is the game's pressed for two frames and let go for two, from
// the frame it goes down (so the first press is not held back). Movies keep what the player held.
void Port::RapidFire() {
    const int choice = FeatureValue("rapid_fire");
    const uint16_t buttons = choice == 1 ? kPunch1 : choice == 2 ? (kPunch1 | kPunch2 | kPunch3 | kKick1 | kKick2 | kKick3) : 0;
    for (int player = 0; player < Machine::kMaxPlayers; player++) {
        for (int bit = 0; bit < 16; bit++) {
            const uint16_t mask = static_cast<uint16_t>(1u << bit);
            if (!(buttons & mask) || !(machine->players[player] & mask)) {
                rapidHeld[player][bit] = 0;
                continue;
            }
            if ((rapidHeld[player][bit]++ / 2) % 2) {
                machine->players[player] &= static_cast<uint16_t>(~mask);
            }
        }
    }
}

// CPS2RECOMP_CALL_LOG=FROM:TO:FILE (with --cpu interp): every jsr and bsr the game's 68000 runs in frames FROM to TO,
// by where it goes: how often, and from where (for finding a game's routines a frame, docs/NATIVE_VIDEO.md)
namespace {
    std::map<uint32_t, std::map<uint32_t, uint64_t>> g_calls;
    uint64_t g_callFrom = 0, g_callTo = 0;
    std::string g_callFile;
    uint32_t g_callTask = 0; // (CPS2RECOMP_CALL_TASK=ADDR: a long of work RAM, the task record running: its number, 1 the first, kept with each)
    const uint8_t *g_callRam = nullptr;
    // (CPS2RECOMP_CALL_ORDER=FILE: each call too, in the order made, with its frame)
    FILE *g_callOrder = nullptr;
    uint64_t g_callFrame = 0;
    void LogCall(uint32_t from, uint32_t to) {
        if (g_callOrder) {
            fprintf(g_callOrder, "%llu %06X %06X\n", static_cast<unsigned long long>(g_callFrame), from & 0xFFFFFF, to & 0xFFFFFF);
        }
        uint32_t task = 0;
        if (g_callTask && g_callRam) {
            const uint8_t *p = &g_callRam[g_callTask & 0xFFFF];
            task = ((static_cast<uint32_t>(p[2] << 8 | p[3]) & 0x1FF) / 0x20 + 1) << 24; // (the record's number, 1 the first)
        }
        g_calls[to & 0xFFFFFF][(from & 0xFFFFFF) | task]++;
    }
    void WriteCalls() {
        FILE *out = g_callFile.empty() ? nullptr : fopen(g_callFile.c_str(), "w");
        if (!out) {
            return;
        }
        for (const auto &[to, froms] : g_calls) {
            uint64_t all = 0;
            for (const auto &[from, n] : froms) {
                all += n;
            }
            fprintf(out, "%06X %llu", to, static_cast<unsigned long long>(all));
            for (const auto &[from, n] : froms) {
                if (from >> 24) {
                    fprintf(out, " %06X(task %02X):%llu", from & 0xFFFFFF, from >> 24, static_cast<unsigned long long>(n));
                } else {
                    fprintf(out, " %06X:%llu", from, static_cast<unsigned long long>(n));
                }
            }
            fprintf(out, "\n");
        }
        fclose(out);
    }
}

void Port::RunFrame() {
    if (static const char *log = getenv("CPS2RECOMP_CALL_LOG"); log && g_callFile.empty()) {
        unsigned long long from = 0, to = 0;
        char file[1024] = {};
        if (sscanf(log, "%llu:%llu:%1023s", &from, &to, file) == 3) {
            g_callFrom = from;
            g_callTo = to;
            g_callFile = file;
        }
        if (const char *order = getenv("CPS2RECOMP_CALL_ORDER")) {
            g_callOrder = fopen(order, "w");
        }
        if (const char *task = getenv("CPS2RECOMP_CALL_TASK")) {
            g_callTask = static_cast<uint32_t>(strtoul(task, nullptr, 16));
            g_callRam = machine->ram.data();
        }
    }
    g_callFrame = frame;
    if (!g_callFile.empty()) {
        machine->cpu.callLog = frame >= g_callFrom && frame <= g_callTo ? LogCall : nullptr;
    }
    ClearSpareRan();
    SoundtrackFollow();
    machine->SetBoardTiming(BoardTimingWanted());
    if (!opt.moviePlay.empty()) {
        // (the change at this frame or the last before it: the same as each change in turn, and right after the
        // frame count goes back, rewind's or the practice point's)
        auto it = playMovie.changes.upper_bound(frame);
        if (it != playMovie.changes.begin()) {
            --it;
            if (it->first == frame || frame != movieNext) {
                std::copy(it->second.begin(), it->second.end(), pad);
            }
        }
        movieNext = frame + 1;
    } else if (opt.autoPlay) {
        // Made-up play, to reach as much of the game as it can: coins and start buttons now and then, sticks
        // and buttons at random, held for random lengths of time. The same seed gives the same run.
        if (frame >= opt.autoFrom) {
            for (int p = 0; p < opt.autoPlayers; p++) {
                if (Random(autoState) % 8 == 0) {
                    uint16_t held = 0;
                    const uint64_t r = Random(autoState);
                    static const uint16_t kSticks[] = {0, kLeft, kRight, kUp, kDown, kLeft | kDown, kRight | kDown,
                                                       kLeft | kUp, kRight | kUp, 0, kRight, kLeft};
                    held |= kSticks[r % 12];
                    held |= static_cast<uint16_t>((r >> 8) & (r >> 16) & 0x3F0); // punches and kicks
                    if ((r >> 32) % 24 == 0) {
                        held |= kStart;
                    }
                    if ((r >> 40) % 96 == 0) {
                        held |= kCoin;
                    }
                    autoPad[p] = held;
                }
            }
            std::copy(std::begin(autoPad), std::end(autoPad), pad);
        }
    }
    if (!opt.movieRecord.empty() && !net && memcmp(pad, lastRecorded, sizeof(pad)) != 0) { // (online: Netplay records both players)
        std::copy(std::begin(pad), std::end(pad), recMovie.changes[frame].begin());
        memcpy(lastRecorded, pad, sizeof(pad));
    }
    // (tests: --menu FRAME:KEYS[:shot.png]: the menu opened at that frame and those buttons pressed in it:
    // u d l r, a for a button, b for Back, q and e for the tab before and after, x for an option's default, h for a
    // hotkey, k to give it F3 then, c to close the menu and press F3 in the game, which says what it did on stderr)
    if (!opt.menuTest.empty() && strtoull(opt.menuTest.c_str(), nullptr, 10) == frame) {
        const auto MenuTestKey = [this](char key) {
            SDL_Event f3{};
            f3.type = SDL_EVENT_KEY_DOWN;
            f3.key.scancode = SDL_SCANCODE_F3;
            switch (key) {
            case 'h': return menu.Press(recomp::kMenuHotkey);
            case 'k': menu.HotkeyListen(f3, [](recomp::Hotkey) { return std::string(); }); return Menu::Action::None;
            case 'c':
                menu.Close();
                fprintf(stderr, "hotkey: %s\n", menu.HotkeyPress(f3).c_str());
                return Menu::Action::None;
            case 'b': return menu.Press(recomp::kMenuBack);
            case 'q': return menu.Press(recomp::kMenuTabPrev);
            case 'e': return menu.Press(recomp::kMenuTabNext);
            case 'x': return menu.Press(recomp::kMenuDefault);
            default: return menu.Buttons(key == 'u' ? kUp : key == 'd' ? kDown : key == 'l' ? kLeft : key == 'r' ? kRight : key == 'a' ? kPunch1 : 0);
            }
        };
        const size_t first = opt.menuTest.find(':'), second = opt.menuTest.find(':', first + 1);
        const std::string keys =
            opt.menuTest.substr(first + 1, second == std::string::npos ? std::string::npos : second - first - 1);
        if (net && !connecting) {
            // (online: the match's menu, over the game, which goes on; it is left open, as the player would leave it)
            OpenOnlineMenu();
            for (const char key : keys) {
                MenuAct(MenuTestKey(key));
            }
            if (second != std::string::npos) {
                std::vector<uint32_t> pixels;
                Shown(pixels);
                WritePng(opt.menuTest.substr(second + 1).c_str(), pixels.data(), static_cast<uint32_t>(ShownWidth()), Video::kHeight);
            }
            opt.menuTest.clear();
        }
        if (!opt.menuTest.empty()) {
            menu.Open(true);
            for (const char key : keys) {
                MenuAct(MenuTestKey(key));
            }
            if (second != std::string::npos && menu.IsOpen()) {
                std::vector<uint32_t> pixels;
                uint32_t width = 0, height = 0;
                // (CPS2RECOMP_MENU_SIZE=WxH: at that size, as on a screen of it; the menu compact as the Arcade monitor page says)
                int wantWidth = 0, wantHeight = 0;
                if (const char *size = getenv("CPS2RECOMP_MENU_SIZE")) {
                    sscanf(size, "%dx%d", &wantWidth, &wantHeight);
                }
                menu.SetCompact(CompactMenuChosen() || (wantHeight > 0 && wantHeight < 2 * Video::kHeight && port::ArcadeResolution() != port::ArcadeMode::Off));
                MenuPicture(pixels, width, height, wantWidth, wantHeight);
                menu.Draw(pixels.data(), static_cast<int>(width), static_cast<int>(height));
                WritePng(opt.menuTest.substr(second + 1).c_str(), pixels.data(), width, height);
            }
            menu.Close();
        }
    }
    memcpy(machine->players, pad, sizeof(pad));
    RapidFire();
    if (coinFrames > 0) {
        coinFrames--;
        machine->players[0] |= kCoin;
    }
    if (start2Frames > 0) {
        start2Frames--;
        machine->players[1] |= kStart;
    }
    machine->serviceButton = service && !net;
    machine->testSwitch = !net && (test || testFrames > 0 || (frame >= opt.testFrom && frame < opt.testTo));
    if (testFrames > 0) {
        testFrames--;
    }
    uint16_t pressed[Machine::kMaxPlayers]; // (for run-ahead's frames)
    memcpy(pressed, machine->players, sizeof(pressed));

    std::string path, error;
    if (!net && AtFrame(opt.stateLoad, frame, path) && !LoadState(path, error)) {
        fprintf(stderr, "%s\n", error.c_str());
    }
    if (net && connecting) {
        Step(pressed); // (--net-arcade: the host playing while it waits)
        cameIn = cameIn || (pressed[0] & kStart);
    } else if (net) {
        net->Frame(*this, pressed); // (this machine's player plays with player 1's controls; a second with player 2's)
        for (int person = 0; person < net->People(net->Local()); person++) {
            cameIn = cameIn || (pressed[person] & kStart);
        }
        // (the other side of the screen's panel, as the game had it: tag team's routing, fight/tag.h, done; and every
        // other machine's people as they held their controls before the routing, since a side's panel carries only the
        // Start of its person on point and its partner's then)
        const int side = ScreenSide(net->Player(net->Local(), 0));
        otherIn = otherIn || (side >= 0 && (machine->players[1 - side] & kStart));
        for (int player = 0; player < Machine::kMaxPlayers && !otherIn; player++) {
            const int at = net->MachineOf(player);
            otherIn = at >= 0 && at != net->Local() && (tag::Held(player) & kStart);
        }
        Streaks();
    } else if (opt.rollbackTest) {
        RollbackFrame(pressed);
    } else {
        PracticeInput(pressed); // (the practice point's replay, and its attempt's controls: port.h, Practice)
        ReplayKeep(pressed);    // (the instant replay's: the state now and then, the controls every frame)
        RewindKeep(pressed);    // (rewind's: a state every few frames)
        Step(pressed);
    }
    frame++;
    if (!opt.layerLog.empty()) {
        // FROM:ADDR:FILE (ADDR in hex, a word of work RAM: the camera's place). For every frame from FROM on, the frame's
        // number (4 bytes), the word (2), the picture's width (2), then Video::LayerLine for each scroll screen's lines
        // (tools/stage_ends.py reads it). The probe is set from the start, so the frame just run filled it.
        static Video::LayerLine probe[3][Video::kHeight];
        static FILE *file = nullptr;
        static uint64_t from = 0;
        static uint32_t word = 0;
        if (!machine->video.layerProbe) {
            const size_t colon = opt.layerLog.find(':'), colon2 = opt.layerLog.find(':', colon + 1);
            from = strtoull(opt.layerLog.c_str(), nullptr, 10);
            word = static_cast<uint32_t>(strtoul(opt.layerLog.c_str() + colon + 1, nullptr, 16)) & 0xFFFF;
            file = fopen(opt.layerLog.c_str() + colon2 + 1, "wb");
            machine->video.layerProbe = probe;
        } else if (file && frame >= from) {
            const uint32_t number = static_cast<uint32_t>(frame);
            const uint16_t head[2] = {static_cast<uint16_t>(machine->ram[word] << 8 | machine->ram[word + 1]),
                                      static_cast<uint16_t>(machine->video.Width())};
            fwrite(&number, 4, 1, file);
            fwrite(head, 2, 2, file);
            fwrite(probe, sizeof(probe), 1, file);
            fflush(file);
        }
    }
    // (for finding things out: --poke ADDR=BYTE,... every frame; --dump-ram FRAME:file)
    if (!opt.poke.empty()) {
        const char *at = opt.poke.c_str();
        while (*at) {
            char *end;
            const uint32_t address = static_cast<uint32_t>(strtoul(at, &end, 16));
            if (*end != '=') {
                break;
            }
            SetRam8(address, static_cast<uint8_t>(strtoul(end + 1, &end, 16)));
            at = *end == ',' ? end + 1 : end;
        }
    }
    if (!opt.trace.empty()) {
        // LOW-HIGH:FILE (hex addresses): those bytes of work RAM (or of the video memory, 900000-92FFFF) after every
        // frame, one frame after another
        static FILE *file = nullptr;
        static uint32_t low = 0, high = 0;
        if (!file) {
            const size_t dash = opt.trace.find('-'), colon = opt.trace.find(':');
            low = static_cast<uint32_t>(strtoul(opt.trace.c_str(), nullptr, 16)) & 0xFFFFFF;
            high = static_cast<uint32_t>(strtoul(opt.trace.c_str() + dash + 1, nullptr, 16)) & 0xFFFFFF;
            if (low < kVideoRamLow || low >= kVideoRamHigh) {
                low &= 0xFFFF;
                high &= 0xFFFF;
            }
            file = fopen(opt.trace.c_str() + colon + 1, "wb");
        }
        if (file && high >= low) {
            fwrite(Ram(low), 1, high - low + 1, file);
            fflush(file);
        }
    }
    for (const std::string &dump : opt.dumpRam) {
        if (!AtFrame(dump, frame, path)) {
            continue;
        }
        WriteFile(path, machine->ram.data(), machine->ram.size());
        // (and beside it the video memory, and the video chips' and object registers as 16-bit words)
        WriteFile(path + ".video", machine->videoRam.data(), machine->videoRam.size());
        WriteFile(path + ".objects", machine->objectRam.data(), machine->objectRam.size());
        std::vector<uint16_t> regs(machine->cpsA, machine->cpsA + 32);
        regs.insert(regs.end(), machine->cpsB, machine->cpsB + 32);
        regs.insert(regs.end(), machine->objectRegs, machine->objectRegs + 6);
        WriteFile(path + ".regs", regs.data(), regs.size() * 2);
    }
    if (AtFrame(opt.stateSave, frame, path) && !SaveState(path, error)) {
        fprintf(stderr, "%s\n", error.c_str());
    }
    if (hashFile && !opt.rollbackTest && !net) { // (RollbackFrame and online play log a frame when it is confirmed)
        fprintf(hashFile, "%llu %08x\n", static_cast<unsigned long long>(frame), StateHash());
    }
    if (!net) {
        memcpy(machine->players, pressed, sizeof(pressed));
        LagMeasure();
        RunAhead();
    } else if (uint16_t shown[Machine::kMaxPlayers]; net->ShownFrame(shown)) {
        memcpy(machine->players, shown, sizeof(shown));
        LagMeasure();
        RunAhead();
    }
    // (the scene clip's frame: the picture shown, run-ahead's or not, and the records it was drawn from)
    if (scene.Recording()) {
        scene.Frame(*machine);
        menu.recordingClip = scene.Recording();
    }
    StageSweepFrame();
    guest_stages::LookFrame(*machine);
    if (opt.shotEvery && frame % opt.shotEvery == 0) {
        char shot[512];
        snprintf(shot, sizeof(shot), "%s%06llu.png", opt.shotPrefix.c_str(), static_cast<unsigned long long>(frame));
        std::vector<uint32_t> shown;
        Shown(shown);
        WritePng(shot, shown.data(), ShownWidth(), Video::kHeight);
    }
    // (--video gpu-check: the frame the GPU kept, drawn with the readouts over it, as the window would show it, against
    // the CPU's picture with the readouts: gpu_picture.h)
    if (g_gpuPicture && g_gpuPicture->CanDrawKept() && !PictureMade()) {
        std::vector<uint32_t> readouts, shown;
        Readouts(readouts);
        Shown(shown);
        g_gpuPicture->CheckShown(readouts.data(), shown.data(), ShownWidth());
    }
}

void Port::Finish() {
    WriteCalls();
    guest_stages::Report(stderr);
    if (g_callOrder) {
        fclose(g_callOrder);
    }
    if (net && !connecting) {
        net->Leave();
        net->Report(true);
    }
    if (hashFile) {
        fclose(hashFile);
    }
    if (opt.rollbackTest) {
        const RollbackTest &t = rollbackTest;
        const auto ms = [](double total, uint64_t count) { return count ? 1000 * total / count : 0.0; };
        fprintf(stderr, "rollback test: %llu rollbacks, %llu frames run again, at most %llu at once\n",
                static_cast<unsigned long long>(t.rollbacks), static_cast<unsigned long long>(t.rerun),
                static_cast<unsigned long long>(t.most));
        std::vector<float> times = t.frameTimes;
        std::sort(times.begin(), times.end());
        const auto at = [&](double part) { return times.empty() ? 0.0 : 1000.0 * times[static_cast<size_t>(part * (times.size() - 1))]; };
        const size_t over = static_cast<size_t>(times.end() - std::upper_bound(times.begin(), times.end(), 1.0f / Machine::kFramesPerSecond));
        fprintf(stderr, "rollback test: a frame %.3f ms, keeping %.3f ms, putting back %.3f ms, state %zu bytes\n",
                ms(t.stepTime, t.steps), ms(t.keepTime, t.keeps), ms(t.putBackTime, t.putBacks), t.stateSize);
        fprintf(stderr, "rollback test: a frame with its rollback: half under %.2f ms, 99%% %.2f ms, 99.9%% %.2f ms, the worst %.2f ms; "
                        "%zu of %zu over a frame's time\n", at(0.5), at(0.99), at(0.999), at(1.0), over, times.size());
    }
    if (!opt.movieRecord.empty()) {
        recMovie.Save(opt.movieRecord);
    }
    if (!opt.eepromPath.empty() && machine->eeprom.changed) {
        WriteFile(opt.eepromPath, machine->eeprom.words, sizeof(machine->eeprom.words));
    }
    if (!opt.recordEntries.empty()) {
        // The entries files: one address a line, kept sorted; ours added to what is there. The 68000's, and
        // beside it (.z80) the sound driver's.
        auto merge = [](const std::string &path, const std::vector<uint32_t> &found, const char *format) {
            std::set<uint32_t> entries;
            if (FILE *f = fopen(path.c_str(), "r")) {
                char line[64];
                while (fgets(line, sizeof(line), f)) {
                    if (line[0] != '#' && line[0] != '\n') {
                        entries.insert(static_cast<uint32_t>(strtoul(line, nullptr, 16)));
                    }
                }
                fclose(f);
            }
            const size_t before = entries.size();
            entries.insert(found.begin(), found.end());
            if (entries.size() == before && before != 0) {
                return; // (nothing new: the file is left alone)
            }
            if (FILE *f = fopen(path.c_str(), "w")) {
                fprintf(f, "# Places the game's code was seen to reach in ways the translator cannot follow\n");
                for (uint32_t e : entries) {
                    fprintf(f, format, e);
                }
                fclose(f);
            }
            fprintf(stderr, "entries: %zu new (%zu in %s)\n", entries.size() - before, entries.size(), path.c_str());
        };
        merge(opt.recordEntries, machine->cpu.newEntries, "%06X\n");
        merge(opt.recordEntries + ".z80",
              std::vector<uint32_t>(machine->soundCpu.newEntries.begin(), machine->soundCpu.newEntries.end()), "%04X\n");
    }
}

// The list --list-features writes (on Android the launcher asks the game's library for it: port/android.cpp)
// The language of the menu and the readouts (port/i18n.h): RECOMP_LANGUAGE, which the launcher sets to the one chosen
// there, else the system's
void UseLanguage() {
    AddTranslations();
    std::vector<std::string> preferred = recomp::i18n::FromEnvironment();
    int count = 0;
    if (SDL_Locale **locales = SDL_GetPreferredLocales(&count)) {
        for (int i = 0; i < count; i++) {
            preferred.push_back(std::string(locales[i]->language ? locales[i]->language : "") +
                                (locales[i]->country ? std::string("-") + locales[i]->country : std::string()));
        }
        SDL_free(locales);
    }
    recomp::i18n::SetLanguage(recomp::i18n::Resolve("", preferred));
}

// Every text the menu can show of this game (--list-text), a line each (a line break as \n): what a language's table is
// to have (port/i18n.h)
std::string MenuTextList() {
    RegisterGame();
    RegisterPortFeatures();
    std::vector<std::string> texts;
    auto add = [&](const char *text) {
        if (text && *text && std::find(texts.begin(), texts.end(), text) == texts.end()) {
            texts.push_back(text);
        }
    };
    const Game &game = TheGame();
    for (const Feature &f : game.features) {
        add(f.category);
        add(f.title);
        add(f.description);
        for (const char *choice : f.choices) {
            add(choice);
        }
    }
    for (const Command &c : game.commands) {
        add(c.category);
        add(c.title);
        add(c.description);
    }
    for (const Page &p : game.pages) {
        add(p.name);
        add(p.description);
    }
    std::string out;
    for (const std::string &text : texts) {
        for (const char c : text) {
            out += c == '\n' ? std::string("\\n") : std::string(1, c);
        }
        out += '\n';
    }
    return out;
}

std::string LauncherFeatureList() {
    RegisterGame();
    std::string definitionError;
    if (!ReadDeclaredFeatures(definitionError)) {
        fprintf(stderr, "%s\n", definitionError.c_str());
        std::exit(EXIT_FAILURE);
    }
    RegisterPortFeatures();
    std::vector<recomp::FeatureInfo> list = FeatureList();
    // (nor are the ones whose switch is on the launcher's first page: game.h, kFeatureFirstPage)
    list.erase(std::remove_if(list.begin(), list.end(), [](const recomp::FeatureInfo &f) { return f.flags & kFeatureFirstPage; }), list.end());
    // (what the launcher shows, all else being the in-game menu's alone: what has to be set before the game starts,
    // recomp-common settings.h's kFeatureLauncher. The board's own settings are its own card on the game's page, and
    // Keyboard only is on the Controllers page: launcher/src/description.cpp)
    static const char *const kInLauncher[] = {
        "window_size", "display_picture", "display_vsync",                             // the window and how it is drawn
        "crt_resolution", "crt_screen", "crt_monitor", "crt_switchres",                // the arcade monitor's mode
        "online_first_to", "online_unlisted", "online_host_delay", "online_people",   // set before hosting
    };
    for (recomp::FeatureInfo &f : list) {
        if (std::any_of(std::begin(kInLauncher), std::end(kInLauncher), [&f](const char *name) { return f.name == name; })) {
            f.flags |= recomp::kFeatureLauncher;
        }
    }
    return recomp::FeatureListText(list);
}

bool InCollection() {
#ifdef __ANDROID__
    return true; // (Android has only the collection's app)
#endif
    static const bool collection = [] {
        const char *base = SDL_GetBasePath();
        std::error_code missing;
        return base && !std::filesystem::exists(std::string(base) + GAME_ID "-launcher" + kExeSuffix, missing) &&
               std::filesystem::exists(std::string(base) + "cps2recomp-launcher" + kExeSuffix, missing);
    }();
    return collection;
}

} // namespace port

namespace {

void Usage() {
    fprintf(stderr,
            "%s\n"
            "  --rom PATH             the ROM set (%s): a folder with its files, or its zip\n"
            "  --rom2 PATH            (a game that reads another set too) that set: a folder or zip (else\n"
            "                         beside --rom, by the set's name)\n"
            "  --guest ID=PATH        (a game that runs other games' boards beside it) the set of game ID: the same\n"
            "  --eeprom FILE          where the board's settings memory is kept\n"
            "  --cpu MODE             native (translated code), blocks (the interpreter run the same way),\n"
            "                         interp (the interpreter, an instruction at a time), check\n"
            "  --sound SIDE           driver (the sound program as ordinary code), z80 (the program itself),\n"
            "                         check (both, every write to the chip compared)\n"
            "  --video SIDE           gpu (the window's picture drawn on the GPU from the records the game's video\n"
            "                         code hands over, where it can be: the default, unless the Display page's\n"
            "                         Picture drawn by says the CPU), native or cpu (drawn from those records on\n"
            "                         the CPU), board (the picture from the board's registers and memories, the\n"
            "                         reference), check (the native interface the game's video code hands over\n"
            "                         compared with the board's, every record: docs/NATIVE_VIDEO.md), gpu-check\n"
            "                         (that, and each frame drawn on the GPU from it too, read back and compared)\n"
            "  --sound-test SEED[:N]  every N frames (30) a command to the sound side: each song in turn, placed\n"
            "                         some made-up way, and the other commands (with --sound check)\n"
            "  --scale N              window size, times 224 lines (else the Display page's)\n"
            "  --zoom SIZE:PLAYER     the camera zoomed from the start: SIZE 1-4 (1.25x-2x), PLAYER 1 or 2 (3: both,\n"
            "                         4: both by their distance, the panel left as it is)\n"
            "  --fullscreen  --window (a window, after --fullscreen)  --mute  --unthrottled\n"
            "  --headless             no window, no sound, as fast as it goes\n"
            "  --frames N             stop after N frames\n"
            "  --shot-every N:PFX     write PFX<frame>.png every N frames\n"
            "  --hash-log FILE        a line per frame with a hash of the game's state\n"
            "  --hash-picture         ... of the picture only (to compare runs whose timing is not the same)\n"
            "  --hash-no-picture      ... of all but the picture (--rollback-test then draws only the frames shown)\n"
            "  --stage-check          other games' stages: their camera checked against where it is wanted, each frame\n"
            "                         they are shown, and their picture not blank (fight/guest_stages.h)\n"
            "  --stage-look FILE      the stage viewer's pictures of its stage, for tools/stage_look.py\n"
            "  --hash-no-sound        ... and without the sound side, as online play's (to compare a match's --record)\n"
            "  --hash-game            ... without the Z80 and its own memory (to compare --sound driver and z80)\n"
"  --renderer sdl|gpu     how the picture gets to the window: SDL's GPU API unless said (or not there)\n"
            "  --present-test F.png   (headless) the run's last picture drawn by the GPU path, no window, as the\n"
            "                         Display page has it (896x672 at 4:3)\n"
            "  --present-size WxH     ... in a window this big\n"
            "  --present-every N[:P]  (headless) every N frames the picture as the window draws it, zoom and all, to P000123.png\n"
                        "  --cpu-speed N          the 68000 N times as fast as the board's: the game is never short of time\n"
            "  --movie FILE           replay an input movie\n"
            "  --record FILE          record an input movie\n"
            "  --auto SEED[:FROM[:N]] made-up input (from frame FROM; for N players, 2 unless said)\n"
            "  --rollback-test SEED   each frame run as online play's rollback would: the other players' controls late\n"
            "                         and guessed, and the frames guessed wrong run again (--hash-log must not change)\n"
            "  --watch CODE          watch a running match without controlling any player\n"
            "  --net-spectators      hosting: allow spectators (off by default)\n"
            "  --host [PORT]          online play: wait for the other players (on UDP port PORT, 7845 unless said);\n"
            "                         the host is player 1, and its board settings and play features are the match's\n"
            "                         (with tag team's 2 vs 2, a match of four: up to three machines join)\n"
            "  --join ADDRESS[:PORT]  online play: go to the host at ADDRESS (player 2, or the next in a match of four),\n"
            "                         or CODE: the match the host's code names (the online service says where it is)\n"
            "  --net-people N         the people playing at this machine in a match of four, 1 or 2 (its player 1 and\n"
            "                         player 2 controls): the Online page's People at this machine unless said\n"
            "  --net-service URL      the online service that gives hosts codes (online/), or off\n"
            "  --net-list             (hosting) the match in the lobby, the Online card's list of open matches\n"
            "  --net-relay on|only|off  the online service's relay (TURN) for a network that stops direct connections:\n"
            "                         on beside the direct addresses (the default), only (every packet through it), off\n"
            "  --net-forward on|off   the home router asked to forward a port to the game (UPnP or NAT-PMP), as most\n"
            "                         online games do: on (the default) or off\n"
            "  --net-name NAME        ... under this name; and any machine's: shown to the other players\n"
            "  --net-say [FRAME:]TEXT (tests) TEXT said in the chat at FRAME of the match (120 unless said)\n"
            "  --net-typing TEXT      (pictures) TEXT shown as a chat line being typed, all through the match\n"
            "  --net-arcade           (hosting) the game played alone until the other player comes, who then challenges\n"
            "  --net-invite TO:FROM   (hosting) the player TO in the launcher's waiting room invited, from FROM (ids;\n"
            "                         --net-name the inviter's name)\n"
            "  --net-invite-name NAME ... TO's name, as the wait says it\n"
            "  --net-delay N          this machine's own input delay, over the game's and the launcher's (0-8 frames)\n"
            "  --net-fake MS[:JIT[:LOSS]]  (tests) each packet sent MS ms late, give or take JIT, LOSS in 100 lost\n"
            "  --test-switch FROM:TO  the operator's test switch held from frame FROM to TO\n"
            "  --music-test FRAMES    each piece of the game's music in turn on the board, FRAMES each (with the test\n"
            "                         switch held, say), and how long each made a sound for (to tell the ones that end)\n"
            "  --record-entries FILE  add the places reached by computed jumps to FILE (for tools/recomp.py)\n"
            "  --state-save F:FILE  --state-load F:FILE   at frame F\n"
            "  --features A,B         switch optional features on (--list-features lists them; -NAME: off; given\n"
            "                         more than once, all of them in order)\n"
            "  --settings FILE        settings.ini with the player's feature choices\n"
            "  --multi MODE:CHOICES   played as one of the launcher's Multi game page's games (endless:scroll:1,\n"
            "                         endless:shuffle:60); exit code 3 when it is the next game's turn\n"
            "  --shader FILE          draw through that RetroArch shader preset (.slangp), whatever the Display page says\n"
            "  --soundtrack DIR       the custom soundtrack's folder (its songs.txt names the files)\n"
            "  --list-songs           the songs the soundtrack's files are named for: name, number, what it is\n"
            "  --soundtrack-dump FILE (with --headless) write what the custom soundtrack plays to a WAV file\n"
            "  --poke A=B,...         write byte B at RAM (or video memory) address A after every frame (hex)\n"
            "  --dump-ram F:FILE      write work RAM after frame F (and FILE.video, .objects, .regs); more than once\n"
            "  --ram-log FILE         write work RAM (64 KB) at the start of every vertical blank, one after another\n"
            "                         (docs/ARCADE_ACCURACY.md: as the reference harness logs the board's; without\n"
            "                         run-ahead or online play, whose frames run again would be written too)\n"
            "  --cpu-clock MHZ        a CPS-1's 68000 at 10 or 12 MHz in place of game.cfg's (to compare with a model\n"
            "                         of the board at that clock)\n"
            "  --check-rewrites       run every rewritten routine and its original, and report differences\n"
            "  --no-rewrites          run the game's own routines where the sources have rewrites\n"
            "  --trace LOW-HIGH:FILE  those bytes of work RAM (or video memory) after every frame, appended to FILE\n"
            "  --layer-log F:ADDR:FILE from frame F on, what each scroll screen drew on each line, and the word at ADDR\n"
            "  --watch-writes LOW[-HIGH] report the 68000's writes to these addresses (hex), with who made them\n"
            "  --keys TICK:KEY,...    keys pressed at those ticks of the screen: p (stop/go), o (a frame on),\n"
            "                         - and = (speed), n and h (a Multi game run's next game, fast forward on/off),\n"
            "                         k1.. (the game's command keys), cNAME (a command),\n"
            "                         fNAME=N (a feature set)\n"
            "  --stats                speed, at the end\n"
            "  --audio-dump FILE      write the sound to a WAV file\n"
            "  --record-video F.mp4   record the whole run as a video (the Video page's size), headless or not; with\n"
            "                         --movie, a movie's play as a video\n"
            "  --dump-program FILE    write the deciphered 68000 program and stop\n",
            GAME_NAME, GAME_ROM_SET);
}

bool ParseArgs(int argc, char **argv, port::Options &o) {
    for (int i = 1; i < argc; i++) {
        const std::string a = argv[i];
        auto next = [&]() -> const char * { return i + 1 < argc ? argv[++i] : ""; };
        if (a == "--rom") {
            o.romPath = next();
        } else if (a == "--rom2") {
            o.extraRomPath = next();
        } else if (a == "--guest") {
            const std::string given = next();
            const size_t equals = given.find('=');
            if (equals != std::string::npos) {
                o.guestRomPaths[given.substr(0, equals)] = given.substr(equals + 1);
            }
        } else if (a == "--eeprom") {
            o.eepromPath = next();
        } else if (a == "--cpu") {
            o.cpu = next();
        } else if (a == "--music-test") {
            o.musicTest = std::max(1u, static_cast<unsigned>(strtoul(next(), nullptr, 10)));
        } else if (a == "--sound-test") {
            const std::string v = next();
            o.soundTest = strtoull(v.c_str(), nullptr, 10);
            if (const size_t colon = v.find(':'); colon != std::string::npos) {
                o.soundTestEvery = std::max(1u, static_cast<unsigned>(strtoul(v.c_str() + colon + 1, nullptr, 10)));
            }
        } else if (a == "--sound") {
            o.sound = next();
        } else if (a == "--video") {
            o.video = next();
        } else if (a == "--scale") {
            o.scale = atoi(next());
        } else if (a == "--zoom") {
            const char *spec = next();
            o.zoom = atoi(spec);
            const char *colon = strchr(spec, ':');
            o.zoomPlayer = colon ? atoi(colon + 1) - 1 : 0;
        } else if (a == "--fullscreen") {
            o.fullscreen = true;
        } else if (a == "--window") {
            o.fullscreen = false; // (the Multi game page's, after what the game's own Play says)
        } else if (a == "--mute") {
            o.mute = true;
        } else if (a == "--unthrottled") {
            o.unthrottled = true;
        } else if (a == "--headless") {
            o.headless = true;
        } else if (a == "--frames") {
            o.frames = strtoull(next(), nullptr, 10);
        } else if (a == "--present-every") {
            const std::string v = next();
            const size_t colon = v.find(':');
            o.presentEvery = strtoull(v.c_str(), nullptr, 10);
            o.presentPrefix = colon == std::string::npos ? "present_" : v.substr(colon + 1);
        } else if (a == "--shot-every") {
            const std::string v = next();
            const size_t colon = v.find(':');
            o.shotEvery = strtoull(v.c_str(), nullptr, 10);
            o.shotPrefix = colon == std::string::npos ? "shot_" : v.substr(colon + 1);
        } else if (a == "--hash-log") {
            o.hashLog = next();
        } else if (a == "--hash-no-sound") {
            o.hashNoSound = true;
        } else if (a == "--hash-no-picture") {
            o.hashNoPicture = true;
        } else if (a == "--stage-check") {
            guest_stages::Check(true);
        } else if (a == "--stage-look") {
            guest_stages::Look(next());
        } else if (a == "--hash-game") {
            o.hashGame = true;
        } else if (a == "--hash-picture") {
            o.hashPicture = true;
        } else if (a == "--renderer") {
            o.renderer = next();
        } else if (a == "--present-test") {
            o.presentTest = next();
        } else if (a == "--present-size") {
            const std::string v = next();
            o.presentWidth = atoi(v.c_str());
            o.presentHeight = v.find('x') == std::string::npos ? 0 : atoi(v.c_str() + v.find('x') + 1);
        } else if (a == "--cpu-speed") {
            o.cpuSpeed = atoi(next());
        } else if (a == "--movie") {
            o.moviePlay = next();
        } else if (a == "--record") {
            o.movieRecord = next();
        } else if (a == "--auto") {
            const std::string v = next();
            const size_t colon = v.find(':');
            o.autoPlay = true;
            o.autoSeed = strtoull(v.c_str(), nullptr, 10);
            o.autoFrom = colon == std::string::npos ? 0 : strtoull(v.c_str() + colon + 1, nullptr, 10);
            const size_t second = colon == std::string::npos ? colon : v.find(':', colon + 1);
            o.autoPlayers = second == std::string::npos ? 2 : atoi(v.c_str() + second + 1);
        } else if (a == "--rollback-test") {
            o.rollbackTest = strtoull(next(), nullptr, 10) | 1;
        } else if (a == "--host") {
            o.online = o.net.hosting = true;
            if (i + 1 < argc && argv[i + 1][0] >= '0' && argv[i + 1][0] <= '9') {
                o.net.port = atoi(next());
            }
        } else if (a == "--watch") {
            o.online = o.net.watching = true;
            o.net.address = next();
        } else if (a == "--net-spectators") {
            o.netSpectatorsRequested = true;
        } else if (a == "--join") {
            o.online = true;
            o.net.address = next();
        } else if (a == "--net-relay") {
            const std::string how = next();
            o.net.relay = how == "only" ? 1 : how == "off" ? -1 : 0;
        } else if (a == "--net-forward") {
            o.net.forward = next() != "off";
        } else if (a == "--net-list") {
            o.net.listed = true;
        } else if (a == "--net-arcade") {
            o.net.arcade = true;
        } else if (a == "--net-name") {
            o.net.name = next();
        } else if (a == "--net-say") {
            o.netSay = next();
            const size_t colon = o.netSay.find(':');
            if (colon != std::string::npos && colon > 0 && o.netSay.find_first_not_of("0123456789") == colon) {
                o.netSayAt = strtoull(o.netSay.c_str(), nullptr, 10);
                o.netSay.erase(0, colon + 1);
            }
        } else if (a == "--net-command") {
            const std::string v = next();
            const size_t colon = v.find(':');
            if (colon != std::string::npos) {
                o.netCommands.push_back({strtoull(v.c_str(), nullptr, 10), v.substr(colon + 1)});
            }
        } else if (a == "--net-typing") {
            o.netTyping = next();
        } else if (a == "--net-invite") {
            // (two waiting room ids: 12 hex digits each)
            const std::string v = next();
            const size_t colon = v.find(':');
            const auto id = [](const std::string &t) { return t.size() == 12 && t.find_first_not_of("0123456789abcdef") == std::string::npos; };
            if (colon != std::string::npos && id(v.substr(0, colon)) && id(v.substr(colon + 1))) {
                o.net.invite = v.substr(0, colon);
                o.net.from = v.substr(colon + 1);
            }
        } else if (a == "--net-invite-name") {
            o.net.inviteName = next();
        } else if (a == "--net-service") {
            o.net.service = next();
        } else if (a == "--net-delay" || a == "--launcher-net-delay") {
            const std::string value = next();
            if (value.size() != 1 || value[0] < '0' || value[0] > '8') {
                fprintf(stderr, "%s needs a frame count from 0 to 8.\n", a.c_str());
                return false;
            }
            if (a == "--net-delay") o.netDelayOverride = value[0] - '0';
            else o.launcherNetDelay = value[0] - '0';
        } else if (a == "--net-people") {
            o.net.people = std::clamp(atoi(next()), 1, port::Netplay::kMostSeats);
        } else if (a == "--net-fake") {
            const std::string v = next();
            const size_t colon = v.find(':'), second = colon == std::string::npos ? colon : v.find(':', colon + 1);
            o.net.fakeLag = atoi(v.c_str());
            o.net.fakeJitter = colon == std::string::npos ? 0 : atoi(v.c_str() + colon + 1);
            o.net.fakeLoss = second == std::string::npos ? 0 : atoi(v.c_str() + second + 1);
        } else if (a == "--test-switch") {
            const std::string v = next();
            const size_t colon = v.find(':');
            o.testFrom = strtoull(v.c_str(), nullptr, 10);
            o.testTo = colon == std::string::npos ? ~0ull : strtoull(v.c_str() + colon + 1, nullptr, 10);
        } else if (a == "--record-entries") {
            o.recordEntries = next();
        } else if (a == "--state-save") {
            o.stateSave = next();
        } else if (a == "--state-load") {
            o.stateLoad = next();
        } else if (a == "--stats") {
            o.stats = true;
        } else if (a == "--list-features") {
            // For the launcher: one optional feature per line (recomp-common/settings.h; the ones that last a
            // session, switched in the game's menu only, are not in it)
            fputs(port::LauncherFeatureList().c_str(), stdout);
            exit(0);
        } else if (a == "--list-text") {
            // Every text of the menu's for this game (port/i18n.h): what a language's table is to have
            fputs(port::MenuTextList().c_str(), stdout);
            exit(0);
        } else if (a == "--list-songs") {
            // The songs a custom soundtrack's files are named for (the game's own names; no ROM wanted)
            RegisterGame();
            fputs(port::NamedSongs().c_str(), stdout);
            exit(0);
        } else if (a == "--settings") {
            o.settingsPath = next();
        } else if (a == "--shader") {
            o.shader = next();
        } else if (a == "--soundtrack") {
            o.soundtrack = next();
        } else if (a == "--soundtrack-dump") {
            o.soundtrackDump = next();
        } else if (a == "--multi") {
            SetMultiGame(next());
        } else if (a == "--features") {
            // (given more than once, all of them, in order: what the launcher says, then what follows it)
            const std::string more = next();
            o.features = o.features.empty() ? more : o.features + "," + more;
        } else if (a == "--watch-writes") {
            // LOW[-HIGH] (hex): who writes there; needs the interpreter, so the run is done block by block
            const std::string v = next();
            const size_t dash = v.find('-');
            o.watchLow = static_cast<uint32_t>(strtoul(v.c_str(), nullptr, 16));
            o.watchHigh = dash == std::string::npos ? o.watchLow : static_cast<uint32_t>(strtoul(v.c_str() + dash + 1, nullptr, 16));
            o.cpu = "blocks";
        } else if (a == "--check-rewrites") {
            o.checkRewrites = true;
        } else if (a == "--no-rewrites") {
            o.noRewrites = true;
        } else if (a == "--trace") {
            o.trace = next();
        } else if (a == "--layer-log") {
            o.layerLog = next();
        } else if (a == "--dump-ram") {
            o.dumpRam.push_back(next());
        } else if (a == "--ram-log") {
            o.ramLog = next();
        } else if (a == "--cpu-clock") {
            o.cpuClock = atoi(next());
            if (o.cpuClock != 10 && o.cpuClock != 12) {
                Usage();
                return false;
            }
        } else if (a == "--poke") {
            o.poke = next();
        } else if (a == "--keys") {
            o.keys = next();
        } else if (a == "--menu") {
            o.menuTest = next();
        } else if (a == "--audio-dump") {
            o.audioDump = next();
        } else if (a == "--record-video") {
            o.videoOut = next();
        } else if (a == "--dump-program") {
            o.dumpProgram = next();
        } else {
            Usage();
            return false;
        }
    }
    return true;
}

// The game's program opened by itself, with nothing to say what to play (a double click on it rather than on
// the launcher): the launcher beside it is started instead, straight into the game if the player has chosen
// their ROM set in it before (its --play), or at its first page to choose one. The launcher is the game's own, or
// the collection's (cps2recomp-launcher: then at this game's page, or --play ID). True: it was started.
#ifndef __ANDROID__
bool StartLauncher() {
    const char *base = SDL_GetBasePath();
    if (!base) {
        return false;
    }
    const char *kExe = port::kExeSuffix;
    std::string launcher = std::string(base) + GAME_ID "-launcher" + kExe;
    std::error_code missing;
    bool collection = false;
    if (!std::filesystem::exists(launcher, missing)) {
        launcher = std::string(base) + "cps2recomp-launcher" + kExe;
        collection = true;
        if (!std::filesystem::exists(launcher, missing)) {
            return false;
        }
    }
    bool setUp = false;
    // (the launcher's own folder and file; the collection's keeps each game's file in a section of its own. In
    // portable mode the folder is in user/ beside the launcher, as the launcher has it: recomp-common/folders.h)
    const std::string name = recomp::FolderName(collection ? "CPS-2 Recomp" : GAME_NAME);  // (as the launcher names it)
    std::filesystem::path folder = recomp::PortableFolder(base);
    if (!folder.empty()) {
        folder /= std::filesystem::u8path(name);
    } else if (char *pref = SDL_GetPrefPath("", name.c_str())) {
        folder = pref;
        SDL_free(pref);
    }
    if (!folder.empty()) {
        recomp::Ini prefs(folder / "launcher.ini");
        prefs.load();
        setUp = !prefs.get(collection ? "game." GAME_ID : "game", "rom").empty();
    }
    const char *own[] = {launcher.c_str(), setUp ? "--play" : nullptr, nullptr};
    const char *theirs[] = {launcher.c_str(), setUp ? "--play" : "--page", GAME_ID, nullptr};
    SDL_Process *process = SDL_CreateProcess(collection ? theirs : own, false);
    if (!process) {
        return false;
    }
    SDL_DestroyProcess(process); // (only SDL's record of it: the launcher goes on)
    return true;
}
#endif

// A start that went wrong: said on the standard error, and, with a window wanted, in a message box too (a
// windowed program started by a double click has nowhere else to say it)
void SayFailed(const port::Options &opt, const std::string &error) {
    fprintf(stderr, "%s: %s\n", GAME_NAME, error.c_str());
    if (!opt.headless) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, GAME_NAME, error.c_str(), nullptr);
    }
}

} // namespace

#ifdef _WIN32
// (an address as the program or library it is in, and how far into it: the same build's unstripped program
// turns that into a function: tools/release/crash_where.sh)
static void SayWhere(DWORD64 address) {
    HMODULE module = nullptr;
    char path[MAX_PATH] = "?";
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCSTR>(address), &module) && module) {
        GetModuleFileNameA(module, path, sizeof path);
    }
    const char *name = strrchr(path, '\\');
    fprintf(stderr, "  %016llx  %s+0x%llx\n", (unsigned long long)address, name ? name + 1 : path,
            (unsigned long long)(module ? address - reinterpret_cast<DWORD64>(module) : 0));
}

// A crash (a bad memory access and the like): where it happened and the calls that led there, into the log,
// then Windows ends the program as before (the launcher shows its code)
static LONG WINAPI SayCrash(EXCEPTION_POINTERS *e) {
    const EXCEPTION_RECORD *r = e->ExceptionRecord;
    fprintf(stderr, "\n" GAME_ID " crashed: exception %08lX at %p", r->ExceptionCode, r->ExceptionAddress);
    if (r->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && r->NumberParameters >= 2) {
        const ULONG_PTR how = r->ExceptionInformation[0];
        fprintf(stderr, " (%s %p)", how == 0 ? "reading" : how == 1 ? "writing" : "executing",
                reinterpret_cast<void *>(r->ExceptionInformation[1]));
    }
    fprintf(stderr, ", thread %lu\ncalls, the crash first:\n", GetCurrentThreadId());
#if defined(_M_X64) || defined(__x86_64__)
    CONTEXT c = *e->ContextRecord;
    for (int i = 0; i < 40 && c.Rip; i++) {
        SayWhere(c.Rip);
        DWORD64 base = 0;
        RUNTIME_FUNCTION *f = RtlLookupFunctionEntry(c.Rip, &base, nullptr);
        if (!f) {  // (a leaf function: its return address is on top of the stack)
            c.Rip = *reinterpret_cast<DWORD64 *>(c.Rsp);
            c.Rsp += 8;
            continue;
        }
        void *handler = nullptr;
        DWORD64 frame = 0;
        RtlVirtualUnwind(UNW_FLAG_NHANDLER, base, c.Rip, f, &c, &handler, &frame, nullptr);
    }
#else
    SayWhere(reinterpret_cast<DWORD64>(r->ExceptionAddress));
#endif
    fflush(stderr);
    return EXCEPTION_CONTINUE_SEARCH;
}
#endif

int main(int argc, char **argv) {
#ifdef _WIN32
    // (a windowed program has no console: started from one, its output goes there, unless it was sent elsewhere)
    if (!GetStdHandle(STD_ERROR_HANDLE) && AttachConsole(ATTACH_PARENT_PROCESS)) {
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
    }
    recomp::UseSystemProxy();  // (Windows' proxy, for the curl that asks the online service: recomp-common/proxy.h)
    // (Windows' C library has no line buffering, _IOLBF is full buffering: unbuffered, or a crash loses the log)
    setvbuf(stdout, nullptr, _IONBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);
    SetUnhandledExceptionFilter(SayCrash);
    // (Bandicam's Vulkan layer, registered when it is installed and loaded into every Vulkan program even with
    // Bandicam closed, crashes as the GPU device is made (bdcamvk64.dll reads address 0): the Vulkan loader leaves
    // it out. A player who sets the loader's list themselves keeps theirs)
    if (!getenv("VK_LOADER_LAYERS_DISABLE")) {
        _putenv_s("VK_LOADER_LAYERS_DISABLE", "*bandicam*,*bdcam*");
    }
#elif !defined(__ANDROID__)
    // (the launcher sends the output to logs/game.log: written as it goes, so a crash keeps it)
    setvbuf(stdout, nullptr, _IOLBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);
#endif
    port::UseLanguage();
#ifdef __ANDROID__
    port::StartOnAndroid();
#else
    if (argc == 1 && StartLauncher()) {
        return 0;
    }
#endif
    port::Port p;
    if (!ParseArgs(argc, argv, p.opt)) {
        return 2;
    }
    std::string error;
    if (!p.Init(error)) {
        SayFailed(p.opt, argc == 1 ? error + "\n\nStart the game with its launcher (" GAME_ID "-launcher), which passes it your ROM set." : error);
        return 1;
    }
    if (!p.opt.dumpProgram.empty()) {
        FILE *f = fopen(p.opt.dumpProgram.c_str(), "wb");
        if (!f) {
            fprintf(stderr, "cannot write %s\n", p.opt.dumpProgram.c_str());
            return 1;
        }
        fwrite(p.roms.opcodes.data(), 1, p.roms.opcodes.size(), f);
        fclose(f);
        // (and beside it the sound driver's program: the fixed part of its ROM)
        if (FILE *sound = fopen((p.opt.dumpProgram + ".z80").c_str(), "wb")) {
            fwrite(p.roms.soundProgram.data(), 1, std::min<size_t>(p.roms.soundProgram.size(), 0x8000), sound);
            fclose(sound);
        }
        // (... and, where the Z80 deciphers its opcodes another way, the opcodes)
        if (!p.roms.soundOpcodes.empty()) {
            if (FILE *ops = fopen((p.opt.dumpProgram + ".z80ops").c_str(), "wb")) {
                fwrite(p.roms.soundOpcodes.data(), 1, p.roms.soundOpcodes.size(), ops);
                fclose(ops);
            }
        }
        // (and the program as data: what the game's tables are read from)
        if (FILE *data = fopen((p.opt.dumpProgram + ".data").c_str(), "wb")) {
            fwrite(p.roms.program.data(), 1, p.roms.program.size(), data);
            fclose(data);
        }
        // (and the extra set's, if the game reads one: FILE.extra, FILE.extra.data)
        if (!p.extraRoms.opcodes.empty()) {
            if (FILE *extra = fopen((p.opt.dumpProgram + ".extra").c_str(), "wb")) {
                fwrite(p.extraRoms.opcodes.data(), 1, p.extraRoms.opcodes.size(), extra);
                fclose(extra);
            }
            if (FILE *extra = fopen((p.opt.dumpProgram + ".extra.data").c_str(), "wb")) {
                fwrite(p.extraRoms.program.data(), 1, p.extraRoms.program.size(), extra);
                fclose(extra);
            }
        }
        return 0;
    }
    if (p.opt.online && !p.StartOnline(error)) {
        SayFailed(p.opt, error);
        return 1;
    }
    if (p.opt.zoom > 0) {
        p.zoom.Set(p.opt.zoom, std::clamp(p.opt.zoomPlayer, 0, +port::Zoom::kDistance));
    }
    int result = 0;
    const auto begin = std::chrono::steady_clock::now();
    // (--record-video: the whole run, from its first frame: Port::StartVideo, finished below)
    if (!p.opt.videoOut.empty() && !p.StartVideo(p.opt.videoOut)) {
        return 1;
    }
    if (p.opt.headless) {
        std::vector<int16_t> sound;
        std::vector<float> music, musicMade;
        int musicRate = 0;
        // A picture as the window draws it (--present-test, --present-every): through the GPU path, read back, written
        // to `path`; in a window --present-size big, or one the picture's shape at 3 times its lines
        auto present = [&](const std::string &path, const std::vector<uint32_t> &board, const port::Zoomed *zoomed) {
            std::vector<uint32_t> drawn;
            std::string error;
            // (a vertical game's picture turned upright, as the window shows it: port::Turned)
            std::vector<uint32_t> upright;
            const bool turned = port::Turned();
            if (turned) {
                port::TurnPicture(board.data(), p.ShownWidth(), upright);
                zoomed = nullptr;
            }
            const std::vector<uint32_t> &shown = turned ? upright : board;
            int across = turned ? +Video::kHeight : p.ShownWidth();
            int lines = turned ? p.ShownWidth() : +Video::kHeight;
            // (a framed picture: its own size, in the shape of its columns)
            int shapeColumns = across, shapeLines = lines;
            if (zoomed && zoomed->columns) {
                across = zoomed->width;
                lines = zoomed->height;
                shapeColumns = zoomed->columns;
                shapeLines = Video::kHeight;
                zoomed = nullptr;
            }
            const int outHeight = p.opt.presentHeight > 0 ? p.opt.presentHeight : (turned ? 896 : 672);
            const int outWidth = p.opt.presentWidth > 0 ? p.opt.presentWidth
                                                        : static_cast<int>(std::lround(outHeight * port::PictureShape(shapeColumns, shapeLines, port::DisplayShape(), 4.0f / 3.0f)));
            const SDL_FRect where = port::FitPicture(outWidth, outHeight, shapeColumns, shapeLines, port::DisplayShape(), port::DisplayScaling());
            if (SDL_Init(SDL_INIT_VIDEO) &&
                port::Presenter::Render(shown.data(), across, lines, outWidth, outHeight, where, port::DisplayScaling(), drawn, error, zoomed)) {
                WritePng(path.c_str(), drawn.data(), outWidth, outHeight);
                return true;
            }
            fprintf(stderr, "--present-test: %s\n", error.empty() ? SDL_GetError() : error.c_str());
            return false;
        };
        // (online: at the board's pace, as a window would be, unless --unthrottled)
        auto paced = std::chrono::steady_clock::now();
        while (!p.quit && (p.opt.frames == 0 || p.frame < p.opt.frames)) {
            const bool ran = p.Tick();
            p.ReplayMake(); // (the instant replay being made, if it is)
            if (p.net && !p.opt.unthrottled) {
                paced += std::chrono::nanoseconds(static_cast<int64_t>(1e9 / Machine::kFramesPerSecond));
                std::this_thread::sleep_until(paced);
            }
            if (!ran) {
                continue;
            }
            if (p.opt.presentEvery) {
                // (the window's picture made every frame, as the window would, so that the zoom eases as it does there)
                std::vector<uint32_t> shown;
                port::Zoomed zoomed;
                const bool isZoomed = p.Windowed(shown, zoomed);
                if (p.frame % p.opt.presentEvery == 0) {
                    char path[512];
                    snprintf(path, sizeof(path), "%s%06llu.png", p.opt.presentPrefix.c_str(), static_cast<unsigned long long>(p.frame));
                    present(path, shown, isZoomed || zoomed.columns ? &zoomed : nullptr);
                }
            }
            if (!p.opt.audioDump.empty()) {
                sound.insert(sound.end(), p.machine->audio.begin(), p.machine->audio.end());
            }
            bool musicFresh = false;
            if ((!p.opt.soundtrackDump.empty() || p.recorder.Recording()) && p.soundtrackOn) {
                p.soundtrack.Make(p.machine->soundtrack.state, !FastForward(), musicMade);
                musicFresh = true;
            }
            if (p.recorder.Recording()) {
                const std::vector<int16_t> &made = p.machine->audio;
                p.VideoFrame(made.data(), made.size() / 2, musicFresh ? musicMade.data() : nullptr, musicFresh ? musicMade.size() / 2 : 0,
                             p.soundtrack.Rate());
            }
            if (!p.opt.soundtrackDump.empty() && p.soundtrackOn) {
                // (silence where nothing plays, so that the file's time is the game's; at the first song's rate)
                musicRate = musicRate ? musicRate : (musicMade.empty() ? 0 : p.soundtrack.Rate());
                const size_t due = musicRate ? static_cast<size_t>(p.frame * (musicRate / Machine::kFramesPerSecond)) * 2 : 0;
                if (music.size() + musicMade.size() < due) {
                    music.resize(due - musicMade.size());
                }
                music.insert(music.end(), musicMade.begin(), musicMade.end());
            }
            p.machine->audio.clear();
        }
        if (!p.opt.presentTest.empty()) {
            // (the picture the run ended on, through the GPU path and read back: what a window would show)
            std::vector<uint32_t> shown;
            port::Zoomed zoomed;
            const bool isZoomed = p.Windowed(shown, zoomed);
            if (!present(p.opt.presentTest, shown, isZoomed || zoomed.columns ? &zoomed : nullptr)) {
                result = 1;
            }
        }
        if (!p.opt.soundtrackDump.empty() && musicRate) {
            if (FILE *f = fopen(p.opt.soundtrackDump.c_str(), "wb")) {
                const uint32_t bytes = static_cast<uint32_t>(music.size() * 4), rate = static_cast<uint32_t>(musicRate);
                const uint32_t header[11] = {0x46464952, 36 + bytes, 0x45564157, 0x20746D66, 16,      0x00020003,
                                             rate,       rate * 8,   0x00200008, 0x61746164, bytes};
                fwrite(header, 4, 11, f);
                fwrite(music.data(), 4, music.size(), f);
                fclose(f);
            }
        }
        if (!p.opt.audioDump.empty()) {
            if (FILE *f = fopen(p.opt.audioDump.c_str(), "wb")) {
                const uint32_t bytes = static_cast<uint32_t>(sound.size() * 2), rate = QSound::kSampleRate;
                const uint32_t header[11] = {0x46464952, 36 + bytes, 0x45564157, 0x20746D66, 16,      0x00020001,
                                             rate,       rate * 4,   0x00100004, 0x61746164, bytes};
                fwrite(header, 4, 11, f);
                fwrite(sound.data(), 2, sound.size(), f);
                fclose(f);
            }
        }
    } else {
        result = port::RunWindowed(p);
    }
    while (p.replay.making) { // (a replay being made, finished: Port::ReplayMake)
        p.ReplayMake();
    }
    p.StopVideo();
    if (p.opt.stats) {
        const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count();
        fprintf(stderr, "%llu frames in %.2f s: %.0f a second (cpu: %s)\n", static_cast<unsigned long long>(p.frame), seconds,
                p.frame / seconds, p.opt.cpu.c_str());
        if (p.tick != p.frame) { // (slowed, sped up or stopped: the screen's pictures, which were not the game's frames)
            fprintf(stderr, "in %llu ticks of the screen\n", static_cast<unsigned long long>(p.tick));
        }
        if (p.opt.cpu == "native") {
            fprintf(stderr, "sound driver: %llu instructions had no translation\n",
                    static_cast<unsigned long long>(p.machine->soundCpu.interpreted));
            // (about 12 cycles to an instruction)
            fprintf(stderr, "%llu instructions had no translation and ran in the interpreter (about %.3f%% of the run)\n",
                    static_cast<unsigned long long>(p.machine->cpu.interpreted),
                    100.0 * p.machine->cpu.interpreted * 12.0 / std::max<uint64_t>(1, p.machine->cpu.cycles));
            if (p.machine->cpu.interpretedRam) {
                fprintf(stderr, "%llu of them the game's own code in RAM (no translation can have it): %llu of the ROM's\n",
                        static_cast<unsigned long long>(p.machine->cpu.interpretedRam),
                        static_cast<unsigned long long>(p.machine->cpu.interpreted - p.machine->cpu.interpretedRam));
            }
        }
        if (p.machine->soundSide == Machine::SoundSide::Check) {
            fprintf(stderr, "%llu writes to the sound chip the same from the driver and the Z80\n",
                    static_cast<unsigned long long>(p.machine->soundWritesChecked));
            p.machine->ReportSoundTimes();
        }
#ifdef CPS2RECOMP_CHECK
        fprintf(stderr, "%llu translated blocks checked against the interpreter, and %llu of the sound driver's\n",
                static_cast<unsigned long long>(BlocksChecked()), static_cast<unsigned long long>(Z80BlocksChecked()));
#endif
    }
    p.Finish();
    if (p.net && !p.net->ended.empty()) {
        // (why the match ended, if it did before the player left it)
        if (p.net->failed) {
            SayFailed(p.opt, p.net->ended);
            return 5;
        }
        fprintf(stderr, "online: %s\n", p.net->ended.c_str());
        // (a finished set closes quietly: the launcher that started the game is still there, waiting for it)
        if (!p.opt.headless && !p.net->SetFinished()) {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, GAME_NAME, p.net->ended.c_str(), nullptr);
        }
    }
    if (p.opt.checkRewrites && !ReportRewrites()) {
        return 4;
    }
    if (p.machine->nativeVideo.checking && !p.machine->nativeVideo.Report(stderr)) {
        return 6;
    }
#ifdef GAME_GUEST_TABLE
    {
        static const GuestGame kReported[] = GAME_GUEST_TABLE;
        bool same = true;
        for (const GuestGame &guest : kReported) {
            if (Machine *board = cps2recomp::Guest(guest.id); board && board->nativeVideo.checking) {
                same = board->nativeVideo.Report(stderr) && same;
            }
        }
        if (!same) {
            return 6;
        }
    }
#endif
    if (port::g_gpuPicture) {
        // (all reported, the game's and its guests', before any is said to have failed)
        bool same = port::g_gpuPicture->Report(stderr);
        for (const auto &g : p.guestPictures) {
            same = g.picture->Report(stderr) && same;
        }
        if (!same) {
            return 6;
        }
    }
    return ProgramEnded() >= 0 ? ProgramEnded() : result;
}
