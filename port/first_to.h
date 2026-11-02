// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 cps2recomp contributors. Part of cps2recomp.
#pragma once

#include <array>
#include <cstdint>

namespace port {

// Only confirmed frames enter this counter. Predictions (and their replacements during rollback) stay in
// Netplay::Kept until both players' controls are known. This state never changes the board or its replay.
class FirstTo {
public:
    struct Frame {
        int result = -1;
        int rounds = 1;
        bool versus = false;
    };
    void Begin(int choice) {
        *this = FirstTo{};
        target = choice == 1 ? 5 : choice == 2 ? 10 : 0;
    }
    int Target() const { return target; }
    int Score(int side) const { return score[side]; }
    int Winner() const { return winner; } // -1 until the set is complete

    // Returns the player awarded a game, or -1. The frame number also makes duplicate confirmations harmless.
    int Confirm(uint64_t frame, const Frame &now) {
        if (!target || winner >= 0 || (haveFrame && frame <= lastFrame)) {
            return -1;
        }
        haveFrame = true;
        lastFrame = frame;
        if (!now.versus || now.result < 0 || now.result > 3 || now.rounds < 1) {
            quiet = 0;
            armed = false;
            rounds = {};
            return -1;
        }
        if (now.result == 0) {
            // A persistent result, or a short flicker of it, must count only once. Also ignore a result already
            // on screen when the online session starts: first observe a live two-player round.
            if (quiet < 60) {
                ++quiet;
            }
            armed = quiet >= 60;
            return -1;
        }
        quiet = 0;
        if (!armed) {
            return -1;
        }
        armed = false;
        if (now.result == 3) {
            return -1;
        }
        const int side = now.result - 1;
        if (++rounds[side] < now.rounds) {
            return -1;
        }
        rounds = {};
        if (++score[side] == target) {
            winner = side;
        }
        return side;
    }

private:
    int target = 0, winner = -1;
    std::array<int, 2> score{}, rounds{};
    int quiet = 0;
    bool armed = false, haveFrame = false;
    uint64_t lastFrame = 0;
};

} // namespace port
