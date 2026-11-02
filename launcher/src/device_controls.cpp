// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026. Local Input Lab extension to cps2recomp.
#include "device_controls.h"
#include "../../port/raw_keyboards.h"
#include "layout.h"
#include <memory>
namespace {
std::unique_ptr<port::RawKeyboards> reader;
int capturePlayer = -1;
std::string path, error;
std::vector<port::KeyboardPress> bindingPresses;
void Save(const port::KeyboardConfig &config) {
    error = port::SaveKeyboards(path, config) ? "" : "Could not save keyboard assignments.";
}
}
bool CapturingKeyboardControls() { return capturePlayer >= 0; }
SDL_Scancode PollKeyboardBinding(int player) {
    const auto saved=port::LoadKeyboards(path);
    const auto key = port::KeyboardBindingKey(reader ? reader->State().ResolveConfig(saved) : saved, player, bindingPresses);
    bindingPresses.clear();
    return key;
}
bool ConsumeKeyboardControlsEvent(const SDL_Event &event) {
    if (capturePlayer < 0) return false;
    if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) { capturePlayer = -1; return false; }
    if (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) {
        if (event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_ESCAPE) capturePlayer = -1;
        return true;
    }
    return false;
}
void DrawKeyboardControls(launcher::PageContext &page) {
    using namespace ui; using namespace layout;
    const auto &p = palette();
    path = (page.prefs.path().parent_path() / "keyboards.ini").string();
    if (!reader) reader = std::make_unique<port::RawKeyboards>();
    const bool ready = reader->Open(page.window);
    reader->PollFocus();
    auto config = port::LoadKeyboards(path);
    bindingPresses = reader->State().TakePresses();
    const bool assigning = capturePlayer >= 0;
    for (const auto &press : bindingPresses) {
        if (capturePlayer < 0 || press.repeat) continue;
        if (press.key == SDL_SCANCODE_ESCAPE) { capturePlayer = -1; continue; }
        bool used = false;
        for (int other = 0; other < port::kInputPlayers; ++other)
            used = used || (other != capturePlayer && !config.device[other].empty() &&
                            press.device == reader->State().PhysicalID(config.device[other]));
        if (used) {
            error = "That keyboard is already assigned to the other player. Press a key on a different keyboard.";
            continue;
        }
        if (!port::AssignKeyboardDevice(config, capturePlayer, press.device)) continue;
        Save(config); capturePlayer = -1;
    }
    if (assigning) bindingPresses.clear();
    text("KEYBOARDS - SHARED BY ALL GAMES", Font::Semibold, size::kDetail, p.secondary);
    gap(8);
    text("Assign one physical keyboard to each player. Both can use the same keys. "
         "Connecting or disconnecting a keyboard never changes your saved game controls.",
         Font::Regular, size::kDetail, p.secondary, page.width);
    gap(10);
    ImGui::BeginDisabled(page.playing);
    Card card(page.width);
    ImGui::BeginDisabled(!ready);
    row(page.width, "Use separate keyboards", "Assigning both keyboards turns this on. Off merges all keyboards for gameplay.", 46, 26, [&] {
        if (toggle("##separate_keyboards", config.enabled)) { config.enabled = !config.enabled; Save(config); }
    });
    for (int player = 0; player < port::kInputPlayers; ++player) {
        inset_separator(page.width); ImGui::PushID(800 + player);
        const auto snapshot = reader->State().Snapshot(config,player);
        std::string status = config.device[player].empty() ? "Not assigned" :
            snapshot.connected ? snapshot.name + " (connected; " + std::to_string(reader->State().Interfaces(config.device[player]))+
                                 " interfaces)" : "Assigned keyboard disconnected";
        if (capturePlayer == player) status = "Press any key on this player's keyboard. Esc cancels.";
        const std::string title = "Player " + std::to_string(player + 1) + " keyboard";
        row(page.width, title.c_str(), status.c_str(), 240, 34, [&] {
            if (button(capturePlayer == player ? "Cancel" : "Assign keyboard", Kind::Secondary, 155, true, 34)) {
                reader->State().TakePresses();
                capturePlayer = capturePlayer == player ? -1 : player; error.clear();
            }
            ImGui::SameLine(0, px(8));
            if (button("Clear", Kind::Ghost, 64, true, 34)) {
                config.device[player].clear(); capturePlayer = -1; Save(config);
            }
        });
        const auto &keys = reader->State().ForPlayer(config, player);
        std::string held;
        for (int key = 1; key < SDL_SCANCODE_COUNT; ++key) if (keys[key]) {
            if (!held.empty()) held += " + ";
            held += SDL_GetScancodeName(static_cast<SDL_Scancode>(key));
        }
        row(page.width, "Keys held", held.empty() ? "Press several keys to test this keyboard." : held.c_str(), 0, 0, [] {});
        ImGui::PopID();
    }
    inset_separator(page.width);
    ImGui::BeginDisabled(!config.enabled);
    row(page.width, "P2 uses Player 1's layout", "Same keys. P2's saved settings are kept and return when this is off.", 46, 26, [&] {
        if (toggle("##mirror_keyboard", config.mirrorP1)) { config.mirrorP1 = !config.mirrorP1; Save(config); }
    });
    ImGui::EndDisabled();
    ImGui::EndDisabled(); card.end(); ImGui::EndDisabled();
    gap(8);
    if (!ready) text(reader->Error().c_str(), Font::Regular, size::kDetail, p.secondary, page.width);
    if (!config.enabled && !config.device[0].empty() && !config.device[1].empty())
        text("SEPARATE KEYBOARDS ARE OFF. Turn on Use separate keyboards above to play independently. "
             "Your saved layouts will still be kept separately while this is off.", Font::Semibold, size::kBody, p.text, page.width);
    if (config.enabled && (config.device[0].empty() || config.device[1].empty()))
        text("Assign both keyboards above. Unassigned keyboards do not control either player.", Font::Regular, size::kDetail, p.secondary, page.width);
    if (!error.empty()) text(error.c_str(), Font::Regular, size::kDetail, p.text, page.width);
    text("While separate keyboards are on, global shortcuts belong to P1; Esc opens the menu from any keyboard. "
         "This separates the players' input. Each keyboard's own hardware rollover still applies.",
         Font::Regular, size::kDetail, p.secondary, page.width);
    gap(22); request_frames(2);
}

bool PhysicalKeyboardCapture() { return port::LoadKeyboards(path).enabled; }
int PhysicalKeyboardLayout(int player) {
    const auto config = port::LoadKeyboards(path);
    return player == 1 && config.enabled && config.mirrorP1 ? 0 : player;
}
// SDL's Windows keyboard merger must not own the physical event stream.
namespace { const bool rawPolicy = [] {
    SDL_SetHintWithPriority(SDL_HINT_WINDOWS_RAW_KEYBOARD, "0", SDL_HINT_OVERRIDE); return true;
}(); }
