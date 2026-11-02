// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Street Fighter Alpha 2: the training mode. The session is the toolkit's (fight/session.h): reached from the menu (the
// board reset, the coins and Start buttons worked unseen, stopping at the character choice), a fight with no clock and
// no knock-outs, starting over from where the round began, and the aids of the toolkit's fight module (frame data and
// its meter, the input display, the combo readout, the dummy, hit boxes). This file says, from the game's memory, what
// those need to know, and adds the game's own: the stun and Custom Combo readouts, the throw range, the dummy played by
// the game or mashing out of stun, the hidden forms, and the stage and characters a mode wants.
#include "boxes.h"
#include "fight.h"
#include "modes.h"
#include "session.h"
#include "sfa2.h"
#include "hidden_selection.h"
#include "state.h"

#include <algorithm>
#include <cstdio>
#include <iterator>
#include <string>

namespace cps2recomp {

bool AttackInRange(M68K &c, uint32_t player); // guard.cpp
int WideSide();                               // widescreen.cpp
int WideStage();
bool GuardStanding(M68K &c, uint32_t player); // boxes.cpp

namespace {
    using namespace sfa2;

    // The stage: the players stand from one of these to the other, the picture's left edge goes from one of these
    // to the other; a round starts with the players at kStartX
    constexpr uint16_t kStageLeft = 280, kStageRight = 1000, kCameraLeft = 256, kCameraRight = 640;
    constexpr uint16_t kStartX[2] = {544, 736};
    constexpr uint16_t kCornerApart = 100;
    constexpr uint32_t kCameraCopies[] = {At(0xFF8026), At(0xFF82A0), At(0xFF82A8), At(0xFF8290)};
    constexpr uint32_t kCameraX = At(0xFF8290), kCameraY = At(0xFF8294);
    constexpr int kPictureFloor = 239; // the picture's line that height 0 is on, the camera at the bottom (a player on the floor is at kGround, 40: its feet on line 199, measured)
    constexpr uint32_t kPlayersChosen = At(0xFF80AC);

    // A round's fighting, for the aids: both players in it and nobody over the most health
    bool Fighting() {
        if (!RoundFought()) {
            return false;
        }
        for (int n = 0; n < 2; n++) {
            if (static_cast<int16_t>(Ram16(Player(n) + kHealth)) > kHealthFull || Ram8(Player(n) + kState) != 2) {
                return false;
            }
        }
        return true;
    }

    // 00797C: the game has chosen the stage for a match (D0, from the character of one of the players: kStageOf). In
    // a training session with a stage asked for, that one goes in its place.
    bool ChooseStage(M68K &c) {
        const int stage = session::WantedStage();
        if (stage >= 1 && stage <= kStages) {
            SetLow8(c.d[0], static_cast<uint8_t>(stage - 1));
        }
        return false;
    }

    // The hidden characters: the other forms the board has of the characters on the screen they are chosen from, as
    // the "Hidden characters" feature gives them for one who chooses the first form (family.h)
    using family::kHiddenForms;
    bool HiddenFormFor(int n) {
        const int who = FeatureValue("hidden_forms"); // 1 player 1, 2 player 2, 3 both
        return (who & (1 << n)) != 0;
    }

    // A character with a hidden form, chosen by player n: that form instead, if asked for
    void HiddenForm(int n) {
        if (!HiddenFormFor(n)) {
            return;
        }
        for (const auto &form : kHiddenForms) {
            if (Ram8(Player(n) + kCharacter) == form.first) {
                SetRam8(Player(n) + kCharacter, form.other);
                return;
            }
        }
    }

    // Reuse the selector's secret-code flag, already cleared on entry and when the
    // cursor moves. It lives in native RAM, so save states keep the selected variant.
    // The stock preview has no safe graphics for every hidden form: keep the base
    // character until confirmation and show the choice in the HUD instead.
    int g_hiddenShown[2] = {-1, -1}; // display cache, refreshed from the selector every frame
    bool SkipHoldStartCode(M68K &c) {
        const uint32_t record = c.a[4] & 0xFFFFFF;
        const int n = record == Player(0) ? 0 : record == Player(1) ? 1 : -1;
        if (n >= 0 && HiddenFormFor(n)) { c.pc = 0x062852; return true; }
        return false;
    }
    bool ToggleHiddenChoice(M68K &c) {
        const uint32_t record = c.a[4] & 0xFFFFFF, selector = c.a[6] & 0xFFFFFF;
        const int n = record == Player(0) ? 0 : record == Player(1) ? 1 : -1;
        if (n >= 0) {
            g_hiddenShown[n] = -1;
            const int base = Ram8(record + kCharacter);
            const int other = inputlab::ToggleAlpha2(base);
            if (HiddenFormFor(n) && other != base) {
                if (c.d[0] & 0x80) SetRam8(selector + 0x67, Ram8(selector + 0x67) ^ 0x80);
                if (Ram8(selector + 0x67) & 0x80) g_hiddenShown[n] = other;
            }
        }
        return false;
    }
    bool KeepToggledChoice(M68K &c) {
        const uint32_t record = c.a[4] & 0xFFFFFF;
        const int n = record == Player(0) ? 0 : record == Player(1) ? 1 : -1;
        if (n >= 0 && HiddenFormFor(n)) {
            if (Ram8((c.a[6] & 0xFFFFFF) + 0x67) & 0x80) {
                SetRam8(record + kCharacter, static_cast<uint8_t>(inputlab::ToggleAlpha2(Ram8(record + kCharacter))));
            }
            c.pc = 0x0628D8;
            return true;
        }
        return false;
    }

    bool CharacterChosen(M68K &c) {
        const uint32_t record = c.a[4] & 0xFFFFFF; // (the address bus's 24 bits: A4 may hold FFFF8400)
        const int n = record == (Player(0) & 0xFFFFFF) ? 0 : record == (Player(1) & 0xFFFFFF) ? 1 : -1;
        if (n >= 0) {
            HiddenForm(n);
        }
        return false;
    }

    // 007914: the game is about to choose the stage, from the characters chosen. The characters a mode wants go in
    // their place first (two alike are told apart by the game's own colours), and their colours (kColourChoice: the
    // round works out the set from it); then, if asked for, a character with a hidden form is that form (in the
    // modes too: the endless battle's fighters).
    bool ChooseCharacters(M68K &) {
        for (int n = 0; n < 2; n++) {
            if (Wanted().characters[n] >= 0) {
                SetRam8(Player(n) + kCharacter, static_cast<uint8_t>(Wanted().characters[n]));
                HiddenForm(n); // modes choose their fighters without the character screen
            }
            if (Wanted().colours[n] >= 0) {
                SetRam8(Player(n) + kColourChoice, static_cast<uint8_t>(Wanted().colours[n]));
            }
        }
        return false;
    }

    // --- a step of the game: what each player is doing (for frame data) ---

    bool PlayerStep(M68K &c) {
        const uint32_t player = c.a[6] & 0xFFFFFF;
        const int n = player == Player(0) ? 0 : player == Player(1) ? 1 : -1;
        if (n < 0 || Ram8(player + kState) != 2 || !RoundFought()) {
            return false;
        }
        fight::StepView view;
        const uint8_t control = Ram8(player + kControl);
        view.control = control == kControlOwn ? fight::StepView::Own : control == kControlReeling ? fight::StepView::Reeling : fight::StepView::Held;
        if (view.control == fight::StepView::Reeling) {
            session::Contact(n, Ram8(player + kGuardedHit) == 0xFF);
        } else if (view.control == fight::StepView::Held) {
            session::Contact(n, false);
        }
        view.canAct = CanAct(n);
        const uint32_t frame = Ram32(player + kFrame);
        view.frozen = Ram8(player + kHitStop) != 0;
        view.attacking = Ram8(player + kAction) == kActionAttack || Ram8(player + kAttackOut) != 0;
        view.jumping = Ram8(player + kAction) == kActionJump;
        view.attackOut = frame < 0x400000 && Mem8(c, frame + kFrameAttack) != 0 && !(Mem8(c, frame + kFrameHarmless) & 0x80);
        session::Step(n, view); // (and the frame data log: MoveStart)
        return false;
    }

    // What the game read off the panel for a player this frame, as Button bits
    uint16_t PanelHeld(int player) {
        const uint16_t word = Ram16(0xFF8000 + static_cast<uint32_t>(kControls[player]));
        uint16_t held = word & 0x0F;
        held |= static_cast<uint16_t>(((word >> 8) & 0x07) << 4);  // punches
        held |= static_cast<uint16_t>(((word >> 12) & 0x07) << 7); // kicks
        return held;
    }

    fight::MoveLog::Start MoveStart(int n) {
        const uint32_t player = Player(n);
        const uint8_t who = Ram8(player + kCharacter);
        fight::MoveLog::Start start;
        start.character = who < kCharacters ? kCharacterNames[who] : "?";
        start.input = fight::Notation(PanelHeld(n), Ram8(player + kFacesRight) == 0);
        start.air = Ram8(player + kAction) == kActionJump || Ram16(player + kY) != kGround;
        start.id = Ram32(player + kFrame); // (the move's first animation frame)
        return start;
    }

    // A Custom Combo going on: its time left, as a bar of text under the player's own bar
    std::string CustomComboLine(int n) {
        const int left = Ram8(Player(n) + kCustomCombo);
        if (!left) {
            return {};
        }
        constexpr int kFull = 0xB0, kCells = 12;
        std::string bar = "CUSTOM COMBO ";
        const int cells = std::clamp((left * kCells + kFull - 1) / kFull, 0, kCells);
        bar.append(static_cast<size_t>(cells), '|');
        return bar;
    }

    // The stun gauge as a line under the combo readout: how far to being stunned
    std::string StunLine(int n) {
        const uint8_t stun = Ram8(Player(n) + kStun), most = Ram8(Player(n) + kStunMost);
        if (!stun || !most) {
            return {};
        }
        char text[48];
        snprintf(text, sizeof(text), "stun %u / %u", stun, most);
        return text;
    }

    // The throw's reach, as the game tests it (0260E8): a point 20 in front of the thrower must be within 20 of the
    // near edge of the other player's push box. Drawn as that zone in front of each player on the floor, and the
    // edge it is tested against as a line.
    constexpr int kThrowAhead = 20, kThrowReach = 20, kThrowZoneHeight = 0x48;
    void ThrowRanges(Machine &machine, std::vector<fight::StageBox> &boxes) {
        for (int n = 0; n < 2; n++) {
            const uint32_t player = Player(n), other = Player(1 - n);
            if (Ram16(player + kY) != kGround || !CanAct(n)) {
                continue;
            }
            const bool right = Ram8(player + kFacesRight) != 0;
            const int point = static_cast<int16_t>(Ram16(player + kX)) + (right ? kThrowAhead : -kThrowAhead);
            boxes.push_back({fight::BoxKind::ThrowRange, point, kThrowZoneHeight / 2, kThrowReach, kThrowZoneHeight / 2, false});
            // (the other's push box's edge: its offset less its half width, turned about as the box is)
            const uint32_t frame = Ram32(other + kFrame), table = Ram32(other + kPushBoxes);
            if (frame >= 0x400000 || table >= 0x400000) {
                continue;
            }
            const int8_t number = static_cast<int8_t>(machine.Read8(frame + kFramePush));
            if (number <= 0) {
                continue;
            }
            const uint32_t box = table + static_cast<uint32_t>(number) * 8;
            int edge = static_cast<int16_t>(machine.Read16(box)) - static_cast<int16_t>(machine.Read16(box + kBoxHalfWidth));
            if (Ram8(other + kFacesRight)) {
                edge = -edge;
            }
            edge += static_cast<int16_t>(Ram16(other + kX));
            boxes.push_back({fight::BoxKind::Throw, edge, kThrowZoneHeight / 2, 1, kThrowZoneHeight / 2, false});
        }
    }

    // The game's own aid: the super combo gauge kept full (in a fight, and not while a Custom Combo goes on: the gauge
    // is spent as it begins, and filled again once it is over)
    void Hud(Machine &, std::vector<HudLine> &, std::vector<HudBox> &) {
        for (int n = 0; n < 2; n++) {
            const uint32_t player = Player(n);
            if (FeatureOn("gauge_refill") && Ram8(player + kState) == 2 && !Ram8(player + kCustomCombo) &&
                Ram16(player + kSpecialGauge) < kSpecialGaugeFull) {
                SetRam16(player + kSpecialGauge, kSpecialGaugeFull);
            }
        }
    }

    // Under each player's bar: the Custom Combo's time, the combo readout and the stun line (or the stun line alone)
    void PlayerReadout(int n, const fight::Combos &combos, std::vector<HudLine> &lines) {
        if (FeatureOn("custom_combo_readout")) {
            const std::string cc = CustomComboLine(n);
            if (!cc.empty()) {
                lines.push_back({n == 0 ? 0.03f : 0.97f, 0.11f, n == 0 ? -1 : 1, cc});
            }
        }
        const bool readout = FeatureOn("damage_readout"), stun = FeatureOn("stun_readout");
        if (readout || stun) {
            const std::string extra = stun ? StunLine(n) : std::string();
            if (readout) {
                combos.Hud(n, lines, 0.17f, true, extra);
            } else if (!extra.empty()) {
                lines.push_back({n == 0 ? 0.03f : 0.97f, 0.17f, n == 0 ? -1 : 1, extra});
            }
        }
    }

    // The boxes the game tests, and the throw ranges if asked for
    std::vector<fight::StageBox> Boxes(Machine &machine) {
        std::vector<fight::StageBox> stage = AllBoxes(machine);
        if (FeatureOn("throw_range")) {
            ThrowRanges(machine, stage);
        }
        return stage;
    }

    // The dummy: the game knows when it is knocked down; the game's own computer player plays it if asked for
    bool g_computerWas = false;
    void DummyView(Machine &, fight::DummyView &view) {
        view.knowsDown = true;
        const bool computer = FeatureOn("dummy_computer") && view.fighting;
        if (computer || g_computerWas) {
            SetRam8(Player(1) + kComputer, computer ? 1 : 0);
        }
        g_computerWas = computer;
        view.down = Ram8(Player(1) + kControl) == kControlReeling && Ram8(Player(1) + kKnocksDown) != 0;
    }
    // Stunned, the dummy mashes: a new direction and a new button every frame (the stun's count goes down a step more
    // for each, player.cpp's Stunned)
    void AfterDummy(Machine &machine, const fight::DummyView &view) {
        const bool computer = FeatureOn("dummy_computer") && view.fighting;
        if (FeatureOn("dummy_stun_mash") && view.fighting && !computer && Ram8(Player(1) + kControl) == kControlOwn &&
            Ram8(Player(1) + kAction) == kActionStunned) {
            static bool other = false;
            other = !other;
            machine.players[1] = other ? static_cast<uint16_t>(kLeft | kPunch1) : static_cast<uint16_t>(kRight | kKick1);
        }
    }
} // namespace

// A hit's damage is being taken (fight.cpp)
void TrainingHit(int player, unsigned damage, bool chip) {
    session::Hit(player, damage, 0, chip);
}
void TrainingDamageTaken(int) {}

// A training session is on, or the stage viewer: no clock and no knock-outs (fight.cpp, damage.cpp)
bool TrainingMode() {
    return session::NoKnockOut();
}

// "Training mode" on the menu's Alternative modes page (modes.cpp)
void StartTraining(Machine &machine) {
    session::StartSession(machine);
}

void TrainingBeforeFrame(Machine &machine) {
    session::BeforeFrame(machine);
}
void TrainingFrame(Machine &machine) {
    session::Frame(machine);
#ifdef INPUTLAB_SFA2
    if (Ram16(kMode) == 2 && Ram16(kGameStep) == kGameChoosing) {
        std::vector<HudLine> choices;
        for (int n = 0; n < 2; ++n) {
            if (!HiddenFormFor(n) || g_hiddenShown[n] < 0) continue;
            const int hidden = g_hiddenShown[n];
            const char *name = hidden == 21 ? "Zangief (classic)" : hidden == 22 ? "Dhalsim (classic)" : kCharacterNames[hidden];
            choices.push_back({n == 0 ? 0.03f : 0.97f, 0.90f, n == 0 ? -1 : 1, name});
        }
        SetHud(std::move(choices));
    }
#endif
}
void TrainingState(Archive &ar) {
    session::State(ar);
}

// The character a player's side is to be for `character`: its hidden form, if the menu asks for that side's (the
// team battle's tag style, a fighter tagged in: livetag.cpp)
int HiddenFormOf(int n, int character) {
    if (HiddenFormFor(n)) {
        for (const auto &form : kHiddenForms) {
            if (character == form.first) {
                return form.other;
            }
        }
    }
    return character;
}

void RegisterTraining() {
    Game &game = TheGame();
    AddHook(kPlayerDispatch, "PlayerStep", PlayerStep);
    AddHook(At(0x00797C), "ChooseStage", ChooseStage);
    AddHook(At(0x007914), "ChooseCharacters", ChooseCharacters);
#ifdef INPUTLAB_SFA2
    AddHook(0x0627F6, "SkipHoldStartCode", SkipHoldStartCode);
    AddHook(0x062894, "ToggleHiddenChoice", ToggleHiddenChoice);
    AddHook(0x0628A6, "KeepToggledChoice", KeepToggledChoice);
#else
    AddHook(At(0x0628D8), "CharacterChosen", CharacterChosen);
#endif
    game.features.push_back({"hidden_forms", "Cheats", "Hidden characters",
#ifdef INPUTLAB_SFA2
        "Highlight Ryu or Akuma and press START to toggle Evil Ryu or Shin Akuma. Highlight Chun-Li, Dhalsim or Zangief and press START to toggle their classic forms. Confirm with a punch or kick for the colours. Shin Akuma has the same palette for both. For player 1, player 2 or both.",
#else
        family::kHiddenWords,
#endif
        kFeatureOwn, {"Off", "Player 1", "Player 2", "Both"}});
    session::Description d;
    d.player1 = kPlayer1;
    d.playerSize = kPlayerSize;
    d.x = kX;
    d.health = kHealth;
    d.healthFull = kHealthFull;
    d.healthShown = kHealthShown;
    d.clock = kClock;
    d.clockFull = kClockFull;
    d.cameraX = kCameraX;
    d.cameraY = kCameraY;
    d.cameraCopies.assign(std::begin(kCameraCopies), std::end(kCameraCopies));
    d.startX[0] = kStartX[0];
    d.startX[1] = kStartX[1];
    d.stageLeft = kStageLeft;
    d.stageRight = kStageRight;
    d.cameraLeft = kCameraLeft;
    d.cameraRight = kCameraRight;
    d.cornerApart = kCornerApart;
    d.pictureFloor = kPictureFloor;
    d.fighting = Fighting;
    d.mayRefill = CanAct;
    d.chosen = [](int n) { return (Ram8(kPlayersChosen) & (1 << n)) != 0; };
    d.panelHeld = PanelHeld;
    d.facingLeft = [](int n) { return Ram8(Player(n) + kFacesRight) == 0; };
    d.moveStart = MoveStart;
    d.boxes = Boxes;
    d.guardView = [](Machine &machine, bool &inRange, bool &standing) {
        inRange = AttackInRange(machine.cpu, Player(1));
        standing = inRange && GuardStanding(machine.cpu, Player(1));
    };
    d.playerReadout = PlayerReadout;
    d.inputY = 0.36f;
    d.inputSpacing = 0.052f;
    d.framesY = 0.735f;
    d.framesTop = 190;
    d.dummyView = DummyView;
    d.afterDummy = AfterDummy;
    d.dummyAfterAfresh = true;
    d.framesFirstInState = true;
    d.wideStage = WideStage;
    d.wideSide = WideSide;
    d.stageChoices = modes::StageChoices({"The game's choice"});
    d.stageWords = "Where a training session's fight is: the game's own choice (the stage of one of the two characters) or a stage "
                   "named here. Takes effect when the characters are next chosen.";
    d.inputWords = "What each player presses and for how many frames: the stick as a keypad number from the player's side (6 forward, "
                   "4 back, 2 down), then the buttons.";
    d.framesPrintVariable = "SFA2_FRAMES";
    d.hud = Hud;
    d.readoutFeatures = {{"stun_readout", "Training mode", "Stun readout",
                          "Under each bar: how much stun the player has taken and how much stuns them (it drains when they are left "
                          "alone for a while)."},
                         {"custom_combo_readout", "Training mode", "Custom Combo time",
                          "While a player's Custom Combo goes on, how much of its time is left, under that player's bar."}};
    d.boxFeatures = {{"throw_range", "Training mode", "Throw range",
                      "With hit boxes on: the zone in front of each player that the other's push box edge (the yellow line) has to be "
                      "in for a throw, as the game tests it (both on the floor and free to act)."}};
    fight::DummyOptions &dummy = d.dummy;
    // The Alpha counter: while guarding, back, down-back, down with a punch (or a kick)
    dummy.guardActions.push_back({"Alpha counter, punch", [](int frame, uint16_t, uint16_t away) -> uint16_t {
                                      static constexpr int kEach = 2;
                                      const int at = (frame / kEach) % 4;
                                      return at == 0 ? away : at == 1 ? static_cast<uint16_t>(away | kDown) : at == 2 ? static_cast<uint16_t>(kDown | kPunch2) : 0;
                                  }});
    dummy.guardActions.push_back({"Alpha counter, kick", [](int frame, uint16_t, uint16_t away) -> uint16_t {
                                      static constexpr int kEach = 2;
                                      const int at = (frame / kEach) % 4;
                                      return at == 0 ? away : at == 1 ? static_cast<uint16_t>(away | kDown) : at == 2 ? static_cast<uint16_t>(kDown | kKick2) : 0;
                                  }});
    dummy.guardActionsWords = "What player 2 does while guarding a hit: an Alpha counter (back, down-back, down with a punch or a kick; "
                              "it costs a level of the super combo gauge, so pair it with \"Gauge refills\").";
    dummy.heldAction = [](int frame, uint16_t, uint16_t away) -> uint16_t {
        return static_cast<uint16_t>(away | ((frame & 1) ? (kPunch2 | kPunch3) : 0));
    };
    dummy.heldActionTitle = "Softens throws";
    dummy.heldActionWords = "Player 2 does a throw's own command while being thrown (a \"tech hit\", which takes less damage).";
    // Getting up quickly from a knock-down ("quick rise": down, down-back, back with a punch, as it lands; found by
    // trying: a sweep's knock-down took 74 frames, 43 with this)
    dummy.getUp.push_back({"Quick rise", [](int frame, uint16_t, uint16_t away) -> uint16_t {
                               const int at = (frame / 2) % 4;
                               return at == 0 ? kDown : at == 1 ? static_cast<uint16_t>(kDown | away) : at == 2 ? away : static_cast<uint16_t>(away | kPunch1);
                           }});
    session::Register(d);
    // (after the session's own, so it follows "Health refills"; a training aid, as the session's are)
    game.features.push_back({"gauge_refill", modes::kTraining, "Gauge refills",
                             "Both players' super combo gauge kept full (three levels), filled again after a Custom Combo."});
    TrainingAids({"gauge_refill"});
    game.features.push_back({"dummy_stun_mash", "Dummy", "Mashes out of stun",
                             "Stunned, the dummy works the stick and the buttons as fast as it can, as a player does to get out sooner."});
    game.features.push_back({"dummy_computer", "Dummy", "Played by the game",
                             "Player 2 is the game's own computer player, as in a one-player game (at the board's difficulty "
                             "setting).",
                             kFeatureExclusive});
}

} // namespace cps2recomp
