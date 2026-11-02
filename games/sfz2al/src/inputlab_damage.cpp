// SPDX-License-Identifier: GPL-3.0-only
// SFZ2AL Asia 960826: instruction sites checked against this revision.
#include "damage_policy.h"
#include "sfa2.h"
namespace cps2recomp {
namespace {
damage_policy::References References() {
    return damage_policy::Players(0xFF8400, 0xFF8800, 0xA5, true, Ram16(0xFF8138) == 2, 0xFF8C00);
}
bool Rank(M68K &c, unsigned reg, bool incoming, uint32_t resume) {
    return damage_policy::AddRank(c, References(), 0xA7, reg, incoming, resume);
}
bool Class(M68K &c, uint32_t resume) {
    return damage_policy::VictimClass(c, References(), 0xA5, resume);
}
}
void RegisterInputLabDamage() {
    damage_policy::RegisterFeature();
    // With this policy selected run the original routine, including its third
    // fighter/projectile ownership paths; hooks replace only rank/class reads.
    RewriteWhen(0x010972, [](M68K &) { return !FeatureOn("damage_hits"); });
    AddHook(0x01099E, "DamageAttackerRank", [](M68K &c) { return Rank(c, 2, false, 0x0109A2); });
    AddHook(0x010A9C, "DamageVictimClass", [](M68K &c) { return Class(c, 0x010AA0); });
    AddHook(0x010AB4, "DamageVictimRank", [](M68K &c) { return Rank(c, 0, true, 0x010AB8); });
    AddHook(0x00F322, "HelperDamageRank", [](M68K &c) { return Rank(c, 2, false, 0x00F326); });
    AddHook(0x00F33C, "AlternateDamageRank", [](M68K &c) { return Rank(c, 2, false, 0x00F340); });
    AddHook(0x00F43C, "AlternateVictimClass", [](M68K &c) { return Class(c, 0x00F440); });
    AddHook(0x00F454, "AlternateVictimRank", [](M68K &c) { return Rank(c, 0, true, 0x00F458); });
    AddHook(0x0261FA, "ThrowDamageRank", [](M68K &c) { return Rank(c, 2, false, 0x0261FE); });
    AddHook(0x026298, "HoldDamageRank", [](M68K &c) { return Rank(c, 1, false, 0x02629C); });
}
} // namespace cps2recomp
