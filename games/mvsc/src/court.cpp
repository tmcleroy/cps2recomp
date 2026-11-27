// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Marvel vs. Capcom: Ball battle (the toolkit's ball game, fight/court.h; docs/COURT.md): where the game keeps what the
// ball needs (the fighters' boxes and their attacks, how strong each is), and what is done to a fighter. The game's own
// code runs as it is, hooked, as in Alpha 3:
//
//   - the players' push (013758, the four pairs of records pushed apart) is the court's while it runs: each character
//     on point kept on its half
//   - nobody is thrown: the throw test (0186E6) says no
//   - one character a side: the chords of a punch and a kick of the same strength (HP + HK, MP + MK) are never matched
//     (01A0C2), so nobody tags, calls the special partner or starts a Duo Team Attack or a team super; the partner rests
//     off the picture all game
//   - what the ball does to a fighter is a hit of the game's own: put in the game's list of hits found (the victim's
//     +87 and +88, as the hit test 0D7270 leaves them) before the hits are dealt with (0D6EDA), with an attack's entry
//     of the strength the ball's speed says (01A952), so the game's own reel, knock-down or guard runs from there
//   - nobody loses health: a hit's damage taken as nothing (01B74E, 01BEA0)
//
// The game's heights grow downwards: the court's are the height over the floor (Ground() less y), and the camera's place
// down is turned into one (Description::cameraHeight).
#include "alternative.h"
#include "battle.h"
#include "court.h"
#include "lift.h"
#include "marvel.h"
#include "state.h"
#include "mvsc.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace cps2recomp {

int CourtSide();

namespace {
    using namespace mvsc;

    // An attack's entry (0x20 bytes, the attacker's table of them by its +52 at kHitTables, its number +80; the hit at
    // 01A934 takes it so): +0D the button's strength in its low two bits (0 light, 1 medium, 2 hard; 3 most special moves
    // and supers: seen in the tables, each character's first twelve entries its standing and crouching punches and kicks
    // in that order)
    constexpr uint32_t kHitTables = 0x0E68AE, kHitSize = 0x20, kAttackNumber = 0x80, kAttackStrength = 0x0D;
    // A record's hit found this step (0D7270 on): +87 which of its four boxes was hit (1-4; 0 none), +88 the record
    // whose attack hit it; +A2 that box's number
    constexpr uint32_t kHitBox = 0x87, kHitBy = 0x88, kHitBoxNumber = 0xA2;
    // The entries the ball hits with, by the court's reaction (Ryu's, which every character takes from anyone: the
    // entry is the hit's data, its address all the game keeps of it): his standing light, medium and hard punches (00-02)
    // and for a knock-down his sweep (0B: reaction 08, the victim tripped and lying; a low, but the court knocks down only
    // one not guarding). Checked with MVSC_COURT_ENTRY and CPS2RECOMP_COURT_SHOT: each reels as from Ryu's own.
    constexpr uint32_t kRyuEntries = 0x0F1544;
    uint32_t Entry(court::Reaction reaction) {
        static const char *forced = getenv("MVSC_COURT_ENTRY"); // (for finding the facts: this entry for every hit)
        if (forced && reaction != court::kGuard) {
            return static_cast<uint32_t>(strtoul(forced, nullptr, 16));
        }
        switch (reaction) {
        case court::kGuard:
        case court::kReelLight: return kRyuEntries + 0x00 * kHitSize;
        case court::kReelMedium: return kRyuEntries + 0x01 * kHitSize;
        case court::kReelHard: return kRyuEntries + 0x02 * kHitSize;
        case court::kKnockDown: return kRyuEntries + 0x0B * kHitSize;
        default: return 0;
        }
    }
    constexpr uint32_t kActive = 0x00;
    constexpr uint16_t kControlReeling = 0x0C; // (+06 as a hit leaves it: 01B50E)
    constexpr int kHalfWidth = 32;             // (how far from the line a character's middle is kept, besides kLineRoom)

    int Word(uint32_t at) {
        return static_cast<int16_t>(Ram16(at));
    }

    struct State {
        uint8_t levels[2] = {0, 0};          // each side's gauge levels the step before
        bool spending[2] = {false, false};   // a level spent (a super under way) until the character can act again
        uint8_t injected[2] = {0, 0};        // the ball's hit put in the game's list this step (the entry to use: Entry)
        uint8_t attackerStop[2] = {0, 0};    //   and the other side's hitstop before it (put back: the attacker is far away)
        int32_t sweep = 0;                   // frames of the court so far, for the camera's sweep at its start
    } g;

    bool CanAct(uint32_t player) {
        const uint8_t action = Ram8(player + kAction);
        return Ram16(player + kControl) == 0 &&
               (action == kActionStand || action == kActionWalk || action == kActionCrouch || action == kActionGuard);
    }

    void WatchGauges() {
        for (int side = 0; side < 2; side++) {
            const uint32_t player = Player(side);
            const uint8_t levels = Ram8(player + kGaugeLevel);
            if (levels < g.levels[side]) {
                g.spending[side] = true;
            } else if (CanAct(player)) {
                g.spending[side] = false;
            }
            g.levels[side] = levels;
        }
    }

    // (for finding the facts: MVSC_COURT_LOG=1 writes each attack's entry as it comes out, and the sounds queued)
    bool Logging() {
        static const bool on = getenv("MVSC_COURT_LOG") != nullptr;
        return on;
    }
    void LogSounds(Machine &machine) {
        static uint16_t last = 0xFFFF;
        const uint16_t end = Ram16(kSoundQueueEnd) & kSoundQueueMask;
        if (last == 0xFFFF) {
            last = end;
        }
        for (uint16_t at = last; at != end; at = static_cast<uint16_t>((at + 0x10) & kSoundQueueMask)) {
            fprintf(stderr, "court f%llu: sound %04X\n", static_cast<unsigned long long>(machine.frame), Ram16(kSoundQueue + at + 2));
        }
        last = end;
    }

    // A box of an object's (its table's eight bytes: across from where it stands and half its width, then down and half its
    // height, signed words), in the court's terms: across as the game's, up from the floor
    bool Place(Machine &machine, uint32_t object, uint32_t box, court::Shape &shape) {
        shape.halfWidth = static_cast<int16_t>(machine.Read16(box + 2));
        shape.halfHeight = static_cast<int16_t>(machine.Read16(box + 6));
        const int across = static_cast<int16_t>(machine.Read16(box + 0)), down = static_cast<int16_t>(machine.Read16(box + 4));
        const uint8_t flip = Ram8(object + kFlip);
        shape.x = Word(object + kX) + ((flip & 1) ? -across : across);
        shape.y = Ground() - (Word(object + kY) + ((flip & 2) ? -down : down));
        return shape.halfWidth > 0 && shape.halfHeight > 0;
    }

    void Boxes(Machine &machine, uint32_t object, int side, bool thrown, std::vector<court::Shape> &out) {
        const uint32_t table = Ram32(object + kBoxTable) & 0xFFFFFF;
        if (table < 0x1000 || table >= 0x400000) {
            return;
        }
        if (!thrown && Ram8(object + kCanBeHit)) {
            for (uint32_t n = 0; n < 4; n++) {
                court::Shape shape;
                const uint16_t id = Ram16(object + kHurtBoxes + 2 * n) & 0x1FFF;
                if (id && Place(machine, object, table + id * 8u, shape)) {
                    shape.kind = court::Shape::kBody;
                    shape.side = side;
                    out.push_back(shape);
                }
            }
        }
        if (!Ram8(object + kCanHit)) {
            return;
        }
        const uint8_t number = Ram8(object + kAttackNumber) & 0x7F;
        const uint32_t at = kHitTables + 2u * (Ram16(object + kCharacter) & 0x7E);
        const uint32_t entries = (static_cast<uint32_t>(machine.Read16(at)) << 16 | machine.Read16(at + 2)) & 0xFFFFFF;
        const uint8_t strength = entries >= 0x1000 && entries < 0x400000 ? machine.Read8(entries + number * kHitSize + kAttackStrength) : 0;
        if (Logging()) {
            static uint32_t last[64] = {};
            const uint32_t slot = ((object - 0xFF0000) / 0x100) & 63;
            if (last[slot] != (object << 8 | number)) {
                last[slot] = object << 8 | number;
                fprintf(stderr, "court f%llu: obj %06X side %d char %02X action %02X attack %02X entry %06X:", static_cast<unsigned long long>(machine.frame),
                        object, side, Ram16(object + kCharacter), Ram8(object + kAction), number, entries + number * kHitSize);
                for (uint32_t i = 0; i < kHitSize; i++) {
                    fprintf(stderr, " %02X", machine.Read8(entries + number * kHitSize + i));
                }
                fprintf(stderr, "\n");
            }
        }
        // (a button's attack is the character doing kActionAttack, on the floor or in a jump; a special move's 2A, what it
        // throws 2C: seen with MVSC_COURT_LOG. Strider's entries have bit 3 set besides)
        const bool special = Ram8(object + kAction) != kActionAttack;
        const int button = strength & 3;
        for (uint32_t n = 0; n < 2; n++) {
            const uint16_t id = Ram16(object + kAttackBoxes + 2 * n);
            court::Shape shape;
            // (the high bit: a throw's reach, not an attack)
            if (!(id & 0x1FFF) || (id & 0x8000) || !Place(machine, object, table + (id & 0x1FFFu) * 8u, shape)) {
                continue;
            }
            shape.kind = court::Shape::kAttack;
            shape.side = side;
            shape.thrown = thrown;
            shape.id = (object & 0xFFFF) << 8 | number;
            shape.power = g.spending[side] ? court::kSuper
                        : thrown || special ? court::kSpecial
                        : button >= 2 ? court::kHard
                        : button == 1 ? court::kMedium
                                      : court::kLight;
            out.push_back(shape);
        }
    }

    void Shapes(Machine &machine, std::vector<court::Shape> &out) {
        WatchGauges();
        if (Logging()) {
            LogSounds(machine);
            static uint32_t was[2] = {0, 0};
            for (int side = 0; side < 2; side++) {
                const uint32_t player = Player(side);
                const uint32_t now = static_cast<uint32_t>(Ram16(player + kControl)) << 8 | Ram8(player + kAction);
                if (now != was[side]) {
                    fprintf(stderr, "court f%llu: side %d control %04X action %02X\n", static_cast<unsigned long long>(machine.frame), side,
                            now >> 8, now & 0xFF);
                    was[side] = now;
                }
            }
        }
        for (int side = 0; side < 2; side++) {
            const uint32_t player = Player(side);
            if (Ram16(player + 0x04) != 2) { // (its state: 2 fighting)
                continue;
            }
            Boxes(machine, player, side, false, out);
            // What it has thrown: the list's records, newest first, to the first that is not one (fight/marvel.cpp's)
            const uint32_t list = kThrownList + static_cast<uint32_t>(side) * kThrownListSpace;
            for (uint32_t i = 1; i < 32; i++) {
                const uint32_t thrown = Ram32(list - 4 * i) & 0xFFFFFF;
                if (thrown < kPlayer1 + 4 * kPlayerSize || thrown >= 0xFFFF00) {
                    break;
                }
                Boxes(machine, thrown, side, true, out);
            }
        }
    }

    // 013758: the four pairs of records pushed apart (0137CE each). On the court the characters on point are kept on their
    // halves instead (they never meet), and the rest (013788 on) goes as it was
    bool PushApart(M68K &c) {
        if (!court::On()) {
            return false;
        }
        SetRam8(0xFF401A, 0); // (clr.b -0x3FE6(a5), as it begins)
        for (int side = 0; side < 2; side++) {
            const uint32_t player = Player(side);
            const int x = Word(player + kX);
            const int kept = court::KeepToHalf(side, x, kHalfWidth);
            if (kept != x) {
                SetRam32(player + kX, static_cast<uint32_t>(kept) << 16);
            }
        }
        c.pc = 0x01378C;
        return true;
    }

    // 0186E6: the throw test (the thrower in A6, the one it would take in its +C0; D0 not 0 a throw, which 0182F0 and the
    // air throws at 02052C and 020546 go by): on the court its "no" (01870E: D0 0), as nobody is thrown there
    bool NoThrow(M68K &c) {
        if (!court::On()) {
            return false;
        }
        c.pc = 0x01870E;
        return true;
    }

    // 01A0C2: the chord matcher (D0 the buttons to have been pressed together over the last three polls; D0 1 if so). On
    // the court the chords of a punch and a kick of the same strength (0440 HP + HK: the tag at 019456 and 019526, the
    // team moves at 0196F0 and in the moves' tables; 0220 MP + MK: the special partner at 0195CE) are never matched
    bool NoTeamChords(M68K &c) {
        const uint16_t chord = static_cast<uint16_t>(c.d[0]);
        if (!court::On() || (chord != 0x440 && chord != 0x220)) {
            return false;
        }
        c.pc = 0x01A0DE; // (moveq #0,d0; rts)
        return true;
    }

    // 0D6EDA: the step's hits found (the tests at 0D710E and 0D719E), and about to be dealt with, each record's in turn
    // (0D6EE4 on: +87 not 0, the attacker its +88, the hit itself 01A8C6). What the ball did to a character (court.h,
    // ReactionWanted) is put in as a hit of the other side's character on point, its entry chosen below (CourtEntry).
    bool BallHits(M68K &) {
        g.injected[0] = g.injected[1] = 0;
        if (!court::On() || Ram8(kFight) < kFighting) {
            return false;
        }
        for (int side = 0; side < 2; side++) {
            int away = 0;
            const court::Reaction reaction = court::ReactionWanted(side, away);
            if (reaction == court::kNoReaction) {
                continue;
            }
            const uint32_t victim = Player(side), attacker = Player(1 - side);
            if (!Ram8(victim + kActive) || !Ram8(attacker + kActive)) {
                continue;
            }
            SetRam8(victim + kHitBox, 2);
            SetRam32(victim + kHitBy, attacker);
            SetRam16(victim + kHitBoxNumber, Ram16(victim + kHurtBoxes + 2) & 0x1FFF);
            g.injected[side] = static_cast<uint8_t>(reaction);
            g.attackerStop[side] = Ram8(attacker + kHitStop);
        }
        return false;
    }

    // 01A952: the hit's entry (A0) about to be kept in the victim's +8C: the ball's in its place
    bool CourtEntry(M68K &c) {
        const int side = SideOf(c.a[6]);
        if (side < 0 || !g.injected[side] || (c.a[6] & 0xFFFFFF) != Player(side)) {
            return false;
        }
        c.a[0] = Entry(static_cast<court::Reaction>(g.injected[side]));
        if (Logging()) {
            fprintf(stderr, "court: the ball hits side %d (reaction %d): entry %06X\n", side, g.injected[side], c.a[0]);
        }
        return false;
    }

    // 01A9F4: whether the victim guards the hit (0171EC, then the way it must be guarded: 01BAB8 and 01BAA0), which goes
    // by its guard begun as an attack came near (+B0 24, 26: 017294); the ball comes near no attack list's way, so one
    // the court has guarded (Description::guarding: holding back, free) goes to the game's guard (01AA08) at once
    bool BallGuarded(M68K &c) {
        const int side = SideOf(c.a[6]);
        if (side < 0 || g.injected[side] != court::kGuard || (c.a[6] & 0xFFFFFF) != Player(side)) {
            return false;
        }
        c.pc = 0x01AA08;
        return true;
    }

    // 0D6E9E: the step's hits dealt with: the attacker the ball's hit froze (01A96E: both sides take the entry's hitstop)
    // goes on as it was, being far away
    bool BallHitsDone(M68K &) {
        for (int side = 0; side < 2; side++) {
            if (g.injected[side]) {
                SetRam8(Player(1 - side) + kHitStop, g.attackerStop[side]);
                g.injected[side] = 0;
            }
        }
        return false;
    }

    // 01D12A (in 01D11C, as a hit lands: 01AA68; the hitting side's record in A0): the round's first hit (FF4011 0 until
    // then, the side with bit 7 set after; the game says FIRST ATTACK as it is set, and gives its bonus), then the rest
    // (01D180 on). On the court, the rest only
    bool NoFirstAttack(M68K &c) {
        if (!court::On()) {
            return false;
        }
        c.pc = 0x01D180;
        return true;
    }

    // 01B74E and 01BEA0 (sub.w d5,0x270(a6)): a hit's damage taken; nothing on the court
    bool NoDamage(M68K &c) {
        if (court::On()) {
            SetLow16(c.d[5], 0);
        }
        return false;
    }
} // namespace

// Before each frame, after the battle's (game.cpp). As a court game begins (under the battle's fade from black) the
// camera is put at the stage's right end for kSweep frames, then at its left, then back in the middle: the game draws a
// stage's scroll screens as its camera goes, only as far past the board's picture as it needs, so a camera held in the
// middle from the start would leave the court's outer columns as they were (black, or another part of the map); the
// camera's jumps have the game draw them. The fighters are put back in the middles of their halves after (at an end,
// the camera's picture moves them: fight/marvel.cpp's KeepIn).
void CourtBeforeFrame(Machine &) {
    constexpr int kSweep = 2;
    if (!court::On()) {
        g.sweep = 0;
        return;
    }
    // (not on another game's stage, drawn by its own game; MVSC_COURT_NO_SWEEP=1 for measuring)
    static const char *noSweep = getenv("MVSC_COURT_NO_SWEEP");
    if (g.sweep > 2 * kSweep + 1 || noSweep || BattleStage() > kStages) {
        return;
    }
    g.sweep++;
    if (Logging()) {
        fprintf(stderr, "court sweep %d: x %d %d camera %d\n", g.sweep, Word(Player(0) + kX), Word(Player(1) + kX), Word(CameraRecord() + kCameraX));
    }
    if (g.sweep <= 2 * kSweep) {
        int left, right;
        CameraEnds(Ram8(kStage), left, right);
        modes::CameraAt().x = g.sweep <= kSweep ? right : std::max(left, 0); // (CameraWanted: -1 is none)
        modes::CameraAt().beyondEnds = true;
        return;
    }
    modes::CameraAt().beyondEnds = false;
    // (once the camera is back: the players' step comes before the camera's in a frame)
    if (g.sweep == 2 * kSweep + 2) {
        const int half = Video::kWidth / 2 + CourtSide();
        for (int side = 0; side < 2; side++) {
            SetRam32(Player(side) + kX, static_cast<uint32_t>(court::Line() + (side ? half : -half) / 2) << 16);
        }
    }
}

// The court's side for the stage that is on (fight/marvel.h, Engine::courtSide): as much of the stage either side of the
// board's picture as all its planes are drawn there, judged by eye from the court's picture at Video::kMaxSide
// (MVSC_COURT_SIDE=N forces one, for measuring). Another game's stage is drawn by that game, to 64 a side.
int CourtSide() {
    static constexpr int kCourtSides[kStages] = {64, 80, 192, 192, 192, 192, 16, 32, 32, 64};
    if (const char *forced = getenv("MVSC_COURT_SIDE")) {
        return std::clamp(atoi(forced), 0, static_cast<int>(Video::kMaxSide));
    }
    const int stage = Ram8(kStage);
    return BattleStage() > kStages || stage < 0 || stage >= kStages ? Video::kWideSide : kCourtSides[stage];
}

void RegisterCourt() {
    court::Description d;
    d.shapes = Shapes;
    d.fighterX = [](int side) { return Word(Player(side) + kX); };
    d.fighterY = [](int side) { return Ground() - Word(Player(side) + kY); };
    d.place = [](int side, int x) { SetRam32(Player(side) + kX, static_cast<uint32_t>(x) << 16); };
    d.halfWidth = [](int) { return kHalfWidth; };
    d.canAct = [](int side) { return CanAct(Player(side)); };
    d.guarding = [](int side) {
        const uint32_t player = Player(side);
        const bool back = Ram16(kPanel[side]) & ((Ram8(player + kFlip) & 1) ? 2 : 1);
        const uint8_t action = Ram8(player + kAction);
        return back && (CanAct(player) || action == kActionJump || action == kActionGuard || action == kActionCrouchGuard);
    };
    d.stunned = [](int side) {
        const uint32_t player = Player(side);
        const uint8_t action = Ram8(player + kAction);
        return Ram16(player + kControl) == kControlReeling && action != kActionGuard && action != kActionCrouchGuard;
    };
    d.floor = 0;
    // (the picture's top is the camera's word down + 0x0F, and the floor 0xD8 below the camera's lowest place:
    // fight/marvel.cpp's boxes; so with the camera at the bottom the floor is the picture's line 0xC9)
    d.pictureFloor = 0xD8 - 0x0F;
    d.cameraHeight = [](int cameraY) { return Word(CameraRecord() + kCameraTop + 2) - cameraY; };
    d.pictureLeft = kPictureLeft;
    d.pictureTop = 32;
    // (the bars, names, clock and the faces beside the bars not shown: court.h. The faces are drawn in the characters' own
    // colour sets, from tiles 16EC0 to 16EFF: seen in the object lists of court games, Venom's 16ECC, Captain Commando's
    // 16EE4, Strider's 16EEC)
    d.panel = [](uint32_t tile, uint16_t attributes) { return ThePanel(tile, attributes) || (tile >= 0x16EC0 && tile < 0x16F00); };
    d.wideStage = marvel::WideStage;
    d.freeze = [](int side, int steps) { SetRam8(Player(side) + kHitStop, static_cast<uint8_t>(steps)); };
    d.playSound = modes::PlaySound;
    // (the game's own hit sounds, queued as the ball's hits of Ryu's entries landed, MVSC_COURT_LOG: 0104, 0105, 0106 his
    // punches, 0108-010A his kicks; player 2's side's 05xx)
    d.hitSounds[court::kLight] = 0x0104;
    d.hitSounds[court::kMedium] = 0x0105;
    d.hitSounds[court::kHard] = 0x0106;
    d.hitSounds[court::kSpecial] = d.hitSounds[court::kSuper] = 0x010A;
    d.everyFrame = [] {
        for (uint32_t record = kPlayer1; record < kPlayer1 + 4 * kPlayerSize; record += kPlayerSize) {
            SetRam16(record + kHealth, kHealthFull);
            SetRam16(record + kRedHealth, kHealthFull);
        }
    };
    // (the gauges' last levels and the camera's sweep: in a save state, for rollback; a save state from before has none)
    d.state = [](Archive &ar) {
        if (ar.version == 0 || ar.version >= 15) {
            ar.Pod(g);
        }
    };
    court::Register(d);
    AddHook(HookAt(0x013758), "CourtPushApart", PushApart);
    AddHook(HookAt(0x0186E6), "CourtNoThrow", NoThrow);
    AddHook(HookAt(0x01A0C2), "CourtNoTeamChords", NoTeamChords);
    AddHook(HookAt(0x0D6EDA), "CourtBallHits", BallHits);
    AddHook(HookAt(0x01A952), "CourtEntry", CourtEntry);
    AddHook(HookAt(0x01A9F4), "CourtBallGuarded", BallGuarded);
    AddHook(HookAt(0x0D6E9E), "CourtBallHitsDone", BallHitsDone);
    AddHook(HookAt(0x01D12A), "CourtNoFirstAttack", NoFirstAttack);
    AddHook(HookAt(0x01B74E), "CourtNoDamage", NoDamage);
    AddHook(HookAt(0x01BEA0), "CourtNoDamage", NoDamage);
}

} // namespace cps2recomp
