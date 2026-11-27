// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
//
// Court (court.h; docs/COURT.md): the ball, the rules and the score, the computer's side, the ball drawn.
#include "court.h"

#include "battle.h"
#include "state.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace cps2recomp::court {

namespace {
    Description d;
    bool g_described = false;
    int g_frames = 0; // (the machine's frames, for the tests' logs: the --shot-every pictures' numbers)
    bool Logging() {
        static const bool log = getenv("CPS2RECOMP_COURT_LOG") != nullptr;
        return log;
    }

    // --- the ball (pixels and frames: the picture's sixtieths of a second) ---
    constexpr float kRadius = 15.0f;
    constexpr float kGravity = 0.13f;
    constexpr float kDrag = 0.997f;          // of its speed kept each frame in the air
    constexpr float kMostSpeed = 15.0f;
    constexpr float kFloorBounce = 0.72f;    // of its speed down kept by a bounce off the floor
    constexpr float kFloorGrip = 0.9f;       //   and of its speed across
    constexpr float kRoll = 0.985f;          // ... rolling along the floor, each frame
    constexpr float kCeilingBounce = 0.7f;   // off the top of the picture
    constexpr float kSpinGrip = 0.35f;       // of its spin turned into speed across at a bounce
    // A fighter's body is soft: of the speed into it kept (out again), and of the speed along it
    constexpr float kBodyBounce = 0.35f, kBodySlide = 0.8f;
    constexpr float kBodyRollOff = 1.6f, kBodyLift = 2.2f;
    constexpr float kBodyGentle = 4.0f; // (slower than this into a fighter, it rolls off towards the line) // (the least it goes off one sideways, and up off its top)
    // An attack: the ball's speed by its power, more for each hit of a side's in a row (the third: a power shot), and a
    // little more for each time the ball has crossed the line in the rally
    constexpr float kSpeeds[5] = {4.4f, 5.7f, 7.0f, 10.0f, 12.5f}; // (a special well beyond any normal)
    constexpr float kThrownSpeed = 0.9f;     // (a fireball's of its power)
    constexpr float kStreakMore = 0.12f, kPowerShot = 1.3f;
    constexpr int kStreakApart = 10;         // frames at least between hits that count as more than one in a row
    constexpr int kStreakFor = 70;           //   and at most
    constexpr float kRallyMore = 0.02f, kRallyMost = 1.25f;
    // Which way: forward (towards the line), up or down by where on the ball the attack lands, and a lift
    constexpr float kForward = 1.0f, kUpDown = 1.1f, kLift = 0.25f;
    constexpr int kFreezes[5] = {6, 9, 12, 16, 18};  // steps the attacker is frozen for, by power
    constexpr int kBallFreezes[5] = {5, 7, 10, 14, 16}; // frames the ball is held for (the hit's weight)
    constexpr int kGauge[5] = {3, 5, 7, 8, 0};       // of a hundred of a gauge, filled by a hit
    // A ball this fast into a fighter, hit by the other side, knocks it down (a super, a power shot)
    constexpr float kKnockDownSpeed = 9.6f;
    // A ball from the other side at least this fast makes a fighter reel, light, medium, then hard from these
    constexpr float kHurtSpeed = 3.5f, kReelMediumSpeed = 6.0f, kReelHardSpeed = 8.5f;
    // A light or medium hit leaves the ball drifting for a moment, slowly, so a chain combo can follow it up
    constexpr int kFloatFor = 14;
    constexpr float kFloatRate = 0.3f;
    // A light kick low under a ball on the floor chips it up, gently, to be struck again
    constexpr float kChipLow = 36.0f, kChipUp = 4.8f, kChipOn = 1.0f; // (up to about head height)
    // Aftertouch: up or down held by the striker while a hit holds the ball bends its flight, up to
    // this much (radians), by how many of the hold's frames it was held
    constexpr float kAftertouchMost = 0.44f;
    constexpr int kStrikeSlack = 14;         // pixels an attack's box reaches further for the ball (a big ball, met generously)
    constexpr int kComputerSlack[3] = {2, 6, 10}; // ... and more for the computer's, by its level (its play is not exact)
    constexpr int kBodyCool = 8;             // frames after a bounce off a fighter before it counts again
    constexpr int kOwnBodyCool = 14;         // frames a ball just struck passes through its striker's body

    // --- the game ---
    constexpr int kServeFor = 50;            // frames the served ball hangs before it drops
    constexpr float kServeHeight = 150.0f;   //   above the floor
    constexpr int kPointFor = 80;            // frames the point is shown before the next serve
    constexpr int kWonFor = 300;             // frames the winner is shown before another game
    constexpr int kPlaceFor = 3;             // frames the fighters are put at their places as a game begins

    enum Phase : int32_t { kOff, kServe, kPlay, kPoint, kWon };

    struct Ball {
        float x = 0, y = 0, vx = 0, vy = 0;
        float spin = 0, turn = 0;            // radians a frame, and how far it has turned
        int32_t held = 0;                    // frames it is held for still (a hit's weight)
        int32_t floaty = 0;                  // frames it drifts slowly after a light or medium hit (time for the next)
        int32_t holdFor = 0;                 // frames the hit's hold had in all (aftertouch: of them, up and down held)
        int32_t bend = 0;                    //   up held less down held, frames
        int32_t squash = 0;                  // frames of being squashed by a bounce (drawn)
        bool hot = false;                    // a power shot or a super: it can knock a fighter down
    };

    struct State {
        Phase phase = kOff;
        int32_t since = 0;
        Ball ball;
        Ball shown;                          // the ball as the picture now being made shows it (a frame behind)
        int32_t score[2] = {0, 0};
        int32_t points = 3;                  // the game's to win
        int32_t serveTo = 0;                 // the side the next serve drops on
        int32_t pointTo = -1, winner = -1;   // who won the last point, the game
        int32_t lastSide = -1;               // the side that hit the ball last
        uint32_t lastId[2] = {0, 0};         // the attack of each side that hit it last (an attack hits once)
        int32_t sinceHit[2] = {999, 999};
        int32_t streakSide = -1, streak = 0;
        int32_t crossings = 0;               // the rally's: times the ball has gone over the line
        int32_t ballHalf = 0;                // the half it is over (0 the left)
        int32_t still = 0;                   // frames it has lain still
        int32_t bodyCool[2] = {0, 0};
        int32_t leaving[2] = {0, 0};          // frames the ball is leaving a side's striker (its body let through)
        Reaction react[2] = {kNoReaction, kNoReaction};
        int32_t reactAway[2] = {0, 0};
        int32_t placeFor = 0;                // frames the fighters are still put at their starting places
        uint16_t controls[2] = {0, 0};       // each side's controls the frame before
        uint16_t button[2] = {0, 0};         //   and the attack button it pressed last (a light kick chips: Strike)
        int32_t said = 0;                    // frames a hit's word ("POWER SHOT") is shown for
        int32_t saidKind = 0;
        // The court: the picture's ends in the stage, the line, the camera's place, the floor
        int32_t left = 0, right = 384, line = 192;
        int32_t cameraX = 0, cameraY = 0;
        // The computer's side
        bool computer = false;
        int32_t level = 1;
        uint32_t random = 12345;
        struct Ai {
            bool on = false;                 // (this side is the computer's)
            int32_t think = 0, pressFor = 0, restFor = 0, jumpFor = 0, motion = 0;
            int32_t blockFor = 0; // (frames it holds back yet, to block a ball coming at it)
            bool decided = false; // (whether to block the ball coming at it has been decided)
            uint16_t press = 0, jumpWay = 0;
            int32_t targetX = 0;
        } ai[2];
    } g;

    float Floor() {
        return static_cast<float>(d.floor) + kRadius;
    }
    // The top of the picture, in the stage (the ball bounces off it: the court's ceiling)
    float Ceiling() {
        return static_cast<float>(g.cameraY + d.pictureFloor - d.pictureTop) - kRadius;
    }
    int Forward(int side) {
        return side == 0 ? 1 : -1;
    }
    float Speed(const Ball &b) {
        return std::sqrt(b.vx * b.vx + b.vy * b.vy);
    }
    void Cap(Ball &b) {
        const float speed = Speed(b);
        if (speed > kMostSpeed) {
            b.vx *= kMostSpeed / speed;
            b.vy *= kMostSpeed / speed;
        }
    }
    void Sound(uint16_t code) {
        if (code && d.playSound) {
            d.playSound(code);
        }
    }
    uint32_t Random() {
        g.random = g.random * 1103515245u + 12345u;
        return g.random >> 16;
    }

    void Serve(int side) {
        g.phase = kServe;
        g.since = 0;
        g.ball = {};
        g.ball.x = static_cast<float>(side == 0 ? (g.left + g.line) / 2 : (g.line + g.right) / 2);
        g.ball.y = static_cast<float>(d.floor) + kServeHeight;
        g.ball.held = kServeFor;
        g.lastSide = -1;
        g.lastId[0] = g.lastId[1] = 0;
        g.streakSide = -1;
        g.streak = 0;
        g.crossings = 0;
        g.ballHalf = side;
        g.still = 0;
        g.serveTo = side;
    }

    void Point(int side) {
        g.score[side]++;
        g.pointTo = side;
        g.since = 0;
        if (d.pointSound) {
            Sound(d.pointSound);
        }
        if (g.score[side] >= g.points) {
            g.phase = kWon;
            g.winner = side;
        } else {
            g.phase = kPoint;
        }
    }

    // --- an attack on the ball ---

    // The nearest point of a box to the ball's middle, and whether the ball reaches into the box
    bool Touches(const Shape &s, const Ball &b, float &nx, float &ny, float &depth) {
        // (an attack reaches a little further for the ball than for a fighter: a big ball is met generously)
        const int slack = s.kind != Shape::kAttack ? 0 : kStrikeSlack + (g.ai[s.side].on ? kComputerSlack[std::clamp(g.level, 0, 2)] : 0);
        const float cx = std::clamp(b.x, static_cast<float>(s.x - s.halfWidth - slack), static_cast<float>(s.x + s.halfWidth + slack));
        const float cy = std::clamp(b.y, static_cast<float>(s.y - s.halfHeight - slack), static_cast<float>(s.y + s.halfHeight + slack));
        float dx = b.x - cx, dy = b.y - cy;
        float distance = std::sqrt(dx * dx + dy * dy);
        if (distance >= kRadius) {
            return false;
        }
        if (distance < 0.01f) {
            // (its middle inside the box: out by the way from the box's middle)
            dx = b.x - static_cast<float>(s.x);
            dy = b.y - static_cast<float>(s.y);
            distance = std::max(std::sqrt(dx * dx + dy * dy), 0.01f);
            depth = kRadius;
        } else {
            depth = kRadius - distance;
        }
        nx = dx / distance;
        ny = dy / distance;
        return true;
    }

    void Hit(const Shape &s, float nx, float ny) {
        Ball &b = g.ball;
        const int side = s.side;
        // In a row: this side's hits, each long enough after the last and not too long
        if (g.streakSide == side && g.sinceHit[side] >= kStreakApart && g.sinceHit[side] <= kStreakFor) {
            g.streak++;
        } else if (g.streakSide != side || g.sinceHit[side] > kStreakFor) {
            g.streakSide = side;
            g.streak = 1;
        }
        const int power = static_cast<int>(s.power);
        float speed = kSpeeds[power] * (s.thrown ? kThrownSpeed : 1.0f);
        speed *= 1.0f + kStreakMore * static_cast<float>(std::min(g.streak, 3) - 1);
        const bool powerShot = g.streak == 3;
        if (powerShot) {
            speed *= kPowerShot;
            g.streak = 0;
        }
        speed *= std::min(1.0f + kRallyMore * static_cast<float>(g.crossings), kRallyMost);
        // Which way: forward (an attack drives the ball the way the fighter faces it: towards the other half), up or
        // down by where on the ball it landed (struck below its middle it goes up, above it down: a spike), and up a little
        const float below = std::clamp((b.y - static_cast<float>(s.y)) / (static_cast<float>(s.halfHeight) + kRadius), -1.0f, 1.0f);
        const float forward = static_cast<float>(Forward(side));
        float wx = forward * (kForward + 0.25f * std::max(nx * forward, 0.0f)); // (met squarely from behind: a little more so)
        float wy = kUpDown * below + kLift;
        const float length = std::max(std::sqrt(wx * wx + wy * wy), 0.01f);
        wx /= length;
        wy /= length;
        b.vx = wx * speed;
        b.vy = wy * speed;
        // A light kick low under a ball on the floor: chipped up, gently, to be struck again (the kick told by the button
        // last pressed)
        const bool chip = s.power == kLight && !s.thrown && g.button[side] == kKick1 && static_cast<float>(s.y - d.floor) < kChipLow && b.y < Floor() + kRadius + 10;
        if (chip) {
            b.vx = forward * kChipOn;
            b.vy = kChipUp;
            if (Logging()) {
                fprintf(stderr, "court f%d chip side %d\n", g_frames, side);
            }
        }
        // Spin: struck above its middle, it turns forward (topspin: a bounce sends it on); below, back
        b.spin = std::clamp(-below * static_cast<float>(Forward(side)) * speed * 0.02f, -0.35f, 0.35f);
        Cap(b);
        b.hot = powerShot || s.power == kSpecial || s.power == kSuper; // (a special, like a super, knocks down who it hits)
        b.held = kBallFreezes[power] + (b.hot ? 4 : 0);
        // (a light or medium hit, not a chip: the ball drifts a moment, for the next of a chain)
        b.floaty = !chip && !b.hot && (s.power == kLight || s.power == kMedium) ? kFloatFor : 0;
        b.holdFor = b.held;
        b.bend = 0;
        b.squash = 6;
        g.lastSide = side;
        g.lastId[side] = s.id;
        g.leaving[side] = kOwnBodyCool; // (it is leaving the striker: its own body does not stop it)
        g.sinceHit[side] = 0;
        g.still = 0;
        if (!s.thrown && d.freeze) {
            d.freeze(side, kFreezes[power] + (b.hot ? 4 : 0));
        }
        if (d.addGauge && d.gaugeFull) {
            d.addGauge(side, d.gaugeFull * kGauge[power] / 100);
        }
        if (d.hitSounds[power]) {
            Sound(d.hitSounds[power]);
        }
        if (Logging()) {
            fprintf(stderr, "court f%d hit side %d power %d thrown %d streak %d speed %.1f way %.2f,%.2f at %.0f,%.0f\n", g_frames, side, power, s.thrown, g.streak,
                    Speed(b), wx, wy, b.x, b.y);
        }
        if (b.hot) {
            g.said = 50;
            g.saidKind = powerShot ? 1 : s.power == kSpecial ? 5 : 2;
        }
    }

    // The ball and the fighters: attacks strike it, bodies turn it away
    std::vector<Shape> g_shown; // (for tests, CPS2RECOMP_COURT_BOXES=1: the boxes the ball met, drawn)

    void Contacts(Machine &machine) {
        std::vector<Shape> shapes;
        d.shapes(machine, shapes);
        static const bool boxes = getenv("CPS2RECOMP_COURT_BOXES") != nullptr;
        if (boxes) {
            g_shown = shapes;
            int attacks = 0;
            for (const Shape &s : shapes) {
                attacks += s.kind == Shape::kAttack;
            }
            fprintf(stderr, "court f%d shapes %zu attacks %d ball %.0f,%.0f cam %d,%d\n", g_frames, shapes.size(), attacks, g.ball.x, g.ball.y, g.cameraX, g.cameraY);
        }
        Ball &b = g.ball;
        // (an attack hits the ball once while it is out: the same button again, a new attack, hits it again)
        for (int side = 0; side < 2; side++) {
            const bool out = std::any_of(shapes.begin(), shapes.end(), [&](const Shape &s) {
                return s.kind == Shape::kAttack && s.side == side && s.id == g.lastId[side];
            });
            if (!out) {
                g.lastId[side] = 0;
            }
        }
        // The strongest attack touching it, of a side that has not hit it with that attack already
        const Shape *best = nullptr;
        float bestX = 0, bestY = 0;
        for (const Shape &s : shapes) {
            float nx = 0, ny = 0, depth = 0;
            if (s.kind != Shape::kAttack || s.id == g.lastId[s.side] || !Touches(s, b, nx, ny, depth)) {
                continue;
            }
            // (a fighter strikes the ball over its own half, not reaching over the line for it)
            // (but what it has thrown hits the ball anywhere)
            const float over = (b.x - static_cast<float>(g.line)) * static_cast<float>(Forward(s.side));
            if (!s.thrown && over > kRadius) {
                continue;
            }
            if (!best || s.power > best->power) {
                best = &s;
                bestX = nx;
                bestY = ny;
            }
        }
        if (best) {
            Hit(*best, bestX, bestY);
            return;
        }
        // Bodies: soft, the ball sent off them, and fast enough from the other side, knocked down
        for (const Shape &s : shapes) {
            float nx = 0, ny = 0, depth = 0;
            // (a fighter in hitstun is passed through: hit, it is out of the play until it recovers)
            if (s.kind != Shape::kBody || g.leaving[s.side] > 0 || (d.stunned && d.stunned(s.side)) || !Touches(s, b, nx, ny, depth)) {
                continue;
            }
            b.x += nx * depth;
            b.y += ny * depth;
            const float into = b.vx * nx + b.vy * ny;
            if (into >= 0 || g.bodyCool[s.side] > 0) {
                continue;
            }
            // (hit by the other side: a hot ball, or one fast enough, knocks the fighter down; any other that is not slow
            // makes it reel, by how fast; unless it is guarding, which blocks even a knock-down)
            const float speed = Speed(b);
            const bool other = g.lastSide >= 0 && g.lastSide != s.side;
            const bool knocks = other && (speed >= kKnockDownSpeed || (b.hot && speed >= kKnockDownSpeed * 0.8f));
            const bool hits = other && speed >= kHurtSpeed;
            Reaction reaction = kNoReaction;
            if ((knocks || hits) && d.guarding && d.guarding(s.side)) {
                reaction = kGuard;
            } else if (knocks) {
                reaction = kKnockDown;
                g.said = 50;
                g.saidKind = 3;
            } else if (hits) {
                reaction = speed < kReelMediumSpeed ? kReelLight : speed < kReelHardSpeed ? kReelMedium : kReelHard;
            }
            if (reaction != kNoReaction) {
                if (Logging()) {
                    fprintf(stderr, "court f%d reaction side %d %d speed %.1f\n", g_frames, s.side, reaction, speed);
                }
                g.react[s.side] = reaction;
                g.reactAway[s.side] = b.vx >= 0 ? 1 : -1;
                if (reaction != kGuard && reaction != kKnockDown) {
                    Sound(d.hitSounds[reaction == kReelLight ? kLight : reaction == kReelMedium ? kMedium : kHard]);
                }
            }
            // Out again: what went into the body, less what a soft body takes; along it, a little slowed
            const float tx = b.vx - into * nx, ty = b.vy - into * ny;
            b.vx = tx * kBodySlide - into * kBodyBounce * nx;
            b.vy = ty * kBodySlide - into * kBodyBounce * ny;
            // ... never left lying on a fighter: rolled off and lifted. A gentle one (a serve dropped on a head) goes
            // towards the line, never off the back of the fighter to its own end; a fast one away from its middle
            const float off = b.x - static_cast<float>(s.x);
            const float way = speed < kBodyGentle || std::fabs(off) <= 2.0f ? static_cast<float>(Forward(s.side)) : (off > 0 ? 1.0f : -1.0f);
            if (b.vx * way < kBodyRollOff) {
                b.vx = way * kBodyRollOff;
            }
            if (ny > 0.5f && b.vy < kBodyLift) {
                b.vy = kBodyLift;
            }
            b.spin *= 0.5f;
            b.squash = 5;
            b.hot = false;
            g.bodyCool[s.side] = kBodyCool;
            if (Logging()) {
                fprintf(stderr, "court f%d body side %d speed %.1f -> %.1f,%.1f\n", g_frames, s.side, speed, b.vx, b.vy);
            }
            if (d.bodySound) {
                Sound(d.bodySound);
            }
            break;
        }
    }

    void Fly() {
        Ball &b = g.ball;
        if (b.squash > 0) {
            b.squash--;
        }
        if (b.held > 0) {
            b.held--;
            if (b.held == 0 && b.bend != 0 && b.holdFor > 0) {
                // (aftertouch: its way turned up or down as the striker held, the speed kept)
                // (its angle over the floor, the way it is going, bent and kept short of straight up or down: it never
                // turns back)
                const float way = b.vx >= 0 ? 1.0f : -1.0f, speed = Speed(b);
                const float angle = std::clamp(std::atan2(b.vy, std::fabs(b.vx)) +
                                                   kAftertouchMost * static_cast<float>(b.bend) / static_cast<float>(b.holdFor),
                                               -1.3f, 1.35f);
                b.vx = way * speed * std::cos(angle);
                b.vy = speed * std::sin(angle);
                if (Logging()) {
                    fprintf(stderr, "court f%d aftertouch %d of %d: v %.1f,%.1f\n", g_frames, b.bend, b.holdFor, b.vx, b.vy);
                }
            }
            return;
        }
        // (drifting after a light or medium hit: everything slowed, gravity too, then the full flight)
        const float rate = b.floaty > 0 ? kFloatRate : 1.0f;
        if (b.floaty > 0) {
            b.floaty--;
        }
        b.vy -= kGravity * rate;
        b.vx *= kDrag;
        b.vy *= kDrag;
        b.x += b.vx * rate;
        b.y += b.vy * rate;
        b.turn += (b.vx / kRadius * 0.5f + b.spin) * rate;
        // The floor
        if (b.y <= Floor()) {
            b.y = Floor();
            if (b.vy < -1.2f) {
                b.vy = -b.vy * kFloorBounce;
                b.vx = b.vx * kFloorGrip + b.spin * kSpinGrip * kRadius;
                b.spin *= 0.5f;
                b.squash = 4;
                b.hot = false; // (a bounce takes the sting out of it)
            } else {
                b.vy = 0;
                b.vx *= kRoll;
                b.spin *= 0.9f;
            }
        }
        // The top of the picture
        if (b.y >= Ceiling() && b.vy > 0) {
            b.y = Ceiling();
            b.vy = -b.vy * kCeilingBounce;
        }
        Cap(b);
    }

    // --- the computer's side ---

    // Where the ball comes down to a height a fighter can strike it at, on this side's half: worked out by flying a
    // copy of it on (no fighters), as far as a second ahead
    bool Landing(int side, float &x, int &frames) {
        Ball b = g.ball;
        // (lying or rolling on the floor over its half: where it is now, a little on)
        if (b.y <= Floor() + 1.0f && std::fabs(b.vy) < 1.0f && b.held == 0) {
            x = b.x + b.vx * 10;
            frames = 0;
            return side == 0 ? b.x < g.line : b.x > g.line;
        }
        for (int f = 0; f < 60; f++) {
            if (b.held > 0) {
                b.held--;
            } else {
                b.vy -= kGravity;
                b.x += b.vx;
                b.y += b.vy;
                if (b.y <= Floor()) {
                    b.y = Floor();
                    b.vy = -b.vy * kFloorBounce;
                }
                if (b.y >= Ceiling() && b.vy > 0) {
                    b.vy = -b.vy * kCeilingBounce;
                }
            }
            const bool ours = side == 0 ? b.x < g.line : b.x > g.line;
            if (ours && b.y < static_cast<float>(d.floor) + 110.0f && b.vy <= 0) {
                x = b.x;
                frames = f;
                return true;
            }
        }
        return false;
    }

    constexpr int kAttackComes = 7; // (frames an attack takes to come out, about)
    constexpr float kLow = 38.0f;   // (a ball this low over a fighter's feet: met with a crouching attack)

    uint16_t Computer(int side, State::Ai &a) {
        const int me = d.fighterX(side), meY = d.fighterY ? d.fighterY(side) : d.floor;
        const int forward = Forward(side);
        uint16_t out = 0;
        // (a press is held for a few frames, then let go before the next)
        if (a.pressFor > 0) {
            a.pressFor--;
            out |= a.press;
            if (a.pressFor == 0) {
                a.restFor = 5;
            }
        } else if (a.restFor > 0) {
            a.restFor--;
        }
        if (a.jumpFor > 0) {
            a.jumpFor--;
            out |= a.jumpWay;
        }
        if (g.phase != kPlay && g.phase != kServe) {
            return out;
        }
        static constexpr int kThinkEvery[3] = {14, 8, 4};
        static constexpr int kWobble[3] = {36, 18, 6};
        const int level = std::clamp(g.level, 0, 2);
        // Thinking, now and then (slower at the easy level): where to stand to meet the ball
        if (--a.think <= 0) {
            a.think = kThinkEvery[level];
            float x = 0;
            int frames = 0;
            if (Landing(side, x, frames)) {
                // (a little behind the ball, so that an attack forward meets it)
                a.targetX = static_cast<int>(x) - forward * 34;
            } else {
                // (the ball over the other half: ready in the middle of its own, a step back from the line)
                a.targetX = side == 0 ? (g.left + 2 * g.line) / 3 - 10 : (2 * g.line + g.right) / 3 + 10;
            }
            a.targetX += static_cast<int>(Random() % (2 * kWobble[level] + 1)) - kWobble[level];
        }
        const Ball &b = g.ball;
        const bool air = meY > d.floor + 4;
        // Where the ball will be from the fighter `frames` on: ahead (towards the line), and above its feet
        auto soon = [&](int frames, float &ahead, float &above) {
            Ball c = b;
            for (int f = 0; f < frames; f++) {
                if (c.held > 0) {
                    c.held--;
                    continue;
                }
                c.vy -= kGravity;
                c.x += c.vx;
                c.y = std::max(c.y + c.vy, Floor());
            }
            ahead = (c.x - static_cast<float>(me)) * static_cast<float>(forward);
            above = c.y - static_cast<float>(meY);
        };
        // A fast ball coming at it: at the medium and hard levels, often blocked rather than met (an attack that is slow to
        // come out leaves the fighter hit in it; Alpha 2's are slower than kComes says), decided once for each ball that
        // comes and held to while it comes
        {
            const float toward = -b.vx * static_cast<float>(forward); // (its speed towards this fighter)
            const float distance = (b.x - static_cast<float>(me)) * static_cast<float>(forward);
            const bool coming = toward > kHurtSpeed && distance > 0 && distance < 70 + toward * 8 && b.y - static_cast<float>(meY) < 130;
            if (!coming) {
                a.decided = false;
                a.blockFor = 0;
            } else if (!a.decided) {
                a.decided = true;
                const bool fast = b.hot || toward >= kKnockDownSpeed * 0.7f;
                static constexpr int kBlockChance[3][2] = {{0, 0}, {15, 45}, {40, 85}}; // percent: by level, slower or fast
                a.blockFor = static_cast<int>(Random() % 100) < kBlockChance[level][fast ? 1 : 0] ? 40 : 0;
            }
            if (a.blockFor > 0 && !air && a.pressFor == 0) {
                a.blockFor--;
                return static_cast<uint16_t>(out | (forward > 0 ? kLeft : kRight));
            }
        }
        // Striking: an attack whose button meets the ball as it comes out (a light one soonest, a hard one furthest), the
        // harder ones tried first at the harder levels
        static constexpr uint16_t kButtons[6] = {kPunch1, kPunch2, kPunch3, kKick1, kKick2, kKick3};
        static constexpr int kComes[3] = {4, 6, 9};      // frames each strength's attack takes to come out, about
        static constexpr int kReach[3] = {50, 58, 64};   // and how far in front it reaches
        if (a.pressFor == 0 && a.restFor == 0 && a.motion == 0 && (air || !d.canAct || d.canAct(side))) {
            const int order[3][3] = {{0, 1, 2}, {1, 2, 0}, {2, 1, 0}};
            const int kick = static_cast<int>(Random() % 2);
            for (const int strength : order[level]) {
                float ahead = 0, above = 0;
                soon(kComes[strength], ahead, above);
                const bool meets = air ? ahead > -12 && ahead < kReach[strength] - 6 && above > -60 && above < 80
                                       : ahead > 4 && ahead < kReach[strength] && above > -6 && above < 96;
                if (!meets) {
                    continue;
                }
                a.press = kButtons[kick * 3 + strength];
                // (a ball on the floor: a crouching kick, low)
                if (!air && above < kLow) {
                    a.press = static_cast<uint16_t>(kDown | kButtons[3 + strength]);
                }
                a.pressFor = 3;
                if (Logging()) {
                    fprintf(stderr, "court f%d press side %d %04X ahead %.0f above %.0f air %d\n", g_frames, side, a.press, ahead, above, air);
                }
                return static_cast<uint16_t>(out | a.press);
            }
        }
        float ahead = 0, above = 0;
        soon(kAttackComes, ahead, above);
        // Now and then a special instead, the ball a little further off (a quarter circle forward and a punch: most
        // fighters have one); not at the easy level
        if (!air && a.motion == 0 && a.pressFor == 0 && a.restFor == 0 && level > 0 && ahead > 40 && ahead < 120 && above > 10 &&
            above < 110 && Random() % (level == 2 ? 6 : 14) == 0) {
            a.motion = 1;
        }
        if (a.motion > 0) {
            // (down, down-forward, forward and the punch: three frames each)
            const uint16_t way = forward > 0 ? kRight : kLeft;
            const uint16_t steps[4] = {kDown, static_cast<uint16_t>(kDown | way), way, static_cast<uint16_t>(way | (kPunch1 << (Random() % 3)))};
            const int step = (a.motion - 1) / 3;
            a.motion = step >= 3 && (a.motion - 1) % 3 == 2 ? 0 : a.motion + 1;
            if (a.motion == 0) {
                a.restFor = 12;
            }
            return static_cast<uint16_t>(out | steps[std::min(step, 3)]);
        }
        if (air || !d.canAct || !d.canAct(side)) {
            return out;
        }
        // Up to meet one coming down overhead: a jump towards it
        // (only for a ball over its own half, and staying there: never at one over the line)
        const auto ours = [&](float x) { return side == 0 ? x < static_cast<float>(g.line) - kRadius : x > static_cast<float>(g.line) + kRadius; };
        if (a.jumpFor == 0 && above > 100 && above < 230 && std::fabs(b.x - static_cast<float>(me)) < 70 && b.vy < 1.5f && ours(b.x) &&
            ours(b.x + b.vx * 10)) {
            const float way = b.x - static_cast<float>(me) + b.vx * 10;
            a.jumpWay = static_cast<uint16_t>(kUp | (std::fabs(way) < 12 ? 0 : way > 0 ? kRight : kLeft));
            a.jumpFor = 4;
            out |= a.jumpWay;
            return out;
        }
        // A fast ball coming at it that it is not striking, blocked (held back) at the medium and hard levels
        if (level > 0 && a.pressFor == 0) {
            const float toward = -b.vx * static_cast<float>(forward); // (its speed towards this fighter)
            const float distance = (b.x - static_cast<float>(me)) * static_cast<float>(forward);
            if (toward > kHurtSpeed && distance > 0 && distance < 70 + toward * 6 && b.y - static_cast<float>(meY) < 130) {
                return static_cast<uint16_t>(out | (forward > 0 ? kLeft : kRight));
            }
        }
        // A ball low behind it, on its half: over it with a jump back (walking back into it, its body would push it
        // off its own end)
        const float behind = (static_cast<float>(me) - b.x) * static_cast<float>(forward);
        if (a.jumpFor == 0 && behind > 8 && behind < 75 && b.y - static_cast<float>(d.floor) < 70 && ours(b.x)) {
            a.jumpWay = static_cast<uint16_t>(kUp | (forward > 0 ? kLeft : kRight));
            a.jumpFor = 4;
            return static_cast<uint16_t>(out | a.jumpWay);
        }
        if (me < a.targetX - 8) {
            out |= kRight;
        } else if (me > a.targetX + 8) {
            out |= kLeft;
        }
        return out;
    }

    // --- drawn ---

    void Disc(std::vector<HudBox> &boxes, float x, float y, float w, float h, uint8_t r, uint8_t gr, uint8_t bl, uint8_t fill = 255) {
        HudBox box;
        box.x = static_cast<int>(std::lround(x - w / 2));
        box.y = static_cast<int>(std::lround(y - h / 2));
        box.width = std::max(static_cast<int>(std::lround(w)), 1);
        box.height = std::max(static_cast<int>(std::lround(h)), 1);
        box.r = r;
        box.g = gr;
        box.b = bl;
        box.fill = fill;
        box.round = true;
        boxes.push_back(box);
    }

    void Draw(std::vector<HudBox> &boxes, int cameraX, int cameraY) {
        const Ball &b = g.shown;
        if (g.phase == kOff) {
            return;
        }
        const int wide = d.wideStage ? d.wideStage() : 0;
        // The line down the middle: faint, from the floor up
        {
            const int floorRow = d.pictureFloor - (d.floor - cameraY);
            HudBox line;
            line.x = g.line - cameraX + wide - 1;
            line.y = 0;
            line.width = 2;
            line.height = floorRow + 4;
            line.r = line.g = line.b = 255;
            line.fill = 255;
            line.round = false;
            for (int y = 0; y < line.height; y += 8) {
                HudBox dash = line;
                dash.y = y;
                dash.height = std::min(5, line.height - y);
                dash.fill = 120;
                dash.round = true; // (no outline)
                dash.r = 255, dash.g = 255, dash.b = 255;
                boxes.push_back(dash);
            }
        }
        for (const Shape &s : g_shown) {
            HudBox box;
            box.x = s.x - s.halfWidth - cameraX + wide;
            box.y = d.pictureFloor - (s.y - cameraY) - s.halfHeight;
            box.width = s.halfWidth * 2;
            box.height = s.halfHeight * 2;
            box.r = s.kind == Shape::kAttack ? 255 : 70;
            box.g = s.kind == Shape::kAttack ? 40 : 130;
            box.b = s.kind == Shape::kAttack ? 40 : 255;
            boxes.push_back(box);
        }
        if (g.phase == kWon || g.phase == kPoint) {
            return;
        }
        // (served: it blinks while it hangs)
        if (g.phase == kServe && (g.since / 4) % 2 == 1 && g.since < kServeFor - 10) {
            return;
        }
        const float px = b.x - static_cast<float>(cameraX) + static_cast<float>(wide);
        const float py = static_cast<float>(d.pictureFloor) - (b.y - static_cast<float>(cameraY));
        // Its shadow on the floor, smaller the higher it is
        const float floorY = static_cast<float>(d.pictureFloor) - static_cast<float>(d.floor - cameraY);
        const float high = std::clamp((b.y - Floor()) / 200.0f, 0.0f, 1.0f);
        Disc(boxes, px, floorY, kRadius * 2 * (1.0f - 0.5f * high), 6, 0, 0, 0, static_cast<uint8_t>(110 - 60 * high));
        // Squashed by a bounce
        const float squash = b.squash > 0 ? 0.06f * static_cast<float>(b.squash) : 0.0f;
        const float w = kRadius * 2 * (1.0f + squash), h = kRadius * 2 * (1.0f - squash);
        // A hot ball glows
        const bool glow = b.hot && (g.since / 2) % 2 == 0;
        Disc(boxes, px, py, w + 2, h + 2, 16, 12, 20);
        if (glow) {
            Disc(boxes, px, py, w, h, 255, 120, 30);
            Disc(boxes, px - 1, py - 1, w - 5, h - 5, 255, 200, 80);
        } else {
            Disc(boxes, px, py, w, h, 150, 40, 60);
            Disc(boxes, px - 1, py - 1, w - 5, h - 5, 230, 70, 90);
        }
        // Its seam, two marks round its edge that turn with it
        for (int n = 0; n < 2; n++) {
            const float a = b.turn + static_cast<float>(n) * 3.14159f;
            Disc(boxes, px + std::cos(a) * (w / 2 - 4), py + std::sin(a) * (h / 2 - 4), 5, 5, 250, 235, 210);
        }
        // A shine
        Disc(boxes, px - w * 0.22f, py - h * 0.24f, 6, 5, 255, 245, 245);
    }
} // namespace

void Register(const Description &description) {
    d = description;
    g_described = true;
}
bool Described() {
    return g_described;
}

void RegisterPage(const char *modes, void (*start)(Machine &machine), const std::vector<const char *> &fighters) {
    if (!g_described) {
        return;
    }
    Game &game = TheGame();
    static constexpr const char *kCourt = "Ball battle"; // (the menu page's name: "the court" in these sources)
    game.pages.push_back({kCourt, modes, "A ball game: each fighter on a half of the court, hitting a big ball past the other."});
    game.commands.push_back({"court_start", kCourt, "Start",
                             "A line splits the stage in two and each fighter keeps to a half. Hit the ball with your attacks "
                             "(the harder the attack, the faster it flies; where you hit it says which way) past the other "
                             "fighter and off their end of the court for a point. Three hits in a row make a power shot, and a "
                             "super or a power shot into a fighter knocks them down. The ball is served on the side that lost "
                             "the point.",
                             start});
    game.features.push_back({"court_points", kCourt, "First to", "Points that win a game.", 0, {"3", "5", "10"}});
    game.features.push_back({"court_opponent", kCourt, "Player 2 side", "Who plays the right half: the CPU, or player 2.", 0, {"CPU", "Player 2"}});
    game.features.push_back({"court_level", kCourt, "CPU level", "How well the CPU plays the ball.", 0, {"Easy", "Medium", "Hard"}});
    game.features.push_back({"court_camera", kCourt, "Camera",
                             "The whole court always, or close in on the play (both fighters and the ball, closer as they come "
                             "together, out to the whole court as they part).",
                             0, {"Whole court", "Follow the play"}});
    // (the Display page's own widescreen: the same setting, here too because the court is so much better for it)
    game.features.push_back({"widescreen", kCourt, "Widescreen (recommended)",
                             "The court shown 16:9, not 4:3: more of it at once. The same as the Display page's Widescreen.", 0});
    game.features.push_back({"court_one", kCourt, "Fighter 1", "Player 1's fighter: someone at random, or this one.", 0, fighters});
    game.features.push_back({"court_two", kCourt, "Fighter 2", "Player 2's (or the CPU's) fighter: someone at random, or this one.", 0, fighters});
    // (the fighters' rows and the stage are this game's own: the collection's launcher is not to set another game's)
    OwnFeatures({"court_one", "court_two"});
}

bool On() {
    return g.phase != kOff;
}
bool Playing() {
    return g.phase == kServe || g.phase == kPlay;
}
int Line() {
    return g.line;
}
int KeepToHalf(int side, int x, int halfWidth) {
    const int room = halfWidth + kLineRoom;
    return side == 0 ? std::min(x, g.line - room) : std::max(x, g.line + room);
}
Reaction ReactionWanted(int side, int &away) {
    if (side < 0 || side > 1 || g.react[side] == kNoReaction) {
        return kNoReaction;
    }
    const Reaction reaction = g.react[side];
    g.react[side] = kNoReaction;
    away = g.reactAway[side];
    return reaction;
}

void Begin(int cameraX, int cameraY, int left, int right, int points, bool computer, int level) {
    cameraX += d.pictureLeft; // (the camera in the court's terms: Description::pictureLeft, cameraHeight)
    cameraY = d.cameraHeight ? d.cameraHeight(cameraY) : cameraY;
    const uint32_t random = g.random;
    g = {};
    g.random = random * 2654435761u + 1;
    g.cameraX = cameraX;
    g.cameraY = cameraY;
    g.left = left;
    g.right = right;
    g.line = (left + right) / 2;
    g.points = points;
    g.computer = computer;
    g.level = level;
    Serve(0);
    g.shown = g.ball;
    g.placeFor = kPlaceFor;
}

void BeforeFrame(Machine &, uint16_t &one, uint16_t &two) {
    (void)one;
    if (g.phase == kOff) {
        return;
    }
    // A game begun: each fighter in the middle of its half (the round's own places are a screen's middle apart)
    if (g.placeFor > 0 && d.place) {
        g.placeFor--;
        d.place(0, (g.left + g.line) / 2);
        d.place(1, (g.line + g.right) / 2);
    }
    // (for tests, CPS2RECOMP_COURT_CPU=1: player 1's side the computer's too)
    static const bool both = getenv("CPS2RECOMP_COURT_CPU") != nullptr;
    g.ai[0].on = both;
    g.ai[1].on = g.computer;
    if (both) {
        one = Computer(0, g.ai[0]);
    }
    if (g.computer) {
        two = Computer(1, g.ai[1]);
    }
    // The attack button each side pressed last
    for (int side = 0; side < 2; side++) {
        const uint16_t controls = side == 0 ? one : two;
        const uint16_t pressed = static_cast<uint16_t>(controls & ~g.controls[side]);
        g.controls[side] = controls;
        for (const uint16_t button : {kPunch1, kPunch2, kPunch3, kKick1, kKick2, kKick3}) {
            if (pressed & button) {
                g.button[side] = button;
            }
        }
    }
    // Aftertouch: the striker's stick up or down while the hit holds the ball
    if (g.ball.held > 0 && g.lastSide >= 0) {
        const uint16_t controls = g.lastSide == 0 ? one : two;
        g.ball.bend += (controls & kUp) ? 1 : (controls & kDown) ? -1 : 0;
    }
}

void Frame(Machine &machine, std::vector<HudLine> &lines, std::vector<HudBox> &boxes, int cameraX, int cameraY, int left, int right) {
    if (g.phase == kOff) {
        return;
    }
    g_frames = static_cast<int>(machine.frame);
    cameraX += d.pictureLeft; // (the camera in the court's terms: Description::pictureLeft, cameraHeight)
    cameraY = d.cameraHeight ? d.cameraHeight(cameraY) : cameraY;
    // (the picture being made shows the ball where it was a frame ago, as it shows the fighters)
    Draw(boxes, cameraX, cameraY);
    g.cameraX = cameraX;
    g.cameraY = cameraY;
    g.left = left;
    g.right = right;
    g.line = (left + right) / 2;
    g.since++;
    if (d.everyFrame) {
        d.everyFrame();
    }
    for (int side = 0; side < 2; side++) {
        g.sinceHit[side] = std::min(g.sinceHit[side] + 1, 999);
        g.bodyCool[side] = std::max(g.bodyCool[side] - 1, 0);
        g.leaving[side] = std::max(g.leaving[side] - 1, 0);
    }
    if (g.said > 0) {
        g.said--;
    }
    switch (g.phase) {
    case kServe:
        // (over the middle of its half as the court is now: the court can widen as a game begins)
        if (g.ball.held > 0) {
            g.ball.x = static_cast<float>(g.serveTo == 0 ? (g.left + g.line) / 2 : (g.line + g.right) / 2);
        }
        Fly();
        Contacts(machine);
        if (g.since >= kServeFor || g.ball.held == 0) {
            g.phase = kPlay;
            g.since = 0;
            // (for tests, CPS2RECOMP_COURT_SHOT=X,Y,VX,VY: the served ball sent off so, hot, as if player 1 had struck it)
            if (const char *shot = getenv("CPS2RECOMP_COURT_SHOT")) {
                float x = 0, y = 0, vx = 0, vy = 0;
                if (sscanf(shot, "%f,%f,%f,%f", &x, &y, &vx, &vy) == 4) {
                    g.ball.x = x, g.ball.y = y, g.ball.vx = vx, g.ball.vy = vy;
                    g.ball.hot = true;
                    g.lastSide = 0;
                }
            }
        }
        break;
    case kPlay: {
        Fly();
        Contacts(machine);
        const Ball &b = g.ball;
        // Over the line: the rally goes on
        const int half = b.x < static_cast<float>(g.line) ? 0 : 1;
        if (half != g.ballHalf) {
            g.ballHalf = half;
            g.crossings++;
            g.streakSide = -1;
            g.streak = 0;
        }
        // Off an end of the court: the other side's point
        if (b.x + kRadius < static_cast<float>(g.left)) {
            Point(1);
        } else if (b.x - kRadius > static_cast<float>(g.right)) {
            Point(0);
        }
        // (a ball lying still is left to lie: a fighter walks up to it and strikes it)
        break;
    }
    case kPoint:
        if (g.since >= kPointFor) {
            Serve(1 - g.pointTo); // (to the side that lost it)
        }
        break;
    default:
        break;
    }
    g.shown = g.ball;
    // (for tests, CPS2RECOMP_COURT_LOG=1: the ball and the fighters, every few frames)
    if (Logging() && g.since % 6 == 0) {
        fprintf(stderr, "court t %d phase %d ball %.0f,%.0f v %.1f,%.1f fighters %d %d target %d %d line %d court %d-%d floor %d cam %d,%d score %d-%d\n",
                g.since, g.phase, g.ball.x, g.ball.y, g.ball.vx, g.ball.vy, d.fighterX(0), d.fighterX(1), g.ai[0].targetX, g.ai[1].targetX,
                g.line, g.left, g.right, d.floor, cameraX, cameraY, g.score[0], g.score[1]);
    }
    // The score, all the while
    char text[64];
    snprintf(text, sizeof(text), "%d  -  %d", g.score[0], g.score[1]);
    lines.push_back({0.5f, 0.2f, 0, text});
    snprintf(text, sizeof(text), "FIRST TO %d", g.points);
    lines.push_back({0.5f, 0.27f, 0, text, 200, 200, 200});
    if (g.phase == kPoint) {
        lines.push_back({0.5f, 0.42f, 0, g.pointTo == 0 ? "POINT PLAYER 1" : (g.computer ? "POINT CPU" : "POINT PLAYER 2")});
    } else if (g.phase == kWon) {
        lines.push_back({0.5f, 0.42f, 0, g.winner == 0 ? "PLAYER 1 WINS" : (g.computer ? "CPU WINS" : "PLAYER 2 WINS")});
    }
    if (g.said > 0) {
        static const char *const kSaid[6] = {"", "POWER SHOT!", "SUPER SHOT!", "KNOCK DOWN!", "", "SPECIAL!"};
        if (*kSaid[std::clamp(g.saidKind, 0, 5)]) {
            lines.push_back({0.5f, 0.34f, 0, kSaid[std::clamp(g.saidKind, 0, 5)], 255, 200, 60});
        }
    }
}

int Over() {
    return g.phase == kWon && g.since >= kWonFor ? g.winner + 1 : 0;
}

void Again() {
    g.placeFor = kPlaceFor;
    g.score[0] = g.score[1] = 0;
    g.winner = -1;
    Serve(g.pointTo >= 0 ? 1 - g.pointTo : 0);
}

void End() {
    g.phase = kOff;
}

void State(Archive &ar) {
    ar.Mark(0x434F5254);
    ar.Pod(g);
    if (d.state && (ar.version == 0 || ar.version >= 11)) { // (a save state from before has none)
        d.state(ar);
    }
}

bool HidesPanel() {
    return g.phase != kOff && d.panel;
}
void TakeObjects(uint16_t *list, int &last) {
    if (!HidesPanel()) {
        return;
    }
    for (int i = 0; i <= last; i += 4) {
        const uint32_t tile = list[i + 2] + ((list[i + 1] & 0x6000u) << 3);
        if ((list[i + 1] & 0x3FF) < d.panelTop && d.panel(tile, list[i + 3])) {
            list[i + 1] = static_cast<uint16_t>((list[i + 1] & ~0x3FFu) | 0x300); // (a line the picture never shows)
        }
    }
}
unsigned HiddenLayers() {
    return g.phase != kOff ? d.hiddenLayers : 0;
}
Hide Hidden() {
    return g.phase != kOff ? d.hidden : nullptr;
}

bool Focus(int &x, int &y, int &reach) {
    if (!Playing()) {
        return false;
    }
    const int wide = d.wideStage ? d.wideStage() : 0;
    x = static_cast<int>(g.shown.x) - g.cameraX + wide;
    y = d.pictureFloor - (static_cast<int>(g.shown.y) - g.cameraY);
    reach = static_cast<int>(kRadius) + 10;
    return true;
}

} // namespace cps2recomp::court
