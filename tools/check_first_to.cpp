// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 cps2recomp contributors. Part of cps2recomp.
#include "../port/first_to.h"

#include <cassert>
#include <cstdio>

using port::FirstTo;

struct Match {
    FirstTo set;
    uint64_t frame = 0;
    explicit Match(int choice) { set.Begin(choice); }
    int Step(int result, bool versus = true, int rounds = 1) {
        return set.Confirm(frame++, {result, rounds, versus});
    }
    void Live() { for (int i = 0; i < 60; ++i) Step(0); }
    int Win(int side, int rounds = 1) {
        Live();
        return Step(side + 1, true, rounds);
    }
};

int main() {
    for (int choice : {1, 2}) {
        Match match(choice);
        const int target = choice == 1 ? 5 : 10;
        assert(match.set.Target() == target);
        // Alternating wins accumulate on both sides, unlike a consecutive-win streak.
        for (int i = 1; i < target; ++i) {
            assert(match.Win(0) == 0);
            assert(match.Win(1) == 1);
            assert(match.set.Score(0) == i && match.set.Score(1) == i);
            assert(match.set.Winner() == -1);
        }
        assert(match.Win(0) == 0);
        assert(match.set.Winner() == 0);
        for (int i = 0; i < target + 2; ++i) match.Win(1);
        assert(match.set.Score(0) == target && match.set.Score(1) == target - 1);
        match.set.Begin(choice);
        assert(match.set.Winner() == -1 && match.set.Score(0) == 0 && match.set.Score(1) == 0);
    }
    for (int choice : {0, -1, 3, 10}) {
        Match casual(choice);
        casual.Win(0);
        assert(casual.set.Target() == 0 && casual.set.Score(0) == 0);
    }
    Match match(1);
    // A result already present on connection, a CPU fight, and a draw never award a game.
    assert(match.Step(1) == -1);
    match.Live();
    assert(match.Step(1, false) == -1);
    match.Live();
    assert(match.Step(3) == -1);
    assert(match.set.Score(0) == 0 && match.set.Score(1) == 0);
    assert(match.Win(1) == 1);
    for (int i = 0; i < 200; ++i) assert(match.Step(2) == -1);
    for (int i = 0; i < 59; ++i) match.Step(0);
    assert(match.Step(2) == -1); // a short gap does not re-arm the result
    assert(match.set.Score(1) == 1);
    match.Live();
    const uint64_t at = match.frame++;
    FirstTo::Frame kept{1, 1, true}; // prediction, not yet submitted to Confirm
    kept.result = 2;                // rollback replaces the predicted result
    assert(match.set.Confirm(at, kept) == 1);
    assert(match.set.Confirm(at, kept) == -1);
    assert(match.set.Confirm(at - 1, {1, 1, true}) == -1);
    assert(match.set.Score(0) == 0 && match.set.Score(1) == 2);

    Match rounds(2);
    assert(rounds.Win(0, 2) == -1);
    assert(rounds.Win(1, 2) == -1);
    assert(rounds.Win(1, 2) == 1);
    assert(rounds.set.Score(1) == 1);
    assert(rounds.Win(0, 2) == -1);
    rounds.Step(0, false); // leaving versus play discards partial round wins
    assert(rounds.Win(0, 2) == -1);
    assert(rounds.Win(0, 2) == 0);
    puts("ok: FT5/FT10 scoring, draws, CPU exclusion, repeated results, confirmation and session reset");
}
