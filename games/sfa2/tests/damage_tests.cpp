// SPDX-License-Identifier: GPL-3.0-only
// Damage policy integration tests against the user's locally generated ROM program.
// Runs the actual 68000 routines, not a copy of the damage formula.
#include "../src/sfa2.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <memory>
#include <vector>

namespace cps2recomp {
void RegisterDamage();
bool TrainingMode() { return false; }
bool TeamHoldsKnockOut(int) { return false; }
void TrainingHit(int, unsigned, bool) {}
}
using namespace cps2recomp;
using namespace sfa2;
static unsigned checks = 0;
static int cpuRank = 31;
static int humanRank = 16;
static int controlPattern = 2;
static void Check(bool ok, const char *message) {
    ++checks;
    if (!ok) { fprintf(stderr, "FAIL: %s\n", message); exit(1); }
}
static std::vector<uint8_t> Read(const char *name) {
    std::ifstream f(name, std::ios::binary);
    return {std::istreambuf_iterator<char>(f), {}};
}
struct Result { int damage; unsigned rng, stun; };
// kind: normal hit, alternate hit, throw, hold tick, projectile, helper.
static Result Hit(Machine &m, int kind, int policy, int attackerSide, int character,
                  int strength, int condition, bool oracle, bool original, bool loadFeatures = true) {
    std::fill(m.ram.begin(), m.ram.end(), 0);
    SetRam(m.ram.data());
    auto &c = m.cpu;
    c.Reset(true);
    c.a[5] = 0xFF8000; c.a[7] = 0xFF7000;
    const uint32_t owner = Player(attackerSide), victim = Player(1 - attackerSide);
    const uint32_t attacker = kind == 4 ? kThrown : kind == 5 ? kHelper1 : owner;
    const uint32_t attack = 0xFF6000;
    c.a[6] = attacker; c.a[1] = c.a[4] = victim; c.a[3] = attack;
    SetRam8(0xFF8090, 0); SetRam8(0xFF80A1, 2);
    if (kind == 1) SetRam16(0xFF8138, 2); // normal mode, normal board damage
    SetRam16(0xFF80AE, 0x1234); // same RNG for both sides and every policy oracle
    const int refHuman = controlPattern == 1 ? cpuRank : humanRank;
    const int refCpu = controlPattern == 1 ? humanRank : cpuRank;
    const int attackRank = policy == 1 ? refHuman : refCpu;
    const int defendRank = policy == 1 ? refCpu : refHuman;
    for (int side = 0; side < 2; ++side) {
        const auto p = Player(side);
        SetRam8(p + kComputer, oracle ? (side == attackerSide ? policy == 2 : policy == 1) : ((controlPattern >> side) & 1));
        SetRam8(p + 0xA7, oracle ? (side == attackerSide ? attackRank : defendRank) : side == 0 ? humanRank : cpuRank);
        SetRam8(p + kCharacter, character);
        SetRam8(p + kSide, side);
        SetRam8(p + kStunMost, 255);
        SetRam16(p + kHealth, kHealthFull);
        SetRam16(p + kOpponent, static_cast<uint16_t>(Player(1 - side)));
    }
    if (attacker != owner) {
        SetRam8(attacker + kSide, attackerSide);
        SetRam16(attacker + kHealth, kHealthFull);
        SetRam8(attacker + 0x08, 8);
        if (kind == 5) SetRam8(attacker + 0x3E, 1);
    }
    const bool chip = condition == 1;
    const int health = condition == 2 ? 30 : kHealthFull;
    SetRam16(victim + kHealth, health);
    SetRam8(victim + kGuardedHit, chip ? 255 : 0);
    if (condition == 3) SetRam8(victim + 0x5E, 6); // combo scaling
    if (condition == 4) SetRam8(victim + 0x28D, 1); // airborne
    if (condition == 5) SetRam8(victim + 0x294, 1); // counter-hit multiplier
    SetRam8(attack + 8, strength | (chip ? 0x80 : 0));
    SetRam8(attack + 0x1A, strength); // helper attack strength
    SetRam8(attack + 0x0C, 0);
    SetRam8(attack + 0x0E, 0); SetRam8(attack + 0x0F, 1);
    SetRam8(attack + 0x14, chip ? 1 : 0);
    if (loadFeatures) LoadFeatures("", "damage_hits=" + std::to_string(oracle ? 0 : policy));
    SetRewrites(false, original);
    if (kind == 1) SetRam16(0xFF27F2, static_cast<uint16_t>(owner)); // A5 -580E
    c.d[1] = c.d[2] = strength; c.d[3] = 0;
    const uint32_t routine = kind == 1 ? 0x00F17A : kind == 2 ? 0x026688 : kind == 3 ? 0x026728 : 0x0108E2;
    Call(c, routine);
    const int damage = kind == 2 ? c.d[3] & 0xFFFF : health - static_cast<int16_t>(Ram16(victim + kHealth));
    if (!oracle) {
        Check(Ram8(Player(0) + kComputer) == (controlPattern & 1) && Ram8(Player(1) + kComputer) == ((controlPattern >> 1) & 1), "AI ownership unchanged");
        Check(Ram8(Player(0) + 0xA7) == humanRank && Ram8(Player(1) + 0xA7) == cpuRank, "AI rank unchanged");
    }
    return {damage, Ram16(0xFF80AE), Ram8(victim + kStun)};
}
int main(int argc, char **argv) {
    if (argc != 3) { fprintf(stderr, "Usage: sfa2_damage_tests program.bin.data program.bin\n"); return 2; }
    RomSet roms; roms.program = Read(argv[1]); roms.opcodes = Read(argv[2]);
    Check(roms.program.size() > 0x9E1DC && roms.program.size() == roms.opcodes.size(), "local program available");
    auto m = std::make_unique<Machine>(roms);
    SetProgram(roms.program.data(), static_cast<uint32_t>(roms.program.size()));
    RegisterDamage();
    for (int rank : {0, 9, 16, 31}) {
        cpuRank = rank;
        for (int kind = 0; kind < 6; ++kind) {
            for (int policy = 1; policy <= 2; ++policy) {
                for (int ch = 0; ch < kCharacters; ++ch) {
                    for (int strength : {1, 4, 8, 16, 24, 31}) {
                        for (int condition = 0; condition < 6; ++condition) {
                            for (int side = 0; side < 2; ++side) {
                                const auto a = Hit(*m, kind, policy, side, ch, strength, condition, false, false);
                                const auto b = Hit(*m, kind, policy, side, ch, strength, condition, true, false);
                                if (a.damage != b.damage || a.rng != b.rng || a.stun != b.stun) {
                                    fprintf(stderr,"kind=%d policy=%d ch=%d strength=%d condition=%d side=%d actual=%d oracle=%d\n",
                                            kind,policy,ch,strength,condition,side,a.damage,b.damage);
                                    Check(false,"policy equals original game with reference attacker/victim pairing");
                                }
                                Check(true, "policy equals original game with reference attacker/victim pairing");
                                if (policy == 1) {
                                    const auto higher = Hit(*m, kind, 2, side, ch, strength, condition, false, false);
                                    Check(cpuRank >= humanRank ? a.damage <= higher.damage : a.damage >= higher.damage, "damage ordering follows the actual CPU/human ranks");
                                }
                                const auto other = Hit(*m, kind, policy, 1-side, ch, strength, condition, false, false);
                                if (a.damage != other.damage) fprintf(stderr,"symmetry kind=%d policy=%d ch=%d str=%d cond=%d side=%d damage=%d/%d\n",kind,policy,ch,strength,condition,side,a.damage,other.damage);
                                Check(a.damage == other.damage, "same character/move/state/RNG is symmetric");
                                if (kind == 0 || kind >= 4) {
                                    const auto native = Hit(*m, kind, policy, side, ch, strength, condition, false, true);
                                    if (native.damage != a.damage) {
                                        fprintf(stderr,"rewrite/native kind=%d policy=%d ch=%d str=%d cond=%d side=%d damage=%d/%d\n",
                                                kind,policy,ch,strength,condition,side,a.damage,native.damage);
                                        Check(false,"rewrite and 68000 original agree");
                                    }
                                    Check(true,"rewrite and 68000 original agree");
                                }
                            }
                        }
                    }
                }
                const auto ex = Hit(*m, kind, policy, 0, 0, 8, 0, false, false);
                if (cpuRank == 31) printf("example kind=%d profile=%s strength=8 damage=%d\n",kind,policy==1?"human":"CPU",ex.damage);
            }
        }
    }
    controlPattern = 1; // CPU on player 1; human on player 2.
    humanRank = 7; cpuRank = 23; // these two variables are the record ranks of P1/P2.
    for (int kind = 0; kind < 6; ++kind) for (int policy : {1, 2}) for (int side = 0; side < 2; ++side) {
        const auto actual = Hit(*m, kind, policy, side, 0, 8, 0, false, false);
        const auto reference = Hit(*m, kind, policy, side, 0, 8, 0, true, false);
        Check(actual.damage == reference.damage && actual.rng == reference.rng, "CPU/human sides can be reversed");
    }
    humanRank = 16; cpuRank = 31;
    for (int pattern : {0, 3}) {
        controlPattern = pattern;
        for (int kind = 0; kind < 6; ++kind) for (int side = 0; side < 2; ++side) {
            const auto vanilla = Hit(*m, kind, 0, side, 0, 8, 0, false, false);
            for (int policy : {1, 2}) {
                const auto normalized = Hit(*m, kind, policy, side, 0, 8, 0, false, false);
                Check(vanilla.damage == normalized.damage && vanilla.rng == normalized.rng, "PvP/CPU-CPU keep Vanilla rules");
            }
        }
    }
    controlPattern = 2;
    std::vector<Result> expected;
    for (int rank : {31, 9, 0, 16, 25}) {
        cpuRank = rank;
        expected.push_back(Hit(*m, 0, 2, 0, 0, 8, 0, true, false));
    }
    LoadFeatures("", "damage_hits=2");
    int at = 0;
    for (int rank : {31, 9, 0, 16, 25}) {
        cpuRank = rank;
        const auto live = Hit(*m, 0, 2, 0, 0, 8, 0, false, false, false);
        Check(live.damage == expected[at++].damage, "CPU rank changes apply without reloading feature");
        printf("live CPU rank=%d damage=%d\n", rank, live.damage);
    }
    // Exercise the original CPU upkeep: its countdown increases the rank accumulator
    // by eight, i.e. one damage rank, without changing the difficulty DIP variable.
    SetRam16(0xFF2CD6, 16 * 8); SetRam16(0xFF2CD8, 1);
    SetRam8(Player(1) + kCharacter, 0); SetRam8(0xFF80AC, 1);
    SetRam8(0xFF80BE, 3); SetRam8(Player(1) + 0xA7, 16);
    Call(m->cpu, 0x01F738);
    Check(Ram8(Player(1) + 0xA7) == 17, "native upkeep changes CPU rank during the round");
    Check(Ram8(0xFF80BE) == 3, "difficulty unchanged while rank changes");
    puts("Native round upkeep: CPU rank 16 -> 17 with difficulty unchanged.");
    printf("%u damage integration checks passed (24 forms, 6 attack paths, both sides, 6 states).\n", checks);
}
