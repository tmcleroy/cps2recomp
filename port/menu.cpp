// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
//
// The in-game menu (menu.h): the shared menu told what a CPS-2 game's is made of.
#include "menu.h"

#include <cstring>

#include "../recomp-common/menu_layout.h"

#include "../machine/machine.h"
#include "../runtime/game.h"

#include <algorithm>
#include <functional>
#include <iterator>
#include <memory>
#include <set>
#include <string>

namespace port {

namespace {
    // The first page's own rows
    enum { kCoin = 1, kStart2, kTest, kLeave };

} // namespace

void Menu::OpenOnline(Online online) {
    m_menu.Close();
    recomp::MenuConfig config;
    config.groups = {{{recomp::MenuRow::Page, 0, kOnlinePage}},
                     {{recomp::MenuRow::Mute}, {recomp::MenuRow::Resume}},
                     {{recomp::MenuRow::Action, kLeave, online.leave, online.leaveDescription}}};
    config.pages.push_back({kOnlinePage, "", "The match's modes and your own picks for them.", false});
    config.pages.insert(config.pages.end(), online.pages.begin(), online.pages.end());
    config.features = std::move(online.features);
    config.commands = std::move(online.commands);
    config.value = std::move(online.value);
    config.setValue = std::move(online.setValue);
    config.muted = [this] { return muted; };
    config.setMuted = [this](bool to) { muted = to; };
    config.confirmName = recomp::sdl::kConfirmName;
    config.closeName = recomp::sdl::kCloseName;
    m_onlineMenu = recomp::Menu();
    m_onlineMenu.SetCompact(true); // (drawn on the game's own picture, at the board's size)
    m_onlineMenu.Configure(std::move(config));
    m_onlineMenu.Open();
    m_onlineMenu.Buttons(recomp::kMenuConfirm); // (into the Online match page, its first row: the first page's first)
    m_online = true;
}

void Menu::Open(bool withStates) {
    m_online = false;
    m_onlineMenu.Close();
    const cps2recomp::Game &game = cps2recomp::TheGame();
    recomp::MenuConfig config;
    // The standard layout (recomp-common/menu_layout.h, taken from this toolkit's, issue #196): Online, Display, Game
    // and Stage first; Alternative modes; the save states; the other pages and the board's own rows; leaving. Under
    // Game: the speed, Mute, and the Board, Cheats and Sound pages.
    // (Game, Board, Cheats and Sound first, in that order, as the Game page lists them: the game's own page, with its
    // description, where it has one; their rows count the features that are on)
    const auto own = [&game](const char *name) -> const cps2recomp::Page * {
        for (const cps2recomp::Page &page : game.pages) {
            if (!strcmp(page.name, name)) {
                return &page;
            }
        }
        return nullptr;
    };
    const char *const standard[] = {recomp::layout::kGame, recomp::layout::kBoard, recomp::layout::kCheats, recomp::layout::kSound};
    for (const char *name : standard) {
        const cps2recomp::Page *page = own(name);
        config.pages.push_back({name, page ? page->parent : "", page ? page->description : ""});
    }
    config.pages.push_back({"Arcade monitor", "Display", "A CRT at the board's own resolution: set on the launcher."});
    std::vector<std::string> modes;
    for (const cps2recomp::Page &page : game.pages) {
        if (std::any_of(std::begin(standard), std::end(standard), [&page](const char *name) { return !strcmp(page.name, name); })) {
            continue;
        }
        // (such pages are ways of running the game: their rows do not count features that are on)
        config.pages.push_back({page.name, page.parent, page.description, false});
        if (page.top && !*page.parent && strcmp(page.name, recomp::layout::kModes)) {
            modes.push_back(page.name);
        }
    }
    recomp::layout::Rows rows;
    // (Stage, for a game with a choice of stage: where the fights are, whatever is played; fight/session.h)
    if (std::any_of(game.features.begin(), game.features.end(),
                    [](const cps2recomp::Feature &feature) { return !strcmp(feature.category, recomp::layout::kStage); })) {
        rows.heads.push_back(recomp::layout::kStage);
    }
    rows.modes = std::move(modes);
    rows.states = withStates;
    rows.actions = {
        {recomp::MenuRow::Action, kCoin, recomp::layout::kInsertCoin, "Puts a coin in the first slot."},
        {recomp::MenuRow::Action, kStart2, recomp::layout::kPlayer2Joins,
         "Presses player 2's Start: with a coin in (or free play), a second player to practise against."},
        {recomp::MenuRow::Action, kTest, recomp::layout::kTestMenu,
         "The game's own operator menu (the board's test switch, F2): its settings, such as difficulty and rounds. "
         "Its Exit row goes back."},
    };
    rows.resetOptionsDescription = "Puts every option back as it first was (the launcher's Enhanced preset), and the speed to full. "
                                   "The game's own settings (its Test menu) and the controls stay as they are.";
    rows.resetDescription = "Starts the board again, as if switched off and on.";
    recomp::layout::Standard(config, rows);
    // (during an online match, the features the match decides for both players are left as it has them: a change on
    // one side alone would leave the two games apart; the host sets them before hosting)
    const std::function<bool(size_t)> decided = matchDecides;
    for (size_t i = 0; i < game.features.size(); i++) {
        const cps2recomp::Feature &feature = game.features[i];
        std::string description = feature.description;
        if (decided && decided(i)) {
            description += " During an online match it stays as the match has it (the host's, set before hosting, or off "
                           "for both), and can't be changed here.";
        }
        config.features.push_back({feature.category, feature.title, description, {feature.choices.begin(), feature.choices.end()},
                                   (feature.flags & cps2recomp::kFeatureFavourites) != 0});
    }
    // (the player's favourite stages, by name, the same for every list of them: settings.ini's [favourites] stages, the
    // names a "|" apart)
    {
        recomp::Ini settings(cps2recomp::SettingsPath());
        settings.load();
        auto favourites = std::make_shared<std::set<std::string>>();
        const std::string kept = settings.get("favourites", "stages");
        for (size_t at = 0; at < kept.size();) {
            const size_t bar = std::min(kept.find('|', at), kept.size());
            if (bar > at) {
                favourites->insert(kept.substr(at, bar - at));
            }
            at = bar + 1;
        }
        auto name = [](size_t feature, int choice) -> std::string {
            const cps2recomp::Feature &f = cps2recomp::TheGame().features[feature];
            return choice >= 0 && choice < static_cast<int>(f.choices.size()) ? f.choices[static_cast<size_t>(choice)] : "";
        };
        config.favourite = [favourites, name](size_t feature, int choice) { return favourites->count(name(feature, choice)) != 0; };
        config.setFavourite = [favourites, name](size_t feature, int choice, bool marked) {
            const std::string stage = name(feature, choice);
            if (stage.empty()) {
                return;
            }
            if (marked) {
                favourites->insert(stage);
            } else {
                favourites->erase(stage);
            }
            std::string all;
            for (const std::string &one : *favourites) {
                all += (all.empty() ? "" : "|") + one;
            }
            recomp::Ini file(cps2recomp::SettingsPath());
            file.load();
            file.set("favourites", "stages", all);
            file.save();
        };
    }
    for (const cps2recomp::Command &command : game.commands) {
        if (hosting && !strcmp(command.name, "online_host")) {
            config.commands.push_back({command.category, "Stop hosting",
                                       "Others can no longer join: the match's code stops working. Your game goes on as it is."});
            continue;
        }
        if (recordingVideo && !strcmp(command.name, "video_record")) {
            config.commands.push_back({command.category, "Stop recording video", "Ends the video being recorded and saves it."});
            continue;
        }
        if (recordingClip && !strcmp(command.name, "scene_clip")) {
            config.commands.push_back({command.category, "Stop recording", "Ends the clip being recorded here; its frames so far are kept."});
            continue;
        }
        config.commands.push_back({command.category, command.title, command.description});
    }
    config.value = [](size_t feature) { return cps2recomp::FeatureSetting(cps2recomp::TheGame().features[feature].name); };
    config.setValue = [decided](size_t feature, int value) {
        if (decided && decided(feature)) {
            return;
        }
        cps2recomp::SetFeatureValue(cps2recomp::TheGame().features[feature].name, value);
    };
    // (of a long choice, those the launcher leaves in the menu: settings.ini's [menu], recomp-common/settings.h)
    {
        recomp::Ini settings(cps2recomp::SettingsPath());
        settings.load();
        std::vector<std::vector<bool>> offered;
        for (const recomp::FeatureInfo &info : cps2recomp::FeatureList()) {
            offered.push_back(recomp::MenuChoices(settings, info));
            if (info.flags & cps2recomp::kFeatureLauncherOnly) {
                // (none but the one chosen and off, which the menu always offers: the row can only be put back to off)
                offered.back().assign(std::max<size_t>(info.choices.size(), 2), false);
            }
        }
        config.offered = [offered](size_t feature, int choice) {
            return feature >= offered.size() || choice < 0 || choice >= static_cast<int>(offered[feature].size()) || offered[feature][static_cast<size_t>(choice)];
        };
    }
    Speeds(config);
    config.speedDescription = "Left/Right: slow motion, or faster than the board. While playing, P (or a click of the right stick) stops "
                              "the game and starts it again; O (or, once stopped, the left trigger) goes on by one frame; - and = change "
                              "the speed.";
    config.muted = [this] { return muted; };
    config.setMuted = [this](bool to) { muted = to; };
    config.confirmName = recomp::sdl::kConfirmName;
    config.closeName = recomp::sdl::kCloseName;
    m_menu.Configure(std::move(config));
    m_menu.Open();
}

Menu::Menu() {
    // (so that - and = work before the menu is first opened)
    recomp::MenuConfig config;
    Speeds(config);
    m_menu.Configure(std::move(config));
}

void Menu::Speeds(recomp::MenuConfig &config) {
    recomp::layout::Speeds(config);
}

Menu::Rate Menu::GameRate() const {
    const recomp::layout::Speed speed = recomp::layout::SpeedOf(m_menu.Speed());
    // (the slower of the player's choice and what the game's sources ask for: runtime/game.h)
    const int slowBy = cps2recomp::SlowMotion();
    if (slowBy > 1 && speed.frames * slowBy > speed.ticks) {
        return {1, slowBy};
    }
    const int fastBy = cps2recomp::FastMotion();
    if (fastBy > 1 && speed.frames == speed.ticks) {
        return {fastBy, 1};
    }
    return {speed.frames, speed.ticks};
}

void Menu::SetFullSpeed() {
    m_menu.SetSpeed(recomp::layout::kFull);
}

void Menu::Faster(int by) {
    m_menu.SetSpeed(m_menu.Speed() + by);
}

Menu::Action Menu::Buttons(uint16_t pressed) {
    unsigned buttons = 0;
    buttons |= (pressed & cps2recomp::kUp) ? recomp::kMenuUp : 0;
    buttons |= (pressed & cps2recomp::kDown) ? recomp::kMenuDown : 0;
    buttons |= (pressed & cps2recomp::kLeft) ? recomp::kMenuLeft : 0;
    buttons |= (pressed & cps2recomp::kRight) ? recomp::kMenuRight : 0;
    if (pressed & (cps2recomp::kPunch1 | cps2recomp::kPunch2 | cps2recomp::kPunch3 | cps2recomp::kKick1 | cps2recomp::kKick2 |
                   cps2recomp::kKick3 | cps2recomp::kStart)) {
        buttons |= recomp::kMenuConfirm;
    }
    // (a press of something else, a coin button say, still takes the note away, as it always did)
    if (pressed && !buttons && IsOpen()) {
        Shown().Note("");
    }
    return Press(buttons);
}

Menu::Action Menu::Press(unsigned buttons) {
    return Act(Shown().Buttons(buttons));
}

bool Menu::Mouse(const SDL_Event &event, SDL_Window *window, const recomp::sdl::MenuPlace &place, Action &action) {
    recomp::Menu::Chosen chosen;
    if (!IsOpen() || !recomp::sdl::MouseEvent(event, window, place, Shown(), chosen)) {
        action = Action::None;
        return false;
    }
    action = Act(chosen);
    return true;
}

Menu::Action Menu::Act(const recomp::Menu::Chosen &chosen) {
    switch (chosen.kind) {
    case recomp::Menu::Chosen::Save: return Action::Save;
    case recomp::Menu::Chosen::Load: return Action::Load;
    case recomp::Menu::Chosen::Quit: return Action::Quit;
    case recomp::Menu::Chosen::Reset: return Action::Reset;
    case recomp::Menu::Chosen::ResetOptions: return Action::ResetOptions;
    case recomp::Menu::Chosen::Command:
        m_command = static_cast<size_t>(chosen.id);
        return m_online ? Action::OnlineCommand : Action::Command;
    case recomp::Menu::Chosen::Action:
        return chosen.id == kLeave    ? Action::Leave
               : chosen.id == kCoin   ? Action::Coin
               : chosen.id == kStart2 ? Action::Start2
               : chosen.id == kTest   ? Action::Test
                                      : Action::None;
    default: return Action::None;
    }
}

} // namespace port
