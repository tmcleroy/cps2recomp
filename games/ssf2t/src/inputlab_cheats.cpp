// SPDX-License-Identifier: GPL-3.0-only
// SSF2X Japan 940223: MAMEPPK's verified boss resources and live damage policy.
// Uses native hooks, not injected ROM caves; ordinary Akuma and CPU stay original.
#include "damage_policy.h"
#include "ssf2t.h"
#include <algorithm>

namespace cps2recomp {
namespace {
using namespace ssf2t;
bool Boss(uint32_t p) { return Ram8((p & 0xFFFFFF) + kAkuma) == 2; }
bool DamageRank(M68K &c, unsigned reg, bool incoming, uint32_t resume) {
    const auto ref = damage_policy::Players(Player(0), Player(1), kPerson, false);
    const uint32_t source = damage_policy::Selected(ref, incoming, FeatureValue("damage_hits"));
    if (!source) return false;
    const uint16_t rank = Ram16(source + 0x58);
    SetLow16(c.d[reg], rank); c.Logic(rank, 1); c.pc = resume; Spend(c, 12);
    return true;
}
bool Cancel(M68K &c) {
    const uint32_t p = c.a[6] & 0xFFFFFF;
    if (Boss(p) && Ram8(p + kState) == kNormal) {
        const int pause = Ram8(p + kHitStop) - 4;
        if (pause < 0) c.d[0] = 0;
        else SetLow8(c.d[0], static_cast<uint8_t>(pause));
        SetRam8(p + kHitStop, static_cast<uint8_t>(std::max(0, pause)));
    }
    return false; // original committed transition to special state
}
}

void ApplyInputLabCharacter(uint32_t p) {
    using namespace ssf2t;
    p &= 0xFFFFFF;
    if ((p != Player(0) && p != Player(1)) || !Ram8(p + kPerson)) return;
    const int choice = FeatureValue(p == Player(0) ? "select_character_1" : "select_character_2");
    if (choice < 1 || choice > 18) return;
    if (choice <= 16) {
        SetRam8(p + kCharacter, static_cast<uint8_t>(choice - 1));
        SetRam8(p + kAkuma, 0);
        if (Ram8(p + kColourChoice) > 9) SetRam8(p + kColourChoice, 0);
    } else {
        SetRam8(p + kCharacter, 0);
        SetRam8(p + kAkuma, static_cast<uint8_t>(choice == 18 ? 2 : 1));
        SetRam8(p + kOld, 0);
        if (Ram8(p + kColourChoice) < 10 || Ram8(p + kColourChoice) > 11) {
            const uint32_t other = p == Player(0) ? Player(1) : Player(0);
            SetRam8(p + kColourChoice, Ram8(other + kAkuma) ? 11 : 10);
        }
    }
}

void ApplyInputLabColour(uint32_t p) {
    using namespace ssf2t;
    p &= 0xFFFFFF;
    if ((p != Player(0) && p != Player(1)) || !Ram8(p + kPerson)) return;
    const int choice = FeatureValue(p == Player(0) ? "select_colour_1" : "select_colour_2");
    if (choice < 1 || choice > 10) return;
    const int palette = choice - 1;
    // Match the ten original MAME button/old-version choices. Akuma has
    // only two native sets; never index its palette table with 0..9.
    const int colour = Ram8(p + kAkuma) ?
        ((palette <= 2 || palette == 8) ? 10 : 11) : palette;
    SetRam8(p + kColourChoice, static_cast<uint8_t>(colour));
}

void RegisterInputLabCheats() {
    using namespace ssf2t;
    damage_policy::RegisterFeature();
    for (int side = 0; side < 2; ++side) {
        Feature f{side ? "select_character_2" : "select_character_1", "Cheats",
            side ? "P2 Select Character" : "P1 Select Character",
            "Choose before the next character is initialized. Akuma keeps the selectable version; Shin Akuma uses "
            "the original CPU boss resources, double air projectile, teleport, Tatsumaki and cancel timing, with human "
            "commands. Does not change AI ownership, damage rank, hitboxes, pushback or juggle rules. Off restores normal selection.", kFeatureOwn};
        f.choices.push_back("Off");
        for (const char *name : kCharacterNames) f.choices.push_back(name);
        f.choices.push_back("Akuma / Gouki");
        f.choices.push_back("Shin Akuma / Gouki (CPU boss version)");
        TheGame().features.push_back(f);
        TheGame().features.push_back({side ? "select_colour_2" : "select_colour_1", "Cheats",
            side ? "P2 Select Colour" : "P1 Select Colour",
            "Force the palette associated with this selection button, from the next round. "
            "Normal characters keep their original ten choices. For Akuma and Shin Akuma: "
            "LP/MP/HP and SSF2 primary use black; LK/MK/HK, Start, Hold any P/K and SSF2 alternate use dark grey. "
            "Off keeps the game's original palette choice. Does not change the character or combat rules.",
            kFeatureOwn, {"Off", "LP button", "MP button", "HP button", "LK button", "MK button", "HK button",
                         "Start button", "Hold any P/K button", "SSF2 primary", "SSF2 alternate"}});
    }
    TheGame().features.push_back({"select_speed", "Cheats", "Select Speed",
        "The board's internal turbo frameskip rule, as in MAMEPPK. No frameskip runs every game step; "
        "this is separate from emulation speed, CPU clocks and screen refresh. Off returns to the board/player choice.",
        kFeatureOwn, {"Off", "No frameskip (like SSF2)", "Turbo 1 (Japan)", "Turbo 2 (Japan)",
                     "Turbo 3 (Japan)", "Turbo 4 (Japan)", "Maximum frameskip (double speed)"}});

    AddHook(0x005084, "SelectedCharacterInit", [](M68K &c) { ApplyInputLabCharacter(c.a[6]); return false; });
    AddHook(0x0BFA78, "SelectedCharacterColours", [](M68K &) {
        ApplyInputLabColour(Player(0)); ApplyInputLabColour(Player(1));
        return false; // original ROM palette loader copies the chosen native sets
    });
    AddHook(0x005096, "BossAttackResources", [](M68K &c) {
        if (!Boss(c.a[6])) return false;
        c.d[0] = 0x23; c.Logic(0x23, 2); c.pc = 0x0050B4; Spend(c, 34); return true;
    });
    AddHook(0x005268, "BossAnimationResources", [](M68K &c) {
        if (!Boss(c.a[6])) return false;
        c.d[6] = 0x21; c.Logic(0x21, 2); c.pc = 0x005282; Spend(c, 34); return true;
    });
    AddHook(0x0A905C, "BossTatsumakiTurns", [](M68K &c) {
        if (Boss(c.a[6]) && Ram8((c.a[6] & 0xFFFFFF) + 0x93)) SetLow8(c.d[0], (c.d[0] + 2) & 255);
        return false;
    });
    AddHook(0x0A8CD0, "BossGroundProjectile", [](M68K &c) {
        if (Boss(c.a[6])) { SetRam8(c.a[4] + 0x20, 0x0C); SetRam8(c.a[4] + 0x80, 0); SetRam8(c.a[4] + 0x46, 1); }
        return false;
    });
    AddHook(0x0A8B70, "BossTeleportAnimation", [](M68K &c) {
        if (Boss(c.a[6])) SetLow16(c.d[1], 0x58);
        return false;
    });
    AddHook(0x0A8DCC, "BossSecondAirProjectile", [](M68K &c) {
        if (!Boss(c.a[6])) return false;
        c.pc = 0x0B8242; return true;
    });
    AddHook(0x0B8266, "BossHumanAirStrength", [](M68K &c) {
        if (!Boss(c.a[6])) return false;
        const uint8_t strength = Ram8((c.a[6] & 0xFFFFFF) + 0x97);
        SetRam8(c.a[4] + 0x21, strength); c.Logic(strength, 0); c.pc = 0x0B826C; Spend(c, 20); return true;
    });
    AddHook(0x07519E, "BossAirTrajectory", [](M68K &c) {
        if (!Boss(c.a[1])) return false;
        SetLow8(c.d[0], c.Add(c.d[0], 3, 0, false, false)); c.pc = 0x0751A6; Spend(c, 28); return true;
    });
    AddHook(0x0A8302, "BossShoryukenCancel", Cancel);
    AddHook(0x0A81AA, "BossTatsumakiCancel", Cancel);
    AddHook(0x0A849A, "BossHadoukenCancel", Cancel);
    AddHook(0x0A863C, "BossRedHadoukenCancel", Cancel);
    AddHook(0x0A801E, "BossTeleportCancel", Cancel);
    AddHook(0x004AF2, "DamageThrowAttackRank", [](M68K &c) { return DamageRank(c, 2, false, 0x004AF6); });
    AddHook(0x004B9A, "DamageThrowDefenseRank", [](M68K &c) { return DamageRank(c, 0, true, 0x004B9E); });
    AddHook(0x0BED38, "DamageHitDefenseRank", [](M68K &c) { return DamageRank(c, 0, true, 0x0BED3C); });
    AddHook(0x0BED8A, "DamageHitAttackRank", [](M68K &c) { return DamageRank(c, 0, false, 0x0BED8E); });
    AddHook(0x0BEDA0, "DamageProjectileAttackRank", [](M68K &c) { return DamageRank(c, 0, false, 0x0BEDA4); });
    // Substitute only the original CMP.B operand. RAM retains the board's
    // current speed, so Off immediately resumes it without cached state.
    AddHook(0x0E9944, "SelectedInternalSpeed", [](M68K &c) {
        const int choice = FeatureValue("select_speed");
        constexpr uint8_t values[] = {0, 0xFF, 0x80, 0x70, 0x60, 0x50, 0};
        if (choice < 1 || choice > 6) return false;
        c.Sub(c.d[0], values[choice], 0, false, false, false);
        c.pc = 0x0E9948; Spend(c, 12); return true;
    });
}
} // namespace cps2recomp
