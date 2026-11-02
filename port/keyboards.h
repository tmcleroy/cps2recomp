// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026. Local Input Lab extension to cps2recomp.
#pragma once
#include <SDL3/SDL.h>
#include "bindings.h"
#include <array>
#include <map>
#include <string>
#include <vector>
namespace port {
inline constexpr int kInputPlayers = kBoundPlayersMost;
struct KeyboardConfig {
    bool enabled = false;
    bool mirrorP1 = true;
    std::array<std::string, kInputPlayers> device;
};
KeyboardConfig LoadKeyboards(const std::string &path);
bool SaveKeyboards(const std::string &path, const KeyboardConfig &config);
bool AssignKeyboardDevice(KeyboardConfig &config, int player, const std::string &device);
struct KeyboardPress {
    std::string device;
    SDL_Scancode key;
    SDL_Keymod mods;
    bool repeat;
    std::string source; // Native interface, retained for disconnect bookkeeping.
};
// State belongs to a physical device. Releasing W on P1 never releases P2's W.
class KeyboardState {
public:
    using Keys = std::array<bool, SDL_SCANCODE_COUNT>;
    struct Device {
        std::string name;
        std::string physicalID;
        bool connected = false;
        Keys keys{}, physical{}, blocked{};
    };
    void Connect(const std::string &id, const std::string &name, const std::string &physicalID = {});
    void Disconnect(const std::string &id);
    void Focus(bool focused);
    void Key(const std::string &id, SDL_Scancode key, bool down);
    const Keys &ForPlayer(const KeyboardConfig &config, int player) const;
    SDL_Keymod Modifiers(const Keys &keys) const;
    std::vector<KeyboardPress> TakePresses();
    const std::map<std::string, Device> &Devices() const { return devices_; }
    bool Connected(const std::string &id) const;
    bool Focused() const { return focused_; }
    std::string PhysicalID(const std::string &id) const;
    KeyboardConfig ResolveConfig(const KeyboardConfig &config) const;
    Device Snapshot(const KeyboardConfig &config, int player) const;
    int PlayerFor(const KeyboardConfig &config, const std::string &id) const;
    int Interfaces(const std::string &id) const;
private:
    Device GroupSnapshot(const std::string &physicalID) const;
    mutable std::array<Device,kInputPlayers> playerCache_;
    bool focused_ = false;
    std::map<std::string, Device> devices_;
    std::vector<KeyboardPress> presses_;
};
SDL_Scancode KeyboardBindingKey(const KeyboardConfig &config, int player, const std::vector<KeyboardPress> &presses);
bool KeyboardHotkeyAllowed(const KeyboardConfig &config, const KeyboardPress &press);
// Physical Windows set-1 scan codes, independent of the desktop's combined key state.
SDL_Scancode RawKeyboardScancode(unsigned makeCode, unsigned flags, unsigned virtualKey);
} // namespace port
