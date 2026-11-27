// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Hyper Street Fighter II: the training session is the toolkit's (fight/session.h); this says where the game keeps
// what it needs, and tells it from the game's own code what the players do: each player's step of a fight (frame data),
// a hit landing or guarded, and the hit boxes as the game's hit tests read them. The game's own aid beside the
// toolkit's: the super combo gauge kept full, and each player's stun against how much stuns them; and the throw range,
// fixed damage (cheats.cpp), and a dummy that guards high or low by the game's own rule (guard.cpp), softens throws
// (Super Turbo's version), mashes out of stun or is played by the game's own computer player (Super Turbo's own, with
// this game's places).
#include "fight.h"
#include "alternative.h"
#include "session.h"
#include "hsf2.h"

#include <cstdio>
#include <iterator>

namespace cps2recomp {

bool RingOutStep(M68K &c); // battle.cpp
bool HandsOver(uint32_t player);
bool AttackInRange(M68K &c, uint32_t player); // guard.cpp
bool GuardStanding(M68K &c, uint32_t player);

namespace {
    using namespace hsf2;

    constexpr uint8_t kGaugeFull = 0x30; // (FBNeo's script; a super is ready at it)
    // The stun: a word of what has built up (each hit adds to it: 06D858), and the frames until it goes away (+5C, the
    // word before it); the player is stunned when it reaches a word of the table at 06D830, by a byte that steps by two
    // each time the player has been stunned (Super Turbo's routine, 07F1F2 there)
    constexpr uint32_t kStun = 0x5E, kStunTimes = 0x2B1, kStunMost = 0x06D830;

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
            if (static_cast<int16_t>(Ram16(Player(n) + kHealth)) > kHealthFull || Ram8(Player(n) + kFighting) != 2) {
                return false;
            }
        }
        return true;
    }

    // 066BBE (a person's) and 06A246 (the computer's): a player's fight step, where it goes on to whatever is in charge
    // of the player (kState); once a step for each player. Its record is as the last step left it, which is what the
    // picture showed and the hit tests tested.
    bool PlayerStep(M68K &c) {
        if (RingOutStep(c)) { // (the ring out's own movements: battle.cpp)
            Return(c);
            return true;
        }
        const uint32_t player = c.a[6] & 0xFFFFFF;
        const int n = PlayerOf(player);
        if (n < 0 || Ram8(player + kFighting) != 2 || !modes::RoundFought()) {
            return false;
        }
        fight::StepView view;
        const uint8_t state = Ram8(player + kState);
        const bool guarded = state == kGuard && Ram8(player + kGuarded);
        view.control = state == kReeling || guarded                                          ? fight::StepView::Reeling :
                       state <= kJump || state == kGuard || state == kNormal || state == kSpecial ? fight::StepView::Own :
                                                                                                   fight::StepView::Held;
        if (state == kGettingUp) {
            view.control = fight::StepView::Held;
        }
        view.frozen = Ram8(player + kHitStop) != 0;
        view.canAct = CanAct(player);
        view.attacking = Ram8(player + kAttacking) != 0;
        view.jumping = state == kJump;
        const uint32_t frame = Ram32(player + kFrame);
        view.attackOut = frame < 0x400000 && c.Read(frame + kFrameAttack, 0) != 0;
        session::Step(n, view);
        return false;
    }

    // 0C26D4 and 0C2CDA: an attack has reached the other player (A2), and the game has made it a hit or a guarded one
    bool HitTaken(M68K &c) {
        const int victim = PlayerOf(c.a[2]);
        if (victim >= 0) {
            session::Contact(victim, Ram8((c.a[2] & 0xFFFFFF) + kGuarded) != 0);
        }
        return false;
    }

    // What the game read off the panel for a player this frame, as Button bits
    uint16_t PanelHeld(int n) {
        const uint16_t word = Ram16(kControls[n]);
        uint16_t held = word & 0x0F;
        held |= static_cast<uint16_t>(((word >> 4) & 0x07) << 4); // punches
        held |= static_cast<uint16_t>(((word >> 8) & 0x07) << 7); // kicks
        return held;
    }

    fight::MoveLog::Start MoveStart(int n) {
        const uint32_t player = Player(n);
        const uint8_t who = Ram8(player + kCharacter);
        fight::MoveLog::Start start;
        start.character = who == kAkuma ? "Akuma" : who < kCharacters ? kCharacterNames[who] : "?";
        start.input = fight::Notation(PanelHeld(n), Ram8(player + kFacesRight) == 0);
        start.air = Ram8(player + kState) == kJump || Ram8(player + kInAir) != 0;
        start.id = Ram32(player + kFrame); // (the move's first animation frame)
        return start;
    }

    // A box of a record's (a player's or a thrown thing's), placed in the stage as the hit test 0C398E places it: words,
    // its middle from where the record stands (across, turned about with the facing; up) and its half sizes (Super
    // Turbo's were bytes)
    void AddBox(M68K &c, std::vector<fight::StageBox> &boxes, fight::BoxKind kind, uint32_t record, uint32_t box, bool thrown = false) {
        const int across = static_cast<int16_t>(c.Read(box, 1));
        const int up = static_cast<int16_t>(c.Read(box + 2, 1));
        const int halfWidth = static_cast<int16_t>(c.Read(box + 4, 1)), halfHeight = static_cast<int16_t>(c.Read(box + 6, 1));
        if (halfWidth <= 0 || halfHeight <= 0) {
            return;
        }
        const int x = static_cast<int16_t>(Ram16(record + kX)) + (Ram8(record + kFacesRight) ? -across : across);
        const int y = static_cast<int16_t>(Ram16(record + kY)) + up;
        boxes.push_back({kind, x, y, halfWidth, halfHeight, thrown, fight::Whose(record, Player(0), Player(1))});
    }

    // The box of a kind named in a record's frame (0: none): its table, from the record's box tables
    uint32_t BoxOf(M68K &c, uint32_t record, uint32_t number, uint32_t table, uint32_t size) {
        const uint32_t frame = Ram32(record + kFrame), tables = Ram32(record + kBoxTables);
        if (frame >= 0x400000 || tables >= 0x400000) {
            return 0;
        }
        const uint8_t which = static_cast<uint8_t>(c.Read(frame + number, 0));
        if (!which) {
            return 0;
        }
        return tables + static_cast<uint32_t>(static_cast<int16_t>(c.Read(tables + table, 1))) + which * size;
    }

    // The throw range: the box of the throw a player last tried, which the game puts in its record as it tries one
    // (kThrowX...), drawn in front of the player while it could throw (on the floor and free to act), with the other's
    // place to be thrown by: a bar at its height, as wide as the test takes it. The throw is in range when the bar
    // reaches into the box.
    void ThrowRanges(std::vector<fight::StageBox> &boxes) {
        for (int n = 0; n < 2; n++) {
            const uint32_t player = Player(n), other = Player(1 - n);
            const int across = static_cast<int16_t>(Ram16(player + kThrowX)), up = static_cast<int16_t>(Ram16(player + kThrowY));
            const int halfWidth = static_cast<int16_t>(Ram16(player + kThrowHalfWidth)), halfHeight = static_cast<int16_t>(Ram16(player + kThrowHalfHeight));
            if (halfWidth <= 0 || halfHeight <= 0 || Ram8(player + kInAir) || !CanAct(player)) {
                continue;
            }
            const int x = static_cast<int16_t>(Ram16(player + kX)) + (Ram8(player + kFacesRight) ? -across : across);
            boxes.push_back({fight::BoxKind::ThrowRange, x, static_cast<int16_t>(Ram16(player + kY)) + up, halfWidth, halfHeight, false,
                             static_cast<int8_t>(n)});
            // (the other's place to be thrown by, as 0046F4 takes it)
            int place = static_cast<int16_t>(Ram16(other + kX));
            if (Ram8(other + kThrowableMoved)) {
                const int moved = static_cast<int16_t>(Ram16(other + kThrowableX));
                place += Ram8(other + kFacesRight) ? moved : -moved;
            }
            const int placeWidth = static_cast<int16_t>(Ram16(other + kThrowableHalfWidth));
            if (placeWidth > 0) {
                boxes.push_back({fight::BoxKind::Throw, place, static_cast<int16_t>(Ram16(other + kY)) + static_cast<int16_t>(Ram16(other + kThrowableY)),
                                 placeWidth, 1, false, static_cast<int8_t>(1 - n)});
            }
        }
    }

    std::vector<fight::StageBox> Boxes(Machine &machine) {
        M68K &c = machine.cpu;
        std::vector<fight::StageBox> boxes;
        for (int n = 0; n < 2; n++) {
            const uint32_t player = Player(n);
            if (Ram8(player + kFighting) != 2) {
                continue;
            }
            if (const uint32_t box = BoxOf(c, player, kFramePush, kTablePush, kBoxSize)) {
                AddBox(c, boxes, fight::BoxKind::Push, player, box);
            }
            constexpr struct {
                uint32_t number, table;
            } kHurt[] = {{kFrameHead, kTableHead}, {kFrameBody, kTableBody}, {kFrameLegs, kTableLegs}};
            for (const auto &hurt : kHurt) {
                if (const uint32_t box = BoxOf(c, player, hurt.number, hurt.table, kBoxSize)) {
                    AddBox(c, boxes, fight::BoxKind::Hurt, player, box);
                }
            }
            if (const uint32_t box = BoxOf(c, player, kFrameAttack, kTableAttack, kAttackBoxSize)) {
                AddBox(c, boxes, fight::BoxKind::Attack, player, box);
            }
        }
        // What the players throw (the pool from FF9554, records of a player's layout): their attacks (the thrown things'
        // hit test, 0C2C0A, takes them as a player's)
        for (int n = 0; n < kThrownRecords; n++) {
            const uint32_t thrown = kThrown + static_cast<uint32_t>(n) * kThrownSize;
            if (Ram8(thrown) != 1) {
                continue;
            }
            if (const uint32_t box = BoxOf(c, thrown, kFrameAttack, kTableAttack, kAttackBoxSize)) {
                AddBox(c, boxes, fight::BoxKind::Attack, thrown, box, true);
            }
        }
        if (FeatureOn("throw_range")) {
            ThrowRanges(boxes);
        }
        return boxes;
    }

    // The game's own aids: the gauge kept full, and each player's stun against how much stuns it
    void Hud(Machine &machine, std::vector<HudLine> &lines, std::vector<HudBox> &) {
        for (int n = 0; n < 2; n++) {
            const uint32_t player = Player(n);
            if (FeatureOn("gauge_refill") && Ram8(player + kSuperGauge) < kGaugeFull) {
                SetRam8(player + kSuperGauge, kGaugeFull);
            }
            if (FeatureOn("stun_readout")) {
                char text[32];
                const uint32_t most = kStunMost + (Ram8(player + kStunTimes) & 0x1E);
                snprintf(text, sizeof text, "stun %d/%d", Ram16(player + kStun), machine.Read16(most));
                lines.push_back({n == 0 ? 0.02f : 0.98f, 0.12f, n == 0 ? -1 : 1, text});
            }
        }
    }

    // The dummy: the game knows when it is knocked down; the game's own computer player plays it if asked for (handed
    // over, and back, only when the game lets who plays a player change: battle.cpp's HandOver; computer.cpp sets the
    // computer up for it). In a session both players are people otherwise; outside one this is never asked (the dummy's
    // view is made in every fight).
    void DummyView(Machine &, fight::DummyView &view) {
        view.knowsDown = true;
        view.down = Ram8(Player(1) + kState) == kDown;
        if (Ram8(Player(1) + kHeldBy) == 0xFF) {
            view.doing = fight::Doing::Held; // (being thrown: its state stays what it was, and its own step does not run)
        }
        const uint8_t person = FeatureOn("dummy_computer") ? 0 : 1;
        if (session::InSession() && view.fighting && Ram8(Player(1) + kPerson) != person && HandsOver(Player(1))) {
            SetRam8(Player(1) + kPerson, person);
        }
    }
    // Stunned, the dummy mashes: the stick from the middle to a side and a button new every other frame, the most the
    // game's count of it takes (kStunned: 2 for the stick and 1 for a button, 3 a step taking the step's time off)
    void AfterDummy(Machine &machine, const fight::DummyView &view) {
        if (FeatureOn("dummy_stun_mash") && view.fighting && Ram8(Player(1) + kPerson) && Ram8(Player(1) + kStunned)) {
            machine.players[1] = machine.frame & 1 ? 0 : static_cast<uint16_t>((machine.frame & 2 ? kLeft : kRight) | kPunch1 | kKick1);
        }
    }
} // namespace

void RegisterTraining() {
    session::Description d;
    d.player1 = kPlayer1;
    d.playerSize = kPlayerSize;
    d.x = kX;
    d.xLong = true;
    d.health = kHealth;
    d.healthFull = kHealthFull;
    d.healthShown = kHealthBefore;
    d.clock = kRoundClock;
    d.clockFull = 0x99; // (BCD)
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
    d.fighting = Fighting;
    d.mayRefill = [](int n) { return CanAct(Player(n)); };
    d.chosen = [](int n) { return Ram8(Player(n) + kChosen) != 0; };
    d.panelHeld = PanelHeld;
    d.facingLeft = [](int n) { return Ram8(Player(n) + kFacesRight) == 0; };
    d.moveStart = MoveStart;
    d.boxes = Boxes;
    d.guardView = [](Machine &machine, bool &inRange, bool &standing) {
        inRange = AttackInRange(machine.cpu, Player(1));
        standing = inRange && GuardStanding(machine.cpu, Player(1));
    };
    d.dummyView = DummyView;
    d.afterDummy = AfterDummy;
    {
        // Softening a throw (kSoftened: both players of Super Turbo's version): the stick to either side and a heavy punch
        // new, every other step of being thrown (checked: the throw's damage halved)
        d.dummy.heldAction = [](int frame, uint16_t, uint16_t away) -> uint16_t {
            return static_cast<uint16_t>(away | ((frame & 1) ? 0 : kPunch3));
        };
        d.dummy.heldActionTitle = "Softens throws";
        d.dummy.heldActionWords = "Player 2 softens a throw as it is thrown (the stick to either side and a medium or heavy button within "
                                  "a moment of the throw's start), and takes half its damage. The game has it only when both are of "
                                  "Super Turbo's version.";
    }
    d.boxFeatures = {{"throw_range", modes::kTraining, "Throw range",
                      "With hit boxes on: the box in front of each player that the other has to be in for a throw, as the game "
                      "tests it, while the player could throw (on the floor and free to act), and the other's place to be thrown "
                      "by, a yellow bar: a throw reaches when the bar is in the box. Each character's shows once it has tried a "
                      "throw: the game puts the box of the throw tried in place only then."}};
    d.wideStage = WideStage;
    d.wideSide = WideSide;
    d.wideMargin = WideMargin;
    d.wideEnds = WideEnds;
    d.stageChoices = modes::StageChoices({"The game's choice"});
    d.stageWords = "Where the fights are, whatever is played (two-player games, training, the other modes): the "
                   "game's own choice or a stage named here. A one-player game's stage stays the opponent's own, "
                   "which the game goes by; another game's stage is shown there all the same. Takes effect when the "
                   "characters are next chosen.";
    d.inputWords = "What each player presses and for how many frames: the stick as a keypad number from the player's side (6 "
                   "forward, 4 back, 2 down), then the buttons.";
    d.hud = Hud;
    d.ownAids = {"gauge_refill", "stun_readout"};
    Game &game = TheGame();
    game.features.push_back({"gauge_refill", modes::kTraining, "Gauge refills", "Both players' super combo gauge kept full."});
    game.features.push_back({"stun_readout", modes::kTraining, "Stun readout",
                             "Under each player's bar: the stun built up, against how much stuns the player (it goes away when they "
                             "are left alone for a while, and takes more each time they have been stunned)."});
    session::Register(d);
    game.features.push_back({"dummy_stun_mash", "Dummy", "Mashes out of stun",
                             "Stunned, the dummy works the stick and the buttons as fast as the game counts them, as a player does to get "
                             "out sooner."});
    game.features.push_back({"dummy_computer", "Dummy", "Played by the game",
                             "Player 2 is the game's own computer player, as in a one-player game (at the board's difficulty setting).",
                             kFeatureExclusive});
    TrainingAids({"throw_range", "dummy_stun_mash", "dummy_computer"});
    AddHook(HookAt(kPlayerStep), "PlayerStep", PlayerStep);
    AddHook(HookAt(kComputerPlayerStep), "ComputerPlayerStep", PlayerStep);
    AddHook(HookAt(kHitTaken), "HitTaken", HitTaken);
    AddHook(HookAt(kThrownHitTaken), "ThrownHitTaken", HitTaken);
}

} // namespace cps2recomp
