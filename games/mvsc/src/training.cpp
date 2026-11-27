// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Marvel vs. Capcom: the training session is the toolkit's (fight/session.h); this says where the game keeps
// what it needs. What the players do (each player's step of a fight, for the frame data; a hit landing; the boxes as
// the game's hit tests read them) is told from the game's own code by the engine's shared code (fight/marvel.h), at
// the character's step (00D0D8, once a step for each of the four, the first characters first). A side's
// two characters keep records of their own, and the game says which is on point (mvsc.h), so the session follows the
// character on point; the one resting is looked after here (its health refilled with the point character's).
//
// The game's own, beyond the session's: the stun readout, fixed damage, and the dummy's throw techs, its guard by the
// game's own tests (guard.cpp) and play by the game's computer. Hooks only: the game's routines
// run as they are, and with these off nothing of them changes.
#include "alternative.h"
#include "fight.h"
#include "marvel.h"
#include "session.h"
#include "mvsc.h"

#include <cstdio>
#include <string>

namespace cps2recomp {

bool AttackInRange(M68K &c, uint32_t player); // guard.cpp
bool GuardStanding(M68K &c, uint32_t player);

namespace {
    using namespace mvsc;

    // --- the stun ---
    // A character's stun left, a byte (+C9): a hit whose entry's byte +6 is not 0 takes 0 to 2 at random (the table at
    // 0E7D04), and one more when player 2's side hits player 1's (01B842: the attacker's side number is the base), but
    // never past 1 at once unless it is 1 already, and none in some reels (+145, +146). At 0 the hit's reaction is the
    // stun's (+F0 6: 01AA96): the character is thrown high into the air and is its own master again on the way down
    // (as after a launcher); there is no standing dizzy to mash out of (seen: the stun put at 1, then a jab). It comes
    // back whole (the character's own, a word by +52 at 0E7CD6: 0x50 for most) once the byte after it (+CA), set to 60
    // at every hit (01B8B4), has counted down (00D7E8). Watched over a fight of jabs, Spider-Man's stun going down.
    constexpr uint32_t kStunLeft = 0xC9, kStunBack = 0xCA, kStunTable = 0x0E7CD6;
    // Held in a throw (+06 8: 01880E), where 018B78 and 018BE2 let it tech: the stick held left or right and a medium or
    // strong punch or kick pressed (+DA & 0660), while the throw's own steps allow it (by the thrower and the throw:
    // the table at 0EA1B4). The two then fly apart (+B0 3E) with no damage. Checked: Ryu's throw of Spider-Man
    // (16 points) did none with the dummy tapping a strong punch with the stick held; held alone, or one press, it did.
    constexpr uint16_t kControlHeld = 0x08;

    int StunMost(M68K &c, uint32_t player) {
        const uint32_t who = Ram16(player + kCharacter);
        return who < 0x80 ? (static_cast<uint16_t>(c.Read(kStunTable + who, 1)) & 0xFF) : 0;
    }

    std::string StunLine(M68K &c, int n) {
        const uint32_t player = Player(n);
        const int most = StunMost(c, player), left = Ram8(player + kStunLeft);
        if (most <= 0 || left >= most || !Ram8(player + kStunBack)) {
            return {};
        }
        char text[48];
        snprintf(text, sizeof(text), "stun %d / %d", most - left, most);
        return text;
    }

    Machine *g_machine = nullptr; // (the port's, for reading the ROM's tables from the HUD's readout)

    // Under each player's bar: the combo readout and the stun line (or the stun line alone)
    void PlayerReadout(int n, const fight::Combos &combos, std::vector<HudLine> &lines) {
        const bool readout = FeatureOn("damage_readout"), stun = FeatureOn("stun_readout");
        if (!readout && !stun) {
            return;
        }
        const std::string extra = stun && g_machine ? StunLine(g_machine->cpu, n) : std::string();
        if (readout) {
            combos.Hud(n, lines, 0.19f, true, extra);
        } else if (!extra.empty()) {
            lines.push_back({n == 0 ? 0.03f : 0.97f, 0.19f, n == 0 ? -1 : 1, extra});
        }
    }

    // --- fixed damage ---
    // A hit's damage (01C952) is scaled by a number from a row of 32 picked at random (the row by the attack's entry,
    // byte +1: 0E960A on; 28 to 38 of 32, 32 on average): at 01C9C0 it multiplies (mulu d0,d5, then /32). In its place,
    // the row's average (the random number still drawn, so the game goes on as it would): a combo always does the same.
    bool RandomPart(M68K &c) {
        if (!FeatureOn("fixed_damage")) {
            return false;
        }
        const uint32_t row = c.a[1] & 0xFFFFFF;
        uint32_t sum = 0;
        for (uint32_t n = 0; n < 32; n++) {
            sum += static_cast<uint8_t>(c.Read(row + n, 0));
        }
        const uint32_t damage = ((c.d[5] & 0xFFFF) * sum + 512) / 1024;
        SetLow16(c.d[5], damage);
        SetLow16(c.d[0], 32); // (times 32 and /32: the average's damage)
        return false;
    }

    // The game's own aids: the gauge kept full, and the partner's health refilled with the point character's
    void Hud(Machine &machine, std::vector<HudLine> &, std::vector<HudBox> &) {
        g_machine = &machine;
        for (int n = 0; n < 2; n++) {
            const uint32_t player = Player(n);
            if (FeatureOn("gauge_refill")) {
                SetRam8(player + kGaugeLevel, kGaugeFull);
                SetRam16(player + kGaugeBar, kGaugeBarFull);
            }
            if (FeatureOn("health_refill")) {
                const uint32_t partner = PartnerOf(n);
                if (Ram16(partner + kHealth) != 0 && Ram16(partner + kHealth) < kHealthFull) {
                    SetRam16(partner + kHealth, kHealthFull);
                    SetRam16(partner + kRedHealth, kHealthFull);
                }
            }
        }
    }

    // --- the dummy ---
    // Who plays player 2's characters (+03 of their records: 1 a person, 0 the game's computer, which reads its own
    // controls: 01367E)
    void DummyPlays(bool computer) {
        for (const uint32_t record : {Player(1), PartnerOf(1)}) {
            const uint8_t want = computer ? 0 : 1;
            if (Ram8(record + 0x03) != want && (computer || Ram8(record + 0x03) == 0)) {
                SetRam8(record + 0x03, want);
            }
        }
    }
    // Each side's place on the panel (023F96, every frame for player 1's first record and player 2's): a side whose
    // record says the computer plays it is one a person may join, and its panel counts down a continue (02439A on).
    // Played by the game in training, player 2's side is still a person's: its place is left as it was.
    bool SideOnPanel(M68K &c) {
        if ((c.a[6] & 0xFFFFFF) == kPlayer1 + kPlayerSize && session::InSession() && FeatureOn("dummy_computer")) {
            Return(c);
            return true;
        }
        return false;
    }
    void DummyView(Machine &machine, fight::DummyView &view) {
        g_machine = &machine;
        if (!session::InSession()) {
            return; // (the session's dummy is asked every frame, in every game: only a training session's is player 2)
        }
        if (view.fighting) {
            DummyPlays(FeatureOn("dummy_computer"));
        }
        // (held in a throw: the session's view of the step has only its own and reeling)
        if (Ram16(Player(1) + kControl) == kControlHeld) {
            view.doing = fight::Doing::Held;
        }
    }
} // namespace

void RegisterTraining() {
    session::Description d;
    d.player1 = kPlayer1;
    d.playerAt = Player;
    d.playerSize = kPlayerSize;
    d.x = kX;
    d.xLong = true;
    d.health = kHealth;
    d.healthCeiling = kRedHealth;
    d.healthFull = kHealthFull;
    d.clock = kRoundClock;
    d.clockFull = 0x99; // (in BCD: a round is being fought while it is 1 to 99)
    d.cameraXAt = marvel::CameraXAt;
    d.cameraYAt = marvel::CameraYAt;
    d.ends = marvel::Ends;
    // (no start places: "Start over" takes the round's own, which are not the same on every stage)
    d.pictureFloor = marvel::kFloorLine;
    d.fighting = [] {
        const uint8_t clock = Ram8(kRoundClock);
        return Ram8(kFight) == kFighting && clock > 0 && clock <= 0x99;
    };
    d.chosen = [](int n) { return Ram8(kChoosing + static_cast<uint32_t>(n) * kChoosingSize) >= kChoosingDone; };
    d.panelHeld = marvel::PanelHeld;
    d.facingLeft = marvel::FacingLeft;
    d.moveStart = marvel::MoveStart;
    d.boxes = marvel::Boxes;
    d.wideStage = marvel::WideStage;
    d.wideSide = marvel::WideSide;
    d.wideMargin = marvel::WideMargin;
    d.wideEnds = marvel::WideEnds;
    d.stageChoices = modes::StageChoices({"The game's choice"});
    d.hud = Hud;
    d.ownAids = {"gauge_refill"};
    d.dummy.downAfter = 34;
    d.guardView = [](Machine &machine, bool &inRange, bool &standing) {
        inRange = AttackInRange(machine.cpu, Player(1));
        standing = inRange && GuardStanding(machine.cpu, Player(1));
    };
    d.playerReadout = PlayerReadout;
    d.dummyView = DummyView;
    d.readoutFeatures = {{"stun_readout", "Training mode", "Stun readout",
                          "Under each bar: how much stun the character on point has taken and how much stuns it (thrown high into "
                          "the air); it all comes back a second after the last hit."}};
    // The throw's tech: the stick held and a medium or strong button pressed, again and again while held
    d.dummy.heldAction = [](int frame, uint16_t, uint16_t away) -> uint16_t {
        return static_cast<uint16_t>(away | ((frame & 1) ? (kPunch2 | kPunch3) : 0));
    };
    d.dummy.heldActionTitle = "Throw techs";
    d.dummy.heldActionWords = "Player 2 techs throws: the stick held and a medium or strong button pressed while being thrown, "
                              "as the game lets it (the two fly apart, with no damage).";
    Game &game = TheGame();
    game.features.push_back({"gauge_refill", modes::kTraining, "Gauge refills", "Both players' hyper combo gauge kept full."});
    session::Register(d);
    game.features.push_back({"dummy_computer", "Dummy", "Played by the game",
                             "Player 2's characters are the game's own computer player, as in a one-player game (at the board's "
                             "difficulty setting).",
                             kFeatureExclusive});
    TrainingAids({"dummy_computer"});
    game.features.push_back({"fixed_damage", "Training mode", "Fixed damage",
                             "No random part in a hit's damage (the average in its place), so a combo always does the same."});
    AddHook(HookAt(0x00D0D8), "PlayerStep", marvel::PlayerStep);
    AddHook(HookAt(0x01C9C0), "RandomPart", RandomPart);
    AddHook(HookAt(0x023F96), "SideOnPanel", SideOnPanel);
}

} // namespace cps2recomp
