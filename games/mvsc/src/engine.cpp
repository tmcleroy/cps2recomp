// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Marvel vs. Capcom: the game described to the toolkit's code for the engine of Capcom's Marvel games
// (fight/marvel.h): the training aids' view of the players and their boxes, the ring out's movements, widescreen and
// the model viewer are done there, from where this game keeps what they need (mvsc.h).
#include "marvel.h"
#include "mvsc.h"

#include <iterator>


// Widescreen at each stage's ends (by the stage byte; fight/wide_ends.h, wide::Stage): how much further in than 64
// the camera stops for the plane the players stand in (the scroll screen going fastest with the camera) and its floor,
// at most 36; and the other scroll screens' places across with the camera at the board's ends, each a far plane drawn
// moved ({0, 0}: it goes with the view, as one going as fast as the players' plane does). Measured by
// tools/stage_ends.sh with every camera kept (kCameraKept).
static constexpr cps2recomp::wide::Stage kWideStages[] = {
    {{0, 0}, {{0, 0}, {0, 0}, {0, 0}}}, // 1
    {{0, 0}, {{0, 0}, {0, 0}, {0, 0}}}, // 2
    {{0, 0}, {{368, 656}, {0, 0}, {1120, 1312}}}, // 3
    {{0, 0}, {{0, 0}, {0, 0}, {0, 0}}}, // 4
    {{0, 0}, {{0, 0}, {0, 0}, {0, 0}}}, // 5
    {{0, 0}, {{0, 0}, {0, 0}, {48, 336}}}, // 6
    {{0, 0}, {{0, 288}, {0, 0}, {72, 312}}}, // 7
    {{0, 0}, {{0, 0}, {144, 240}, {168, 216}}}, // 8
    {{0, 0}, {{0, 0}, {64, 320}, {96, 288}}}, // 9
    {{0, 0}, {{112, 272}, {0, 0}, {0, 0}}}, // 10
};
namespace cps2recomp {

int CourtSide(); // court.cpp

using namespace mvsc;

void DescribeEngine() {
    marvel::Engine e;
    e.player1 = kPlayer1;
    e.playerSize = kPlayerSize;
    e.records = 4; // (the first characters, then the second ones)
    e.playerAt = Player; // (the one on point of each side)
    e.x = kX;
    e.y = kY;
    e.speedX = kSpeedX;
    e.speedY = kSpeedY;
    e.pull = kPullY;
    e.control = kControl;
    e.frame = kFrame;
    e.flip = kFlip;
    e.character = kCharacter;
    e.action = kAction;
    e.atEdge = 0xB9; // (held at the picture's edge: 013AD6)
    e.attackPhase = kAttackPhase;
    e.hitStop = kHitStop;
    e.boxTable = kBoxTable;
    e.attackBoxes = kAttackBoxes;
    e.hurtBoxes = kHurtBoxes;
    e.pushBox = kPushBox;
    e.canHit = kCanHit;
    e.canBeHit = kCanBeHit;
    e.pushTable = kPushTable;
    e.thrownList = kThrownList;
    e.thrownListSpace = kThrownListSpace;
    e.panel[0] = kPanel[0];
    e.panel[1] = kPanel[1];
    e.characterNames.assign(std::begin(kCharacterNames), std::end(kCharacterNames));
    e.stageNames.assign(std::begin(kStageNames), std::end(kStageNames));
    // (the model viewer: the players' characters, the hidden ones among them; not Onslaught, whose fights the game makes
    // its own way: put in the picks it is Ryu)
    for (int n = 0; n < kCharacters; n++) {
        if (Choosable(2 * n)) {
            e.viewerOrder.push_back(n);
        }
    }
    e.cameraRecord = CameraRecord;
    e.wideStages = kWideStages;
    e.wideStageCount = static_cast<int>(std::size(kWideStages));
    for (const KeptAt &kept : kCameraKept) {
        e.keepCameraPlaces.push_back({kept.at, kept.after});
    }
    e.cameraX = kCameraX;
    e.cameraY = kCameraY;
    e.cameraLeft = kCameraLeft;
    e.cameraRight = kCameraRight;
    e.cameraTop = kCameraTop;
    // (the camera's step works on a copy of its place: kept between its ends across at 0CDBB6, then down at 0CDBDA,
    // each copy put back after, at 0CDBCE and 0CDBF2)
    e.cameraWorkX = kCameraWorkX;
    e.cameraWorkY = kCameraWorkY;
    e.keepCameraDown = HookAt(0x0CDBDA);
    e.keptAcross = HookAt(0x0CDBCE);
    e.keptDown = HookAt(0x0CDBF2);
    e.pictureLeft = kPictureLeft;
    e.wall = kWall;
    e.planes = 0x02563A; // (longs by a character's byte +30: the camera's word of the plane it stands in; 013AE2)
    e.ground = Ground;
    e.cameraEnds = CameraEnds;
    e.stage = kStage;
    e.cameraWay = kCameraWay;
    e.followsOne = kCameraFollowsOne;
    e.cameraWayTwice = true; // (0CB2B2 goes by the byte two after it when a point character's +267 says)
    e.clock = kRoundClock;
    e.courtSide = CourtSide; // (Ball battle: court.cpp)
    e.playing = [] { return Ram8(kPlaying) != 0; };
    e.inFight = [] { return Ram8(kPlaying) != 0 && Ram16(kGameStep) == kGameMatch && Ram8(kFight) != 0; };
    e.notTheStage = NotTheStage;
    e.thePanel = ThePanel;
    e.player1Set = kPlayer1Set;
    e.player2Set = kPlayer2Set;
    e.shadowSet = kShadowSet;
    e.palettes = kPalettesInSet;
    // (the animations: groups of words of offsets from a list a character at 0E676C, which 0135E6 starts one from;
    // frames of 0x10 bytes, copied into the record by 001DFC)
    e.animationLists = 0x0E676C;
    e.frameBytes = 0x10;
    e.groups = 26;
    e.perGroup = 96;
    e.copyFrame = marvel::CopyFrameBoxSets; // (001DFC)
    marvel::Describe(e);
}

} // namespace cps2recomp
