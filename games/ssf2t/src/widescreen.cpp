// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Super Street Fighter II Turbo: widescreen, as the other fighting games have it. A fight is shown 512 columns wide
// where the board shows 384 (16:9 for 4:3, the height and the size of everything as they were): more of the stage on
// each side. The fight is played in that picture too: the players can go to its edges, and the camera stops where its
// edges come to the stage's art's ends (64 nearer the middle at each end; on Cammy's a little further in, and its far
// planes drawn moved: fight/wide_ends.h). With the feature off none of that is there and the game is the board's. It
// is off in every preset: it changes how the game plays.
//
// With "Invisible walls" (widescreen_walls) as well, the fight is shown in the wide picture but played as on the
// board: nothing here touches the rules (the margin is 0), so the players are stopped where the board's picture ends
// and the camera goes as far as the board's does. Only the picture is wider, and near a stage's end the view is kept
// inside the stage, so the board's picture is then off the middle of the wide one.
//
// The rules the wide picture changes are written again here as readable C++ (rewrites: lift.h), each with the margin
// as a line in it: the camera following the players across (its ends 64 nearer the middle; a mode's wanted place:
// battle.h) and up (a mode's wanted place), and keeping the players inside the picture (64 further out on each side;
// the ring out lets one over an end). The panel is objects (ssf2t.h), which stay in the middle; the stage's scroll
// screens and its pieces go with the view.
#include "alternative.h"
#include "battle.h"
#include "court.h"
#include "ssf2t.h"
#include "state.h"
#include "wide_ends.h"

#include <algorithm>
#include <cstdlib>
#include <iterator>

namespace cps2recomp {

bool TagIsOff(uint32_t player); // livetag.cpp
int BattleStage();               // fight/battle.cpp

namespace {
    using namespace ssf2t;

    constexpr int kSide = Video::kWideSide;
    // The camera's record (ssf2t.h, kCamera), and in it
    constexpr uint32_t kPlaceX = 0x06, kPlaceY = 0x0A, kMovedX = 0x30, kMovedY = 0x32, kGoingX = 0x24, kGoingY = 0x25;
    // A player's (from 0CC4E4 and 0043F4): its half width, and what moves where it is seen from (while it reaches out:
    // the reach across, by its facing), the other player being where it is seen from (while held), and in a corner
    constexpr uint32_t kHalfWidth = 0x1C2, kReaching = 0x2A7, kReach = 0x2AA, kSeenAsOther = family::kSeenAsOther, kCornered = 0x48;
    constexpr uint32_t kNotKeptIn = family::kNotKeptIn, kHolding = 0x63; // (kSeenAsOther, kNotKeptIn: 0 in a game without)
    constexpr uint32_t kVarCamera = At(0xFF8ED4); // (A5+ED4, the camera's place across, as 0043F4 reads it)
    constexpr uint32_t kOnLedge = 0x163;      // a player's byte: its height is taken as the floor's (0CC64C)
    // Bytes and a word that hold the camera's place up still (0CC630): a flag of its own, the round's end, a super's flash
    constexpr uint32_t kCameraHeld = At(0xFF8F90), kRoundHeld = At(0xFF8DDE), kSuperHeld = At(0xFF8DDC);

    bool g_play = false;  // fights are played in the wide picture
    bool g_shown = false; // fights are shown in the wide picture (played in it, or played as on the board)
    int g_side = 0, g_shift = 0, g_cameraBefore = 0;

    // The court's (fight/court.h): the wide picture forced, the camera held in the stage's middle, and as much of the
    // stage to each side of the board's picture as all its planes are drawn there (each stage's own, by kStage, the same
    // both ways: judged from the court's picture at Video::kMaxSide), so the court is as wide as the stage is drawn.
    // (Another game's stage is drawn by that game, to 64 a side.)
    // (Ryu's castle roofs end, Ken's boats and Chun-Li's street come round again, Balrog's casino ends at 96, Cammy's and
    // Fei Long's planes behind come round again past 64; the rest to the board's camera's ends)
    // (a game of the family whose stages are drawn otherwise: family.h's SSF2T_FAMILY_COURT_SIDES)
#ifdef SSF2T_FAMILY_COURT_SIDES
    constexpr int kCourtSides[] = SSF2T_FAMILY_COURT_SIDES;
#else
    constexpr int kCourtSides[] = {128, 192, 144, 192, 128, 128, 192, 192, 192, 160, 96, 192, 64, 160, 64, 144};
#endif
    int Side() {
        if (!court::On()) {
            return kSide;
        }
        if (const char *forced = getenv("SSF2T_COURT_SIDE")) { // (for measuring a stage: its court at that side)
            return std::clamp(atoi(forced), 0, static_cast<int>(Video::kMaxSide));
        }
        const int stage = Ram16(kStage);
        return BattleStage() > kStages || stage < 0 || stage >= static_cast<int>(std::size(kCourtSides)) ? kSide : kCourtSides[stage];
    }

    int Margin() {
        return g_play ? Side() : 0;
    }
    // Where the wide picture would show a stage past what the board shows of it (tools/stage_ends.sh): only on Cammy's
    // castle ruins, whose floor's far lines go slower than the camera (the camera stops 6 further in) and whose two
    // planes behind go at 0.64 and 0.34 of its speed (drawn moved instead: their places with the camera at the board's
    // ends, for the 8-pixel and the 32-pixel scroll screens). On every other stage each plane goes with the camera.
    constexpr int kCammys = 12;
    constexpr wide::Ends kCammysEnds = {6, 6};
    constexpr int16_t kCammysPlanes[2][2] = {{324, 571}, {383, 512}};
    wide::Ends StageEnds() {
        return Ram16(kStage) == kCammys && !court::On() ? kCammysEnds : wide::Ends{}; // (the court's sides are its own)
    }
    // How far short of the board's ends the camera stops, at each end, in the picture played in
    int LeftMargin() {
        return g_play ? Side() + StageEnds().left : 0;
    }
    int RightMargin() {
        return g_play ? Side() + StageEnds().right : 0;
    }
    int16_t S16(uint32_t value) {
        return static_cast<int16_t>(value);
    }

    // --- the camera ---

    uint16_t Low(uint32_t reg) {
        return static_cast<uint16_t>(reg);
    }

    // Where a player is seen from by the camera, into a data register's low word: where it stands, or the other
    // player's place while the other holds it (its flag), and its reach while it reaches out (forward from where it
    // faces), which goes into `reach` (all of it: moveq)
    void SeenAt(M68K &c, int at, int reach, uint32_t player, uint32_t other) {
        // (the team battle's tag style: a player off the stage is not followed, the other's place taken for it)
        SetLow16(c.d[at], Ram16(((kSeenAsOther && Ram8(player + kSeenAsOther)) || TagIsOff(player) ? other : player) + kX));
        if (!family::kTurbo) {
            return; // (Super Street Fighter II: no reach, and its register left alone)
        }
        c.d[reach] = 0;
        if (Ram8(player + kReaching)) {
            SetLow16(c.d[reach], Ram16(player + kReach));
            if (!Ram8(player + kFacesRight)) {
                SetLow16(c.d[reach], static_cast<uint16_t>(0u - Low(c.d[reach])));
            }
            SetLow16(c.d[at], static_cast<uint16_t>(Low(c.d[at]) + Low(c.d[reach])));
        }
    }

    // The end of the camera's move across by D1 (right) or D0 (left): at most 6 a frame (D6), not past its end (in the
    // wide picture 64 nearer the middle); what it moved by kept for the scroll screens (kMovedX), and the way (kGoingX)
    void MoveAcross(M68K &c, uint32_t camera, bool right) {
        const int by = right ? 1 : 0;
        c.d[6] = right ? 6u : 0xFFFFFFFAu;
        if (right ? Low(c.d[1]) >= 6 : Low(c.d[0]) < 0xFFFA) {
            SetLow16(c.d[by], Low(c.d[6]));
        }
        const uint16_t before = Ram16(camera + kPlaceX);
        SetLow16(c.d[3], before);
        const uint16_t moved = static_cast<uint16_t>(before + Low(c.d[by]));
        SetRam16(camera + kPlaceX, moved);
        // (the end: in the wide picture 64 nearer the middle, or more)
        const uint16_t end = static_cast<uint16_t>(right ? Ram16(kCameraEndRight) - RightMargin() : Ram16(kCameraEndLeft) + LeftMargin());
        SetLow16(c.d[2], end);
        if (right ? end >= moved : end < moved) {
            SetRam16(camera + kMovedX, Low(c.d[by]));
        } else {
            SetRam16(camera + kPlaceX, end);
            SetLow16(c.d[2], static_cast<uint16_t>(end - before));
            SetRam16(camera + kMovedX, Low(c.d[2]));
        }
        SetRam8(camera + kGoingX, right ? 0x04 : 0x0C);
    }

    // 0CC4E4. The camera's place across (A6 its record): it keeps both players in the picture. Near together (their
    // outer edges less than 0x100 apart) it moves only when one comes within 64 of the picture's edge; further apart
    // (to 0x180) it keeps their middle in the picture's middle; at most 6 a frame, and not past its ends (in the wide
    // picture, 64 nearer the middle). A mode can want it at a place of its own (battle.h).
    void CameraAcross(M68K &c) {
        const uint32_t camera = c.a[6] & 0xFFFFFF;
        if (modes::CameraAt().x >= 0) {
            // (a mode wants the camera at a place of its own: it goes there, as far as its ends let it)
            const int end0 = Ram16(kCameraEndLeft) + LeftMargin(), end1 = Ram16(kCameraEndRight) - RightMargin();
            const uint16_t place = Ram16(camera + kPlaceX);
            const uint16_t wanted = static_cast<uint16_t>(std::clamp(modes::CameraAt().x, end0, end1));
            SetRam16(camera + kMovedX, static_cast<uint16_t>(wanted - place));
            SetRam16(camera + kPlaceX, wanted);
            return;
        }
        // (the two players' places, the left one first, each with its outer edge)
        SeenAt(c, 2, 4, Player(0), Player(1));
        SeenAt(c, 3, 5, Player(1), Player(0));
        c.a[1] = 0xFF000000u | Player(0);
        c.a[2] = 0xFF000000u | Player(1);
        if (!(S16(c.d[3]) > S16(c.d[2]))) {
            std::swap(c.d[2], c.d[3]);
            std::swap(c.a[1], c.a[2]);
            if (family::kTurbo) {
                std::swap(c.d[4], c.d[5]);
            }
        }
        const uint32_t first = c.a[1] & 0xFFFFFF, second = c.a[2] & 0xFFFFFF;
        SetLow16(c.d[2], static_cast<uint16_t>(Low(c.d[2]) - Ram16(first + kHalfWidth)));
        SetLow16(c.d[3], static_cast<uint16_t>(Low(c.d[3]) + Ram16(second + kHalfWidth)));
        SetLow16(c.d[0], Low(c.d[2]));
        SetLow16(c.d[1], static_cast<uint16_t>(Low(c.d[3]) - Low(c.d[2])));
        const uint16_t place = Ram16(camera + kPlaceX);
        if (Low(c.d[1]) < 0x100) {
            // (near together: moved only by the one near an edge)
            SetLow16(c.d[0], Low(c.d[2]));
            SetLow16(c.d[1], static_cast<uint16_t>(Low(c.d[3]) - place - 0x140));
            if (S16(c.d[1]) >= 0) {
                MoveAcross(c, camera, true);
                return;
            }
            SetLow16(c.d[0], static_cast<uint16_t>(Low(c.d[0]) - place - 0x40));
            if (S16(c.d[0]) < 0) {
                MoveAcross(c, camera, false);
            }
            return;
        }
        if (Low(c.d[1]) >= 0x180) {
            return;
        }
        // (further apart: their middle in the middle)
        const uint16_t reach1 = family::kTurbo ? Low(c.d[4]) : 0, reach2 = family::kTurbo ? Low(c.d[5]) : 0;
        uint16_t middle = static_cast<uint16_t>(Ram16(first + kX) + reach1 - Ram16(first + kHalfWidth) + Ram16(second + kX) + reach2 +
                                                Ram16(second + kHalfWidth));
        SetLow16(c.d[0], static_cast<uint16_t>((middle >> 1) - place - 0xC0));
        if (S16(c.d[0]) < 0) {
            MoveAcross(c, camera, false);
            return;
        }
        SetLow16(c.d[1], Low(c.d[0]));
        MoveAcross(c, camera, true);
    }

    // 0CC630. The camera's place up (A6 its record): it follows the higher player once that one is more than the floor's
    // height up (an eighth of how much more), at most 2 a frame, and not past its ends; not while the round's end or a
    // super's flash holds it. A mode can want it at a place of its own (the ring out's way down: beyond the ends).
    void CameraUp(M68K &c) {
        const uint32_t camera = c.a[6] & 0xFFFFFF;
        if (modes::CameraAt().y >= 0) {
            const CameraWanted &wanted = modes::CameraAt();
            const uint16_t place = Ram16(camera + kPlaceY);
            const uint16_t to = static_cast<uint16_t>(wanted.beyondEnds ? wanted.y :
                                                     std::clamp<int>(wanted.y, Ram16(kCameraEndBottom), Ram16(kCameraEndTop)));
            SetRam16(camera + kMovedY, static_cast<uint16_t>(to - place));
            SetRam16(camera + kPlaceY, to);
            return;
        }
        if ((kCameraHeld && Ram8(kCameraHeld)) || Ram8(kRoundHeld) || Ram16(kSuperHeld)) {
            return;
        }
        // (each player's height: the floor's for one standing on something, a ledge)
        SetLow16(c.d[0], Ram8(Player(0) + kOnLedge) ? kGround : Ram16(Player(0) + kY));
        SetLow16(c.d[1], Ram8(Player(1) + kOnLedge) ? kGround : Ram16(Player(1) + kY));
        if (!(Low(c.d[1]) < Low(c.d[0]))) {
            SetLow16(c.d[0], Low(c.d[1]));
        }
        if (!(Low(c.d[0]) < 0xA8)) {
            SetLow16(c.d[0], 0xA8);
        }
        const uint16_t place = Ram16(camera + kPlaceY);
        SetLow16(c.d[0], static_cast<uint16_t>((static_cast<uint16_t>(Low(c.d[0]) - kGround) >> 3) - place));
        SetLow16(c.d[2], place);
        const bool up = S16(c.d[0]) >= 0;
        SetRam8(camera + kGoingY, up ? 0x00 : 0x08);
        if (up ? S16(c.d[0]) >= 3 : !(S16(c.d[0]) > -3)) {
            c.d[0] = up ? 2u : 0xFFFFFFFEu; // (moveq)
        }
        const uint16_t moved = static_cast<uint16_t>(place + Low(c.d[0]));
        SetRam16(camera + kPlaceY, moved);
        const uint16_t end = up ? Ram16(kCameraEndTop) : Ram16(kCameraEndBottom);
        SetLow16(c.d[1], end);
        if (up ? end > moved : S16(end) < S16(moved)) {
            SetRam16(camera + kMovedY, Low(c.d[0]));
        } else {
            SetRam16(camera + kPlaceY, end);
            SetLow16(c.d[1], static_cast<uint16_t>(end - place));
            SetRam16(camera + kMovedY, Low(c.d[1]));
        }
    }

    // --- the players inside the picture ---

    // The player moved to `to` at an edge (`edge` 1 left, 2 right; `reach` what it reaches out by), and the one it holds
    // with it
    void ToEdge(M68K &c, uint32_t player, uint16_t to, uint16_t reach, uint8_t edge) {
        SetRam8(player + kCornered, edge);
        const uint16_t by = static_cast<uint16_t>(to - Ram16(player + kX));
        SetLow16(c.d[1], by);
        SetRam16(player + kX, static_cast<uint16_t>(to - reach));
        if (Ram8(player + kHolding)) {
            c.a[4] = static_cast<uint32_t>(static_cast<int16_t>(Ram16(player + kOpponent)));
            const uint32_t other = c.a[4] & 0xFFFFFF;
            SetRam16(other + kX, static_cast<uint16_t>(Ram16(other + kX) + by));
        }
    }

    // 0043F4. The player at A6 kept inside the picture (in the wide picture, its edges): no nearer an edge than its half
    // width (its reach, while it reaches out, counted in); one held is moved with it
    void KeepIn(M68K &c) {
        const uint32_t player = c.a[6] & 0xFFFFFF;
        if (kNotKeptIn && Ram8(player + kNotKeptIn)) {
            return;
        }
        SetRam8(player + kCornered, 0);
        if (kSeenAsOther && Ram8(player + kSeenAsOther)) {
            return;
        }
        // (the ring out: a player knocked about at an end of the stage goes over it; the tag style's off the stage)
        if (RingOutLetsOut(player) || TagIsOff(player)) {
            return;
        }
        const uint16_t shown = static_cast<uint16_t>(Ram16(kVarCamera) - Margin());
        const uint16_t half = Ram16(player + kHalfWidth);
        uint16_t leftmost = static_cast<uint16_t>(shown + half);
        SetLow16(c.d[0], leftmost);
        uint16_t at = Ram16(player + kX), reach = 0;
        if (family::kTurbo) {
            c.d[3] = 0; // (moveq; Super Street Fighter II has no reach, and leaves D2 and D3 alone)
        }
        if (family::kTurbo && Ram8(player + kReaching)) {
            reach = Ram16(player + kReach);
            if (!Ram8(player + kFacesRight)) {
                reach = static_cast<uint16_t>(0u - reach);
            }
            SetLow16(c.d[3], reach);
            at = static_cast<uint16_t>(at + reach);
        }
        if (family::kTurbo) {
            SetLow16(c.d[2], at);
        }
        if (!(S16(leftmost) < S16(at))) {
            ToEdge(c, player, leftmost, reach, 1);
        }
        const uint16_t across = static_cast<uint16_t>(0x180 + 2 * Margin() - half);
        SetLow16(c.d[1], across);
        const uint16_t rightmost = static_cast<uint16_t>(shown + across);
        SetLow16(c.d[0], rightmost);
        if (S16(rightmost) > S16(at)) {
            return;
        }
        ToEdge(c, player, rightmost, reach, 2);
    }

    // 090F6A and 0A3472 (Vega's jump to the wall, the Flying Barcelona's first part: two copies) and 0B3156 (a third,
    // without the stick), where D1 is the wall to jump to and the game goes on to the speeds to
    // get there in 20 frames, then clings once KeepIn corners him (A6 the player). The game takes the walls for the board's
    // picture's edges (the camera's place, and 0x180 right of it): the one the stick points at, or with the stick in the
    // middle (and in 0B3156 always) the nearer, tested unsigned (his place less the camera's, under 0xC0). In the wide
    // picture the walls are 64 further out, where KeepIn corners him, and a Vega in its added columns on the left was
    // taken to be near the right wall (his place less the camera's under 0: a great number unsigned), so he jumped the
    // wrong way and bounced back at the left wall. Played wide, the wall is the wide picture's edge, chosen signed.
    void ToWall(M68K &c, bool steered) {
        if (!Margin()) {
            return; // (the board's picture: the game's choice)
        }
        const uint32_t player = c.a[6] & 0xFFFFFF;
        const uint16_t camera = Ram16(kVarCamera);
        const uint16_t stick = steered ? static_cast<uint16_t>(Ram16(player + family::kButtonsNow) & 3) : 0;
        const bool right = stick ? !(stick & 2) : S16(static_cast<uint16_t>(Ram16(player + kX) - camera)) >= 0xC0;
        SetLow16(c.d[1], static_cast<uint16_t>(right ? camera + 0x180 + Margin() : camera - Margin()));
    }

    // --- the picture ---

    int CameraLeftEnd() {
        return Ram16(kCameraEndLeft);
    }
    int CameraRightEnd() {
        return Ram16(kCameraEndRight);
    }
    bool InFight() {
        return Ram16(kMode) == kModeGame && Ram16(kGameStep) == kGameMatch && Ram16(kMatchStep) >= 6 && Ram16(kMatchStep) <= 0x10;
    }
    bool OnStage(int camera) {
        return CameraRightEnd() >= CameraLeftEnd() + 2 * kSide && camera >= CameraLeftEnd() && camera <= CameraRightEnd();
    }

    // An object's place: the panel's stay in the middle, the stage's and the fighters' go with the view
    int MoveObject(uint32_t tile, uint16_t attributes, int) {
        return ThePanel(tile, attributes) ? 0 : g_shift;
    }

    // 003036, 00306E, 0030A6 (and a fourth in the World Warrior's line: family::kNearPicture1 to 4), from where each tests
    // the object's place across (A6 its record, A0 the record of the scroll screen it rides on, D0 and D1 its place): the
    // object is drawn only while it is within 0x40 of the board's picture across, and within the routine's room up and
    // down. A stage's pieces (Blanka's fish, Honda's lantern, Guile's crates, the breakable signs) go through these, so in
    // the wide picture they were left out in its added columns: played wide the room across is that much more on each
    // side. (With invisible walls the game stays the board's: the margin is 0 and the game's code runs.)
    struct Room {
        uint16_t across, above, down; // (0x40 left of the picture to `across` on; `above` it to `down` on)
    };
    constexpr Room kRooms[4] = {{0x200, 0x40, 0x180}, {0x240, 0x40, 0x1C0}, {0x280, 0x40, 0x200}, {0x280, 0x60, 0x280}};

    bool NearPicture(M68K &c, const Room &room) {
        if (!Margin()) {
            return false;
        }
        const uint32_t object = c.a[6] & 0xFFFFFF, layer = c.a[0] & 0xFFFFFF;
        SetLow16(c.d[0], static_cast<uint16_t>(Low(c.d[0]) - Ram16(layer + kPlaceX) + 0x40 + Margin()));
        SetLow16(c.d[1], static_cast<uint16_t>(Low(c.d[1]) - Ram16(layer + kPlaceY) + room.above));
        if (Low(c.d[0]) > room.across + 2 * Margin() || Low(c.d[1]) > room.down) {
            Return(c); // (not drawn)
            return true;
        }
        SetRam8(object + 1, static_cast<uint8_t>(Ram8(object + 1) + 1));
        c.pc = family::kNearPictureDrawn;
        return true;
    }

    // The wide picture's switches for the frame about to run (the port's, or a spare's): the camera's place now and
    // where it was for the picture being made (a frame behind)
    void SetWide(Machine &machine, int camera, int shown) {
        g_side = g_shift = 0;
        if (g_shown && OnStage(camera) && OnStage(shown)) {
            g_side = Side();
            // (the view's left edge is 64 left of the camera, and no further left than the camera's own end: nearer
            // the end than that, the stage is drawn to the side by what is missing. Played in the wide picture the
            // camera never goes there; with invisible walls it does, and the board's picture is then to one side)
            g_shift = wide::ViewShift(shown, CameraLeftEnd(), CameraRightEnd(), g_side, StageEnds());
        }
        Video::Wide &wide = machine.video.wide;
        wide.side = g_side;
        wide.scroll[0] = wide.scroll[1] = wide.scroll[2] = g_shift;
        if (g_side && Ram16(kStage) == kCammys) {
            // (Cammy's planes behind, kept inside their painting over the camera's range: fight/wide_ends.h)
            const int end0 = CameraLeftEnd(), end1 = CameraRightEnd();
            const wide::Ends ends = StageEnds();
            const int from = g_play ? end0 + g_side + ends.left : end0, to = g_play ? end1 - g_side - ends.right : end1;
            for (int n = 0; n < 2; n++) {
                const int16_t *places = kCammysPlanes[n];
                wide.scroll[n == 0 ? 0 : 2] = wide::FarScroll(places[1] - places[0], end1 - end0, shown, end0, end1, from, to, g_side, ends);
            }
        }
        wide.object = g_side ? MoveObject : nullptr;
        wide.tile = nullptr;
        wide.fillHoles = g_side != 0;
    }

    struct Kept {
        int side, shift;
    } g_kept;
} // namespace

bool WidePlay() {
    return g_play;
}
int WideMargin() {
    return Margin();
}
void WideEnds(int &left, int &right) {
    left = LeftMargin();
    right = RightMargin();
}
int WideSide() {
    return g_side;
}
int WideStage() {
    return g_side + g_shift;
}

// The camera's place a frame ago, which the picture is drawn from (it is a frame behind), in a save state: a frame
// put back (a save state, online play's rollback) is drawn, and its players kept in, as it was the first time
void WidescreenState(Archive &ar) {
    if (ar.version == 0 || ar.version >= 8) { // (a save state from before has none)
        ar.Pod(g_cameraBefore);
    }
}

void WidescreenBeforeFrame(Machine &machine) {
    const int camera = static_cast<int16_t>(Ram16(kCameraX)), shown = g_cameraBefore;
    g_cameraBefore = camera;
    // (fights only; the stage viewer keeps the board's picture. With invisible walls the rules stay the board's. The
    // court, fight/court.h: always the wide picture, played in it, for the room)
    const bool court = court::On();
    g_shown = (FeatureOn("widescreen") || court) && InFight() && !modes::StageViewer();
    g_play = g_shown && (!FeatureOn("widescreen_walls") || court);
    SetWide(machine, camera, shown);
}

// A spare machine's frame (the modes' other machines) is drawn as wide as the port's picture is, so that the two can
// be put together: from the spare's own camera (where it is now, for want of where it was)
void WideForSpare(Machine &machine) {
    g_kept = {g_side, g_shift};
    const int camera = static_cast<int16_t>(Ram16(kCameraX));
    SetWide(machine, camera, camera);
    // (exactly as wide as the port's picture, even where the spare is not on its stage yet)
    if (g_side != g_kept.side) {
        Video::Wide &wide = machine.video.wide;
        g_side = wide.side = g_kept.side;
        g_shift = 0;
        wide.scroll[0] = wide.scroll[1] = wide.scroll[2] = 0;
        wide.object = g_side ? MoveObject : nullptr;
    }
}
void WideForPort(Machine &) {
    g_side = g_kept.side;
    g_shift = g_kept.shift;
}

namespace {
    using namespace ssf2t;
    // Where a player's fighter stands in the picture, for the window's zoom (fight/alternative.h, modes::Focus): the
    // middle of its feet, the wide picture's columns counted
    void FocusPlace(int player, int &x, int &y) {
        x = static_cast<int16_t>(Ram16(Player(player) + kX)) - static_cast<int16_t>(Ram16(kCameraX)) + WideStage();
        y = 239 - (static_cast<int16_t>(Ram16(Player(player) + kY)) - static_cast<int16_t>(Ram16(kCameraY)));
    }
} // namespace

void RegisterWidescreen() {
    modes::Focus(FocusPlace, 40, 104);
    TheGame().widestPicture = [] { return Video::kWidth + (FeatureOn("widescreen") ? 2 * kSide : 0); };
    AddRewrite(At(0x0CC4E4), "CameraAcross", CameraAcross, kFreeFlags, 418);
    AddRewrite(At(0x0CC630), "CameraUp", CameraUp, kFreeFlags, 322);
    AddRewrite(At(0x0043F4), "KeepIn", KeepIn, kFreeFlags, 215);
    AddHook(HookAt(At(0x090F6A)), "ToWall", [](M68K &c) { return ToWall(c, true), false; });
    AddHook(HookAt(At(0x0A3472)), "ToWallOld", [](M68K &c) { return ToWall(c, true), false; });
    AddHook(HookAt(At(0x0B3156)), "ToWallComputer", [](M68K &c) { return ToWall(c, false), false; });
    AddHook(HookAt(family::kNearPicture1), "NearPicture1", [](M68K &c) { return NearPicture(c, kRooms[0]); });
    AddHook(HookAt(family::kNearPicture2), "NearPicture2", [](M68K &c) { return NearPicture(c, kRooms[1]); });
    AddHook(HookAt(family::kNearPicture3), "NearPicture3", [](M68K &c) { return NearPicture(c, kRooms[2]); });
    AddHook(HookAt(family::kNearPicture4), "NearPicture4", [](M68K &c) { return NearPicture(c, kRooms[3]); });
    // (not in the Enhanced preset: it shows more of the stage than the board does, and a player who wants the game as
    // it is played in competition must not get that without asking)
    TheGame().features.push_back({"widescreen", "Display", "Widescreen",
                                  "Fights are shown and played in 16:9, with more of the stage on each side: the players can go to "
                                  "the wide picture's edges, so they can stand further apart than on the board. Off unless switched "
                                  "on here, in every preset: it changes how the game plays.",
                                  kFeatureFirstPage});
    TheGame().features.push_back({"widescreen_walls", "Display", "Invisible walls",
                                  "With widescreen: fights are still shown in 16:9, but played exactly as on the board. The players "
                                  "are stopped where the board's picture ends, by walls that are not seen, and the camera goes as far "
                                  "as the board's; near a stage's end the view stays on the stage, so the play area is then to one "
                                  "side of the picture. Nothing without widescreen.",
                                  0});
}

} // namespace cps2recomp
