// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
//
// Court: the "Alternative modes" page's ball game (docs/COURT.md). The stage is a court cut in two by a line down the
// middle of the picture; each fighter stays on its own half, and the two hit a large ball that flies about the whole
// picture. A ball that leaves the picture past a fighter's end is the other's point; first to 3, 5 or 10 wins. The
// fighters' attacks hit the ball (and each other, nobody's health going down), harder attacks sending it faster, and where on the
// ball an attack lands says which way it goes. The ball bounces off a fighter like a soft body; one hit hard enough
// (a super, or a third hit in a row) knocks the fighter it hits down. The camera keeps both fighters and the ball in
// the picture (the zoom by distance, the ball counted: port/zoom.h).
//
// The toolkit's part is the game itself, the same for every fighting game: the ball's flight and bounces, what an
// attack does to it, the rules and the score, the computer's side, the ball drawn and the score shown, the menu's page.
// It runs in the battle modes' match (battle.h: who fights where, the game's computer kept out, no clock, no
// knock-out) as one of their kinds. A game's sources describe the rest (Description): the boxes of its fighters and
// what they throw, how strong an attack is, and the few things done to a fighter (frozen by a hit, knocked down, its
// gauge filled); they keep the fighters on their halves (KeepToHalf).
#pragma once

#include "game.h"
#include "machine.h"

#include <cstdint>
#include <vector>

namespace cps2recomp {
class Archive;
}

namespace cps2recomp::court {

// How hard an attack is, as the ball takes it
enum Power : uint8_t { kLight, kMedium, kHard, kSpecial, kSuper };

// A box in the stage (its middle and half sizes; y upwards, the game's own heights) of side `side`'s (0 player 1)
struct Shape {
    enum Kind : uint8_t { kBody, kAttack } kind = kBody;
    int side = 0;
    int x = 0, y = 0, halfWidth = 0, halfHeight = 0;
    // An attack's: how hard, which attack it is (the same number for as long as the same attack is out: an attack hits
    // the ball once), and whether it is thrown (a fireball and the like) rather than the fighter's own limb
    Power power = kLight;
    uint32_t id = 0;
    bool thrown = false;
};

struct Description {
    // The boxes of both sides now: the fighters' bodies (where the ball bounces off them) and every attack that is out,
    // the fighters' and what they have thrown
    void (*shapes)(Machine &machine, std::vector<Shape> &out) = nullptr;
    // The fighter of side `side`: where it stands across and how high, on the floor and able to act (the computer's
    // side asks), facing right
    int (*fighterX)(int side) = nullptr;
    int (*fighterY)(int side) = nullptr; // (its height: `floor` standing)
    int (*halfWidth)(int side) = nullptr;
    // ... and put where it stands (a game begun: each in the middle of its half)
    void (*place)(int side, int x) = nullptr;
    bool (*canAct)(int side) = nullptr;
    // ... and guarding now: holding back, free to guard (a ball blocked)
    bool (*guarding)(int side) = nullptr;
    // ... and in hitstun (reeling from a hit, knocked down and flying): the ball passes through it
    bool (*stunned)(int side) = nullptr;
    // The floor's height (the game's y of a fighter standing)
    int floor = 0;
    // The picture's line that height 0 is on with the camera at the bottom (as session.h's), and the wide picture's
    // columns to the left of the board's (0: none)
    int pictureFloor = 239;
    // ... and the lines at the picture's top that the ball stays under (the game's panel: health bars and clock)
    int pictureTop = 0;
    int (*wideStage)() = nullptr;
    // The picture's left edge less the camera's place across (the battle's camera: battle::Description::pictureLeft), and
    // for a game whose heights grow downwards, the camera's place down (the picture's top, as the battle has it) as a
    // height of the court's (y upwards, as `pictureFloor` counts it; nullptr: the battle's as it is)
    int pictureLeft = 0;
    int (*cameraHeight)(int cameraY) = nullptr;
    // A fighter's attack that has hit the ball: frozen for so many of the game's steps, as a hit freezes the attacker
    void (*freeze)(int side, int steps) = nullptr;
    // A fighter's special gauge filled by so much (the game's own units: `gaugeFull` a gauge)
    void (*addGauge)(int side, int amount) = nullptr;
    int gaugeFull = 0;
    // The panel's objects not drawn while a court game is played (the health bars and the clock: not the gauge), as
    // Video::hideObject has them (nullptr: the panel as it is)
    bool (*hidden)(uint32_t tile, uint16_t attributes) = nullptr;
    // The scroll screens not drawn while a court game is played (Video::layerMask's bits: 1 the objects, 2-8 the scroll
    // screens): a game whose bars and clock are a scroll screen of their own, which past the board's picture and 64 more
    // a side shows its map again (Street Fighter Alpha's)
    unsigned hiddenLayers = 0;
    // The panel's objects at the top of the picture (the health bars, the names, the clock: nothing to the ball game,
    // where health stays full and there is no clock), not drawn while a court game is played: the game's test of a
    // panel object, and the line (an object's y in the board's list) above which they are. The gauges, below, stay.
    bool (*panel)(uint32_t tile, uint16_t attributes) = nullptr;
    int panelTop = 80;
    // Each frame of a court game, before the ball moves: anything the game's sources keep so while it is played
    void (*everyFrame)() = nullptr;
    // What the game's own court sources keep from frame to frame (a gauge's last value, a knock-back's way...), in a
    // save state with the court's own: online play's rollback puts frames back and runs them again
    void (*state)(Archive &ar) = nullptr;
    // A sound asked for as the game asks (the battle's playSound)
    void (*playSound)(uint16_t code) = nullptr;
    // Sounds: a hit of each power, the ball bouncing off a fighter, a point won (0: none)
    uint16_t hitSounds[5] = {};
    uint16_t bodySound = 0, pointSound = 0;
};

// The game's description; the battle modes' page for it is made by battle::Register (the menu's rows: Register)
void Register(const Description &description);
bool Described();
// The menu's page and its rows (battle.cpp, with its Start and the battle's choices of fighter; the stage is the menu's
// Stage page, as every way of playing's)
void RegisterPage(const char *modes, void (*start)(Machine &machine), const std::vector<const char *> &fighters);

// A court game is being played (the battle's kind), and its fight is on (the ball in play or about to be)
bool On();
bool Playing();
// Where the line down the middle is (the stage's x), and the room kept on either side of it: a fighter's middle is
// kept at least its half width and this far from it (the game's sources: where it keeps the fighters apart)
int Line();
constexpr int kLineRoom = 6;
// The fighter of side `side` (0 player 1, on the left) at `x` kept on its half: the x it may have
int KeepToHalf(int side, int x, int halfWidth);
// What the ball has done to a fighter, for the game's sources at the fighter's step: once, the reaction it is to be put
// in (away from the ball: `away` -1 to the left, 1 to the right). A knock-down by a hot or fast ball, a reel by the
// ball's speed, or a guard (the fighter holding back: Description::guarding).
enum Reaction : uint8_t { kNoReaction, kGuard, kReelLight, kReelMedium, kReelHard, kKnockDown };
Reaction ReactionWanted(int side, int &away);

// The battle's match: begun (the camera's place, the fighters standing), each frame before and after the machine's
// (`one`, `two`: the controls, the computer's side made here), the score and the ball drawn (`lines` and the boxes),
// and save states. Over() says when a side has won the game: 1 player 1, 2 player 2 (0: not yet).
void Begin(int cameraX, int cameraY, int left, int right, int points, bool computer, int level);
void BeforeFrame(Machine &machine, uint16_t &one, uint16_t &two);
void Frame(Machine &machine, std::vector<HudLine> &lines, std::vector<HudBox> &boxes, int cameraX, int cameraY, int left, int right);
int Over();
void Again();
void End();
void State(Archive &ar);
// The game's objects not drawn while a court game is played (Description::hidden), for the battle
using Hide = bool (*)(uint32_t tile, uint16_t attributes);
Hide Hidden();
unsigned HiddenLayers(); // (Description::hiddenLayers, while a court game is played)
// The panel's objects at the top taken out of the object list (Video::objectsTaken), while a court game is played and
// the game has described its panel (HidesPanel)
bool HidesPanel();
void TakeObjects(uint16_t *list, int &last);
// For the camera's zoom (runtime/game.h, Game::focusAlso): where the ball is in the picture, and how far it reaches
bool Focus(int &x, int &y, int &reach);

} // namespace cps2recomp::court
