// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Cyberbonzo
#include "punisher.h"

#include <algorithm>
#include <initializer_list>

namespace cps2recomp {
namespace {
using namespace punisher;
constexpr int kSide = Video::kWideSide;
constexpr uint32_t kCameraX = kVars - 0xC8A;

int cameraShift = 0; // recalculated from the board before every frame, including replayed frames

bool WideStage() {
    const uint16_t step = Ram16(kStep);
    return FeatureOn("widescreen") && (step == kStepPlay || step == 0x0c);
}

// The sprite writer rejects objects before they reach the board's object list. Only
// override a taken branch when the rejected object falls within the added columns.
bool Admit(M68K &cpu, bool extra) {
    if (!extra || !WideStage()) return false;
    cpu.pc += Program(cpu.pc)[1] ? 2 : 4; // fall through the short or word branch
    return true;
}

// The object queue first tests the actor as a whole. Some actors use a table of
// bounding boxes; keep its margins and vertical test when opening the wide view.
template<int Reg>
bool ActorEdge(M68K &cpu) {
    const int x = static_cast<int16_t>(cpu.d[Reg]);
    const int right = Reg == 0 ? 512 : static_cast<int>((Program(cpu.a[1] - 2)[0] << 8) | Program(cpu.a[1] - 2)[1]);
    return Admit(cpu, (x < 0 && x >= -kSide - cameraShift) ||
                      (x > right && x <= right + kSide - cameraShift));
}

template<int Reg, int Right>
bool UnsignedEdge(M68K &cpu) {
    const auto x = static_cast<int16_t>(cpu.d[Reg]);
    return Admit(cpu, (x < 0 && x > -16 - cameraShift) || (x > Right && x <= Right + kSide - cameraShift));
}

template<int First>
bool LeftEdge(M68K &cpu) {
    const auto x = static_cast<int16_t>(cpu.d[3]);
    return Admit(cpu, x < First && x >= First - kSide - cameraShift);
}

bool RightEdge(M68K &cpu) {
    const auto x = static_cast<int16_t>(cpu.d[3]);
    return Admit(cpu, x >= 448 && x < 448 + kSide - cameraShift);
}

bool FlippedRightEdge(M68K &cpu) {
    const auto x = static_cast<int16_t>(cpu.d[3]);
    const int limit = static_cast<int16_t>(cpu.d[5]);
    const bool inclusive = Program(cpu.pc)[0] == 0x62; // BHI rather than BGE
    const int firstOutside = limit + inclusive;
    return Admit(cpu, x >= firstOutside && x < firstOutside + kSide - cameraShift);
}

void ObjectEdges(HookFn fn, std::initializer_list<uint32_t> addresses) {
    for (auto address : addresses) AddHook(address, "WideObjectEdge", fn);
}

// Retain the board's character margin, reaction to collisions, depth limits
// and special moves. Only change the two horizontal bounds.
bool PlayerBounds(M68K &cpu) {
    if (!FeatureOn("widescreen") || Ram16(kStep) != kStepPlay) return false;
    const uint32_t player = cpu.a[6] & 0xffffff;
    const int x = static_cast<int16_t>(Ram16(player + 0x20) - Ram16(kCameraX));
    const int bound = std::clamp(x, 32 - kSide - cameraShift, Video::kWidth - 32 + kSide - cameraShift);
    if (bound == x) {
        cpu.pc = 0x0207A0;
    } else {
        cpu.d[1] = (cpu.d[1] & 0xffff0000) | static_cast<uint16_t>(bound);
        cpu.pc = 0x02077A;
    }
    return true;
}
// The barrel round uses a sight actor with its own clamp. Its world position
// also drives hit detection, so move the sight itself out to the visible edges.
bool BonusSightBounds(M68K &cpu) {
    if (!WideStage() || Ram8(kStage) != 7) return false;
    const uint32_t position = (cpu.a[6] + 0x20) & 0xffffff;
    const int x = static_cast<int16_t>(Ram16(position));
    const int left = static_cast<int16_t>(Ram16(kCameraX)) - kSide - cameraShift;
    const int bound = std::clamp(x, left, left + Video::kWidth + 2 * kSide - 1);
    if (bound != x) SetRam16(position, static_cast<uint16_t>(bound));
    cpu.pc = 0x02654E; // retain the original vertical clamp
    return true;
}

// The world ends with empty columns in some rooms (the bar and the laboratory).
// Inspect the two scenery planes, ignoring the fixed text and life bars on scroll 1.
// If the other side has scenery, shift the view by just the missing width.
bool EmptyColumn(const Machine &m, int plane, int x, uint16_t blank) {
    const int size = 8 << plane;
    const unsigned base = (m.cpsA[1 + plane] << 8) & 0x3c000;
    if (base + 0x4000 > m.videoRam.size()) return false;
    const int col = ((static_cast<uint16_t>(x + m.cpsA[6 + 2 * plane])) / size) & 63;
    const int group = 32 >> plane;
    const int top = Video::kFirstLine + m.cpsA[7 + 2 * plane];
    for (int y = top / size; y <= (top + Video::kHeight - 1) / size; ++y) {
        const int row = y & 63;
        const int tile = (row % group) + col * group + (row / group) * 64 * group;
        const uint8_t *at = &m.videoRam[base + 4 * tile];
        const int code = (at[0] << 8) | at[1];
        if (code != blank && (code || at[2] || at[3])) return false;
    }
    return true;
}

int EmptySide(const Machine &m, bool right) {
    int n = 0;
    while (n < kSide) {
        const int x = right ? Video::kFirstColumn + Video::kWidth + kSide - 1 - n : n;
        if (!EmptyColumn(m, 1, x, 0x5400) || !EmptyColumn(m, 2, x, 0x1b80)) break;
        // Advance to the nearest tile boundary on either plane.
        const int x2 = (x + m.cpsA[8]) & 15, x3 = (x + m.cpsA[10]) & 31;
        n += right ? std::min(x2 + 1, x3 + 1) : std::min(16 - x2, 32 - x3);
    }
    return std::min(n, kSide);
}

// On the continue screen the two scenery planes share a fixed map. The game
// never writes its outer columns; tile zero is lettering on this graphics set.
bool FixedScreenSide(int which, int, int, int, uint16_t &code, uint16_t &attr) {
    constexpr uint16_t blanks[] = {0x0020, 0x5400, 0x1b80};
    code = blanks[which];
    attr = 0;
    return true;
}

int ObjectShift(uint32_t, uint16_t, int) {
    return -cameraShift;
}

// The barrel arena repeats one tile across the roof above its scoreboard,
// but the map stops at the original screen edges in that row.
bool BarrelRoof(int plane, int col, int row, int, uint16_t &code, uint16_t &attr) {
    if (plane != 2 || row != 39 || (col != 8 && col != 9 && col != 22 && col != 23)) return false;
    code = 0x1e40;
    attr = 0x0002;
    return true;
}

bool HasBarrelRoof(const Machine &m) {
    if (Ram8(kStage) != 7 || !WideStage()) return false;
    const unsigned base = (m.cpsA[3] << 8) & 0x3c000;
    constexpr int index = (39 % 8) + 10 * 8 + (39 / 8) * 512;
    if (base + index * 4 + 4 > m.videoRam.size()) return false;
    const uint8_t *tile = &m.videoRam[base + index * 4];
    return tile[0] == 0x1e && tile[1] == 0x40 && tile[2] == 0 && tile[3] == 2;
}
} // namespace

void PunisherWideFrame(Machine &machine) {
    auto &wide = machine.video.wide;
    wide.side = FeatureOn("widescreen") ? kSide : 0;
    const bool fixedMap = machine.cpsA[2] == machine.cpsA[3];
    wide.tile = (wide.side && fixedMap) ? FixedScreenSide : nullptr;
    if (wide.side && !fixedMap && HasBarrelRoof(machine)) wide.tile = BarrelRoof;
    cameraShift = (wide.side && !fixedMap) ? EmptySide(machine, true) - EmptySide(machine, false) : 0;
    wide.scroll[1] = wide.scroll[2] = cameraShift;
    wide.object = wide.side ? ObjectShift : nullptr;
    wide.signedCps1X = wide.side != 0;
}

void RegisterPunisherWide() {
    Game &game = TheGame();
    game.features.push_back({"widescreen", "Display", "Widescreen",
        "Draw the stage in 16:9 and extend the horizontal playing area.", kFeatureFirstPage});
    game.widestPicture = [] { return Video::kWidth + (FeatureOn("widescreen") ? 2 * kSide : 0); };
    AddHook(HookAt(0x02075E), "WidePlayerBounds", PlayerBounds);
    AddHook(HookAt(0x02652E), "WideBonusSight", BonusSightBounds);
    AddHook(HookAt(0x000FA8), "WideActorEdge", ActorEdge<0>);
    AddHook(HookAt(0x000FEC), "WideActorBox", ActorEdge<1>);
    ObjectEdges(UnsignedEdge<3, 448>, {
        HookAt(0x0150D0), HookAt(0x0151BE), HookAt(0x015272), HookAt(0x015588), HookAt(0x01582C),
        HookAt(0x015902), HookAt(0x015920), HookAt(0x01593A), HookAt(0x016036), HookAt(0x016144),
        HookAt(0x0161BC), HookAt(0x016274), HookAt(0x0162F8), HookAt(0x01684E), HookAt(0x0168D8),
        HookAt(0x016C3C), HookAt(0x016C5E), HookAt(0x016C78), HookAt(0x016D12), HookAt(0x016D34),
        HookAt(0x016D4E)
    });
    ObjectEdges(UnsignedEdge<3, 464>, {
        HookAt(0x0151EC), HookAt(0x0152A6), HookAt(0x015600), HookAt(0x01595C), HookAt(0x01597A),
        HookAt(0x015994), HookAt(0x016172), HookAt(0x0161EA), HookAt(0x0162A8), HookAt(0x01632C),
        HookAt(0x016C9A), HookAt(0x016CBC), HookAt(0x016CD6), HookAt(0x016D70), HookAt(0x016D92),
        HookAt(0x016DAC)
    });
    ObjectEdges(LeftEdge<-64>, {
        HookAt(0x015320), HookAt(0x0153E0), HookAt(0x015436), HookAt(0x0154C2), HookAt(0x0156AE),
        HookAt(0x0163AA), HookAt(0x016438), HookAt(0x0164F8), HookAt(0x01654E), HookAt(0x0165AC),
        HookAt(0x016616), HookAt(0x0166A6), HookAt(0x016776), HookAt(0x01698E),
        HookAt(0x016A6A)
    });
    ObjectEdges(RightEdge, {
        HookAt(0x015328), HookAt(0x0153E8), HookAt(0x01543E), HookAt(0x0154CA), HookAt(0x0156B6),
        HookAt(0x0163B2), HookAt(0x01643E), HookAt(0x016500), HookAt(0x016556), HookAt(0x0165B4),
        HookAt(0x01661E), HookAt(0x0166AE), HookAt(0x01677E), HookAt(0x016996),
        HookAt(0x016A72)
    });
    ObjectEdges(LeftEdge<49>, {
        HookAt(0x015354), HookAt(0x015522), HookAt(0x015706), HookAt(0x0163DE), HookAt(0x016468),
        HookAt(0x016706), HookAt(0x0167DA), HookAt(0x0169E6),
        HookAt(0x016AC2)
    });
    ObjectEdges(FlippedRightEdge, {
        HookAt(0x015362), HookAt(0x015530), HookAt(0x015714), HookAt(0x015896), HookAt(0x0163EA),
        HookAt(0x016476), HookAt(0x016714), HookAt(0x0167E8), HookAt(0x0169F4),
        HookAt(0x016AD0)
    });
    ObjectEdges(UnsignedEdge<0, 448>, {
        HookAt(0x015A24), HookAt(0x015A8E), HookAt(0x015AF8), HookAt(0x015B30)
    });
    ObjectEdges(UnsignedEdge<0, 464>, {
        HookAt(0x015A58), HookAt(0x015AC2)
    });
}
} // namespace cps2recomp
