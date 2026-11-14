// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Morphus56K and contributors. Part of cps2recomp.
// ROM-free regression for MVC's Cross Fever gauge clock across board resets and save states.
// Link with games/mvsc/src/crossfever.cpp. Only the surrounding bus, feature and hook services are stand-ins;
// the registered hooks and their state serialization are the game's actual implementation.
#include "game.h"
#include "lift.h"
#include "state.h"
#include "tag.h"
#include "mvsc.h"

#include <algorithm>
#include <array>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>

namespace {
    using namespace cps2recomp;
    std::array<uint8_t, 0x10000> memory;
    std::map<uint32_t, HookFn> hooks;
    bool enabled = true, routing = true;

    void Require(bool value, const std::string &why) {
        if (!value) {
            throw std::runtime_error(why);
        }
    }

    struct Bus : M68KBus {
        uint8_t Read8(uint32_t address) override { return memory[address & 0xFFFF]; }
        uint16_t Read16(uint32_t address) override { return static_cast<uint16_t>(Read8(address) << 8 | Read8(address + 1)); }
        void Write8(uint32_t address, uint8_t value) override { memory[address & 0xFFFF] = value; }
        void Write16(uint32_t address, uint16_t value) override {
            Write8(address, static_cast<uint8_t>(value >> 8));
            Write8(address + 1, static_cast<uint8_t>(value));
        }
        uint16_t Fetch16(uint32_t address) override { return Read16(address); }
    } bus;
} // namespace

namespace cps2recomp {
    void RegisterCrossFever();
    void CrossFeverState(Archive &ar);
    void TagState(Archive &ar);
    int TagPick(uint32_t record);

    Game &TheGame() { static Game game; return game; }
    bool FeatureOn(const char *) { return enabled; }
    uint8_t *Ram(uint32_t address) { return &memory[address & 0xFFFF]; }
    bool BattleMode() { return false; }
    namespace session { bool InSession() { return false; } }
    namespace modes {
        bool AttractMode() { return false; }
        bool StageViewer() { return false; }
    }
    namespace marvel { bool ModelViewer() { return false; } }
    void AddHook(uint32_t address, const char *, HookFn fn) {
        Require(hooks.emplace(address, fn).second, "duplicate hook address");
    }
    namespace tag {
        void Register(const Description &) {}
        bool Routing() { return routing; }
        int Person(int side, int pick) { return side + 2 * pick; }
        uint16_t Held(int) { return 0; }
    }
    uint32_t M68K::Read(uint32_t address, unsigned size) {
        uint32_t value = 0;
        for (unsigned n = 0; n < (1u << size); n++) {
            value = value << 8 | bus.Read8(address + n);
        }
        return value;
    }
    void M68K::Write(uint32_t address, unsigned size, uint32_t value) {
        for (unsigned n = 1u << size; n > 0; n--) {
            bus.Write8(address + n - 1, static_cast<uint8_t>(value));
            value >>= 8;
        }
    }
} // namespace cps2recomp

namespace {
    // The native gauge routine is not executed here. Walk only its two returns, checking that the hook requested
    // exactly one point for each side and restored its stack. The boolean is the observable credit opportunity.
    bool Step() {
        M68K cpu(bus);
        cpu.a[7] = 0xFF8000;
        cpu.Write(cpu.a[7], 2, 0x00CEE8);
        cpu.pc = 0x01CEAA;
        const auto hook = hooks.at(cpu.pc);
        if (!hook(cpu)) {
            return false;
        }
        Require(cpu.pc == 0x01CD1E && cpu.d[0] == 1 && cpu.a[6] == mvsc::kPlayer1, "first gauge request");
        cpu.pc = cpu.Pop32();
        Require(cpu.pc == 0x01CEAA && hook(cpu), "first gauge return");
        Require(cpu.pc == 0x01CD1E && cpu.d[0] == 1 && cpu.a[6] == mvsc::kPlayer1 + mvsc::kPlayerSize,
                "second gauge request");
        cpu.pc = cpu.Pop32();
        Require(cpu.pc == 0x01CEAA && !hook(cpu) && cpu.a[7] == 0xFF8000, "second gauge return");
        return true;
    }

    void ClearOldState() {
        std::vector<uint8_t> bytes;
        Archive ar(bytes, false);
        ar.version = 10;
        CrossFeverState(ar);
        Require(ar.Ok(), "legacy state must start a fresh gauge clock");
        memory.fill(0);
        enabled = routing = true;
    }

    void MakeRecords() {
        SetRam32(mvsc::kPlayer1 + mvsc::kPartner, mvsc::kPlayer1 + mvsc::kPartners);
        SetRam32(mvsc::kPlayer1 + mvsc::kPlayerSize + mvsc::kPartner,
                 mvsc::kPlayer1 + mvsc::kPlayerSize + mvsc::kPartners);
    }

    void Boot() {
        M68K cpu(bus);
        cpu.pc = 0x000FAC; // verified stock boot call: jsr $007E7A (settings read)
        cpu.cycles = 1234;
        for (unsigned n = 0; n < 8; n++) {
            cpu.d[n] = n + 11;
            cpu.a[n] = n + 21;
        }
        cpu.flagC = cpu.flagX = cpu.flagN = true;
        const auto before = cpu;
        const auto ramBefore = memory;
        const auto found = hooks.find(cpu.pc);
        // Without a hook the board simply executes its stock call, which cannot clear the host's static clock.
        if (found != hooks.end()) {
            Require(!found->second(cpu), "boot must let the stock settings call run");
        }
        Require(cpu.pc == before.pc && cpu.cycles == before.cycles &&
                    std::equal(std::begin(cpu.d), std::end(cpu.d), std::begin(before.d)) &&
                    std::equal(std::begin(cpu.a), std::end(cpu.a), std::begin(before.a)) &&
                    cpu.flagC == before.flagC && cpu.flagV == before.flagV && cpu.flagZ == before.flagZ &&
                    cpu.flagN == before.flagN && cpu.flagX == before.flagX && memory == ramBefore,
                "boot clock reset must not change the CPU or board memory");
    }

    std::vector<bool> Credits(unsigned steps = 21) {
        std::vector<bool> result;
        while (steps--) {
            result.push_back(Step());
        }
        return result;
    }

    std::vector<bool> AfterBoot(unsigned prior, unsigned intro, bool feature = true, bool route = true) {
        ClearOldState();
        for (unsigned n = 0; n < prior; n++) { Step(); }
        enabled = feature;
        routing = route;
        Boot();
        enabled = routing = true;
        // Intro steps still advance the clock even before the partner records are valid; they must not be
        // mistaken for a reset that erases a previous run's phase.
        for (unsigned n = 0; n < intro; n++) { Step(); }
        MakeRecords();
        return Credits();
    }

    void StateVersions() {
        // The preview and upstream used 11 for different additions. Only the preview contains MVC's TAOR/CFVS.
        for (unsigned version : {11u, 12u}) {
            ClearOldState();
            MakeRecords();
            Credits(3);
            std::vector<uint8_t> bytes;
            Archive save(bytes, true);
            save.Mark(0x54414F52);
            uint8_t swaps[2] = {1, 0};
            save.Pod(swaps);
            CrossFeverState(save);
            save.Mark(0x44535447); // the following guest-stage part must remain readable
            Boot();
            Archive load(bytes, false);
            load.version = version;
            TagState(load);
            CrossFeverState(load);
            load.Mark(0x44535447);
            Require(load.Ok() && TagPick(mvsc::kPlayer1) == 1 && TagPick(mvsc::kPlayer1 + mvsc::kPlayerSize) == 0,
                    "marked MVC state lost controller ownership");
            Require(!Step() && !Step() && !Step() && Step(), "marked MVC state lost its gauge phase");
        }
        // An upstream MVC 11 state goes straight to the next section, without either preview section.
        std::vector<uint8_t> upstream;
        Archive save(upstream, true);
        save.Mark(0x44535447);
        Credits(3);
        Archive load(upstream, false);
        load.version = 11;
        TagState(load);
        CrossFeverState(load);
        Require(load.NextMark(0x44535447), "absent preview parts consumed the upstream section");
        load.Mark(0x44535447);
        Require(load.Ok() && TagPick(mvsc::kPlayer1) == 0, "upstream state did not reset preview ownership");
        for (unsigned n = 0; n < 6; n++) { Require(!Step(), "upstream state retained a stale clock"); }
        Require(Step(), "upstream state's fresh clock did not reach seven");
        // New states require their parts: a truncated or missing clock is an error, not a legacy default.
        for (auto bytes : {std::vector<uint8_t>{}, std::vector<uint8_t>{1, 2, 3}, upstream}) {
            Archive broken(bytes, false);
            broken.version = 12;
            Require(!broken.NextMark(0x43465653), "invalid clock mark accepted");
            CrossFeverState(broken);
            Require(!broken.Ok(), "new state silently accepted a missing clock");
        }
    }
} // namespace

int main() {
    try {
        RegisterCrossFever();
        for (unsigned intro : {0u, 1u, 6u, 7u, 100u, 180u}) {
            const auto cold = AfterBoot(0, intro);
            for (unsigned prior = 1; prior < 7; prior++) {
                for (unsigned mode = 0; mode < 3; mode++) {
                    Require(AfterBoot(prior, intro, mode != 1, mode != 2) == cold,
                            "reset retained gauge phase " + std::to_string(prior) + " after " + std::to_string(intro) + " intro steps");
                }
            }
        }
        const auto cold = AfterBoot(0, 0);
        ClearOldState();
        enabled = false;
        Credits(20); // outside Cross Fever the clock saturates, rather than wrapping
        Boot();
        enabled = true;
        MakeRecords();
        Require(Credits() == cold, "boot retained the saturated clock from a non-Cross-Fever match");
        for (unsigned phase = 0; phase < 7; phase++) {
            ClearOldState();
            MakeRecords();
            Credits(phase);
            std::vector<uint8_t> bytes;
            Archive save(bytes, true);
            CrossFeverState(save);
            const auto expected = Credits();
            Credits(5);
            Archive load(bytes, false);
            load.version = 11;
            CrossFeverState(load);
            Require(load.Ok() && Credits() == expected, "save/load lost the in-match gauge phase");
        }
        ClearOldState();
        MakeRecords();
        Credits(3);
        memory.fill(0); // a match's records are replaced, without a board reset
        Credits(2);
        MakeRecords();
        Require(!Step() && Step(), "normal match transitions must retain gauge phase");
        StateVersions();
        std::cout << "PASS: Cross Fever reset, clock, ownership and legacy/current marked states\n";
    } catch (const std::exception &error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
