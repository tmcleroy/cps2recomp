// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026. Local Input Lab extension to cps2recomp.
#include "device_controls.h"
#include "layout.h"
#include "../../port/gamepad_assignment.h"


namespace {
void DrawGamepadAssignments(launcher::PageContext &page) {
    using namespace layout; using namespace ui;
    const auto &palette = ui::palette();
    const auto path = (page.prefs.path().parent_path() / "gamepads.ini").string();
    auto config = port::LoadGamepadAssignments(path);
    std::vector<port::GamepadDevice> devices;
    int count = 0; auto *ids = SDL_GetGamepads(&count);
    for (int n = 0; n < count; ++n) if (auto *pad = SDL_OpenGamepad(ids[n])) {
        devices.push_back(port::DescribeGamepad(pad)); SDL_CloseGamepad(pad);
    }
    SDL_free(ids);
    text("GAMEPADS - SHARED BY ALL GAMES", Font::Semibold, size::kDetail, palette.secondary);
    gap(8);
    text("Choose which physical gamepad controls each player. A single gamepad can be assigned to Player 2 while Player 1 uses a keyboard.",
         Font::Regular, size::kDetail, palette.secondary, page.width);
    gap(10);
    ImGui::BeginDisabled(page.playing);
    Card card(page.width);
    for (int player = 0; player < port::kBoundPlayersMost; ++player) {
        if (player) inset_separator(page.width);
        ImGui::PushID(920 + player);
        std::vector<std::string> values{"auto", "none"}, labels{"Automatic", "Not assigned"};
        for (size_t n = 0; n < devices.size(); ++n) {
            values.push_back(devices[n].key);
            labels.push_back(devices[n].name + " (Controller " + std::to_string(n + 1) + ")");
        }
        int selected = 0;
        for (size_t n = 0; n < values.size(); ++n) if (values[n] == config.device[player]) selected = static_cast<int>(n);
        if (selected == 0 && config.device[player] != "auto") {
            values.push_back(config.device[player]); labels.push_back("Saved gamepad (disconnected)"); selected = static_cast<int>(values.size() - 1);
        }
        const auto resolved = port::ResolveGamepadAssignments(config, devices);
        std::string status = "No gamepad assigned or connected.";
        for (const auto &device : devices) if (resolved[player] == device.id) status = "Controls this player: " + device.name;
        const std::string title = "Player " + std::to_string(player + 1) + " gamepad";
        row(page.width, title.c_str(), status.c_str(), 390, 36, [&] {
            if (dropdown("##physical_gamepad", labels, selected, 390)) {
                port::AssignGamepad(config, player, values[selected]);
                port::SaveGamepadAssignments(path, config);
            }
        });
        ImGui::PopID();
    }
    card.end(); ImGui::EndDisabled(); gap(18);
}
} // namespace

void DrawGamepadControls(launcher::PageContext &page) { DrawGamepadAssignments(page); }
