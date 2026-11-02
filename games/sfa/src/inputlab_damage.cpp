// SPDX-License-Identifier: GPL-3.0-only
// Exact SFA Europe 950727 sites, ported from the verified MAMEPPK policy.
#include "damage_policy.h"
#include "sfa.h"
namespace cps2recomp {
namespace {
damage_policy::References References() {
    return damage_policy::Players(0xFF8400, 0xFF8800, 0xC5, true, Ram8(0xFFAE38) != 0, 0xFF8C00);
}
bool Rank(M68K &c, unsigned reg, bool incoming, bool grab, uint32_t resume) {
    const uint32_t normalCpuOwner = Ram8(0xFFAE38) && !grab ?
        (Ram8(0xFF8C4C) ? 0xFF8800 : 0xFF8400) : 0;
    return damage_policy::AddRank(c, References(), 0xC7, reg, incoming, resume, normalCpuOwner);
}
}
void RegisterInputLabDamage() {
    damage_policy::RegisterFeature();
    AddHook(0x00F88C, "DamageAttackerRank", [](M68K &c) { return Rank(c, 2, false, false, 0x00F890); });
    AddHook(0x00F958, "DamageVictimClass", [](M68K &c) { return damage_policy::VictimClass(c, References(), 0xC5, 0x00F95C); });
    AddHook(0x00F970, "DamageVictimRank", [](M68K &c) { return Rank(c, 0, true, false, 0x00F974); });
    AddHook(0x021242, "ThrowDamageRank", [](M68K &c) { return Rank(c, 2, false, true, 0x021246); });
    AddHook(0x0212A6, "HoldDamageRank", [](M68K &c) { return Rank(c, 1, false, true, 0x0212AA); });
}
} // namespace cps2recomp
