// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of recomp-common: https://github.com/tmcleroy/recomp-common
//
// Which key and which gamepad button does what, for each player: the launcher's Controllers page writes the
// player's choices to settings.ini and the game reads them. Both include this file (it needs only SDL3) and
// hand it the port's table of actions; nothing else here is about a system.
//
//   [keyboard1]         a key by SDL's name for it ("Up", "A", "Return"), or "none"
//   up = Up
//   [gamepad1]          a button or a trigger by SDL's name ("a", "leftshoulder", "righttrigger"), or "none"
//   lp = x
//
// (and [keyboard2], [gamepad2] for player 2, and so on for a port with more players). What is not in the file
// is as the table's usual layout has it. The first gamepad is player 1's, the second player 2's, and so on.
//
// A game reads the file once when it starts (LoadBindings) and from then on looks only at the table in
// memory: nothing is looked up by name while playing.
#pragma once

#include <SDL3/SDL.h>

#include <fstream>
#include <string>

namespace recomp {

inline constexpr int kBindPlayers = 2;    // how many players a table has unless it says
inline constexpr int kBindMaxPlayers = 4; // ... and the most it can say
inline constexpr int kBindMaxActions = 32;

// A gamepad button, trigger or signed stick direction, or nothing
struct PadInput {
    int button = -1; // SDL_GamepadButton
    int axis = -1;   // SDL_GamepadAxis
    int direction = 1; // -1 or +1 for a stick half-axis; triggers use +1
    bool operator==(const PadInput &other) const {
        return button == other.button && axis == other.axis && (axis < 0 || direction == other.direction);
    }
    bool operator!=(const PadInput &other) const { return !(*this == other); }
};

// Something that can be bound: its name in settings.ini, what the launcher calls it, and the usual layout
// (each player's key and gamepad input when nothing is chosen)
struct BindAction {
    const char *name;
    const char *title;
    SDL_Scancode key[kBindMaxPlayers]; // (players a table lists none for: no key, no gamepad input)
    PadInput pad[kBindMaxPlayers];
};

// A port's actions (at most kBindMaxActions) and how many players have them (1 to kBindMaxPlayers)
struct BindTable {
    const BindAction *actions = nullptr;
    int count = 0;
    int players = kBindPlayers;
};

struct Bindings {
    SDL_Scancode key[kBindMaxPlayers][kBindMaxActions];
    PadInput pad[kBindMaxPlayers][kBindMaxActions];
};

// The layout with nothing chosen
inline Bindings DefaultBindings(const BindTable &table) {
    Bindings b{};
    for (int player = 0; player < kBindMaxPlayers; player++) {
        for (int action = 0; action < kBindMaxActions; action++) {
            const bool has = action < table.count && player < table.players;
            b.key[player][action] = has ? table.actions[action].key[player] : SDL_SCANCODE_UNKNOWN;
            b.pad[player][action] = has ? table.actions[action].pad[player] : PadInput{};
        }
    }
    return b;
}

// --- as text, for settings.ini ---

inline std::string KeyText(SDL_Scancode key) {
    const char *name = key == SDL_SCANCODE_UNKNOWN ? "" : SDL_GetScancodeName(key);
    return name && *name ? name : "none";
}
inline SDL_Scancode KeyFromText(const std::string &text) {
    return text == "none" ? SDL_SCANCODE_UNKNOWN : SDL_GetScancodeFromName(text.c_str());
}
inline constexpr int kPadThreshold = 16000;
inline bool IsStickAxis(int axis) {
    return axis >= SDL_GAMEPAD_AXIS_LEFTX && axis <= SDL_GAMEPAD_AXIS_RIGHTY;
}
inline std::string PadText(PadInput input) {
    const char *name = input.button >= 0 ? SDL_GetGamepadStringForButton(static_cast<SDL_GamepadButton>(input.button))
                       : input.axis >= 0 ? SDL_GetGamepadStringForAxis(static_cast<SDL_GamepadAxis>(input.axis)) : nullptr;
    if (!name || !*name) return "none";
    return std::string(name) + (input.button < 0 && IsStickAxis(input.axis) ? (input.direction < 0 ? "-" : "+") : "");
}
inline PadInput PadFromText(const std::string &text) {
    if (text.empty() || text == "none") return {};
    const int button = SDL_GetGamepadButtonFromString(text.c_str());
    if (button >= 0) return {button, -1};
    const char sign = text.back();
    const bool signedAxis = sign == '-' || sign == '+';
    const std::string name = signedAxis ? text.substr(0, text.size() - 1) : text;
    const int axis = SDL_GetGamepadAxisFromString(name.c_str());
    if (signedAxis && IsStickAxis(axis)) return {-1, axis, sign == '-' ? -1 : 1};
    if (!signedAxis && (axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER || axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER)) return {-1, axis};
    return {}; // Stick axes require a direction; preserve rejection of ambiguous/invalid values.
}
// Shared by the launcher's capture path and the native virtual-controller tests.
inline PadInput PadInputFromEvent(const SDL_Event &event) {
    if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) return {event.gbutton.button, -1};
    if (event.type != SDL_EVENT_GAMEPAD_AXIS_MOTION) return {};
    const int axis = event.gaxis.axis, value = event.gaxis.value;
    if (IsStickAxis(axis) && (value < -kPadThreshold || value > kPadThreshold)) return {-1, axis, value < 0 ? -1 : 1};
    if ((axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER || axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) && value > kPadThreshold) return {-1, axis};
    return {};
}
inline bool PadInputPressed(SDL_Gamepad *gamepad, PadInput input) {
    if (!gamepad) return false;
    if (input.button >= 0) return SDL_GetGamepadButton(gamepad, static_cast<SDL_GamepadButton>(input.button));
    if (input.axis < 0) return false;
    const int value = SDL_GetGamepadAxis(gamepad, static_cast<SDL_GamepadAxis>(input.axis));
    return input.direction < 0 ? value < -kPadThreshold : value > kPadThreshold;
}
inline std::string BindingSection(bool gamepad, int player) {
    return (gamepad ? "gamepad" : "keyboard") + std::to_string(player + 1);
}

// One line of settings.ini taken in: `section` is the heading it is under
inline void ApplyBinding(Bindings &b, const BindTable &table, const std::string &section, const std::string &name, const std::string &value) {
    for (int player = 0; player < table.players; player++) {
        for (int gamepad = 0; gamepad < 2; gamepad++) {
            if (section != BindingSection(gamepad, player)) {
                continue;
            }
            for (int action = 0; action < table.count; action++) {
                if (name == table.actions[action].name) {
                    if (gamepad) {
                        b.pad[player][action] = PadFromText(value);
                    } else if (value == "none" || KeyFromText(value) != SDL_SCANCODE_UNKNOWN) {
                        b.key[player][action] = KeyFromText(value);
                    }
                }
            }
        }
    }
}

// The player's choices from settings.ini (the usual layout where it says nothing, or there is no file)
inline Bindings LoadBindings(const std::string &settingsPath, const BindTable &table) {
    Bindings b = DefaultBindings(table);
    std::ifstream file(settingsPath);
    std::string line, section;
    auto trim = [](std::string text) {
        const size_t first = text.find_first_not_of(" \t\r"), last = text.find_last_not_of(" \t\r");
        return first == std::string::npos ? std::string() : text.substr(first, last - first + 1);
    };
    while (std::getline(file, line)) {
        line = trim(line);
        const size_t equals = line.find('=');
        if (!line.empty() && line[0] == '[') {
            section = line.substr(1, line.find(']') - 1);
        } else if (equals != std::string::npos) {
            ApplyBinding(b, table, section, trim(line.substr(0, equals)), trim(line.substr(equals + 1)));
        }
    }
    return b;
}

// Some player has this gamepad input bound (then it does not also do what it does otherwise while playing)
inline bool PadInputBound(const Bindings &b, const BindTable &table, PadInput input) {
    for (int player = 0; player < table.players; player++) {
        for (int action = 0; action < table.count; action++) {
            if (b.pad[player][action] == input) {
                return true;
            }
        }
    }
    return false;
}

inline void BindKeyboardKey(Bindings &bindings, const BindTable &table, int player, int action,
                            SDL_Scancode key, bool playerScoped = false) {
    if (player < 0 || player >= table.players || action < 0 || action >= table.count) return;
    const auto was = bindings.key[player][action];
    for (int who = 0; who < table.players; ++who) {
        if (playerScoped && who != player) continue;
        for (int other = 0; other < table.count; ++other)
            if ((who != player || other != action) && key != SDL_SCANCODE_UNKNOWN && bindings.key[who][other] == key)
                bindings.key[who][other] = who == player ? was : SDL_SCANCODE_UNKNOWN;
    }
    bindings.key[player][action] = key;
}

} // namespace recomp
