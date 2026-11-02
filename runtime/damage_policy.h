// SPDX-License-Identifier: GPL-3.0-only
// Input Lab: live damage references, without changing a fighter's AI or rank.
#pragma once
#include "game.h"
#include "lift.h"

namespace cps2recomp::damage_policy {
struct References {
    uint32_t human = 0, cpu = 0;
    explicit operator bool() const { return human != 0 && cpu != 0; }
};
inline References Players(uint32_t first, uint32_t second, unsigned owner, bool humanIsZero,
                          bool dramatic = false, uint32_t third = 0) {
    const auto human = [=](uint32_t p) { return (Ram8(p + owner) == 0) == humanIsZero; };
    if (dramatic) {
        if (!third || human(third)) return {};
        if (human(first)) return {first, third};
        if (human(second)) return {second, third};
        return {};
    }
    if (human(first) == human(second)) return {};
    return human(first) ? References{first, second} : References{second, first};
}
inline uint32_t Selected(References ref, bool incoming, int policy) {
    if (!ref || policy < 1 || policy > 2) return 0;
    return (policy == 1) != incoming ? ref.human : ref.cpu;
}
inline bool AddRank(M68K &c, References ref, unsigned offset, unsigned dest, bool incoming,
                    uint32_t resume, uint32_t outgoingOverride = 0) {
    const int policy = FeatureValue("damage_hits");
    uint32_t source = Selected(ref, incoming, policy);
    if (!source) return false;
    if (!incoming && policy == 2 && outgoingOverride) source = outgoingOverride;
    const uint32_t result = c.Add(c.d[dest], Ram8(source + offset), 0, false, false);
    c.d[dest] = (c.d[dest] & 0xFFFFFF00u) | (result & 255);
    c.pc = resume; Spend(c, 12);
    return true;
}
inline bool VictimClass(M68K &c, References ref, unsigned offset, uint32_t resume) {
    const uint32_t source = Selected(ref, true, FeatureValue("damage_hits"));
    if (!source) return false;
    c.Logic(Ram8(source + offset), 0); c.pc = resume; Spend(c, 12);
    return true;
}
inline void RegisterFeature(const char *extra = "") {
    TheGame().features.push_back({"damage_hits", "Cheats", "Damage hits",
        "Vanilla keeps the original rules. Equal to human uses the live human-versus-CPU damage profile for both sides; "
        "Equal to CPU uses the live CPU-versus-human profile. Difficulty and adaptive rank remain live. "
        "Character, move, blocking, health, combo and random modifiers remain. AI ownership and ranks are never changed. "
        "Ordinary PvP and CPU-versus-CPU retain Vanilla. Dramatic Battle uses its third fighter when supported.",
        kFeatureOwn, {"Vanilla", "Equal to human", "Equal to CPU"}});
}
} // namespace cps2recomp::damage_policy
