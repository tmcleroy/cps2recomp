// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
//
// The in-game menu: save states (save, load, slot), a coin, the speed the game runs at, the game's optional
// features and commands (a page for each of their categories), and leaving the game, while playing.
// The frontend opens it, stops the game while it is open and gives it the player's presses; it draws itself
// over a copy of the game's picture.
//
// The menu itself is the one the recomp ports share (recomp-common/menu.h, a copy of recomp-common's: see
// tools/sync-common.sh). This is what is CPS-2's and this toolkit's about it: the rows of the first page,
// the buttons, the speeds, and the games' features and commands handed over.
#pragma once

#include "../recomp-common/menu.h"
#include "../recomp-common/menu_sdl.h"

#include <cstdint>
#include <functional>
#include <string>

namespace port {

class Menu {
public:
    enum class Action { None, Save, Load, Coin, Start2, Test, Reset, ResetOptions, Command, Quit, OnlineCommand, Leave };

    Menu();
    // withStates: show the save state rows
    void Open(bool withStates);
    // During an online match (Port::OpenOnlineMenu): the match's own menu, drawn small over the game, which goes on for
    // both players. It opens on its Online match page (the host's Back to Versus and seats, as commands; a page for each
    // mode, with its Start and its options, as on the Alternative modes pages; Network stats; this player's own
    // readouts), and its first page has that page, Mute, Resume and leaving the match. A command chosen is
    // Action::OnlineCommand (Command(): its place in `commands`), leaving Action::Leave.
    struct Online {
        std::vector<recomp::MenuFeature> features;
        std::vector<recomp::MenuCommand> commands;
        std::vector<recomp::MenuPage> pages; // (the Online match page's own pages: parent kOnlinePage)
        std::function<int(size_t feature)> value;
        std::function<void(size_t feature, int value)> setValue;
        std::string leave, leaveDescription;
    };
    static constexpr const char *kOnlinePage = "Online match";
    void OpenOnline(Online online);
    bool OnlineOpen() const {
        return m_online && m_onlineMenu.IsOpen();
    }
    void Close() {
        m_menu.Close();
        m_onlineMenu.Close();
    }
    bool IsOpen() const {
        return m_menu.IsOpen() || m_onlineMenu.IsOpen();
    }
    // Buttons newly pressed (cps2recomp::Button bits): Up/Down choose, any punch, kick or Start changes or
    // goes into a page, Left/Right change a row with several values.
    // Returns what the frontend is to do (with Slot()); it closes the menu if that went well.
    // (The tests' way in. A player's presses come through Press.)
    Action Buttons(uint16_t pressed);
    // The menu's own buttons newly pressed (recomp::kMenu... bits), which the frontend makes from keys and
    // gamepad buttons that are the same whatever the game's are set to: the arrows, the d-pad and the left
    // stick; Space, Enter and A choose; Backspace and B go back.
    Action Press(unsigned buttons);
    // The mouse, while the menu is open (recomp-common/menu_sdl.h): over a row chooses it, a click does what Confirm
    // does, the right button goes back, the wheel goes up and down. `place`: where the menu's picture is in the
    // window's pixels, and the size Draw was given. True when the event was the mouse's (the picture is to be drawn
    // again), with what the frontend is to do in `action`, as from Press. A touch is left to the touch controls.
    bool Mouse(const SDL_Event &event, SDL_Window *window, const recomp::sdl::MenuPlace &place, Action &action);
    // The "Sound" row: the frontend plays nothing while this is set (--mute starts with it set)
    bool muted = false;
    bool hosting = false; // (a match hosted from the Online page, waited for: its row is Stop hosting)
    bool recordingVideo = false; // (a video being recorded: the Video page's Record video row is Stop recording)
    bool recordingClip = false; // (a scene clip being recorded: the Scene export page's Record clip row is Stop recording)
    // During an online match: whether the match decides this feature (TheGame().features' place) for both players. The
    // menu then shows it as the match has it and doesn't change it (a change on one side alone would leave the games apart).
    std::function<bool(size_t feature)> matchDecides;
    int Slot() const {
        return m_menu.Slot();
    }
    void SetSlot(int slot) {
        m_menu.SetSlot(slot);
    }
    // After Action::Command: which of the game's commands (TheGame().commands)
    size_t Command() const {
        return m_command;
    }
    // The speed the game runs at (the player's choice): `frames` frames in `ticks` of the screen's
    struct Rate {
        int frames, ticks;
    };
    Rate GameRate() const;
    bool FullSpeed() const {
        const Rate rate = GameRate();
        return rate.frames == rate.ticks;
    }
    // The next speed up (by 1) or down (by -1)
    void Faster(int by);
    void SetFullSpeed();
    // A line to tell the player (why not, say), until the next press
    void Note(const std::string &text) {
        Shown().Note(text);
    }
    // Over a picture of 32-bit pixels (red in the low byte)
    void Draw(uint32_t *pixels, int width, int height) const {
        Shown().Draw(pixels, width, height);
    }
    // The rows in small letters, close together, for a screen of few lines (the Arcade monitor page's Compact menu)
    void SetCompact(bool compact) {
        m_menu.SetCompact(compact);
    }
    bool Compact() const {
        return m_menu.Compact();
    }

private:
    static void Speeds(recomp::MenuConfig &config);
    Action Act(const recomp::Menu::Chosen &chosen);
    // (the menu open, or last open: the online match's while it is)
    recomp::Menu &Shown() {
        return m_online ? m_onlineMenu : m_menu;
    }
    const recomp::Menu &Shown() const {
        return m_online ? m_onlineMenu : m_menu;
    }

    recomp::Menu m_menu;
    recomp::Menu m_onlineMenu; // (made afresh at each OpenOnline: on its page, its first row)
    bool m_online = false;
    size_t m_command = 0;
};

} // namespace port
