// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
//
// The window: SDL3 picture, sound and controls around Port::RunFrame().
#include "game_info.h"

#include "../recomp-common/arcade_screen.h"
#include "bindings.h"
#include "gamepad_slots.h"
#include "keyboard_input.h"
#include "online_pad_queue.h"
#include "socd.h"
#include "i18n.h"
#include "port.h"
#include "png_write.h"
#include "present.h"
#include "sound_filter.h"
#include "touch.h"

#include "../runtime/game.h"
#include "../runtime/spare.h"
#include "../recomp-common/gamepads.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>

using namespace cps2recomp;

namespace port {

#ifdef __APPLE__
void HandFrontToLauncher(); // (front_macos.mm)
#endif

namespace {

    // What each player's keys and gamepad buttons do: the player's choices (bindings.h; the launcher's
    // Controllers page), or the usual layout. A game started from the collection's launcher has the table every
    // game there shares; one with a launcher of its own, its own table.
    const BoundTable &Table() {
        static const BoundTable &table = MakeBoundTable(GamePlayers(), GameButtons(), InCollection());
        return table;
    }
    Bindings g_bindings = recomp::DefaultBindings(Table().table);
    KeyboardInput keyboard(g_bindings, Table());
    int Players() {
        return Table().table.players;
    }

    static_assert(kBoundActions[0].bit == kUp && kBoundActions[1].bit == kDown && kBoundActions[2].bit == kLeft &&
                  kBoundActions[3].bit == kRight && kBoundActions[4].bit == kPunch1 && kBoundActions[9].bit == kKick3 &&
                  kBoundActions[10].bit == kStart && kBoundActions[11].bit == kCoin &&
                  kBoundActions[12].bit == (kPunch1 | kPunch2 | kPunch3) && kBoundActions[13].bit == (kKick1 | kKick2 | kKick3));

    SocdCleaner socdCleaners[Machine::kMaxPlayers]; // (each player's own: port/socd.h)

    constexpr int kDead = 16000; // how far a stick or a trigger must go to count

    // The Arcade monitor page (recomp-common's arcade_screen.h, the same in every arcade port): full screen at the board's
    // 384 x 224 or a super resolution, at the refresh nearest the board's, on the chosen screen; or in a mode Switchres
    // makes at the board's exact rate; and the frames synced to the screen, with the frame delay
    recomp::sdl::ArcadeChoices ArcadeChosen() {
        recomp::sdl::ArcadeChoices choices;
        choices.mode = static_cast<recomp::sdl::ArcadeMode>(ArcadeResolution());
        choices.screen = DisplayScreen();
        choices.switchres = SwitchresChosen();
        choices.monitor = ArcadeMonitor();
        choices.boardPace = !ArcadeSyncChosen();
        return choices;
    }
    recomp::sdl::ArcadeScreen &Arcade() {
        static recomp::sdl::ArcadeScreen screen({Video::kWidth, Video::kHeight, Machine::kFramesPerSecond});
        return screen;
    }

    bool KeyBound(SDL_Scancode key) {
        return keyboard.Bound(key);
    }
    bool ButtonBound(SDL_GamepadButton button) {
        return PadInputBound(g_bindings, Table(), {button, -1});
    }

    // The camera's zoom (zoom.h) is the left stick's click held and a shoulder pressed: left for player 1, right for
    // player 2, the right stick's click for both fighters at once; the d-pad (or the stick) moves the camera. While the
    // click is held (and is not the game's), the shoulders and the directions are not the game's. On the keyboard,
    // Left Shift held, Q, E and W, and the arrows.
    bool ZoomHeld(SDL_Gamepad *gp) {
        return Zoom::Possible() && gp && !ButtonBound(SDL_GAMEPAD_BUTTON_LEFT_STICK) && SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_LEFT_STICK);
    }
    bool Shoulder(int button) {
        return button == SDL_GAMEPAD_BUTTON_LEFT_SHOULDER || button == SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER;
    }
    bool DPad(int button) {
        return button == SDL_GAMEPAD_BUTTON_DPAD_UP || button == SDL_GAMEPAD_BUTTON_DPAD_DOWN || button == SDL_GAMEPAD_BUTTON_DPAD_LEFT ||
               button == SDL_GAMEPAD_BUTTON_DPAD_RIGHT;
    }
    bool KeyZoomHeld(const bool *keys) {
        return Zoom::Possible() && keys[SDL_SCANCODE_LSHIFT] && !KeyBound(SDL_SCANCODE_LSHIFT);
    }

    uint16_t ReadGamepad(SDL_Gamepad *gp, int player) {
        uint16_t pad = 0;
        const bool zoomHeld = ZoomHeld(gp);
        for (int action = 0; action < Table().table.count; action++) {
            const PadInput input = g_bindings.pad[player][action];
            if (zoomHeld && (Shoulder(input.button) || DPad(input.button))) {
                continue;
            }
            if ((input.button >= 0 && SDL_GetGamepadButton(gp, static_cast<SDL_GamepadButton>(input.button))) ||
                (input.axis >= 0 && SDL_GetGamepadAxis(gp, static_cast<SDL_GamepadAxis>(input.axis)) > kDead)) {
                pad |= Table().bits[action];
            }
        }
        const int x = zoomHeld ? 0 : SDL_GetGamepadAxis(gp, SDL_GAMEPAD_AXIS_LEFTX);
        const int y = zoomHeld ? 0 : SDL_GetGamepadAxis(gp, SDL_GAMEPAD_AXIS_LEFTY);
        if (x < -kDead) pad |= kLeft;
        if (x > kDead) pad |= kRight;
        if (y < -kDead) pad |= kUp;
        if (y > kDead) pad |= kDown;
        return pad;
    }

    // A Multi game run's window: where the game before left it (each game is a program of its own, and its window
    // would open where the system puts a new one). The game ending for the next writes it down; the next game reads
    // it, if it was written moments ago (not a run before), and opens its window there, as tall.
    struct Placement {
        int x = 0, y = 0, height = 0;
        bool maximized = false;
    };
    std::filesystem::path PlacementFile() {
        std::error_code error;
        const std::filesystem::path folder = std::filesystem::temp_directory_path(error);
        return error ? std::filesystem::path() : folder / "cps2recomp-multi-window.txt";
    }
    void KeepPlacement(SDL_Window *window) {
        const std::filesystem::path file = PlacementFile();
        if (file.empty() || (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN)) {
            return;
        }
        Placement at;
        int width = 0;
        SDL_GetWindowPosition(window, &at.x, &at.y);
        SDL_GetWindowSize(window, &width, &at.height);
        at.maximized = SDL_GetWindowFlags(window) & SDL_WINDOW_MAXIMIZED;
        std::ofstream(file) << at.x << ' ' << at.y << ' ' << at.height << ' ' << at.maximized << '\n';
    }
    bool TakePlacement(Placement &at) {
        const std::filesystem::path file = PlacementFile();
        std::error_code error;
        if (file.empty() || !std::filesystem::exists(file, error)) {
            return false;
        }
        const auto age = std::filesystem::file_time_type::clock::now() - std::filesystem::last_write_time(file, error);
        bool fresh = !error && age < std::chrono::minutes(1);
        {
            std::ifstream in(file);
            fresh = fresh && (in >> at.x >> at.y >> at.height >> at.maximized) && at.height > 0;
        }
        std::filesystem::remove(file, error);
        return fresh;
    }

} // namespace

int RunWindowed(Port &port) {
    SDL_SetHint(SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES, "256");
    if (!port.opt.settingsPath.empty()) {
        g_bindings = LoadBindings(port.opt.settingsPath, Table());
    }
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD)) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    // (Show pictures folder, the model viewer's: the folder as a file:// address, its bytes outside the plain ones as %XX)
    SetFolderOpener([](const std::string &folder) {
        std::string url = "file://";
#ifdef _WIN32
        url += '/';
#endif
        for (const unsigned char c : folder) {
            const bool plain = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '/' || c == '-' ||
                               c == '_' || c == '.' || c == '~' || c == ':';
            if (plain) {
                url += static_cast<char>(c);
            } else if (c == '\\') {
                url += '/';
            } else {
                static const char kHex[] = "0123456789ABCDEF";
                url += '%';
                url += kHex[c >> 4];
                url += kHex[c & 15];
            }
        }
        SDL_OpenURL(url.c_str());
    });
    // (controllers SDL doesn't know, issue #213: gamecontrollerdb.txt beside the program, in the launcher's folder or in
    // the game's own; recomp-common/gamepads.h)
    {
        const char *base = SDL_GetBasePath();
        const std::string mine = port.PlayerFolder();
        recomp::AddGamepadMappings({base ? std::filesystem::u8path(base) : std::filesystem::path(), LauncherFolder(),
                                    mine.empty() ? std::filesystem::path() : std::filesystem::u8path(mine)});
    }
    // (the window: so many times the board's 224 lines, --scale's or the Display page's)
    const int scale = port.opt.scale > 0 ? port.opt.scale : WindowSize();
    // (the window's name: game.cfg's "title" where a game has one, else its name)
#ifdef GAME_TITLE
    const char *title = GAME_TITLE;
#else
    const char *title = GAME_NAME;
#endif
    // (the picture's shape: the board's 384 x 224 shown 4:3 unless the Display page says otherwise, and as
    // much wider as the widest picture the game's sources may ask for; 4:3 for a picture that fills it)
    const int columns = TheGame().widestPicture ? std::clamp(TheGame().widestPicture(), +Video::kWidth, +Video::kMaxWidth) : Video::kWidth;
    auto windowShape = [](int across) {
        // (a vertical game's picture turned upright: Turned)
        return Turned() ? PictureShape(Video::kHeight, across, DisplayShape(), 3.0f / 4.0f * Video::kWidth / across)
                        : PictureShape(across, Video::kHeight, DisplayShape(), 4.0f / 3.0f * across / Video::kWidth);
    };
    // (the window's height for so many times the board's lines: a third more for a picture stood upright, its rows the
    // board's columns)
    auto windowTall = [](int times) { return Turned() ? Video::kHeight * times * 4 / 3 : Video::kHeight * times; };
    // (in a Multi game run, a window where the game before's was: a full screen one is full screen as asked)
    Placement placed;
    const bool carried = !MultiGame().empty() && TakePlacement(placed) && !port.opt.fullscreen;
    const int windowHeight = carried ? placed.height : windowTall(scale);
    // The window, as an OpenGL window for a shader (shading.h: nothing else draws to it), `at` a place for it (`width`
    // 0: as wide as the picture's shape makes it)
    auto makeWindow = [&](bool opengl, const Placement *at, int width, int height, bool fullscreen) {
        const SDL_PropertiesID properties = SDL_CreateProperties();
        SDL_SetStringProperty(properties, SDL_PROP_WINDOW_CREATE_TITLE_STRING, title);
        SDL_SetNumberProperty(properties, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, width > 0 ? width : std::lround(height * windowShape(columns)));
        SDL_SetNumberProperty(properties, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, height);
        if (at) {
            SDL_SetNumberProperty(properties, SDL_PROP_WINDOW_CREATE_X_NUMBER, at->x);
            SDL_SetNumberProperty(properties, SDL_PROP_WINDOW_CREATE_Y_NUMBER, at->y);
        } else {
#ifndef __ANDROID__
            // (on the Arcade monitor page's screen)
            const SDL_DisplayID display = recomp::sdl::ChosenDisplay(DisplayScreen());
            SDL_SetNumberProperty(properties, SDL_PROP_WINDOW_CREATE_X_NUMBER, SDL_WINDOWPOS_CENTERED_DISPLAY(display));
            SDL_SetNumberProperty(properties, SDL_PROP_WINDOW_CREATE_Y_NUMBER, SDL_WINDOWPOS_CENTERED_DISPLAY(display));
#endif
        }
#ifdef __ANDROID__
        fullscreen = true; // (a phone's whole screen, its bars hidden)
        const bool fullscreenNow = true;
#else
        // (an arcade resolution: made a window, given its mode, then made full screen)
        const bool fullscreenNow = fullscreen && ArcadeResolution() == ArcadeMode::Off;
#endif
        SDL_SetNumberProperty(properties, SDL_PROP_WINDOW_CREATE_FLAGS_NUMBER,
                              SDL_WINDOW_RESIZABLE | (fullscreenNow ? SDL_WINDOW_FULLSCREEN : 0) | (at && at->maximized ? SDL_WINDOW_MAXIMIZED : 0) |
                                  (opengl ? SDL_WINDOW_OPENGL : 0) | (getenv("CPS2RECOMP_HIDDEN_WINDOW") ? SDL_WINDOW_HIDDEN : 0)); // (hidden: for tests)
        SDL_Window *made = SDL_CreateWindowWithProperties(properties);
        SDL_DestroyProperties(properties);
#ifndef __ANDROID__
        if (made) {
            Arcade().SetFullscreen(made, ArcadeChosen(), fullscreen);
            if (fullscreen && !fullscreenNow) {
                SDL_SetWindowFullscreen(made, true);
            }
        }
#endif
        // (in front: on macOS a program started by one that is not, as the launcher is behind a Multi game run's
        // last game, opens behind the other windows)
        if (made && !getenv("CPS2RECOMP_HIDDEN_WINDOW")) {
            SDL_RaiseWindow(made);
        }
        return made;
    };
    // The picture goes to the window through SDL's GPU API, or through SDL's renderer where there is no
    // device for that (and the dummy video driver of the tests has none) or when asked: --renderer sdl. With a
    // shader preset chosen (the Display page's Shader, or --shader) it goes through the preset with OpenGL instead,
    // where librashader is beside the game and makes the preset; else as without one.
    Presenter presenter;
    std::string presentError;
    const bool wantGpu = port.opt.renderer != "sdl" && strcmp(SDL_GetCurrentVideoDriver(), "dummy") != 0;
    const bool canShade = strcmp(SDL_GetCurrentVideoDriver(), "dummy") != 0;
    std::string preset = canShade ? port.ShaderPreset() : "";
    int shaderValue = FeatureValue("display_shader");
    std::string shaderError;
    if (!preset.empty() && !Shading::Available(shaderError)) {
        fprintf(stderr, "shader: %s; the picture is drawn without it\n", shaderError.c_str());
        preset.clear();
    }
    SDL_Window *window = makeWindow(!preset.empty(), carried ? &placed : nullptr, 0, windowHeight, port.opt.fullscreen);
    // (opened as the preset says, or as without one when that fails)
    auto openPresenter = [&]() {
        if (!preset.empty()) {
            if (presenter.OpenShaded(window, preset, shaderError)) {
                return true;
            }
            fprintf(stderr, "shader %s: %s; the picture is drawn without it\n", preset.c_str(), shaderError.c_str());
            preset.clear();
            // (an OpenGL window is drawn to by nothing else: one that is not, where this one is)
            Placement at;
            int width = 0;
            SDL_GetWindowPosition(window, &at.x, &at.y);
            SDL_GetWindowSize(window, &width, &at.height);
            at.maximized = SDL_GetWindowFlags(window) & SDL_WINDOW_MAXIMIZED;
            const bool fullscreen = SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN;
            SDL_DestroyWindow(window);
            window = makeWindow(false, &at, width, at.height, fullscreen);
            if (!window) {
                presentError = SDL_GetError();
                return false;
            }
        }
        return presenter.Open(window, wantGpu, presentError);
    };
    presenter.SetVsync(FeatureOn("display_vsync")); // (kept when it is opened again)
    if (!window || !openPresenter()) {
        fprintf(stderr, "SDL window: %s\n", window ? presentError.c_str() : SDL_GetError());
        return 1;
    }
    if (wantGpu && !presenter.UsesGpu() && !presenter.Shaded()) {
        fprintf(stderr, "no GPU device (%s): the picture goes through SDL's renderer\n", presentError.c_str());
    }
    auto sayPresenter = [&]() {
        if (presenter.Shaded() && getenv("CPS2RECOMP_SHADER_SHOT")) {
            presenter.Shader()->keepLast = true;
        }
        if (port.opt.stats || ArcadeResolution() != ArcadeMode::Off) {
            fprintf(stderr, "picture: %s\n", presenter.Shaded()  ? ("OpenGL, shader " + preset).c_str()
                                             : presenter.UsesGpu() ? SDL_GetGPUDeviceDriver(presenter.Device())
                                                                   : SDL_GetRendererName(presenter.Renderer()));
        }
    };
    sayPresenter();
    // The Display page's Shader changed in the menu: another preset in the same window, or the window made again
    // for or without OpenGL, where it is and as large
    auto followShader = [&]() {
        const int value = FeatureValue("display_shader");
        if (value == shaderValue || !port.opt.shader.empty() || !canShade) {
            return;
        }
        shaderValue = value;
        std::string next = port.ShaderPreset();
        if (!next.empty() && !Shading::Available(shaderError)) {
            fprintf(stderr, "shader: %s; the picture is drawn without it\n", shaderError.c_str());
            next.clear();
        }
        if (next == preset) {
            return;
        }
        if (!next.empty() && presenter.Shaded()) {
            if (presenter.SetPreset(next, shaderError)) {
                preset = next;
                sayPresenter();
            } else {
                fprintf(stderr, "shader %s: %s; the one before is kept\n", next.c_str(), shaderError.c_str());
            }
            return;
        }
        Placement at;
        int width = 0;
        SDL_GetWindowPosition(window, &at.x, &at.y);
        SDL_GetWindowSize(window, &width, &at.height);
        at.maximized = SDL_GetWindowFlags(window) & SDL_WINDOW_MAXIMIZED;
        const bool fullscreen = SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN;
        port.gpuPicture.Close(); // (its resources are the presenter's device's)
        for (auto &g : port.guestPictures) {
            g.picture->Close();
        }
        presenter.Close();
        SDL_DestroyWindow(window);
        preset = next;
        window = makeWindow(!preset.empty(), &at, width, at.height, fullscreen);
        if (!window || !openPresenter()) {
            fprintf(stderr, "SDL window: %s\n", window ? presentError.c_str() : SDL_GetError());
            port.quit = true;
            return;
        }
        sayPresenter();
    };

    // Sound: how much is kept waiting to be played (seconds), and how much is too much. The device is asked
    // for small buffers, so that little waits there either. (CPS2RECOMP_AUDIO_LOG: what is waiting, printed
    // once a second, and nothing heard.)
    constexpr double kAudioWaiting = 0.028, kAudioMost = 0.11;
    const bool audioLog = getenv("CPS2RECOMP_AUDIO_LOG") != nullptr;
    double audioLeast = 1;
    int audioDropped = 0;
    // (--mute starts with no sound; the menu's Sound row switches it on, so the device is opened all the same)
    SDL_AudioStream *audio = nullptr;
    SoundFilter soundFilter; // (the menu's Sound filter: sound_filter.h)
    port.menu.muted = port.opt.mute;
    bool silent = false;
    {
        const SDL_AudioSpec spec{SDL_AUDIO_S16, 2, QSound::kSampleRate};
        audio = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
        if (audio) {
            SDL_ResumeAudioStreamDevice(audio);
            if (audioLog) {
                SDL_AudioSpec device{};
                int frames = 0;
                SDL_GetAudioDeviceFormat(SDL_GetAudioStreamDevice(audio), &device, &frames);
                fprintf(stderr, "audio: device %d Hz, %d channels, %d frames a buffer (%s)\n", device.freq, device.channels, frames, SDL_GetCurrentAudioDriver());
            }
        } else {
            fprintf(stderr, "Audio: %s\n", SDL_GetError());
        }
    }
    // The custom soundtrack (soundtrack.h): a stream of its own, at its files' rate, opened when it first plays
    SDL_AudioStream *music = nullptr;
    SDL_AudioSpec musicSpec{SDL_AUDIO_F32, 2, 0};
    std::vector<float> musicMade;
    bool musicSilent = false;

    GamepadSlots controllers;
    OnlinePadQueue onlinePads;
    const auto &gamepads = controllers.Pads();
    // Each player's gamepad: those the launcher's Controllers page gave a player, then the rest in their seats' order
    // (recomp-common bindings.h: [controllers]); given out again when one comes or goes, not every frame
    const recomp::PadChoices padChoices = port.opt.settingsPath.empty() ? recomp::PadChoices{} : recomp::LoadPadChoices(port.opt.settingsPath);
    SDL_Gamepad *playerPads[recomp::kBindMaxPlayers] = {};
    int padsGivenFor = -1; // (the players they were given out for; -1: to do again)
    auto playerOf = [&](SDL_JoystickID id) {
        for (int player = 0; player < recomp::kBindMaxPlayers; player++) {
            if (playerPads[player] && SDL_GetGamepadID(playerPads[player]) == id) {
                return player;
            }
        }
        return -1;
    };
    // The in-game menu: opened and closed by Esc, F1, a gamepad stick's click or its Start and Select
    // together (the game is left from its "Quit game"). While it is open the game stands still and the
    // controls work the menu; buttons held as it closes do not reach the game until let go.
    std::vector<uint32_t> menuBase, menuPicture, shown;
    uint32_t menuWidth = 0, menuHeight = 0;
    // (the menu's picture's size where it is drawn on the screen's own pixels: Port::MenuPicture; 0: as usual)
    int menuWantWidth = 0, menuWantHeight = 0;
    bool menuDirty = false;
    uint16_t previousPad = 0, heldAtClose = 0;
    recomp::MenuRepeat menuRepeat; // (the arrows held go on through the rows and the choices)
    int triggerHeld = 0;
    // (the practice point's left shoulder: when it went down, 0 if not waiting; tapped, back to the point; held
    // kPointHold, the point set. And the left trigger once it has made a chord with the shoulder: not rewind until let go)
    constexpr uint64_t kPointHold = 500; // milliseconds
    uint64_t shoulderDownAt = 0;
    bool triggerChorded = false;
    recomp::sdl::MenuChord menuChord; // a gamepad's Start and Select (Back) together: the menu
    // (where the menu's picture was last shown, in the window's pixels, for the mouse: recomp-common/menu_sdl.h)
    recomp::sdl::MenuPlace menuPlace;
    // (with a zoom to work, the left stick's click opens the menu when it is let go, unless a shoulder was pressed
    // while it was held: then it was the zoom's)
    bool stickWaiting = false;
    auto syncGamepads = [&] {
        if (controllers.SetEnabled(!FeatureOn("keyboard_only"))) {
            stickWaiting = false;
            onlinePads.Clear();
            padsGivenFor = -1;
        }
    };
    auto toggleMenu = [&]() {
        if (port.net) {
            // (online: the game goes on for both players; Esc gives up a wait, and in a match opens the match's own menu
            // over it, or asks whether to leave when there is nothing to choose: Port::OpenOnlineMenu)
            if (port.connecting && !port.net->Arcade()) {
                port.LeaveOnline();
            } else if (port.menu.IsOpen() || port.leaving) {
                port.menu.Close();
                port.leaving = false;
            } else {
                port.OpenOnlineMenu();
            }
            return;
        }
        if (port.menu.IsOpen()) {
            port.menu.Close();
        } else {
            port.MenuPicture(menuBase, menuWidth, menuHeight, menuWantWidth, menuWantHeight);
            port.menu.Open(true);
            menuDirty = true;
        }
    };
    // Online, the chat (Port::typing): T on the keyboard, the right stick's click or the touch screen's Chat button
    // opens a line to type, and the system's text input with it (on a Steam Deck or a phone, its own on-screen
    // keyboard); Enter sends it, Esc (or the click again) puts it away. Meanwhile the keyboard is the chat's, and after
    // it, not the game's until every key is let go (the Enter that sent it is not the game's Start).
    uint64_t openedByT = 0; // (when T opened it: its own letter, if the system sends it as text too, is not the line's)
    bool keysAfterChat = false;
    auto chatOpen = [&](bool open) {
        if (open == port.typing || (open && (!port.net || port.connecting || port.leaving || port.menu.IsOpen()))) {
            return;
        }
        port.typing = open;
        port.typed.clear();
        if (open) {
            int w = 0, h = 0;
            SDL_GetWindowSize(window, &w, &h);
            const SDL_Rect area{0, h * 3 / 4, w / 2, h / 12};
            SDL_SetTextInputArea(window, &area, 0);
            SDL_StartTextInput(window);
        } else {
            SDL_StopTextInput(window);
            keysAfterChat = true;
        }
    };
    auto quickState = [&](bool save) {
        if (port.net) {
            return; // (online: no save states)
        }
        std::string error;
        const std::string path = port.StatePath(port.menu.Slot());
        if (!(save ? port.SaveState(path, error) : port.LoadState(path, error))) {
            fprintf(stderr, "state %d: %s\n", port.menu.Slot(), error.c_str());
        }
    };
    // A Multi game run (the launcher's Multi game page): N or a gamepad's left shoulder for the next game, F or
    // the left trigger held to fast-forward (each when the player has not given it to the game)
    const bool multi = !MultiGame().empty();
    // (a touch screen's controls: touch.h)
    TouchControls touch;
    std::vector<uint32_t> touchPicture;
    // (the menu's Zoom row, as last seen: a change there is the camera's, and a zoom's press is the row's)
    int zoomRow = port.zoom.Choice();
    const double kFrameNs = 1e9 / Machine::kFramesPerSecond;
    uint64_t nextFrame = SDL_GetTicksNS();
    bool running = true;
    while (running && !port.quit && (port.opt.frames == 0 || port.frame < port.opt.frames)) {
        SDL_Event ev;
        const bool menuWasOpen = port.menu.IsOpen() || port.leaving;
        syncGamepads();
        if (port.typing && (!port.net || port.connecting)) {
            chatOpen(false); // (the match over, or waited for again)
        }
        while (SDL_PollEvent(&ev)) {
            if ((ev.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN || ev.type == SDL_EVENT_GAMEPAD_BUTTON_UP) &&
                !controllers.Has(ev.gbutton.which)) {
                continue;
            }
            if (touch.Event(ev)) {
                continue;
            }
            if (port.typing && ev.type == SDL_EVENT_TEXT_INPUT) {
                if (openedByT && SDL_GetTicks() - openedByT < 250 && (!strcmp(ev.text.text, "t") || !strcmp(ev.text.text, "T"))) {
                    openedByT = 0;
                    continue;
                }
                openedByT = 0;
                for (const char *c = ev.text.text; *c; c++) {
                    if (*c >= 32 && *c < 127 && port.typed.size() < Netplay::kChatMost) {
                        port.typed += *c;
                    }
                }
                continue;
            }
            if (port.typing && ev.type == SDL_EVENT_KEY_DOWN) {
                if (ev.key.scancode == SDL_SCANCODE_RETURN || ev.key.scancode == SDL_SCANCODE_KP_ENTER) {
                    port.Say();
                    chatOpen(false);
                } else if (ev.key.scancode == SDL_SCANCODE_ESCAPE || ev.key.scancode == SDL_SCANCODE_AC_BACK) {
                    chatOpen(false);
                } else if (ev.key.scancode == SDL_SCANCODE_BACKSPACE && !port.typed.empty()) {
                    port.typed.pop_back();
                }
                continue; // (every other key the line's, as text)
            }
            if ((ev.type == SDL_EVENT_KEY_DOWN && !ev.key.repeat) || ev.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
                port.LagPressed(ev.common.timestamp); // (the input lag readout: Port::Lag)
            }
            // The mouse in the menu: over a row chooses it, a click confirms, the right button goes back, the wheel
            // goes up and down (a touch is the touch controls': touch.h)
            if (Menu::Action action; port.menu.Mouse(ev, window, menuPlace, action)) {
                port.MenuAct(action);
                syncGamepads(); // a click can change the device choice before the next queued button event
                menuDirty = true;
                if (!port.menu.IsOpen()) {
                    heldAtClose = previousPad;
                }
                continue;
            }
            // (online, Esc's question: the right button leaves it, as Back does)
            if (port.leaving && ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN && ev.button.which != SDL_TOUCH_MOUSEID &&
                ev.button.button == SDL_BUTTON_RIGHT) {
                port.LeaveQuestion(recomp::kMenuBack);
                continue;
            }
            switch (ev.type) {
            case SDL_EVENT_QUIT:
                port.KeepPicture(); // (the launcher's tile of the game: its last picture)
                running = false;
                break;
            case SDL_EVENT_KEY_DOWN:
                if (!port.menu.IsOpen() && !ev.key.repeat &&
                    (ev.key.scancode == SDL_SCANCODE_Q || ev.key.scancode == SDL_SCANCODE_E || ev.key.scancode == SDL_SCANCODE_W) &&
                    KeyZoomHeld(SDL_GetKeyboardState(nullptr))) {
                    if (ev.key.scancode == SDL_SCANCODE_W) {
                        port.zoom.Both();
                    } else {
                        port.zoom.Press(ev.key.scancode == SDL_SCANCODE_E ? 1 : 0);
                    }
                } else if (recomp::sdl::MenuKey(ev.key.scancode, !KeyBound(SDL_SCANCODE_TAB))) {
                    // (AC_BACK: a phone's Back)
                    toggleMenu();
                } else if (KeyBound(ev.key.scancode)) {
                    // (a key the player has given to the game does only that)
                } else if (port.net && ev.key.scancode == SDL_SCANCODE_T && !ev.key.repeat) {
                    chatOpen(true);
                    openedByT = port.typing ? SDL_GetTicks() : 0;
                } else if (multi && !port.menu.IsOpen() && ev.key.scancode == SDL_SCANCODE_N && !ev.key.repeat) {
                    port.Key('n');
                } else if (ev.key.scancode == SDL_SCANCODE_F5 || ev.key.scancode == SDL_SCANCODE_F9) {
                    // F5 keeps the game in the slot, F9 goes back to it; F6 / F7: the slot before, the next
                    if (!port.menu.IsOpen()) {
                        quickState(ev.key.scancode == SDL_SCANCODE_F5);
                    }
                } else if (ev.key.scancode == SDL_SCANCODE_F6 || ev.key.scancode == SDL_SCANCODE_F7) {
                    port.menu.SetSlot(port.menu.Slot() + (ev.key.scancode == SDL_SCANCODE_F7 ? 1 : -1));
                } else if (!port.menu.IsOpen() && ev.key.scancode == SDL_SCANCODE_P && !ev.key.repeat) {
                    port.Key('p'); // stops the game, and starts it again
                } else if (!port.menu.IsOpen() && ev.key.scancode == SDL_SCANCODE_O) {
                    port.Key('o'); // on by one frame (held: frame after frame)
                } else if (ev.key.scancode == SDL_SCANCODE_MINUS || ev.key.scancode == SDL_SCANCODE_EQUALS) {
                    port.Key(ev.key.scancode == SDL_SCANCODE_MINUS ? '-' : '='); // slower, faster
                } else if (!port.menu.IsOpen() && ev.key.scancode == SDL_SCANCODE_BACKSPACE && !ev.key.repeat) {
                    port.Key('k', kKeyRestart);
                } else if (!port.menu.IsOpen() && ev.key.scancode == SDL_SCANCODE_BACKSLASH && !ev.key.repeat) {
                    port.Key('k', kKeyExtra);
                } else if (!port.menu.IsOpen() && ev.key.scancode == SDL_SCANCODE_RIGHTBRACKET && !ev.key.repeat) {
                    port.Key(']'); // (the practice point set: port.h, Practice)
                } else if (ev.key.scancode == SDL_SCANCODE_F11 || ev.key.scancode == SDL_SCANCODE_F12 || ev.key.scancode == SDL_SCANCODE_RETURN) {
                    using Shortcut = KeyboardInput::Shortcut;
                    switch (keyboard.KeyDown(ev.key.scancode, ev.key.mod, ev.key.repeat)) {
                    case Shortcut::Fullscreen: SDL_SetWindowFullscreen(window, !(SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN)); break;
                    case Shortcut::Screenshot: port.Screenshot(); break;
                    case Shortcut::Scene: port.SaveScene(); break;
                    case Shortcut::Clip: port.SceneClip(); break;
                    case Shortcut::Record: port.RecordVideo(); break;
                    case Shortcut::Replay: port.SaveReplay(); break;
                    case Shortcut::None: break;
                    }
                } else if (ev.key.scancode == SDL_SCANCODE_F2) {
                    port.test = !port.test; // the operator's test switch
                }
                break;
            case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
                if (!port.menu.IsOpen() && Shoulder(ev.gbutton.button) && ZoomHeld(SDL_GetGamepadFromID(ev.gbutton.which))) {
                    port.zoom.Press(ev.gbutton.button == SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER ? 1 : 0);
                    stickWaiting = false;
                } else if (!port.menu.IsOpen() && ev.gbutton.button == SDL_GAMEPAD_BUTTON_RIGHT_STICK &&
                           !ButtonBound(SDL_GAMEPAD_BUTTON_RIGHT_STICK) && ZoomHeld(SDL_GetGamepadFromID(ev.gbutton.which))) {
                    port.zoom.Both(); // (not the right stick's own pause)
                    stickWaiting = false;
                } else if (ButtonBound(static_cast<SDL_GamepadButton>(ev.gbutton.button))) {
                    // (a button the player has given to the game does only that)
                } else if (ev.gbutton.button == SDL_GAMEPAD_BUTTON_LEFT_STICK && Zoom::Possible() && !port.menu.IsOpen()) {
                    stickWaiting = true;
                } else if (recomp::sdl::MenuButton(static_cast<SDL_GamepadButton>(ev.gbutton.button))) {
                    toggleMenu();
                } else if (port.menu.IsOpen()) {
                    break;
                } else if (ev.gbutton.button == SDL_GAMEPAD_BUTTON_RIGHT_STICK) {
                    if (port.net) {
                        chatOpen(!port.typing);
                    } else {
                        port.Key('p');
                    }
                } else if (ev.gbutton.button == SDL_GAMEPAD_BUTTON_LEFT_SHOULDER) {
                    // (the left trigger held: the game's Replay, or the practice point's; with the practice point on,
                    // tapped: back to it, held: the point set; port.h, Practice)
                    SDL_Gamepad *gp = SDL_GetGamepadFromID(ev.gbutton.which);
                    const bool chord = !multi && gp && SDL_GetGamepadAxis(gp, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) > kDead;
                    if (chord) {
                        triggerChorded = true;
                        port.Key('k', kKeyExtra);
                    } else if (!multi && port.PracticeOn()) {
                        shoulderDownAt = std::max<uint64_t>(1, SDL_GetTicks());
                    } else {
                        port.Key(multi ? 'n' : 'k', kKeyRestart);
                    }
                }
                break;
            case SDL_EVENT_GAMEPAD_BUTTON_UP:
                if (ev.gbutton.button == SDL_GAMEPAD_BUTTON_LEFT_SHOULDER && shoulderDownAt) {
                    shoulderDownAt = 0;
                    if (!port.menu.IsOpen()) {
                        port.Key('k', kKeyRestart);
                    }
                }
                if (ev.gbutton.button == SDL_GAMEPAD_BUTTON_LEFT_STICK && stickWaiting) {
                    stickWaiting = false;
                    if (!port.menu.IsOpen()) {
                        toggleMenu();
                    }
                }
                break;
            case SDL_EVENT_GAMEPAD_ADDED:
                controllers.Added(ev.gdevice.which);
                padsGivenFor = -1;
                break;
            case SDL_EVENT_GAMEPAD_REMOVED: {
                // (the seat is the player it was: a gamepad that played nobody leaves none)
                const int player = playerOf(ev.gdevice.which);
                const int seat = controllers.Removed(ev.gdevice.which) >= 0 ? player : -1;
                padsGivenFor = -1;
                onlinePads.ReleaseSeat(seat);
                if (seat >= 0 && !port.net && !port.paused && !port.menu.IsOpen() && TheGame().pauseOnPadRemoval &&
                    TheGame().pauseOnPadRemoval(seat)) {
                    toggleMenu();
                    port.menu.Note(recomp::i18n::TrFormat("P{} controller disconnected. Close the menu to resume.",
                                                          {std::to_string(seat + 1)}));
                }
                break;
            }
            default: break;
            }
        }

        if (touch.TakeMenuPress()) {
            toggleMenu();
        }
        port.PumpHost(); // (a match hosted from the menu, listened for, the menu open or not)
        touch.Chat(port.net != nullptr); // (online, from the start or a match hosted from the menu)
        if (touch.TakeZoomPress()) {
            if (port.net) {
                chatOpen(!port.typing);
            } else {
                port.zoom.Both();
            }
        }
        if (Zoom::Possible()) {
            const int asked = FeatureValue("zoom");
            if (asked != zoomRow) {
                port.zoom.Choose(asked);
            }
            zoomRow = port.zoom.Choice();
            if (asked != zoomRow) {
                SetFeatureForNow("zoom", zoomRow);
                menuDirty = true;
            }
        }
        if (port.opt.moviePlay.empty() && !port.opt.autoPlay) {
            // The keyboard is every player's (each has the keys chosen for it); the first gamepad is player
            // 1's, a second player 2's, and so on
            uint16_t pads[Machine::kMaxPlayers] = {};
            int keyCount = 0;
            const bool *keys = SDL_GetKeyboardState(&keyCount);
            if (keysAfterChat) {
                keysAfterChat = std::any_of(keys, keys + keyCount, [](bool down) { return down; });
            }
            keys = keyboard.Poll(keys, keyCount);
            const bool captured = port.typing || keysAfterChat;
            const bool shortcutAlt = keyboard.ShortcutAlt(SDL_GetModState());
            const bool zoomHeld = KeyZoomHeld(keys) && !shortcutAlt;
            for (int player = 0; player < Players(); player++) {
                pads[player] |= keyboard.Read(keys, player, zoomHeld, captured);
            }
            port.service = !captured && !shortcutAlt && keys[SDL_SCANCODE_9] && !KeyBound(SDL_SCANCODE_9);
            bool hurry = multi && keys[SDL_SCANCODE_F] && !KeyBound(SDL_SCANCODE_F);
            // (rewind: R held, or a gamepad's left trigger below; port.h, Rewind)
            const bool rewindOn = port.RewindOn() && !multi;
            bool rewinding = rewindOn && !captured && !shortcutAlt && keys[SDL_SCANCODE_R] && !KeyBound(SDL_SCANCODE_R);
            if (shoulderDownAt && SDL_GetTicks() - shoulderDownAt >= kPointHold) {
                shoulderDownAt = 0;
                if (!port.menu.IsOpen()) {
                    port.Key(']');
                }
            }
            // (the camera moved by the arrows with Left Shift held, and by each gamepad's d-pad or stick with its click held)
            if (!port.menu.IsOpen()) {
                int across = 0, down = 0;
                if (zoomHeld && !captured) {
                    across += keys[SDL_SCANCODE_RIGHT] - keys[SDL_SCANCODE_LEFT];
                    down += keys[SDL_SCANCODE_DOWN] - keys[SDL_SCANCODE_UP];
                }
                for (SDL_Gamepad *gp : gamepads) {
                    if (!gp) continue;
                    if (ZoomHeld(gp)) {
                        const int x = SDL_GetGamepadAxis(gp, SDL_GAMEPAD_AXIS_LEFTX), y = SDL_GetGamepadAxis(gp, SDL_GAMEPAD_AXIS_LEFTY);
                        const int right = SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_DPAD_RIGHT) || x > kDead;
                        const int left = SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_DPAD_LEFT) || x < -kDead;
                        const int lower = SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_DPAD_DOWN) || y > kDead;
                        const int higher = SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_DPAD_UP) || y < -kDead;
                        across += right - left;
                        down += lower - higher;
                        // (the click was the camera's, not the menu's, once it has been moved with it)
                        if (right || left || lower || higher) {
                            stickWaiting = false;
                        }
                    }
                }
                port.zoom.Pan(std::clamp(across, -1, 1), std::clamp(down, -1, 1));
            }
            pads[0] |= touch.Read(port.menu.IsOpen());
            bool trigger = false;
            const bool stepOnTrigger = !PadInputBound(g_bindings, Table(), {-1, SDL_GAMEPAD_AXIS_LEFT_TRIGGER});
            if (padsGivenFor != Players()) {
                recomp::AssignPads(padChoices, gamepads.data(), static_cast<int>(gamepads.size()), Players(), playerPads);
                padsGivenFor = Players();
            }
            for (int i = 0; i < Players() && i < recomp::kBindMaxPlayers; i++) {
                SDL_Gamepad *const gp = playerPads[i];
                if (!gp) continue;
                pads[i] |= ReadGamepad(gp, i);
                const bool left = stepOnTrigger && SDL_GetGamepadAxis(gp, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) > kDead;
                trigger = trigger || left;
                hurry = hurry || (multi && left);
                // (while the game runs, the left trigger is its player's Tag: kTag, or with rewind on, rewind; in a Multi
                // game run, fast forward)
                if (left && !port.paused && rewindOn) {
                    rewinding = rewinding || !triggerChorded;
                } else if (left && !port.paused && !multi) {
                    pads[i] |= kTag;
                }
            }
            if (!trigger) {
                triggerChorded = false;
            }
            port.rewindHeld = rewinding && !port.menu.IsOpen();
            // The left trigger goes on by one frame (held: frame after frame, after a moment), once the game
            // has been stopped: it does not stop it, so a touch of it while playing does nothing
            port.hurry = hurry && !port.menu.IsOpen();
            triggerHeld = trigger && port.paused ? triggerHeld + 1 : 0;
            if (!port.menu.IsOpen() && (triggerHeld == 1 || (triggerHeld > 24 && triggerHeld % 4 == 0))) {
                port.Key('o');
            }
            // (opposite directions held together, as the SOCD cleaner says: port/socd.h)
            const auto socd = static_cast<Socd>(std::clamp(FeatureValue("socd"), 0, 3));
            for (int player = 0; player < Machine::kMaxPlayers; player++) {
                pads[player] = socdCleaners[player].Clean(socd, pads[player]);
            }
            uint16_t all = 0;
            for (const uint16_t one : pads) {
                all |= one;
            }
            uint16_t pressed = all & ~previousPad;
            previousPad = all;
            if (menuWasOpen && !port.menu.IsOpen() && !port.leaving) {
                heldAtClose = previousPad; // a key or mouse event closed it before this keyboard poll
            }
            // Start and Select held together open and close the menu, whatever they are bound to (the second
            // of them to go down is not also a press in the menu; neither reaches the game until let go)
            // (the players' gamepads: those beyond them play nothing)
            if (menuChord.Step(playerPads, std::min(Players(), recomp::kBindMaxPlayers))) {
                toggleMenu();
                pressed = 0;
                heldAtClose = previousPad;
            }
            // The menu's buttons are the same whatever the game's are set to (recomp-common/menu_sdl.h: the arrows, the
            // d-pad and the left stick; Space, Enter and A choose; Backspace and B go back), and the game's own
            // directions and the touch screen's
            unsigned menuNow = recomp::sdl::MenuHeld(keys, gamepads.data(), static_cast<int>(gamepads.size()));
            menuNow |= (previousPad & kUp) ? recomp::kMenuUp : 0;
            menuNow |= (previousPad & kDown) ? recomp::kMenuDown : 0;
            menuNow |= (previousPad & kLeft) ? recomp::kMenuLeft : 0;
            menuNow |= (previousPad & kRight) ? recomp::kMenuRight : 0;
            menuNow |= touch.Menu();
            unsigned menuPressed = menuRepeat.Step(menuNow);
            if (menuChord.held) {
                menuPressed = 0;
            }
            if (port.menu.IsOpen()) {
                if (menuPressed) {
                    port.MenuAct(port.menu.Press(menuPressed));
                    menuDirty = true;
                } else if (pressed) {
                    port.menu.Note(""); // (a press of something else still takes the note away)
                    menuDirty = true;
                }
                if (!port.menu.IsOpen() || port.net) {
                    heldAtClose = previousPad; // (online, the game goes on: this side's player stands still meanwhile)
                }
            } else if (port.leaving) {
                // (online, Esc's question: Confirm leaves, Back stays; this side's player stands still meanwhile)
                if (menuPressed) {
                    port.LeaveQuestion(menuPressed);
                }
                heldAtClose = previousPad;
            }
            heldAtClose &= previousPad;
            for (int player = 0; player < Machine::kMaxPlayers; player++) {
                port.pad[player] = pads[player] & ~heldAtClose;
            }
        }

        // The window takes the picture's shape again when that changes (widescreen, or the Display page's
        // shape), and its size when the Display page's size does: a window only, not full screen or maximized
        {
            static int shownColumns = columns, shownSize = scale;
            static Shape shownShape = DisplayShape();
            static bool shownTurned = Turned();
            const int now = port.FramedColumns() ? port.FramedColumns()
                            : TheGame().widestPicture ? std::clamp(TheGame().widestPicture(), +Video::kWidth, +Video::kMaxWidth) : columns;
            const int size = port.opt.scale > 0 ? port.opt.scale : WindowSize();
            if (now != shownColumns || DisplayShape() != shownShape || size != shownSize || Turned() != shownTurned) {
                if (!(SDL_GetWindowFlags(window) & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_MAXIMIZED))) {
                    int windowWidth = 0, windowHeight = 0;
                    SDL_GetWindowSize(window, &windowWidth, &windowHeight);
                    if (size != shownSize || Turned() != shownTurned) {
                        windowHeight = windowTall(size);
                    }
                    SDL_SetWindowSize(window, static_cast<int>(std::lround(windowHeight * windowShape(now))), windowHeight);
                }
                shownColumns = now;
                shownShape = DisplayShape();
                shownSize = size;
                shownTurned = Turned();
            }
        }
        followShader();
#ifndef __ANDROID__
        // The Arcade monitor page's screen or resolution changed (the menu can put them back to off): the window to that
        // screen, full screen in that mode
        if (window) {
            static int shownScreen = DisplayScreen();
            static ArcadeMode shownArcade = ArcadeResolution();
            static bool shownSwitchres = SwitchresChosen();
            if (DisplayScreen() != shownScreen || ArcadeResolution() != shownArcade || SwitchresChosen() != shownSwitchres) {
                const bool full = SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN;
                if (full) {
                    SDL_SetWindowFullscreen(window, false);
                    SDL_SyncWindow(window);
                }
                if (DisplayScreen() != shownScreen) {
                    recomp::sdl::CentreOnScreen(window, DisplayScreen());
                }
                Arcade().SetFullscreen(window, ArcadeChosen(), full);
                if (full) {
                    SDL_SetWindowFullscreen(window, true);
                }
                SDL_SyncWindow(window);
                shownScreen = DisplayScreen();
                shownArcade = ArcadeResolution();
                shownSwitchres = SwitchresChosen();
            }
        }
#endif
        // (Sync to the screen, in full screen at an arcade mode (the one the window was given, or the desktop in the mode
        // Switchres made) and not online: each frame waits for the screen's refresh, and the frame delay: ArcadeScreen)
        const bool arcadeMode = Arcade().OnArcade(window);
        const bool syncScreen = Arcade().Paced(window, ArcadeChosen(), port.net != nullptr);
        // (the Display page's VSync, as changed in the menu)
        presenter.SetVsync(FeatureOn("display_vsync") || syncScreen);
        if (port.quit) {
            break;
        }
        // The picture in the window as the Display page says (present.h)
        int width = 0, height = 0;
        presenter.Size(width, height);
        presenter.SetScaling(DisplayScaling());
        // (a vertical game's picture turned upright: as many columns as the board's lines, as many rows as its columns;
        // the menu stays as it is, over the turned picture in its middle: Port::MenuPicture)
        const bool turned = Turned();
        // (a framed picture, Port::Framed: in the shape of its columns)
        const int shapeColumns = port.FramedColumns() ? port.FramedColumns() : port.ShownWidth();
        // (full screen in an arcade resolution: the picture fills the screen's mode, whose shape the monitor gives it; a
        // super resolution's columns stretched over its many, its lines one for one)
        const bool arcadeScreen = arcadeMode;
        const Shape shape = arcadeScreen ? Shape::Fill : DisplayShape();
        SDL_FRect where = turned ? FitPicture(width, height, Video::kHeight, port.ShownWidth(), shape, DisplayScaling())
                                 : FitPicture(width, height, shapeColumns, Video::kHeight, shape, DisplayScaling());
        // (an arcade mode of more lines than the board's, a super resolution of 240: its 224 lines one for one in the
        // middle, the lines above and below black, never 224 stretched over 240)
        if (arcadeScreen && !turned && height > Video::kHeight && height < 2 * Video::kHeight) {
            where.h = static_cast<float>(Video::kHeight);
            where.y = std::floor((height - Video::kHeight) / 2.0f);
        }
        const SDL_FRect menuWhere = turned ? FitPicture(width, height, port.ShownWidth(), Video::kHeight, shape, DisplayScaling()) : where;
        // (the menu compact where the Arcade monitor page asks for it, or on the arcade monitor itself; drawn again when
        // that changes)
        if (const bool compact = CompactMenuChosen() || arcadeScreen; compact != port.menu.Compact()) {
            port.menu.SetCompact(compact);
            menuBase.clear();
            menuDirty = true;
        }
        // (the picture shown smaller than the menu's usual 672 lines (a small window, an arcade monitor): the menu drawn
        // on the screen's own pixels there, so that its letters are not shrunk; on an arcade monitor, on the board's
        // pixels, which a super resolution stretches as it does the game's; drawn again when that changes)
        {
            int wantWidth = static_cast<int>(std::lround(menuWhere.w)), wantHeight = static_cast<int>(std::lround(menuWhere.h));
            if (arcadeScreen && wantHeight == Video::kHeight) {
                wantWidth = std::min(wantWidth, port.ShownWidth());
            }
            const bool own = wantHeight >= Video::kHeight && wantHeight < 3 * Video::kHeight;
            if ((own ? wantWidth : 0) != menuWantWidth || (own ? wantHeight : 0) != menuWantHeight) {
                menuWantWidth = own ? wantWidth : 0;
                menuWantHeight = own ? wantHeight : 0;
                menuBase.clear(); // (made again at the new size)
                menuDirty = true;
            }
        }
        touch.Layout(width, height, where);
        int touchWidth = 0, touchHeight = 0;
        if (touch.TakePicture(touchPicture, touchWidth, touchHeight)) {
            presenter.SetOverlay(touchPicture.empty() ? nullptr : touchPicture.data(), touchWidth, touchHeight);
        }

        if (port.menu.IsOpen() && port.net) {
            // (online, the menu on the game's own picture, where it is shown: for the mouse, menu_sdl.h)
            menuPlace = {where.x, where.y, where.w, where.h, turned ? +Video::kHeight : port.ShownWidth(),
                         turned ? port.ShownWidth() : +Video::kHeight};
        }
        if (port.menu.IsOpen() && !port.net) {
            onlinePads.Clear();
            // The game's picture as it was, with the menu over it (online, the game goes on, the menu drawn on its picture:
            // Port::Shown)
            if (menuBase.empty()) {
                port.MenuPicture(menuBase, menuWidth, menuHeight, menuWantWidth, menuWantHeight); // (the size changed)
                menuDirty = true;
            }
            if (menuDirty) {
                menuDirty = false;
                menuPicture = menuBase;
                port.menu.Draw(menuPicture.data(), static_cast<int>(menuWidth), static_cast<int>(menuHeight));
            }
            // (not through a shader: the menu is drawn three times the board's size, and is not the game's picture)
            presenter.Show(menuPicture.data(), static_cast<int>(menuWidth), static_cast<int>(menuHeight), menuWhere, nullptr, false);
            // (Presenter::Size is the window's pixels, as the mouse's place is made into: menu_sdl.h)
            menuPlace = {menuWhere.x, menuWhere.y, menuWhere.w, menuWhere.h, static_cast<int>(menuWidth), static_cast<int>(menuHeight)};
            SDL_Delay(8);
            nextFrame = SDL_GetTicksNS();
            // (the menu's time is not the game's: the rates measured, and the frame delay's last frame, start again)
            Arcade().Menu();
            continue;
        }

        // (the game's time to the screen's: under 1 slowed, over 1 faster than the board)
        const auto rate = port.menu.GameRate();
        const double pace = port.paused ? 1.0 : static_cast<double>(rate.frames) / rate.ticks;
        // Only the playing window can observe physical input during a network wait. Take a complete mapped
        // poll after menu/chord filtering, and retire it only when the game accepts a frame.
        const bool sampleOnline = port.net && !port.menu.IsOpen() && !port.connecting && !port.net->Watching() && !port.leaving &&
                                  !port.typing && !port.opt.autoPlay && port.opt.moviePlay.empty() && !FastForward() &&
                                  FeatureValue("online_wait_input") == 0;
        OnlinePadQueue::Pads live{};
        const int localPeople = sampleOnline ? port.net->People(port.net->Local()) : 0;
        if (sampleOnline) {
            std::copy_n(port.pad, localPeople, live.begin());
            const auto &sample = onlinePads.Poll(live, SDL_GetTicks());
            std::copy_n(sample.begin(), localPeople, port.pad);
        } else {
            onlinePads.Clear();
        }
        const bool ran = port.Tick();
        if (sampleOnline) {
            if (ran) onlinePads.Accepted();
            std::copy_n(live.begin(), localPeople, port.pad);
        }
        port.ReplayMake(); // (the instant replay being made, if it is: a few of its frames beside the game's)

        if (audio && silent != (port.menu.muted || audioLog)) {
            silent = port.menu.muted || audioLog;
            SDL_SetAudioStreamGain(audio, silent ? 0.0f : 1.0f);
        }
        std::vector<int16_t> &made = port.machine->audio;
        if (audio && ran && !made.empty()) {
            // The game makes sound at its own pace and the sound device takes it at its own, and the two
            // clocks never quite agree. So the sound is played a touch faster when it is piling up and a
            // touch slower when it is running out (a fraction of a percent, far too little to hear), which
            // keeps a frame or two of it waiting: no gaps, no growing delay.
            // (Slowed, the sound is played slower by as much; faster, faster.)
            double waiting = SDL_GetAudioStreamQueued(audio) / (4.0 * QSound::kSampleRate) / pace;
            // (When much more than that is waiting (the game caught up after being held up, say) what waits
            // is thrown away: a click once, rather than sound that stays late for the seconds it would take
            // to play it off.)
            if (waiting > kAudioMost) {
                SDL_ClearAudioStream(audio);
                waiting = 0;
                audioDropped++;
            }
            const double ratio = (1.0 + std::clamp((waiting - kAudioWaiting) * 0.5, -0.02, 0.02)) * pace;
            SDL_SetAudioStreamFrequencyRatio(audio, static_cast<float>(ratio));
            audioLeast = std::min(audioLeast, waiting);
            if (audioLog && port.frame % 60 == 0) {
                fprintf(stderr, "audio: frame %llu, %.0f ms waiting (least %.0f), ratio %.4f, thrown away %d times\n",
                        static_cast<unsigned long long>(port.frame), waiting * 1000, audioLeast * 1000, ratio, audioDropped);
                audioLeast = 1;
            }
            soundFilter.Apply(made.data(), made.size() / 2, FeatureValue("sound_filter"), QSound::kSampleRate);
            SDL_PutAudioStreamData(audio, made.data(), static_cast<int>(made.size() * sizeof(int16_t)));
        }
        // (the video being recorded: the game's sound as heard, the sound filter's and all; the soundtrack's below)
        const bool recording = ran && port.recorder.Recording();
        std::vector<int16_t> recorded;
        if (recording) {
            recorded = made;
        }
        made.clear();
        bool musicFresh = false;
        if (ran && port.soundtrackOn && port.soundtrack.Any()) {
            // (fed as the game's sound is: the game's time's worth each frame, kept a frame or two ahead of the device)
            port.soundtrack.Make(port.machine->soundtrack.state, !FastForward() && !port.hurry, musicMade);
            musicFresh = true;
            if (!musicMade.empty()) {
                const int rate = port.soundtrack.Rate();
                if (!music) {
                    musicSpec.freq = rate;
                    music = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &musicSpec, nullptr, nullptr);
                    if (music) {
                        SDL_ResumeAudioStreamDevice(music);
                    } else {
                        fprintf(stderr, "Audio (soundtrack): %s\n", SDL_GetError());
                    }
                } else if (rate != musicSpec.freq) {
                    musicSpec.freq = rate;
                    SDL_SetAudioStreamFormat(music, &musicSpec, nullptr);
                }
            }
            if (music && !musicMade.empty()) {
                if (musicSilent != (port.menu.muted || audioLog)) {
                    musicSilent = port.menu.muted || audioLog;
                    SDL_SetAudioStreamGain(music, musicSilent ? 0.0f : 1.0f);
                }
                double waiting = SDL_GetAudioStreamQueued(music) / (8.0 * musicSpec.freq) / pace;
                if (waiting > kAudioMost) {
                    SDL_ClearAudioStream(music);
                    waiting = 0;
                }
                const double ratio = (1.0 + std::clamp((waiting - kAudioWaiting) * 0.5, -0.02, 0.02)) * pace;
                SDL_SetAudioStreamFrequencyRatio(music, static_cast<float>(ratio));
                SDL_PutAudioStreamData(music, musicMade.data(), static_cast<int>(musicMade.size() * sizeof(float)));
            }
        }

        if (recording) {
            port.VideoFrame(recorded.data(), recorded.size() / 2, musicFresh ? musicMade.data() : nullptr, musicFresh ? musicMade.size() / 2 : 0,
                            port.soundtrack.Rate());
        }

        // (zoomed: the part of the picture the camera looks at, the readout's panels as they are)
        Zoomed zoomed;
        // (--video gpu: the game's picture drawn on the GPU from the native records, with the readouts over it, when the
        // presenter draws with SDL_GPU and the GPU can draw the frame: gpu_picture.h; else the CPU's picture)
        bool drawn = false;
        Arcade().WorkDone(); // (the frame run; drawing it and showing it are the presenter's, below)
        if (port.gpuWanted && presenter.UsesGpu() && !presenter.Shaded() && !turned && !port.FramedColumns()) {
            if (port.gpuPicture.Device() != presenter.Device()) {
                port.gpuPicture.Close();
                for (auto &g : port.guestPictures) {
                    g.picture->Close();
                }
                std::string gpuError;
                bool guestsOpen = true;
                for (auto &g : port.guestPictures) {
                    guestsOpen = guestsOpen && g.picture->Open(presenter.Device(), g.roms->graphics.data(), g.roms->graphics.size(), gpuError);
                }
                if (!guestsOpen ||
                    !port.gpuPicture.Open(presenter.Device(), port.roms.graphics.data(), port.roms.graphics.size(), gpuError) ||
                    (!port.extraRoms.graphics.empty() &&
                     !port.gpuPicture.SetExtraGraphics(port.extraRoms.graphics.data(), port.extraRoms.graphics.size(), gpuError))) {
                    fprintf(stderr, "--video gpu: %s; the CPU's picture is shown\n", gpuError.c_str());
                    port.gpuWanted = false;
                }
            }
            // (not with the panel drawn apart, which the GPU's picture does not do: zoom.h, by distance)
            if (port.gpuWanted && port.gpuPicture.CanDrawKept() && port.gpuPicture.KeptWidth() == port.ShownWidth() &&
                !cps2recomp::PictureMade() && !port.zoom.KeepsPanel()) {
                static std::vector<uint32_t> readouts;
                const bool isZoomed = port.Windowed(readouts, zoomed, true);
                drawn = presenter.ShowDrawn(
                    [&](SDL_GPUCommandBuffer *commands) { return port.gpuPicture.DrawKept(commands, readouts.data(), isZoomed); },
                    port.ShownWidth(), Video::kHeight, where, isZoomed ? &zoomed : nullptr);
            }
        }
        if (!drawn && turned) {
            // (the camera's zoom is a fighting game's: not with the picture turned)
            static std::vector<uint32_t> upright;
            port.Windowed(shown, zoomed);
            TurnPicture(shown.data(), port.ShownWidth(), upright);
            presenter.Show(upright.data(), Video::kHeight, port.ShownWidth(), where, nullptr);
        } else if (!drawn) {
            const bool isZoomed = port.Windowed(shown, zoomed);
            if (zoomed.columns) {
                presenter.Show(shown.data(), zoomed.width, zoomed.height, where, nullptr); // (framed: Port::Framed)
            } else {
                presenter.Show(shown.data(), port.ShownWidth(), Video::kHeight, where, isZoomed ? &zoomed : nullptr);
            }
        }
        port.LagShown(ran, SDL_GetTicksNS()); // (the input lag readout: Port::Lag)

        if (syncScreen && !port.opt.unthrottled && !FastForward()) {
            // (paced by the screen: the picture just shown waited for its refresh; the frame delay waits before the next)
            nextFrame = Arcade().Shown();
        } else if (!port.opt.unthrottled && !FastForward()) {
            Arcade().BoardPace(arcadeMode); // (in an arcade mode, the rate as measured goes in the log)
            nextFrame += static_cast<uint64_t>(kFrameNs);
            const uint64_t now = SDL_GetTicksNS();
            if (nextFrame > now) {
                SDL_DelayPrecise(nextFrame - now);
            } else if (now - nextFrame > 100000000ull) {
                nextFrame = now; // (fell far behind: start counting again)
            }
        }
    }
    // (for the checks: what a shaded window showed last, CPS2RECOMP_SHADER_SHOT=FILE.png)
    if (const char *shot = getenv("CPS2RECOMP_SHADER_SHOT"); shot && presenter.Shaded()) {
        std::vector<uint32_t> pixels;
        int width = 0, height = 0;
        if (presenter.Shader()->ReadBack(pixels, width, height)) {
            WritePng(shot, pixels.data(), static_cast<uint32_t>(width), static_cast<uint32_t>(height));
        }
    }
    controllers.Close();
    if (audio) {
        SDL_DestroyAudioStream(audio);
    }
    if (music) {
        SDL_DestroyAudioStream(music);
    }
    // (on to a Multi game run's next game: its window where this one is; on macOS the launcher starts it, and must
    // be in front for it to come forward)
    if (ProgramEnded() == kNextGame && !MultiGame().empty()) {
        KeepPlacement(window);
#ifdef __APPLE__
        HandFrontToLauncher();
#endif
    }
    port.gpuPicture.Close();
    for (auto &g : port.guestPictures) {
        g.picture->Close();
    }
    presenter.Close();
    SDL_DestroyWindow(window);
    while (port.replay.making) { // (a replay being made, finished first)
        port.ReplayMake();
    }
    port.StopVideo(); // (a video being recorded finished: before SDL_Quit, which takes its sound converters with it)
    Arcade().Close(); // (the refresh clock stopped, the desktop's mode as it was)
    SDL_Quit();
    return 0;
}

} // namespace port
