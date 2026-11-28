// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
//
// Shared between the port's entry point (main.cpp) and its window (frontend_sdl.cpp).
#pragma once

#include "../machine/machine.h"
#include "menu.h"
#include "movie.h"
#include "netplay.h"
#include "present.h"
#include "rewind.h"
#include "scene.h"
#include "gpu_picture.h"
#include "soundtrack.h"
#include "video_record.h"
#include "zoom.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <filesystem>
#include <iterator>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

namespace port {

// One online fighter-pick row. Netplay::Run orders a frame's events by sender, then OnlineEvent reads each
// sender's comma-separated commands in order. A replay can already have later picks in this history.
struct OnlinePickHistory {
    using Position = std::tuple<uint64_t, int, size_t>; // frame, sender, command byte offset
    explicit OnlinePickHistory(int initial) : base(initial) {}
    void Record(Position at, int value) { picks[at] = value; }
    int Before(Position at) const {
        const auto next = picks.lower_bound(at);
        return next == picks.begin() ? base : std::prev(next)->second;
    }
private:
    int base;
    std::map<Position, int> picks;
};

struct Options {
    std::string romPath = "rom";
    std::string extraRomPath;  // --rom2: the extra set (game.cfg's extra_set), if the game has one (default: beside --rom)
    std::map<std::string, std::string> guestRomPaths; // --guest ID=PATH: the sets of the games game.cfg's guests names (default: beside --rom)
    std::string eepromPath;    // the board's settings memory (created when missing)
    std::string moviePlay;     // input movie to replay
    std::string movieRecord;   // input movie to write
    std::string shotPrefix;    // PNG prefix for --shot-every
    std::string hashLog;       // per-frame state hash log
    std::string recordEntries; // entries file to add to (tools/recomp.py reads it)
    std::string dumpProgram;   // write the deciphered program here and stop
    std::string audioDump;     // WAV file of the sound (headless)
    std::string videoOut;      // --record-video FILE.mp4: the whole run recorded as a video (video_record.h), as the Video page's Record video does
    std::string soundtrack;    // --soundtrack DIR: the custom soundtrack's folder (soundtrack.h; default: beside settings.ini)
    std::string soundtrackDump; // --soundtrack-dump FILE: WAV file of what the custom soundtrack plays (headless)
    std::string settingsPath;  // settings.ini with the player's feature choices
    std::string features;      // --features A,B,-C
    std::string menuTest;      // --menu FRAME:KEYS[:shot.png]
    std::string keys;          // --keys TICK:KEY,TICK:KEY...
    std::vector<std::string> dumpRam; // --dump-ram FRAME:file (as many as given)
    std::string poke;                 // --poke ADDR=BYTE,...
    uint32_t watchLow = 1, watchHigh = 0; // --watch-writes LOW[-HIGH]
    std::string trace;         // --trace LOW-HIGH:file
    std::string layerLog;      // --layer-log FROM:ADDR:file
    bool checkRewrites = false, noRewrites = false;
    std::string cpu;           // native | blocks | interp | check (default: native when there is translated code)
    std::string sound;         // driver | z80 | check (default: driver when the game's sound program is a known one)
    std::string video;         // board | check (docs/NATIVE_VIDEO.md; default: board)
    std::string stateSave, stateLoad; // FRAME:path
    uint64_t frames = 0;       // stop after this many (0: run on)
    uint64_t shotEvery = 0;
    uint64_t autoSeed = 0;     // --auto: made-up input from this seed
    uint64_t soundTest = 0;    // --sound-test SEED[:EVERY]: every song and command sent to the sound side
    unsigned musicTest = 0;    // --music-test FRAMES: each piece of music on the board in turn
    unsigned soundTestEvery = 30;
    uint64_t autoFrom = 0;     // ... from this frame on
    int autoPlayers = 2;       // ... for this many players
    uint64_t testFrom = 0, testTo = 0; // --test-switch FROM:TO: the operator's test switch held over these frames
    bool autoPlay = false;
    bool headless = false, mute = false, unthrottled = false, fullscreen = false, stats = false;
    bool hashPicture = false; // --hash-log hashes the picture only
    bool hashNoPicture = false; // --hash-no-picture: the state hash without the picture
    bool hashNoSound = false;   // --hash-no-sound: ... without the sound side (as online play's)
    bool hashGame = false;     // --hash-game: the state hash without the Z80 and its own memory (to compare --sound driver and z80)
    int cpuSpeed = 1;         // --cpu-speed
    int cpuClock = 0;         // --cpu-clock (0: game.cfg's)
    std::string ramLog;       // --ram-log
    std::string renderer;     // --renderer sdl|gpu (gpu unless said)
    std::string shader;       // --shader FILE.slangp: that shader preset, whatever the Display page's Shader says (shading.h)
    std::string presentTest;  // --present-test FILE.png: the last picture drawn by the GPU path with no window
    int presentWidth = 0, presentHeight = 0; // --present-size WxH: the window it is drawn in
    uint64_t presentEvery = 0;  // --present-every N:PREFIX: ... every N frames (the zoom followed every frame)
    std::string presentPrefix;
    int scale = 0;            // --scale: the window this many times the board's lines (0: the Display page's)
    int zoom = 0, zoomPlayer = 0; // --zoom SIZE:PLAYER: the camera's zoom from the start (zoom.h; SIZE 0-4, PLAYER 1 or 2, 3 both)
    uint64_t rollbackTest = 0;  // --rollback-test SEED: each frame run as online play's rollback would (RollbackTest)
    bool online = false;        // --host or --join: online play (netplay.h), as `net` says
    Netplay::Settings net;
    bool netSpectatorsRequested = false; // explicit --net-spectators, independent of any previous menu-hosted match
    int netDelayOverride = -1; // --net-delay overrides the saved host preference, including when explicitly zero
    int launcherNetDelay = -1; // the launcher's global Online setting, below an explicit per-game choice
    std::string netSay;         // --net-say [FRAME:]TEXT: (tests) the line said in the chat at FRAME of the match (120)
    uint64_t netSayAt = 120;
    // --net-command FRAME:TEXT, again for more: (tests) an event this side makes at FRAME of the match (Netplay::Command:
    // "mode=1:7" the ball battle with seed 7, "court_one=3" a fighter)
    std::vector<std::pair<uint64_t, std::string>> netCommands;
    std::string netTyping;      // --net-typing TEXT: (pictures) a line shown as being typed, all through the match
};

// --rollback-test: what online play's rollback does to the game (issue #70), with no network. Player 1's controls
// are this side's; the others' come from the other side late, by a lag that changes at random (0 to kMostLag
// frames). Until they come they are guessed (as last known: what rollback sessions guess), and when they come
// and a guess was wrong, the machine and the game's sources are put back to that frame and the frames since are
// run again. A run with it must go exactly as one without (--hash-log the same on every frame): anything the
// game keeps outside its save state, or that a frame run twice does twice, shows up as a difference.
struct RollbackTest {
    static constexpr int kMostLag = 8;
    static constexpr int kKept = 16; // (frames kept: more than the most a rollback goes back)
    struct Kept {
        std::vector<uint8_t> state; // the machine and the game's sources before the frame
        size_t sound = 0;           // the machine's sound before it
        Movie::Pads used{}, real{}; // the controls it was run with, and the real ones
        uint32_t hash = 0;          // (--hash-log: the state after it, as last run)
    };
    std::array<Kept, kKept> kept;
    uint64_t random = 0;
    uint64_t confirmed = 0; // the frames before this have had the others' real controls
    int lag = 0;
    uint64_t rollbacks = 0, rerun = 0, most = 0; // (for the report at the end)
    // (and what it cost: the frames, the keeping and the putting back, in seconds and how many)
    double stepTime = 0, keepTime = 0, putBackTime = 0;
    std::vector<float> frameTimes; // (each frame's, its rollback's frames and keeping included)
    uint64_t steps = 0, keeps = 0, putBacks = 0;
    size_t stateSize = 0;
};

struct Port {
    Options opt;
    cps2recomp::RomSet roms;
    cps2recomp::RomSet extraRoms; // (game.cfg's extra_set: another set the game reads, if it has one)
    std::unique_ptr<cps2recomp::Machine> machine;
    uint64_t frame = 0;       // frames run
    uint16_t pad[cps2recomp::Machine::kMaxPlayers] = {}; // what the players hold (cps2recomp::Button bits), set by the window
    bool service = false, test = false;
    Movie playMovie, recMovie;
    uint64_t movieNext = 0; // (the frame the movie played last was before)
    uint16_t lastRecorded[cps2recomp::Machine::kMaxPlayers] = {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF};
    FILE *hashFile = nullptr;
    uint64_t autoState = 0;
    uint16_t autoPad[cps2recomp::Machine::kMaxPlayers] = {};
    Menu menu;
    // Stopping, slowing and speeding up the game: a tick is one picture of the screen; the game runs a frame in
    // every tick, or so many frames in so many ticks (menu.GameRate()), or while stopped only when asked to step
    uint64_t tick = 0;
    int owed = 0;         // the game's frames due, in ticks of the rate (whole frames run at once)
    bool paused = false;
    int steps = 0;        // frames to go on by while stopped
    // In a Multi game run (runtime/game.h, MultiGame): held, the game runs kHurry frames a tick, unheard
    static constexpr int kHurry = 4;
    bool hurry = false;
    // The speed is said at the bottom for a while after it changes (as the zoom's label), not all the while it is not
    // full: the rate said last, and the screen's ticks it is said for yet
    static constexpr int kSpeedTicks = 150; // (two and a half seconds)
    Menu::Rate saidRate{1, 1};
    int speedTicks = 0;
    // F12 (issue #31): the picture as shown, readout and all (not the zoom's), saved as a PNG in the player's folder's
    // screenshots/; where it went is said at the bottom for a while
    bool Screenshot();
    // The picture as it is, for the launcher's tile of the game (picture.png in the game's folder): on quitting
    void KeepPicture();
    std::string shotSaid;
    int shotTicks = 0;
    // The scene export (scene.h, issue #366): Shift+F12 or the Scene export page's Save scene, the picture and each of the
    // board's layers by itself, in the player's folder's exports/scenes/; Ctrl+F12 or Record clip, the same for each frame
    // of a few seconds (again: stopped there)
    SceneExport scene;
    void SaveScene();
    void SceneClip();
    // The video recording (video_record.h, issue #376): Alt+F12 or the Video page's Record video, what is played and
    // heard as an MP4 in the player's folder's videos/, until either again; or --record-video's file, the whole run
    VideoRecorder recorder;
    uint64_t videoFrame = 0; // (the frame the last picture handed to it was of)
    bool shooting = false;   // (a screenshot being taken: the window's line about the recording not in it)
    void RecordVideo();      // (starts or stops)
    bool StartVideo(const std::string &path);
    void StopVideo();
    // Each frame run while recording: its sound (the game's, as heard; and the custom soundtrack's, at its rate) and the
    // picture as screenshots take it, without the port's notices at the bottom
    void VideoFrame(const int16_t *sound, size_t soundFrames, const float *music, size_t musicFrames, int musicRate);
    // The instant replay (video_record.h): the last seconds of play (the Video page's Replay length) kept as states a
    // second apart and every frame's controls; Save replay (Alt+Shift+F12) plays them again into a video, a few frames
    // in each tick beside the game, which goes on as it does. At each kept state the replay is put back to it, so that
    // whatever a frame run again does differently (a save state loaded, the board reset) lasts a second at most.
    struct Replay {
        struct Kept {
            uint64_t frame = 0;         // (the state before this frame ran)
            std::vector<uint8_t> state;
        };
        struct Input {
            uint16_t players[cps2recomp::Machine::kMaxPlayers] = {};
            bool service = false, test = false;
        };
        std::deque<Kept> kept;          // (the oldest first)
        std::deque<Input> inputs;       // (every frame's since the oldest kept)
        uint64_t inputsFrom = 0;        // (the frame inputs.front() is of)
        // Being made: the states and controls taken when it was asked for, the replay's own machine and game as a
        // state between ticks, its next frame and the frame it ends before
        bool making = false;
        std::deque<Kept> from;
        std::vector<Input> made;
        uint64_t madeFrom = 0, at = 0, end = 0;
        std::vector<uint8_t> state;
        VideoRecorder recorder;
    } replay;
    void ReplayKeep(const uint16_t *pressed); // (each frame, before it runs)
    void SaveReplay();
    void ReplayMake();                        // (each tick: frames of the replay being made)
    // Rewind and the practice point (docs/PRACTICE.md), the Game page's switches, never online. Rewind: while
    // rewindHeld (R, or a gamepad's left trigger, held), a kept state back each tick, three frames, shown and unheard;
    // let go, the game plays on from there. The states are kept every kRewindEvery frames before they run, for about
    // half a minute (rewind.h). The practice point: ']' (a gamepad's left shoulder held) keeps the game as it is; then
    // Backspace (the shoulder tapped) puts it back, and Backslash (the shoulder with the left trigger held) shows the
    // last attempt from it again, the controls as they were pressed, until a button is pressed.
    static constexpr int kRewindEvery = 3;
    static constexpr size_t kRewindMost = 600, kRewindMostBytes = size_t{256} << 20;
    struct Rewind {
        StateRing ring;
        bool was = false;    // (rewinding the tick before)
        int ticks = 0;       // (how long it has been held, for the notice)
    } rewind;
    bool rewindHeld = false; // (set by the window each tick; tests: 'w' switches it)
    bool RewindOn() const;
    void RewindKeep(const uint16_t *pressed); // (each frame, before it runs)
    bool RewindTick();                        // (a tick while held: false if there was nothing to go back to)
    struct Practice {
        std::vector<uint8_t> state; // (none: no point set)
        uint64_t frame = 0;
        std::vector<StateRing::Input> attempt, last, replay; // (since the point; the attempt before; being replayed)
        bool replaying = false;
        size_t replayAt = 0;
        uint16_t heldAtReplay[cps2recomp::Machine::kMaxPlayers] = {};
    } practice;
    bool PracticeOn() const;
    void PracticeSet();
    bool PracticeBack();   // (false: no point set)
    bool PracticeReplay(); // (false: no point set)
    void PracticeInput(uint16_t *pressed); // (each frame, before it runs: the replay's controls, and the attempt kept)
    void Said(const char *what);           // (a few seconds at the bottom of the picture)
    // The stage viewer's Save whole stage (fight/alternative.h, StageSweep): its frames stitched (scene.h), each frame
    StageStitch stageStitch;
    void StageSweepFrame();
    // One tick; true if the game ran (so that there is a new picture and sound)
    bool Tick();
    // The keys that work while playing: 'p' stops and starts, 'o' goes on by a frame, '-' and '=' change the
    // speed, 'k' does the game's command with the key `n` (runtime/game.h, kKeyRestart...), 'n' asks a
    // Multi game run for its next game ('h': its fast forward on and off, for tests; 'z' the zoom pressed for player
    // `n` + 1, 'm' its camera moved a step, `n` 1-4 left, right, up, down, for tests)
    void Key(char key, unsigned n = 0);
    void RunCommand(size_t index);
    // (rapid fire: how long each player has held each button)
    uint32_t rapidHeld[cps2recomp::Machine::kMaxPlayers][16] = {};
    void RapidFire();
    int coinFrames = 0;   // frames left of the menu's coin
    int start2Frames = 0; // ... and of its press of player 2's Start
    int testFrames = 0;   // ... and of its press of the test switch

    // What the menu asked for (it is closed if that went well)
    void MenuAct(Menu::Action action);
    bool quit = false; // the player chose the menu's "Quit game": the frontend stops
    // The game's picture at the size the menu is drawn at (4:3, three times the lines)
    // (`wantWidth` x `wantHeight` given: that size instead, the screen's own pixels where the picture is shown on a small
    // screen or an arcade monitor, so that the menu's letters are drawn on them and not shrunk)
    void MenuPicture(std::vector<uint32_t> &pixels, uint32_t &width, uint32_t &height, int wantWidth = 0, int wantHeight = 0) const;
    // The picture with the readout over it (runtime/game.h, SetHud), as the window and screenshots show it; and
    // where on it the readout's panels went (none: not asked)
    // (`over`: drawn over a picture all of `base` instead of the game's: for the readouts alone, Readouts)
    // (`notices`: the port's lines at the bottom too, as the window has them: the speed, the zoom, a screenshot saved...)
    void Shown(std::vector<uint32_t> &pixels, std::vector<SDL_Rect> *panels = nullptr, bool over = false, uint32_t base = 0,
               bool boxes = true, bool notices = true) const;
    // Draw the requested game HUD parts, then the notices and online overlays on either the board or the framed court.
    void Overlays(uint32_t *px, int W, int H, std::vector<SDL_Rect> *panels, bool notices, unsigned hudParts, bool waiting) const;
    // The readouts alone (all Shown draws over the game's picture), for the GPU's picture (gpu_picture.h): red, green,
    // blue, alpha bytes, the colours times the alpha; from Shown over black and over white, whose difference is what
    // the readouts leave of what is under them
    void Readouts(std::vector<uint32_t> &rgba, std::vector<SDL_Rect> *panels = nullptr) const;
    // The camera's zoom (zoom.h), the window's: a gamepad's left stick held and a shoulder pressed
    Zoom zoom;
    // The picture for the window, a step of the zoom on: as Shown, or zoomed (true), as `zoomed` says (present.h)
    // (`readouts`: the readouts alone, as Readouts, and not stacked when zoomed: the GPU's picture stacks them)
    bool Windowed(std::vector<uint32_t> &pixels, Zoomed &zoomed, bool readouts = false);
    // A mode that frames a picture wider than it is shown (Game::framedColumns): the columns of the shape shown (0: none),
    // and the picture made through its camera, at three times that shape's size (Windowed, with `zoomed`'s size)
    int FramedColumns() const;
    void Framed(std::vector<uint32_t> &pixels, Zoomed &zoomed);
    struct FrameCamera {
        float x = 0, y = 0, size = 0; // (the view's middle in the picture, and its size against the shape's)
        bool settled = false;
    } frameCamera;
    std::vector<uint32_t> framedFight, framedLaid, framedWhite; // (Framed's, kept from frame to frame)
    std::vector<int> framedColumnOf;
    // The picture drawn on the GPU from the native records (--video gpu: docs/NATIVE_VIDEO.md), when the window's
    // presenter draws with SDL_GPU
    GpuPicture gpuPicture;
    bool gpuWanted = false;
    // (the guests' boards', whose stages the game's objects are drawn over: runtime/guest.h; their sets' graphics)
    struct GuestPicture {
        std::unique_ptr<GpuPicture> picture;
        const cps2recomp::RomSet *roms = nullptr;
    };
    std::vector<GuestPicture> guestPictures;
    // How many pixels a line of it has (the machine's picture: wider when the game's sources ask)
    int ShownWidth() const {
        return machine->PictureWidth();
    }
    std::string StatePath(int slot) const;
    std::string PlayerFolder() const; // (the one settings.ini and the saves are in, with a slash; "" if none)

    bool Init(std::string &error);
    // The custom soundtrack (soundtrack.h): its files, played while the port's custom_soundtrack feature is on
    SoundtrackPlayer soundtrack;
    bool soundtrackOn = false;
    int soundtrackValue = 0; // (the feature's choice: 1 on, 2 on with the playlist shuffled)
    std::string SoundtrackFolder() const;
    // The shader preset to draw with (shading.h): --shader's, or the Display page's Shader's; "" for none
    std::string ShaderPreset() const;
    void SoundtrackFollow(); // (each frame: the feature switched on or off in the menu)
    // One frame: input (window, movie or made up), the machine, the per-frame outputs
    void RunFrame();
    // The frame itself, with these controls: what makes the next frame from the last (all that online play runs
    // again in a rollback)
    void Step(const uint16_t *players);
    // The machine and the game's sources, kept in memory and put back (run-ahead's and rollback's)
    void KeepState(std::vector<uint8_t> &data);
    void PutBackState(const std::vector<uint8_t> &data);
    RollbackTest rollbackTest;
    void RollbackFrame(const uint16_t *real);
    // Online play (netplay.h): the other side found before the first frame, and each frame run through it
    std::unique_ptr<Netplay> net;
    bool connecting = false; // (the other side not found yet: a window shows the wait)
    bool leaving = false;    // (online, watching or waiting: Esc asked whether to leave; said on the screen until answered)
    bool cameIn = false;
    // Online: each side's win streak (matches won in a row, the other player's against them), counted in the fights
    // both players have pressed Start for (fight/battle.h: the round's result, and the rounds a match is won with); said
    // under the life bars from two on. Each player's machine counts the same, both running the same game.
    bool otherIn = false;
    int streak[2] = {}, roundsWon[2] = {};
    bool roundCounted = false;
    int roundQuiet = 0;
    void Streaks();     // (this side's player has pressed Start in the match: "PRESS START TO JOIN" is said until then)
    // Online: the chat (issue #196). Each line is said on the picture for a while (`until`, a tick), the newest
    // lowest; and the line this side's player is typing, while they type one (the frontend's: Enter sends it, Esc
    // puts it away; the keyboard is the chat's meanwhile, not the game's)
    struct ChatLine {
        std::string text;
        uint64_t until;
        bool own;
    };
    std::deque<ChatLine> chat;
    bool typing = false;
    std::string typed;
    uint64_t chatHintUntil = 0; // (how to chat, said under the chat for a while as a match begins)
    void Say();       // (the typed line: sent, and said with the rest)
    void HearChat();  // (the other side's lines that have come)
    bool StartOnline(std::string &error);
    // The in-game menu's Online page (issue #196): a match hosted while the game goes on as the player's own, offline
    // (its modes, cheats, the menu, save states, pause: all as without it), `waitingHost` listening meanwhile (PumpHost:
    // every tick, and while the menu is open), listed in the lobby under the launcher's name if the player has one.
    // When someone comes (MatchComing, from Netplay::Waiting) the game starts from the board's reset with the match's
    // features, for both players alike. The menu's row again stops hosting (StopHosting). After the match the player's
    // own game and features are back (BackToSolo): still hosted with the same code when the other player left, not
    // when this one did.
    std::unique_ptr<Netplay> waitingHost;
    void HostFromMenu();
    void PumpHost();
    void MatchComing();
    void StopHosting();
    void BackToSolo(bool stillHosting);
    bool hostedFromMenu = false;
    std::map<std::string, int> ownFeatures; // (as they were before the match's: put back by BackToSolo)
    std::string hostNote;                   // (why the hosting stopped, said for a few seconds: until hostNoteUntil)
    uint64_t hostNoteUntil = 0;
    // Esc's question answered yes: the match left, and the game with it unless it was hosted from the menu
    void LeaveOnline();
    // Online, the modes played in a match (Netplay::Command): an event of `player`'s, run on both sides before the
    // frame it was given for: "mode=N:SEED" (the host's: battle::StartOnline) and "row=value" (a player's own fighters
    // on the modes' pages: player 1's rows the host's, player 2's the other's), commas between
    // (`at`: the frame it is run before. A mode starts with each row as the events before its command left it.)
    void OnlineEvent(int player, int side, const std::string &text, uint64_t at); // (player: the sender's machine)
    std::map<std::string, OnlinePickHistory> onlinePickLog;
    // The host's pause for choosing: while the host's match menu is open the game stands still for both ("setup=1",
    // the host's event; "setup=0" when it closes), and the other player's opens by itself. The other player says
    // "ready=1" when their fighters are chosen (a pick of theirs after it takes it back); a mode other than Versus starts
    // only with the other player ready before its frame (the host's Start waits for it: onlinePendingMode). Only the
    // events themselves are kept, by frame (a rollback runs them again alike); what they make of the pause and Ready is
    // worked out from them (OnlineSetupScan), a mode's start being the same at its frame on both sides. (Spectators
    // run the players' controls without the events: a pause, as a mode's start, is not theirs.)
    std::map<uint64_t, int> onlineSetupLog; // (the host's "setup": frame, value)
    std::map<uint64_t, int> onlineModeLog;  // (the host's "mode": frame, mode)
    std::map<uint64_t, int> onlineReadyLog; // (the other player's Ready as their frame's events leave it: a pick, 0)
    struct OnlineSetupState {
        bool paused = false, ready = false;
        uint64_t pausedFrames = 0; // (how many frames before were paused)
    };
    OnlineSetupState OnlineSetupScan(uint64_t before) const; // (the events of the frames before `before`)
    bool OnlinePausedAt(uint64_t at) const;
    bool OnlineOtherReady() const; // (the other player's Ready, as the frames run so far have it)
    bool onlineSetupSent = false;  // (the host: what it last said)
    int onlinePendingMode = -1;    // (the host: a Start waiting for the other player's Ready)
    int onlineReadyMine = -1;      // (the other player: Ready as chosen, before its event has run)
    void OnlineSetupFrame();       // (each tick of a match: the host's pause and its Start)
    // Esc in a match: the match's own menu (Menu::OpenOnline), drawn over the game, which goes on: the host's modes to
    // start (OnlineCommand), this player's own fighters for them (onlinePicks, sent as events), leaving. Watching, or a
    // host waiting, Esc asks only whether to leave (`leaving`, answered by LeaveQuestion)
    void OpenOnlineMenu();
    void OnlineCommand(size_t index);
    void LeaveQuestion(unsigned pressed);
    std::vector<Netplay::Layout> onlineSeats; // (the host's seat changes on the menu, after the modes: issue #448)
    // Both players back to the character choice (a match of two, a fighting game): each asks ("reselect=1", or 0 to take it
    // back), and at the frame the second ask runs the board starts again and both are put in (OnlineEvent, Step). Kept in
    // the rollback state (KeepState), as the board is: a rollback runs the asks again alike.
    struct OnlineReselect {
        uint8_t asked[2] = {0, 0};
        int32_t boot = -1; // (frames since the board started again, while both are being put in; -1: not)
    } onlineReselect;
    bool OnlineReselectHere() const;
    void OnlineReselectBoot();
    std::string TeamsSaid(const Netplay::Layout &layout) const; // ("Ann + Bob vs Cy + Di")
    std::vector<int> onlineModes;           // (the menu's commands' modes, in its order)
    std::vector<std::string> onlineRows;    // (the menu's features' rows: the game's features they stand for)
    std::map<std::string, int> onlinePicks; // (this player's fighters as chosen, before their events have run)
    void HearEvents();
    void Matched();
    void ArcadeFrame(); // (--net-arcade: the host's own play while it waits for the other player)
    // A window's match that ended with the other side gone: the host waits for them to come back (the same code, or
    // its address), the joining side goes to the host again; the game starts over from the board's reset, the host's
    // sent whole to the other (Netplay's state). `rejoin` says why, while it is waited for.
    void Again(const std::string &why);
    std::string rejoin;
    uint64_t againAt = 0; // (joining: the tick to try again at)
    void RunAhead();
    int RunsAhead() const; // (the frames run-ahead runs now: 0 when it does not)
    // The input lag readout (the Display page's input_lag), in three parts. The game's: how many frames after a press
    // the picture first shows it, found in a fight by running the next frames both ways from one state (nothing held,
    // and player 1's light punch held: the first frame whose picture is not the same), the least of the tries (a
    // fighter that cannot move answers nothing). Online, the match's input delay on top; run-ahead's frames off that.
    // The port's: from a key or button going down (the event's own time) to the frame that has it handed to the
    // screen, the window's to say (LagPressed, LagShown). What the screen itself adds is not known here.
    struct Lag {
        static constexpr int kFrames = 10, kTries = 3;
        int game = 0, tries = 0; // (0: not found yet)
        uint64_t nextAt = 0;
        bool wasOn = false;
        uint64_t pressedAt = 0; // ns; 0: no press waiting to be shown
        double portMs = 0;
        int presses = 0;
    } lag;
    void LagMeasure();
    void LagPressed(uint64_t ns);
    void LagShown(bool ran, uint64_t ns);
    std::string LagLine() const;
    void SoundTest();
    void MusicTest();
    uint64_t musicTestAt = 0, musicTestHeard = 0;
    size_t musicTestIndex = 0;
    uint64_t soundTestState = 0;
    uint16_t soundTestSong = 0;
    void Finish();
    uint32_t StateHash() const;
    bool SaveState(const std::string &path, std::string &error);
    bool LoadState(const std::string &path, std::string &error);
};

int RunWindowed(Port &port);
// The game was started by the collection's launcher (cps2recomp-launcher is beside it, and no launcher of its own)
bool InCollection();
// The optional features as the launcher lists them (--list-features)
std::string LauncherFeatureList();
// The language of the menu and the readouts (port/i18n.h), and every text the menu has (--list-text)
void UseLanguage();
std::string MenuTextList();
// The launcher's own folder (launcher.ini's), the collection's or the game's; empty if there is none
std::filesystem::path LauncherFolder();
#ifdef __ANDROID__
// Android (port/android.cpp): the game's folder and log, which its activity says, before anything else
void StartOnAndroid();
#endif

// The Display page's choices (the port's display_* and window_size features): how the picture is fitted to
// the window, and how many times the board's lines the window opens
Scaling DisplayScaling();
Shape DisplayShape();
// The size an SVG of the board's pixels is shown at (model/image_files.h, WriteSvg; issue #368): three times its rows
// tall, its pixels as the Display page shows them (on the board's 4:3 screen narrower than tall; or square)
inline void SvgSize(int columns, int rows, double &width, double &height) {
    height = rows * 3.0;
    width = columns * 3.0 * (DisplayShape() == Shape::SquarePixels ? 1.0 : (4.0 / 3.0) / (384.0 / 224.0));
}
int WindowSize();
// The Arcade monitor page's full screen resolution (frontend_sdl.cpp: the screen switched to the board's 384 x 224, or
// to a super resolution of its 224 lines), screen (0: the main one, 1..3: the others in the system's order) and Compact
// menu switch
enum class ArcadeMode { Off, Board, Super };
ArcadeMode ArcadeResolution();
// In an arcade mode, frames paced by the screen's refresh: unless the Arcade monitor page's Keep the board's pace is on
bool ArcadeSyncChosen();
// The Arcade monitor page's Make the mode at the board's rate (Switchres: recomp-common/arcade_screen.h), and its
// Monitor (an index of recomp::sdl::kMonitorPresets)
bool SwitchresChosen();
int ArcadeMonitor();
int DisplayScreen();
bool CompactMenuChosen();
// A vertical game's picture shown upright (Game::vertical, the Display page's "Picture turned"): the board's picture,
// `across` columns by its 224 rows, turned a quarter to the left into `out` (224 columns by `across` rows)
bool Turned();
void TurnPicture(const uint32_t *pixels, int across, std::vector<uint32_t> &out);

} // namespace port
