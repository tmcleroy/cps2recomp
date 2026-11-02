// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026. Local Input Lab extension to cps2recomp.
#include "keyboards.h"
#include "../recomp-common/settings.h"
#include <algorithm>
namespace port {
KeyboardConfig LoadKeyboards(const std::string &path) {
    recomp::Ini ini(path); ini.load();
    KeyboardConfig c;
    const auto enabled = ini.get("keyboards", "separate");
    c.enabled = !enabled.empty() && !recomp::SettingOff(enabled);
    const auto mirror = ini.get("keyboards", "mirror_p1");
    c.mirrorP1 = mirror.empty() || !recomp::SettingOff(mirror);
    for (int p = 0; p < kInputPlayers; ++p) c.device[p] = ini.get("keyboards", "player" + std::to_string(p + 1));
    return c;
}
bool SaveKeyboards(const std::string &path, const KeyboardConfig &c) {
    if (path.empty()) return false;
    for (int p = 0; p < kInputPlayers; ++p) for (int other = 0; other < p; ++other)
        if (!c.device[p].empty() && c.device[p] == c.device[other]) return false;
    recomp::Ini ini(path); ini.load();
    ini.set("keyboards", "separate", c.enabled ? "1" : "0");
    ini.set("keyboards", "mirror_p1", c.mirrorP1 ? "1" : "0");
    for (int p = 0; p < kInputPlayers; ++p) ini.set("keyboards", "player" + std::to_string(p + 1), c.device[p]);
    return ini.save();
}
bool AssignKeyboardDevice(KeyboardConfig &c, int player, const std::string &device) {
    if (player < 0 || player >= kInputPlayers || device.empty()) return false;
    for (int other = 0; other < kInputPlayers; ++other)
        if (other != player && device == c.device[other]) return false;
    c.device[player] = device;
    if (!c.device[0].empty() && !c.device[1].empty()) c.enabled = true;
    return true;
}
void KeyboardState::Connect(const std::string &id, const std::string &name, const std::string &physicalID) {
    if (id.empty()) return;
    auto &d = devices_[id];
    const auto previousPhysical=d.physicalID;
    if (!d.connected) d = {};
    d.physicalID=!physicalID.empty()?physicalID:!previousPhysical.empty()?previousPhysical:id;
    d.name = name.empty() ? "Keyboard" : name;
    d.connected = true;
}
void KeyboardState::Disconnect(const std::string &id) {
    const auto it = devices_.find(id);
    if (it != devices_.end()) {
        it->second.connected=false;it->second.keys={};it->second.physical={};it->second.blocked={};
    }
    presses_.erase(std::remove_if(presses_.begin(), presses_.end(), [&](const auto &p) { return p.source == id; }), presses_.end());
}
void KeyboardState::Focus(bool focused) {
    if (focused_ == focused) return;
    focused_ = focused;
    presses_.clear();
    if (!focused) for (auto &[id, d] : devices_) {
        d.keys.fill(false);
        d.blocked = d.physical;
    }
}
SDL_Keymod KeyboardState::Modifiers(const Keys &k) const {
    unsigned mods = 0;
    if (k[SDL_SCANCODE_LCTRL]) mods |= SDL_KMOD_LCTRL;
    if (k[SDL_SCANCODE_RCTRL]) mods |= SDL_KMOD_RCTRL;
    if (k[SDL_SCANCODE_LSHIFT]) mods |= SDL_KMOD_LSHIFT;
    if (k[SDL_SCANCODE_RSHIFT]) mods |= SDL_KMOD_RSHIFT;
    if (k[SDL_SCANCODE_LALT]) mods |= SDL_KMOD_LALT;
    if (k[SDL_SCANCODE_RALT]) mods |= SDL_KMOD_RALT;
    if (k[SDL_SCANCODE_LGUI]) mods |= SDL_KMOD_LGUI;
    if (k[SDL_SCANCODE_RGUI]) mods |= SDL_KMOD_RGUI;
    return static_cast<SDL_Keymod>(mods);
}
void KeyboardState::Key(const std::string &id, SDL_Scancode key, bool down) {
    if (id.empty() || key <= SDL_SCANCODE_UNKNOWN || key >= SDL_SCANCODE_COUNT) return;
    if (!Connected(id)) Connect(id, "Keyboard");
    auto &d = devices_[id];
    const auto group=PhysicalID(id);
    bool repeat=false;
    for(const auto &[other,device]:devices_)if(device.connected && device.physicalID==group && device.keys[key])repeat=true;
    d.physical[key] = down;
    if (!down) d.blocked[key] = false;
    if (!focused_) { d.keys[key] = false; d.blocked[key] = down; return; }
    if(down)for(const auto &[other,device]:devices_)if(device.connected && device.physicalID==group && device.blocked[key]) {
        d.blocked[key]=true;return;
    }
    d.keys[key] = down;
    if (down) {
        if (presses_.size() == 256) presses_.erase(presses_.begin());
        presses_.push_back({group, key, Modifiers(GroupSnapshot(group).keys), repeat, id});
    }
}
const KeyboardState::Keys &KeyboardState::ForPlayer(const KeyboardConfig &c, int p) const {
    static const Keys empty{};
    if(p<0 || p>=kInputPlayers)return empty;
    playerCache_[p]=Snapshot(c,p);
    return playerCache_[p].keys;
}
std::string KeyboardState::PhysicalID(const std::string &id) const {
    const auto it=devices_.find(id);
    return it!=devices_.end() && !it->second.physicalID.empty()?it->second.physicalID:id;
}
KeyboardConfig KeyboardState::ResolveConfig(const KeyboardConfig &c) const {
    auto resolved=c;
    for(int p=0;p<kInputPlayers;++p)resolved.device[p]=PhysicalID(c.device[p]);
    return resolved;
}
KeyboardState::Device KeyboardState::GroupSnapshot(const std::string &group) const {
    Device result;result.physicalID=group;
    if(group.empty())return result;
    for(const auto &[id,d]:devices_)if(d.connected && d.physicalID==group) {
        result.connected=true;
        if(result.name.empty() || result.name=="Keyboard")result.name=d.name;
        for(size_t k=0;k<result.keys.size();++k) {
            result.keys[k]=result.keys[k]||d.keys[k];
            result.physical[k]=result.physical[k]||d.physical[k];
            result.blocked[k]=result.blocked[k]||d.blocked[k];
        }
    }
    return result;
}
KeyboardState::Device KeyboardState::Snapshot(const KeyboardConfig &c,int p) const {
    if(p<0 || p>=kInputPlayers || c.device[p].empty())return {};
    for (int other = 0; other < p; ++other)
        if (PhysicalID(c.device[other]) == PhysicalID(c.device[p])) return {};
    return GroupSnapshot(PhysicalID(c.device[p]));
}
int KeyboardState::PlayerFor(const KeyboardConfig &c,const std::string &id) const {
    if(id.empty())return 0;
    const auto group=PhysicalID(id);
    for(int p=0;p<kInputPlayers;++p)if(!c.device[p].empty() && PhysicalID(c.device[p])==group)return p+1;
    return 0;
}
int KeyboardState::Interfaces(const std::string &id) const {
    const auto group=PhysicalID(id);int count=0;
    for(const auto &[path,d]:devices_)if(d.connected && d.physicalID==group)++count;
    return count;
}
bool KeyboardState::Connected(const std::string &id) const {
    const auto it = devices_.find(id);
    return it != devices_.end() && it->second.connected;
}
std::vector<KeyboardPress> KeyboardState::TakePresses() {
    std::vector<KeyboardPress> result; result.swap(presses_); return result;
}
SDL_Scancode KeyboardBindingKey(const KeyboardConfig &c, int player, const std::vector<KeyboardPress> &presses) {
    if (!c.enabled || player < 0 || player >= kInputPlayers) return SDL_SCANCODE_UNKNOWN;
    for (const auto &press : presses) {
        if (press.repeat) continue;
        if (press.key == SDL_SCANCODE_ESCAPE) return press.key;
        if (!c.device[player].empty() && c.device[0] != c.device[1] && press.device == c.device[player]) return press.key;
    }
    return SDL_SCANCODE_UNKNOWN;
}
bool KeyboardHotkeyAllowed(const KeyboardConfig &c, const KeyboardPress &p) {
    return p.key == SDL_SCANCODE_ESCAPE || (!c.device[0].empty() && p.device == c.device[0]);
}
SDL_Scancode RawKeyboardScancode(unsigned code, unsigned flags, unsigned vk) {
    const bool e0 = (flags & 2) != 0, e1 = (flags & 4) != 0;
    if (vk == 255 || (e1 && vk != 0x13)) return SDL_SCANCODE_UNKNOWN;
    if (vk == 0x13) return SDL_SCANCODE_PAUSE;
    if (e0) switch (code) {
    case 0x1C: return SDL_SCANCODE_KP_ENTER; case 0x1D: return SDL_SCANCODE_RCTRL;
    case 0x35: return SDL_SCANCODE_KP_DIVIDE; case 0x37: return SDL_SCANCODE_PRINTSCREEN;
    case 0x38: return SDL_SCANCODE_RALT; case 0x47: return SDL_SCANCODE_HOME;
    case 0x48: return SDL_SCANCODE_UP; case 0x49: return SDL_SCANCODE_PAGEUP;
    case 0x4B: return SDL_SCANCODE_LEFT; case 0x4D: return SDL_SCANCODE_RIGHT;
    case 0x4F: return SDL_SCANCODE_END; case 0x50: return SDL_SCANCODE_DOWN;
    case 0x51: return SDL_SCANCODE_PAGEDOWN; case 0x52: return SDL_SCANCODE_INSERT;
    case 0x53: return SDL_SCANCODE_DELETE; case 0x5B: return SDL_SCANCODE_LGUI;
    case 0x5C: return SDL_SCANCODE_RGUI; case 0x5D: return SDL_SCANCODE_APPLICATION;
    default: return SDL_SCANCODE_UNKNOWN;
    }
    if (code >= 2 && code <= 10) return static_cast<SDL_Scancode>(SDL_SCANCODE_1 + code - 2);
    if (code == 11) return SDL_SCANCODE_0;
    static constexpr SDL_Scancode top[] = {SDL_SCANCODE_Q,SDL_SCANCODE_W,SDL_SCANCODE_E,SDL_SCANCODE_R,SDL_SCANCODE_T,SDL_SCANCODE_Y,SDL_SCANCODE_U,SDL_SCANCODE_I,SDL_SCANCODE_O,SDL_SCANCODE_P};
    static constexpr SDL_Scancode middle[] = {SDL_SCANCODE_A,SDL_SCANCODE_S,SDL_SCANCODE_D,SDL_SCANCODE_F,SDL_SCANCODE_G,SDL_SCANCODE_H,SDL_SCANCODE_J,SDL_SCANCODE_K,SDL_SCANCODE_L};
    static constexpr SDL_Scancode bottom[] = {SDL_SCANCODE_Z,SDL_SCANCODE_X,SDL_SCANCODE_C,SDL_SCANCODE_V,SDL_SCANCODE_B,SDL_SCANCODE_N,SDL_SCANCODE_M};
    if (code >= 0x10 && code <= 0x19) return top[code-0x10];
    if (code >= 0x1E && code <= 0x26) return middle[code-0x1E];
    if (code >= 0x2C && code <= 0x32) return bottom[code-0x2C];
    if (code >= 0x3B && code <= 0x44) return static_cast<SDL_Scancode>(SDL_SCANCODE_F1 + code-0x3B);
    switch (code) {
    case 1:return SDL_SCANCODE_ESCAPE; case 0x0C:return SDL_SCANCODE_MINUS; case 0x0D:return SDL_SCANCODE_EQUALS;
    case 0x0E:return SDL_SCANCODE_BACKSPACE; case 0x0F:return SDL_SCANCODE_TAB;
    case 0x1A:return SDL_SCANCODE_LEFTBRACKET; case 0x1B:return SDL_SCANCODE_RIGHTBRACKET;
    case 0x1C:return SDL_SCANCODE_RETURN; case 0x1D:return SDL_SCANCODE_LCTRL;
    case 0x27:return SDL_SCANCODE_SEMICOLON; case 0x28:return SDL_SCANCODE_APOSTROPHE;
    case 0x29:return SDL_SCANCODE_GRAVE; case 0x2A:return SDL_SCANCODE_LSHIFT; case 0x2B:return SDL_SCANCODE_BACKSLASH;
    case 0x33:return SDL_SCANCODE_COMMA; case 0x34:return SDL_SCANCODE_PERIOD; case 0x35:return SDL_SCANCODE_SLASH;
    case 0x36:return SDL_SCANCODE_RSHIFT; case 0x37:return SDL_SCANCODE_KP_MULTIPLY; case 0x38:return SDL_SCANCODE_LALT;
    case 0x39:return SDL_SCANCODE_SPACE; case 0x3A:return SDL_SCANCODE_CAPSLOCK;
    case 0x45:return SDL_SCANCODE_NUMLOCKCLEAR; case 0x46:return SDL_SCANCODE_SCROLLLOCK;
    case 0x47:return SDL_SCANCODE_KP_7; case 0x48:return SDL_SCANCODE_KP_8; case 0x49:return SDL_SCANCODE_KP_9;
    case 0x4A:return SDL_SCANCODE_KP_MINUS; case 0x4B:return SDL_SCANCODE_KP_4; case 0x4C:return SDL_SCANCODE_KP_5;
    case 0x4D:return SDL_SCANCODE_KP_6; case 0x4E:return SDL_SCANCODE_KP_PLUS;
    case 0x4F:return SDL_SCANCODE_KP_1; case 0x50:return SDL_SCANCODE_KP_2; case 0x51:return SDL_SCANCODE_KP_3;
    case 0x52:return SDL_SCANCODE_KP_0; case 0x53:return SDL_SCANCODE_KP_PERIOD;
    case 0x56:return SDL_SCANCODE_NONUSBACKSLASH; case 0x57:return SDL_SCANCODE_F11; case 0x58:return SDL_SCANCODE_F12;
    default:return SDL_SCANCODE_UNKNOWN;
    }
}
} // namespace port
