// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Street Fighter Alpha 3: the training mode. The session is the toolkit's (fight/session.h): reached from the menu (the
// board reset, the coins and Start buttons worked unseen, stopping at the character choice), a fight with no clock and
// no knock-outs, starting over from where the round began, and the aids of the toolkit's fight module (frame data and
// its meter, the input display, the combo readout, the dummy, hit boxes). This file says, from the game's memory, what
// those need to know, and adds the game's own: the stun and custom combo readouts, the throw range, fixed damage, the
// dummy played by the game or mashing out of stun, its Alpha counters, throw techs and quick rise.
#include "fight.h"
#include "alternative.h"
#include "session.h"
#include "sfa3.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <string>

namespace cps2recomp {

void CourtStep(uint32_t player); // court.cpp
bool CourtNoThrow(M68K &c);       // court.cpp

bool RingOutStep(M68K &c);                    // battle.cpp
bool AttackInRange(M68K &c, uint32_t player); // guard.cpp
bool GuardStanding(M68K &c, uint32_t player);

namespace {
    using namespace sfa3;

    constexpr int kPictureFloor = 239; // the picture's line that height 0 is on, the camera at the bottom (as Alpha 2's)

    int PlayerOf(uint32_t record) {
        record &= 0xFFFFFF;
        return record == Player(0) ? 0 : record == Player(1) ? 1 : -1;
    }

    // A round's fighting, for the aids: both players in it and nobody over the most health
    bool Fighting() {
        if (!modes::RoundFought()) {
            return false;
        }
        for (int n = 0; n < 2; n++) {
            if (static_cast<int16_t>(Ram16(Player(n) + kHealth)) > kHealthFull || Ram8(Player(n) + kState) != 2) {
                return false;
            }
        }
        return true;
    }

    // 027CBA: a fighting player's step, where it goes on to whatever is in charge of it (027C94, for a person's player
    // and the computer's alike); once a step for each player. Its record is as the last step left it, which is what the
    // picture showed and the hit tests tested.
    bool PlayerStep(M68K &c) {
        if (RingOutStep(c)) { // (the ring out's own movements: battle.cpp)
            Return(c);
            return true;
        }
        CourtStep(c.a[6] & 0xFFFFFF); // (what the court's ball has done to it: court.cpp)
        const uint32_t player = c.a[6] & 0xFFFFFF;
        const int n = PlayerOf(player);
        if (n < 0 || Ram8(player + kState) != 2 || !modes::RoundFought()) {
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
        view.canAct = CanAct(player);
        view.frozen = Ram8(player + kHitStop) != 0;
        view.attacking = Ram8(player + kAction) == kActionAttack;
        view.jumping = Ram8(player + kAction) == kActionJump;
        const uint32_t frame = Ram32(player + kFrame);
        view.attackOut = frame < 0x400000 && Mem8(c, frame + kFrameAttack) != 0;
        session::Step(n, view); // (and the frame data log: MoveStart)
        return false;
    }

    // What the game read off the panel for a player this frame, as Button bits
    uint16_t PanelHeld(int n) {
        const uint16_t word = Ram16(kControls[n]);
        uint16_t held = word & 0x0F;
        held |= static_cast<uint16_t>(((word >> 8) & 0x07) << 4);  // punches
        held |= static_cast<uint16_t>(((word >> 12) & 0x07) << 7); // kicks
        return held;
    }

    fight::MoveLog::Start MoveStart(int n) {
        const uint32_t player = Player(n);
        const uint8_t who = Ram8(player + kCharacter);
        fight::MoveLog::Start start;
        start.character = who < kCharacters && kCharacterNames[who] ? kCharacterNames[who] : "?";
        start.input = fight::Notation(PanelHeld(n), Ram8(player + kFacesRight) == 0);
        start.air = Ram8(player + kAction) == kActionJump || Ram16(player + kY) != kGround;
        start.id = Ram32(player + kFrame); // (the move's first animation frame)
        return start;
    }

    // A box of an object's (eight bytes: its middle's offsets across and up, half its width and height), placed in the
    // stage as the boxes' test (0246FC) places it
    void AddBox(M68K &c, std::vector<fight::StageBox> &boxes, fight::BoxKind kind, uint32_t object, uint32_t box, bool thrown = false) {
        const int across = static_cast<int16_t>(Mem16(c, box + kBoxX)), up = static_cast<int16_t>(Mem16(c, box + kBoxY));
        const int halfWidth = Mem16(c, box + kBoxHalfWidth), halfHeight = Mem16(c, box + kBoxHalfHeight);
        if (halfWidth == 0 || halfHeight == 0) {
            return;
        }
        const int x = static_cast<int16_t>(Ram16(object + kX)) + (Ram8(object + kFacesRight) ? -across : across);
        const int y = static_cast<int16_t>(Ram16(object + kY)) + up;
        boxes.push_back({kind, x, y, halfWidth, halfHeight, thrown, fight::Whose(object, Player(0), Player(1))});
    }

    // An object's boxes, as the hit test (022F86) reads them: those it can be hit in and its push box by the record's
    // numbers, its attack by its frame's
    void ObjectBoxes(M68K &c, std::vector<fight::StageBox> &boxes, uint32_t object, bool thrown) {
        constexpr uint32_t kTables[3] = {kHeadBoxes, kBodyBoxes, kLegBoxes};
        if (!thrown) {
            if (const uint8_t push = Ram8(object + kBoxNumbers + 3)) {
                AddBox(c, boxes, fight::BoxKind::Push, object, Ram32(object + kPushBoxes) + push * kBoxSize);
            }
        }
        for (int part = 0; part < 3; part++) {
            if (const uint8_t number = Ram8(object + kBoxNumbers + static_cast<uint32_t>(part))) {
                AddBox(c, boxes, fight::BoxKind::Hurt, object, Ram32(object + kTables[part]) + number * kBoxSize, thrown);
            }
        }
        const uint32_t frame = Ram32(object + kFrame);
        if (frame < 0x400000) {
            if (const uint8_t attack = Mem8(c, frame + kFrameAttack)) {
                AddBox(c, boxes, fight::BoxKind::Attack, object, Ram32(object + kAttackBoxes) + attack * kAttackBoxSize, thrown);
            }
        }
    }

    // The throw's reach. The throw test (02E3FE) takes a box of the thrower's attack table (D0, its number: each
    // character's own code gives its throw's, Ryu's 4D) and tests it against the other's push box as the hit test
    // tests boxes (024936; 4 wider against a player in the X-ism). The box a character's throw was last tested with is
    // kept, by character, and drawn in front of each player who could throw (on the floor and free to act).
    uint8_t g_throwBox[kCharacters] = {};
    bool ThrowTested(M68K &c) {
        if (CourtNoThrow(c)) { // (nobody is thrown on the court: court.cpp)
            return true;
        }
        const uint32_t player = c.a[6] & 0xFFFFFF;
        if (PlayerOf(player) >= 0) {
            const uint8_t who = Ram8(player + kCharacter);
            if (who < kCharacters) {
                g_throwBox[who] = static_cast<uint8_t>(c.d[0]);
            }
        }
        return false;
    }
    void ThrowRanges(Machine &machine, std::vector<fight::StageBox> &boxes) {
        M68K &c = machine.cpu;
        for (int n = 0; n < 2; n++) {
            const uint32_t player = Player(n);
            const uint8_t who = Ram8(player + kCharacter);
            if (who >= kCharacters || !g_throwBox[who] || Ram8(player + kInAir) || !CanAct(player)) {
                continue;
            }
            const uint32_t box = Ram32(player + kAttackBoxes) + g_throwBox[who] * kAttackBoxSize;
            const int across = static_cast<int16_t>(Mem16(c, box + kBoxX)), up = static_cast<int16_t>(Mem16(c, box + kBoxY));
            const int x = static_cast<int16_t>(Ram16(player + kX)) + (Ram8(player + kFacesRight) ? -across : across);
            boxes.push_back({fight::BoxKind::ThrowRange, x, static_cast<int16_t>(Ram16(player + kY)) + up, Mem16(c, box + kBoxHalfWidth),
                             Mem16(c, box + kBoxHalfHeight), false, static_cast<int8_t>(n)});
        }
    }

    std::vector<fight::StageBox> Boxes(Machine &machine) {
        M68K &c = machine.cpu;
        std::vector<fight::StageBox> boxes;
        for (int n = 0; n < 2; n++) {
            const uint32_t player = Player(n);
            if (Ram8(player + kState) == 2 && Ram32(player + kFrame) < 0x400000) {
                ObjectBoxes(c, boxes, player, false);
            }
        }
        // (what the players throw: the two pools the guard-range test goes through, records laid out as a player's)
        for (const uint32_t pool : {kThrown, kThrownOther}) {
            for (int n = 0; n < kThrownRecords; n++) {
                const uint32_t thrown = pool + static_cast<uint32_t>(n) * kThrownSize;
                if (Ram8(thrown) == 1 && Ram8(thrown + kState) == 2 && Ram32(thrown + kFrame) < 0x400000 && Ram32(thrown + kAttackBoxes) < 0x400000) {
                    ObjectBoxes(c, boxes, thrown, true);
                }
            }
        }
        if (FeatureOn("throw_range")) {
            ThrowRanges(machine, boxes);
        }
        return boxes;
    }

    // The V-ism's custom combo going on: its time left, as a bar of text under the player's own bar
    std::string CustomComboLine(int n) {
        if (!Ram8(Player(n) + kCustomCombo)) {
            return {};
        }
        const int left = Ram16(Player(n) + kCustomComboTime);
        constexpr int kFull = 0x90, kCells = 12;
        std::string bar = "CUSTOM COMBO ";
        bar.append(static_cast<size_t>(std::clamp((left * kCells + kFull - 1) / kFull, 0, kCells)), '|');
        return bar;
    }

    // The stun as a line under the combo readout: how far to being stunned
    std::string StunLine(int n) {
        const uint8_t stun = Ram8(Player(n) + kStun), most = Ram8(Player(n) + kStunMost);
        if (!stun || !most) {
            return {};
        }
        char text[48];
        snprintf(text, sizeof(text), "stun %u / %u", stun, most);
        return text;
    }

    // The largest guard gauge seen for each character and ism: the size a round starts with, before a crush or an Alpha
    // counter made it smaller (02BE1A sets it by the character; the ism changes it)
    uint8_t g_guardSize[kCharacters][4] = {};

    // The game's own aids: the super combo gauge kept full (in a fight, and not while a V-ism custom combo goes on: its
    // time runs down with the gauge, and it is filled again once the combo is over); the guard gauge kept full, at the
    // size the round began with
    void Hud(Machine &, std::vector<HudLine> &, std::vector<HudBox> &) {
        for (int n = 0; n < 2; n++) {
            const uint32_t player = Player(n);
            if (FeatureOn("gauge_refill") && Ram8(player + kState) == 2 && !Ram8(player + kCustomCombo) &&
                Ram16(player + kSpecialGauge) < kSpecialGaugeFull) {
                SetRam16(player + kSpecialGauge, kSpecialGaugeFull);
            }
            const uint8_t who = Ram8(player + kCharacter), ism = Ram8(player + kIsm) & 3;
            if (Ram8(player + kState) != 2 || who >= kCharacters) {
                continue;
            }
            uint8_t &size = g_guardSize[who][ism];
            size = std::max(size, Ram8(player + kGuardSize));
            if (FeatureOn("guard_refill")) {
                SetRam8(player + kGuardTaken, 0);
                if (Ram8(player + kGuardSize) < size) {
                    SetRam8(player + kGuardSize, size);
                }
            }
        }
    }

    // Under each player's bar: the custom combo's time, the combo readout and the stun line (or the stun line alone)
    void PlayerReadout(int n, const fight::Combos &combos, std::vector<HudLine> &lines) {
        if (FeatureOn("custom_combo_readout")) {
            const std::string cc = CustomComboLine(n);
            if (!cc.empty()) {
                lines.push_back({n == 0 ? 0.03f : 0.97f, 0.13f, n == 0 ? -1 : 1, cc});
            }
        }
        const bool readout = FeatureOn("damage_readout"), stun = FeatureOn("stun_readout");
        if (readout || stun) {
            const std::string extra = stun ? StunLine(n) : std::string();
            if (readout) {
                combos.Hud(n, lines, 0.19f, true, extra);
            } else if (!extra.empty()) {
                lines.push_back({n == 0 ? 0.03f : 0.97f, 0.19f, n == 0 ? -1 : 1, extra});
            }
        }
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
        // (knocked down, or hit in the air and falling: both go through 02020202, where the quick rise is taken)
        const uint8_t kind = Ram8(Player(1) + kReelKind);
        view.down = Ram8(Player(1) + kControl) == kControlReeling && (kind == kReelKnockedDown || kind == kReelInAir);
        // (the reels 02B330 lets a player recover from in the air: kind 6, and a knock-down while it flies, step 2)
        view.juggled = Ram8(Player(1) + kControl) == kControlReeling &&
                       (kind == kReelJuggled || (kind == kReelKnockedDown && Ram8(Player(1) + kActionStep) <= 2)) &&
                       static_cast<int16_t>(Ram16(Player(1) + kY)) > static_cast<int16_t>(kGround);
    }
    // Recovering in the air: two punches every other frame (new presses), the stick held the way chosen
    uint16_t AirRecovery(int frame, uint16_t way) {
        return static_cast<uint16_t>(way | ((frame & 1) ? 0 : (kPunch1 | kPunch2)));
    }
    // Stunned, the dummy mashes: a new direction and a new button every frame (the stun, reel kind 0C, counts its time
    // down at +3A by what 02B56A counts of the stick and buttons as well as by one a step: 0299FE)
    void AfterDummy(Machine &machine, const fight::DummyView &view) {
        const bool computer = FeatureOn("dummy_computer") && view.fighting;
        if (FeatureOn("dummy_stun_mash") && view.fighting && !computer && Ram8(Player(1) + kControl) == kControlReeling &&
            Ram8(Player(1) + kReelKind) == kReelStunned) {
            static bool other = false;
            other = !other;
            machine.players[1] = other ? static_cast<uint16_t>(kLeft | kPunch1) : static_cast<uint16_t>(kRight | kKick1);
        }
    }

    // Counter hits. A hit that catches a player in an attack (its +A9 set, +C7 not negative) is a counter hit: 023DD2
    // marks it (+26C, which the reeling and the damage go by: an eighth more, 0240C6; and the word at A5-5CBE, the
    // hit spark's), and 0244A8 then takes the attacker's damage modifier (+294) in place of the one hit's.
    // With "Counter hits" on, every hit on player 2 (the dummy) takes the counter hit's way, whatever it was doing.
    constexpr uint32_t kAttacking = 0xA9, kNoCounter = 0xC7, kCounterHit = 0x26C, kDamageModifier = 0x294;
    constexpr uint32_t kCounterTest = 0x023DD2, kDamageModifiers = 0x0244A8;
    constexpr int32_t kCounterSpark = -0x5CBE; // from A5

    bool CounterForced(M68K &c) {
        return FeatureOn("dummy_counter_hit") && PlayerOf(c.a[1]) == 1;
    }

    // 023DD2: is the hit on A1 a counter hit (D0 1, not zero; marked), or not (D0 0)
    bool CounterTest(M68K &c) {
        if (!CounterForced(c)) {
            return false;
        }
        SetRam8((c.a[1] & 0xFFFFFF) + kCounterHit, 1);
        SetRam16((c.a[5] + kCounterSpark) & 0xFFFFFF, 1);
        c.d[0] = 1;
        c.flagZ = false;
        c.flagN = false;
        Return(c);
        return true;
    }

    // 0244A8: the damage D2 (a byte) by a damage modifier, none for an attack by a thrown object (A6's +8): the
    // attacker's, A6, on a counter hit when it has one, or else the one hit's, A1. A modifier is 1 (twice the damage),
    // 2 (a quarter more) or 3 (a half more), by the table at 0244E2.
    bool DamageModifiers(M68K &c) {
        if (!CounterForced(c)) {
            return false;
        }
        c.d[0] = 0;
        uint16_t damage = static_cast<uint16_t>(c.d[2] & 0xFF);
        if (Mem8(c, (c.a[6] & 0xFFFFFF) + 8) == 0) {
            uint8_t modifier = Mem8(c, (c.a[6] & 0xFFFFFF) + kDamageModifier);
            if (modifier == 0) {
                modifier = Ram8((c.a[1] & 0xFFFFFF) + kDamageModifier);
            }
            if (modifier > 3) {
                return false; // (not one the table has: the game's own way)
            }
            if (modifier == 1) {
                damage = static_cast<uint16_t>(damage << 1);
                c.d[0] = 0x0A;
            } else if (modifier >= 2) {
                const uint16_t part = static_cast<uint16_t>(damage >> (modifier == 2 ? 2 : 1));
                damage = static_cast<uint16_t>(damage + part);
                c.d[0] = part;
            }
        }
        SetLow16(c.d[2], damage);
        Return(c);
        return true;
    }
} // namespace

// "Training mode" on the menu's Alternative modes page (modes.cpp)
void StartTraining(Machine &machine) {
    session::StartSession(machine);
}

void RegisterTraining() {
    Game &game = TheGame();
    AddHook(HookAt(kPlayerDispatch), "PlayerStep", PlayerStep);
    AddHook(HookAt(0x02E3FE), "ThrowTested", ThrowTested);
    AddHook(HookAt(kCounterTest), "CounterTest", CounterTest);
    AddHook(HookAt(kDamageModifiers), "DamageModifiers", DamageModifiers);
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
    d.cornerApart = 100;
    d.pictureFloor = kPictureFloor;
    d.fighting = Fighting;
    d.mayRefill = [](int n) { return CanAct(Player(n)); };
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
    d.wideMargin = WideMargin;
    d.wideEnds = WideEnds;
    d.stageChoices = modes::StageChoices({"The game's choice"});
    d.stageWords = "Where the fights are, whatever is played (the game's own games, training, the other modes): the "
                   "game's own choice (the stage of one of the two characters) or a stage named here. Takes effect "
                   "when the characters are next chosen.";
    d.inputWords = "What each player presses and for how many frames: the stick as a keypad number from the player's side (6 forward, "
                   "4 back, 2 down), then the buttons.";
    d.framesPrintVariable = "SFA3_FRAMES";
    d.hud = Hud;
    d.readoutFeatures = {{"stun_readout", "Training mode", "Stun readout",
                          "Under each bar: how much stun the player has taken and how much stuns them (it drains when they are left "
                          "alone for a while)."},
                         {"custom_combo_readout", "Training mode", "Custom combo time",
                          "While a player's V-ism custom combo goes on, how much of its time is left, under that player's bar."}};
    d.boxFeatures = {{"throw_range", "Training mode", "Throw range",
                      "With hit boxes on: the box in front of each player that the other's push box (green) has to touch for a "
                      "throw, as the game tests it, while the player could throw (on the floor and free to act). Each character's "
                      "shows once it has tried a throw: the game tests the throw's own box only then."}};
    fight::DummyOptions &dummy = d.dummy;
    // The Alpha counter: while guarding, forward with a punch and a kick of the same strength (it costs a level of the
    // super combo gauge and some of the guard gauge; checked: the dummy goes into it, state 02000E, and it hits)
    dummy.guardActions.push_back({"Alpha counter, punch", [](int frame, uint16_t toward, uint16_t) -> uint16_t {
                                      return (frame / 2) % 2 ? 0 : static_cast<uint16_t>(toward | kPunch2 | kKick2);
                                  }});
    dummy.guardActionsWords = "What player 2 does while guarding a hit: an Alpha counter (forward with a medium punch and a medium "
                              "kick; it costs a level of the super combo gauge, so pair it with \"Gauge refills\").";
    // A throw's tech (029D08): within 8 steps of being thrown, back or forward with two punches or two kicks pressed at
    // once (the throw's own command: forward and two punches) softens it (+29B; checked: 20 points of damage became 5)
    dummy.heldAction = [](int frame, uint16_t, uint16_t away) -> uint16_t {
        return static_cast<uint16_t>(away | ((frame & 1) ? (kPunch2 | kPunch3) : 0));
    };
    dummy.heldActionTitle = "Throw techs";
    dummy.heldActionWords = "Player 2 does a throw's own command while being thrown (a \"tech\", which takes less damage).";
    // Getting up quickly from a knock-down (029884: two kicks pressed at once while falling, less than 0x50 above the
    // floor; not in the X-ism)
    dummy.getUp.push_back({"Quick rise", [](int frame, uint16_t, uint16_t) -> uint16_t {
                               return (frame / 2) % 2 ? 0 : static_cast<uint16_t>(kKick1 | kKick2);
                           }});
    // Recovering in the air (02B330, in reels 6 and 8 while flying): two punches pressed at once (or one after the
    // other: +27A keeps it), high enough above the floor (0x30, or 0x10 with the stick held one way or the other);
    // the stick then (02B4BC, +37D) has it come down straight, or drift one way or the other (02B412 on)
    dummy.airRecovery.push_back({"Neutral", [](int frame, uint16_t, uint16_t) -> uint16_t { return AirRecovery(frame, 0); }});
    dummy.airRecovery.push_back({"Forward", [](int frame, uint16_t towards, uint16_t) -> uint16_t { return AirRecovery(frame, towards); }});
    dummy.airRecovery.push_back({"Back", [](int frame, uint16_t, uint16_t away) -> uint16_t { return AirRecovery(frame, away); }});
    dummy.airRecoveryWords = "Hit into the air, player 2 recovers there (two punches at once) as soon as the game lets it: "
                             "straight down, drifting towards player 1 or away; off; or any of those at random, per hit.";
    session::Register(d);
    // (after the session's own, so it follows "Health refills"; a training aid, as the session's are)
    game.features.push_back({"gauge_refill", modes::kTraining, "Gauge refills",
                             "Both players' super combo gauge kept full (every ism's), filled again after a custom combo."});
    game.features.push_back({"guard_refill", modes::kTraining, "Infinite guard gauge",
                             "Both players' guard gauge kept full, so nobody's guard is crushed; and at the size the round "
                             "began with (a guard crush or an Alpha counter makes it smaller)."});
    TrainingAids({"gauge_refill", "guard_refill"});
    game.features.push_back({"dummy_stun_mash", "Dummy", "Mashes out of stun",
                             "Stunned, the dummy works the stick and the buttons as fast as it can, as a player does to get out sooner."});
    game.features.push_back({"dummy_counter_hit", "Dummy", "Counter hits",
                             "Every hit on player 2 is a counter hit, as if it had been caught starting an attack: the "
                             "counter hit's damage, stun, spark and reeling."});
    game.features.push_back({"dummy_computer", "Dummy", "Played by the game",
                             "Player 2 is the game's own computer player, as in a one-player game (at the board's difficulty "
                             "setting).",
                             kFeatureExclusive});
}

} // namespace cps2recomp
