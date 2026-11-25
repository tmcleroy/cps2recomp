// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.

#include "native_render.h"

#include "../runtime/state.h"

#include <cstring>

namespace cps2recomp::native_video {

namespace {
    constexpr int kFirstColumn = 64, kFirstLine = 16; // (where the picture starts in the board's raster)
    constexpr uint16_t kBackdrop = 0xBFF;

    inline uint16_t Word(const uint8_t *p) {
        return static_cast<uint16_t>((p[0] << 8) | p[1]);
    }
    // Eight pixels of a tile row from its four bytes: one bit plane per byte, leftmost pixel in bit 7
    inline void Unpack(const uint8_t *row, uint8_t *pixels) {
        const uint8_t b0 = row[0], b1 = row[1], b2 = row[2], b3 = row[3];
        for (int k = 0; k < 8; k++) {
            const int s = 7 - k;
            pixels[k] = static_cast<uint8_t>(((b0 >> s) & 1) | (((b1 >> s) & 1) << 1) | (((b2 >> s) & 1) << 2) | (((b3 >> s) & 1) << 3));
        }
    }
} // namespace

void Raster::Serialize(Archive &ar) {
    ar.Pod(m_colours);
    ar.Vec(m_tiles);
    ar.Pod(m_priority);
}

// A colour: 4 bits each of brightness, red, green, blue
void Raster::Upload(const PaletteUpload &upload) {
    for (int page = 0; page < kPages; page++) {
        if (!(upload.pages & (1u << page))) {
            continue;
        }
        for (int n = 0; n < kPageColours; n++) {
            const uint16_t colour = upload.colours[page][n];
            const unsigned bright = 0x0F + ((colour >> 12) << 1);
            const unsigned r = ((colour >> 8) & 0x0F) * 0x11 * bright / 0x2D;
            const unsigned g = ((colour >> 4) & 0x0F) * 0x11 * bright / 0x2D;
            const unsigned b = (colour & 0x0F) * 0x11 * bright / 0x2D;
            m_colours[kPageColours * page + n] = r | (g << 8) | (b << 16);
        }
    }
}

// An entry: x (its level in the top three bits), y (two more bits of the tile's number), the tile, the attributes
// (colour set, flips, marked to take the offsets, a block of up to 16 x 16 tiles). The list ends at the first entry
// marked so. Later entries are in front: the tiles are kept front to back.
void Raster::Objects(const ObjectList &list, const PictureOptions &options) {
    m_tiles.clear();
    m_priority = list.priority;
    uint16_t words[kObjectWords];
    memcpy(words, list.words, sizeof(words));
    // (the list ends at the first entry marked so: on a CPS-1, 256 entries, its attributes' high byte FF)
    const int size = m_cps1 ? 0x400 : kObjectWords;
    int last = size - 4;
    for (int n = 0; n < size; n += 4) {
        if (m_cps1 ? (words[n + 3] & 0xFF00) == 0xFF00 : (words[n + 1] >= 0x8000 || words[n + 3] >= 0xFF00)) {
            last = n - 4;
            break;
        }
    }
    if (options.objectsTaken) {
        options.objectsTaken(words, last); // (the game's sources' change to the list, as Video::LatchObjects makes it)
    }
    if (m_cps1) {
        // (a CPS-1's: the first entry in front; a block only when the attributes' high byte says, its tiles going on
        // within their row of sixteen whatever the flips)
        for (int i = 0; i <= last; i += 4) {
            const int x = words[i], y = words[i + 1];
            const uint32_t code = words[i + 2];
            const uint16_t attr = words[i + 3];
            const bool blocks = (attr & 0xFF00) != 0;
            const int across = blocks ? ((attr >> 8) & 0x0F) + 1 : 1, down = blocks ? ((attr >> 12) & 0x0F) + 1 : 1;
            const bool flipX = attr & 0x20, flipY = attr & 0x40;
            for (int ty = 0; ty < down; ty++) {
                for (int tx = 0; tx < across; tx++) {
                    ObjectTile t;
                    const int tileX = static_cast<int16_t>(x) + tx * 16;
                    t.x = static_cast<int16_t>(options.signedCps1X ? tileX : (tileX & 0x1FF));
                    t.y = static_cast<int16_t>((y + ty * 16) & 0x1FF);
                    const int cx = flipX ? across - 1 - tx : tx, cy = flipY ? down - 1 - ty : ty;
                    t.tile = (code & ~0xFu) + ((code + cx) & 0xF) + 0x10 * cy;
                    if (m_gfx) {
                        // (through the graphics mapper, in 8-pixel units as the scroll screens': Video::DrawObjectsCps1)
                        const int64_t mapped = m_gfx->Map(0, t.tile << 1);
                        t.tile = mapped < 0 ? kNoTile : static_cast<uint32_t>(mapped >> 1);
                    }
                    t.colour = static_cast<uint16_t>((attr & 0x1F) * 16);
                    t.flipX = flipX;
                    t.flipY = flipY;
                    t.entry = static_cast<uint16_t>(i / 4);
                    t.entryTile = code;
                    t.attributes = attr;
                    t.shift = static_cast<int16_t>(options.object ? options.object(code, attr, i / 4) : 0);
                    t.hidden = options.hideObject && options.hideObject(code, attr);
                    m_tiles.push_back(t);
                }
            }
        }
        return;
    }
    for (int i = last; i >= 0; i -= 4) {
        const int x = words[i], y = words[i + 1];
        const uint32_t code = words[i + 2] + ((y & 0x6000u) << 3);
        const uint16_t attr = words[i + 3];
        const int across = ((attr >> 8) & 0x0F) + 1, down = ((attr >> 12) & 0x0F) + 1;
        const bool flipX = attr & 0x20, flipY = attr & 0x40;
        for (int ty = 0; ty < down; ty++) {
            for (int tx = 0; tx < across; tx++) {
                ObjectTile t;
                t.x = static_cast<int16_t>(x + tx * 16); // (the level's bits go with it, as the board's sum has them)
                t.y = static_cast<int16_t>(y + ty * 16);
                if (across == 1 && down == 1) {
                    t.tile = code;
                } else {
                    const int cx = flipX ? across - 1 - tx : tx, cy = flipY ? down - 1 - ty : ty;
                    // (a plain block wraps within its row of sixteen tiles; flipped ones do not)
                    t.tile = (!flipX && !flipY) ? (code & ~0xFu) + ((code + cx) & 0xF) + 0x10 * cy : code + cx + 0x10 * cy;
                }
                t.colour = static_cast<uint16_t>((attr & 0x1F) * 16);
                t.flipX = flipX;
                t.flipY = flipY;
                t.relative = attr & 0x80;
                t.level = static_cast<uint8_t>((x >> 13) & 7);
                t.entry = static_cast<uint16_t>(i / 4);
                t.entryTile = code;
                t.attributes = attr;
                t.shift = static_cast<int16_t>(options.object ? options.object(code, attr, i / 4) : 0);
                t.hidden = options.hideObject && options.hideObject(code, attr);
                t.extra = list.fromExtra[i / 4] != 0;
                t.marks = static_cast<uint8_t>((options.panelObject && options.panelObject(code, attr) ? kPanelPixel : 0) |
                                               (options.markFront && (x & 0x1000) ? kFrontPixel : 0));
                m_tiles.push_back(t);
            }
        }
    }
}

void Raster::DrawScroll(int which, int rasterLine, const Layers &layers, const uint8_t *videoRam, const PictureOptions &options, int width,
                        uint16_t *pens, uint8_t *priority, uint8_t mark, const uint8_t *scrollExtra, uint8_t *high) const {
    // (a CPS-2's scroll screens' tiles are in the graphics' second 8 MB; a CPS-1's where the objects' are)
    const uint32_t tiles = m_cps1 ? 0 : 0x800000;
    const uint32_t base = (static_cast<uint32_t>(layers.maps[which]) * 256) & ~0x3FFFu & 0x3FFFF;
    const uint8_t *map = &videoRam[base];
    const int shift = 3 + which, size = 1 << shift;
    const uint32_t mapMask = (64u << shift) - 1; // (the map is 64 x 64 tiles)
    uint32_t scrollX = layers.scroll[which][0];
    const uint32_t scrollY = layers.scroll[which][1];
    const bool rowScroll = which == 1 && (layers.videoControl & 1);
    if (rowScroll) {
        // (scroll 2's row scroll: a place across for each line, from the game's table)
        const uint32_t rows = (static_cast<uint32_t>(layers.rows) * 256) & ~0x7FFu & 0x3FFFF;
        scrollX += Word(&videoRam[rows + (((rasterLine + layers.rowOffset) & 0x3FF) * 2)]);
    }
    const uint32_t y = (rasterLine + scrollY) & mapMask, row = y >> shift;
    // (the board's own columns, where they are in a wider picture, and where the screen is moved to in it)
    const int ownLeft = options.side + options.scroll[which] +
                        (rowScroll && options.rows ? options.rows[(rasterLine + layers.rowOffset) & 0x3FF] : 0);
    const int ownRight = ownLeft + kWidth;
    const uint32_t firstX = static_cast<uint32_t>(kFirstColumn - ownLeft + static_cast<int>(scrollX)) & mapMask;
    const uint16_t colourBase = static_cast<uint16_t>(0x20 * (which + 1));
    int out = -static_cast<int>(firstX & (size - 1));
    uint32_t column = firstX >> shift;
    for (; out < width; out += size, column = (column + 1) & 63) {
        uint32_t index;
        switch (which) {
        case 0: index = (row & 0x1F) + (column << 5) + ((row & 0x20) << 6); break;
        case 1: index = (row & 0x0F) + (column << 4) + ((row & 0x30) << 6); break;
        default: index = (row & 0x07) + (column << 3) + ((row & 0x38) << 6); break;
        }
        uint32_t code = Word(&map[index * 4]);
        uint16_t attr = Word(&map[index * 4 + 2]);
        if (options.tile && (out < ownLeft || out + size > ownRight)) {
            // (beyond the board's own columns the game's sources may know better than the map)
            uint16_t itsCode = 0, itsAttr = 0;
            if (options.tile(which, static_cast<int>(column), static_cast<int>(row), out, itsCode, itsAttr)) {
                code = itsCode;
                attr = itsAttr;
            }
        }
        uint32_t at;
        int rowBytes;
        switch (which) {
        case 0: at = tiles + code * 64 + ((column & 1) ? 4 : 0); rowBytes = 8; break; // (odd columns: a strip's right half)
        case 1: at = tiles + code * 128; rowBytes = 8; break;
        default: code &= 0x3FFF; at = tiles + code * 512; rowBytes = 16; break;
        }
        if (m_gfx) {
            // (a CPS-1 whose mapper does not keep the tiles in order: in 8-pixel units, a 16-pixel tile two, a 32-pixel
            // tile eight; one it has nowhere is not drawn: Video::DrawScroll)
            const int64_t mapped = m_gfx->Map(which + 1, code << (which == 0 ? 0 : which == 1 ? 1 : 3));
            if (mapped < 0) {
                continue;
            }
            at = static_cast<uint32_t>(mapped) * 64 + (which == 0 && (column & 1) ? 4 : 0);
        }
        const int line = static_cast<int>(y & (size - 1));
        const bool flipX = attr & 0x20, flipY = attr & 0x40;
        at += static_cast<uint32_t>(flipY ? size - 1 - line : line) * rowBytes;
        // (a tile of another set's: in its graphics)
        const bool extra = m_extraGraphics && scrollExtra && scrollExtra[(base + index * 4) / 4];
        const uint8_t *graphics = extra ? m_extraGraphics : m_graphics;
        if (at + rowBytes > (extra ? m_extraGraphicsSize : m_graphicsSize)) {
            continue;
        }
        const uint16_t colour = static_cast<uint16_t>(((attr & 0x1F) + colourBase) * 16);
        uint8_t pixels[32];
        for (int part = 0; part < size / 8; part++) {
            Unpack(&graphics[at + part * 4], &pixels[part * 8]);
        }
        for (int k = 0; k < size; k++) {
            const int x = out + k;
            if (x < 0 || x >= width) {
                continue;
            }
            const uint8_t pen = pixels[flipX ? size - 1 - k : k];
            if (pen != 15) {
                pens[x] = static_cast<uint16_t>(colour + pen);
                priority[x] |= mark;
                if (high) {
                    // (a CPS-1's screen just below the objects: its pens the tile's group's mask puts in front of them)
                    high[x] = (layers.masks[(attr >> 7) & 3] >> pen) & 1;
                }
            }
        }
    }
}

void Raster::Overrides(int line, const Layers &layers, const uint8_t *videoRam, const PictureOptions &options,
                       std::vector<TileOverride> &out) const {
    out.clear();
    if (!options.tile) {
        return;
    }
    const int rasterLine = line + kFirstLine, width = kWidth + 2 * options.side;
    for (int which = 0; which < 3; which++) {
        // (as DrawScroll walks the screen's tiles across the line, asking for those not wholly in the board's columns)
        const int shift = 3 + which, size = 1 << shift;
        const uint32_t mapMask = (64u << shift) - 1;
        uint32_t scrollX = layers.scroll[which][0];
        const uint32_t scrollY = layers.scroll[which][1];
        const bool rowScroll = which == 1 && (layers.videoControl & 1);
        if (rowScroll) {
            const uint32_t rows = (static_cast<uint32_t>(layers.rows) * 256) & ~0x7FFu & 0x3FFFF;
            scrollX += Word(&videoRam[rows + (((rasterLine + layers.rowOffset) & 0x3FF) * 2)]);
        }
        const uint32_t y = (rasterLine + scrollY) & mapMask, row = y >> shift;
        const int ownLeft = options.side + options.scroll[which] +
                            (rowScroll && options.rows ? options.rows[(rasterLine + layers.rowOffset) & 0x3FF] : 0);
        const int ownRight = ownLeft + kWidth;
        const uint32_t firstX = static_cast<uint32_t>(kFirstColumn - ownLeft + static_cast<int>(scrollX)) & mapMask;
        int at = -static_cast<int>(firstX & (size - 1));
        uint32_t column = firstX >> shift;
        for (; at < width; at += size, column = (column + 1) & 63) {
            if (at >= ownLeft && at + size <= ownRight) {
                continue;
            }
            uint16_t code = 0, attributes = 0;
            if (options.tile(which, static_cast<int>(column), static_cast<int>(row), at, code, attributes)) {
                out.push_back({static_cast<int16_t>(at), static_cast<uint8_t>(which), code, attributes});
            }
        }
    }
}

// The objects' tiles, front to back: a pixel once an object's stays the front one's; whether it shows over the scroll
// screens' pens there is by its level (masks)
void Raster::DrawObjects(int rasterLine, const Layers &layers, const PictureOptions &options, int width, uint16_t *pens, uint8_t *priority,
                         const uint32_t *masks, uint8_t *marks) const {
    const int xOffset = 64 - layers.xOffset, yOffset = 16 - layers.yOffset;
    for (const ObjectTile &t : m_tiles) {
        if (options.hideObject && options.hideObject(t.entryTile, t.attributes)) {
            continue;
        }
        int x = t.x, y = t.y;
        if (t.relative) {
            x += layers.xOffset;
            y += layers.yOffset;
        }
        const int sy = (y + yOffset) & 0x3FF;
        if (rasterLine < sy || rasterLine >= sy + 16) {
            continue;
        }
        // (where the picture's first column is in the objects' count across, which wraps at 1024: the board's own
        // first column, less the added side and where the game's sources move this object to)
        const int first = kFirstColumn - options.side - (options.object ? options.object(t.entryTile, t.attributes, t.entry) : 0) - 16;
        const int sx = ((x + xOffset - first) & 0x3FF) - 16;
        if (sx <= -16 || sx >= width) {
            continue;
        }
        const int line = rasterLine - sy;
        const uint32_t at = t.tile * 128 + static_cast<uint32_t>(t.flipY ? 15 - line : line) * 8;
        const uint8_t *graphics = t.extra && m_extraGraphics ? m_extraGraphics : m_graphics;
        if (at + 8 > (t.extra && m_extraGraphics ? m_extraGraphicsSize : m_graphicsSize)) {
            continue;
        }
        uint8_t pixels[16];
        Unpack(&graphics[at], pixels);
        Unpack(&graphics[at + 4], pixels + 8);
        const uint32_t mask = masks[t.level] | 0x80000000u;
        for (int k = 0; k < 16; k++) {
            const int px = sx + k;
            if (px < 0 || px >= width) {
                continue;
            }
            const uint8_t pen = pixels[t.flipX ? 15 - k : k];
            if (pen != 15) {
                const bool shown = !((mask >> (priority[px] & 0x1F)) & 1);
                if (shown) {
                    pens[px] = static_cast<uint16_t>(t.colour + pen);
                }
                if (marks && priority[px] != 31) {
                    marks[px] = static_cast<uint8_t>(t.marks & (shown ? 0xFF : ~kFrontPixel));
                }
                if (options.objectEntries && priority[px] != 31) {
                    options.objectEntries[static_cast<size_t>(rasterLine - kFirstLine) * width + px] = static_cast<uint16_t>(t.entry + 1);
                }
                priority[px] = 31;
            }
        }
    }
}

// Pixels nothing was drawn at (the backdrop's pen), in runs no wider than the added side, take the line's own picture
// mirrored at the nearest drawn pixel (or that pixel, where the mirror finds nothing): Video::FillHoles
static void FillHoles(uint16_t *pens, int width, int side) {
    uint16_t drawn[Raster::kMaxWidth];
    memcpy(drawn, pens, static_cast<size_t>(width) * sizeof(uint16_t));
    for (int start = 0; start < width;) {
        if (drawn[start] != kBackdrop) {
            start++;
            continue;
        }
        int end = start;
        while (end < width && drawn[end] == kBackdrop) {
            end++;
        }
        const bool left = start > 0, right = end < width;
        if (end - start <= side && (left || right)) {
            for (int x = start; x < end; x++) {
                const bool fromLeft = left && (!right || x - (start - 1) <= end - x);
                const int edge = fromLeft ? start - 1 : end;
                const int mirror = 2 * edge - x;
                pens[x] = mirror >= 0 && mirror < width && drawn[mirror] != kBackdrop ? drawn[mirror] : drawn[edge];
            }
        }
        start = end;
    }
}

void Raster::LineCps1(int line, const Layers &layers, const uint8_t *videoRam, const PictureOptions &options, uint32_t *out) const {
    const int rasterLine = line + kFirstLine, width = kWidth + 2 * options.side;
    uint16_t pens[kMaxWidth];
    uint8_t priority[kMaxWidth], high[kMaxWidth];
    for (int x = 0; x < width; x++) {
        pens[x] = kBackdrop;
    }
    memset(priority, 0, sizeof(priority));
    memset(high, 0, sizeof(high));
    uint16_t drawn[kMaxWidth];
    if (options.markObjects) {
        memset(drawn, 0xFF, sizeof(drawn));
    }
    // The layer control's four places, back to front, the objects (0) among them
    const uint16_t control = layers.layerControl, videoControl = layers.videoControl;
    const unsigned order[4] = {(control >> 6) & 3u, (control >> 8) & 3u, (control >> 10) & 3u, (control >> 12) & 3u};
    for (int n = 0; n < 4; n++) {
        const unsigned layer = order[n];
        if (layer == 0) {
            if (!(options.layerMask & 1)) {
                continue;
            }
            // The objects, the first entry in front: a pixel once one's stays its, shown unless the screen below has a
            // high pen there
            const int xShift = options.side - kFirstColumn;
            uint8_t claimed[kMaxWidth] = {};
            for (const ObjectTile &t : m_tiles) {
                if (options.hideObject && options.hideObject(t.entryTile, t.attributes)) {
                    continue;
                }
                const int sy = t.y;
                if (rasterLine < sy || rasterLine >= sy + 16) {
                    continue;
                }
                const int sx = t.x + xShift - (options.object ? options.object(t.entryTile, t.attributes, t.entry) : 0);
                if (sx <= -16 || sx >= width) {
                    continue;
                }
                const int row = rasterLine - sy;
                const uint32_t at = t.tile * 128 + static_cast<uint32_t>(t.flipY ? 15 - row : row) * 8;
                if (at + 8 > m_graphicsSize) {
                    continue;
                }
                uint8_t pixels[16];
                Unpack(&m_graphics[at], pixels);
                Unpack(&m_graphics[at + 4], pixels + 8);
                for (int k = 0; k < 16; k++) {
                    const int px = sx + k;
                    if (px < 0 || px >= width || claimed[px]) {
                        continue;
                    }
                    const uint8_t pen = pixels[t.flipX ? 15 - k : k];
                    if (pen != 15) {
                        claimed[px] = 1;
                        if (options.objectEntries) {
                            options.objectEntries[static_cast<size_t>(line) * width + px] = static_cast<uint16_t>(t.entry + 1);
                        }
                        if (!high[px]) {
                            pens[px] = static_cast<uint16_t>(t.colour + pen);
                            if (options.markObjects) {
                                drawn[px] = pens[px];
                            }
                        }
                    }
                }
            }
            continue;
        }
        const bool on = (options.layerMask & (1u << layer)) && (control & m_enable[layer - 1]) &&
                        (layer == 1 || (videoControl & (layer == 2 ? 0x04 : 0x08)));
        if (on) {
            const bool belowObjects = n < 3 && order[n + 1] == 0;
            if (belowObjects) {
                memset(high, 0, sizeof(high));
            }
            DrawScroll(static_cast<int>(layer) - 1, rasterLine, layers, videoRam, options, width, pens, priority, static_cast<uint8_t>(1u << n),
                       nullptr, belowObjects ? high : nullptr);
        }
    }
    for (int x = 0; x < width; x++) {
        out[x] = m_colours[pens[x]];
    }
    if (options.markObjects) {
        // (an object's pixel; and a marked scroll screen's, told by its pen's page: page n is scroll screen n's)
        for (int x = 0; x < width; x++) {
            const unsigned page = pens[x] >> 9;
            const bool layer = drawn[x] != pens[x] && pens[x] != kBackdrop && page >= 1 && page <= 3 && ((options.markLayers >> page) & 1);
            out[x] |= drawn[x] == pens[x] || layer ? 0x80000000u | (static_cast<uint32_t>(pens[x] >> 4) & 0x1F) << 24 : 0;
        }
    }
}

void Raster::Line(int line, const Layers &layers, const uint8_t *videoRam, const PictureOptions &options, uint32_t *out,
                  const uint8_t *scrollExtra) const {
    if (m_cps1) {
        LineCps1(line, layers, videoRam, options, out);
        return;
    }
    const int rasterLine = line + kFirstLine;
    const int width = kWidth + 2 * options.side;
    uint16_t pens[kMaxWidth];
    uint8_t priority[kMaxWidth];
    for (int x = 0; x < width; x++) {
        pens[x] = kBackdrop;
    }
    memset(priority, 0, sizeof(priority));
    // The layer control orders the scroll screens back to front and switches them on; the object list's priority
    // control gives each its level against the objects
    const uint16_t control = layers.layerControl, videoControl = layers.videoControl;
    unsigned l0 = (control >> 6) & 3, l1 = (control >> 8) & 3, l2 = (control >> 10) & 3, l3 = (control >> 12) & 3;
    unsigned p0 = (m_priority >> (4 * l0)) & 0xF, p1 = (m_priority >> (4 * l1)) & 0xF;
    unsigned p2 = (m_priority >> (4 * l2)) & 0xF, p3 = (m_priority >> (4 * l3)) & 0xF;
    if (l0 == 0) {
        l0 = l1;
        l1 = 0;
        p0 = p1;
    }
    if (l1 == 0) {
        l1 = l2;
        l2 = 0;
        p1 = p2;
    }
    if (l2 == 0) {
        l2 = l3;
        l3 = 0;
        p2 = p3;
    }
    // (masks[n]: the combinations of the screens' marks, bit 0 the back one, behind which an object of level n is
    // not drawn)
    uint32_t masks[8] = {};
    {
        uint32_t mask0 = 0xAA, mask1 = 0xCC;
        if (p0 > p1) {
            mask0 &= ~0x88u;
        }
        if (p0 > p2) {
            mask0 &= ~0xA0u;
        }
        if (p1 > p2) {
            mask1 &= ~0xC0u;
        }
        masks[0] = 0xFF;
        for (unsigned n = 1; n < 8; n++) {
            if (n <= p0 && n <= p1 && n <= p2) {
                masks[n] = 0xFE;
                continue;
            }
            masks[n] = 0;
            if (n <= p0) {
                masks[n] |= mask0;
            }
            if (n <= p1) {
                masks[n] |= mask1;
            }
            if (n <= p2) {
                masks[n] |= 0xF0;
            }
        }
    }
    auto on = [&](unsigned layer) {
        if (!(options.layerMask & (1u << layer))) {
            return false;
        }
        switch (layer) {
        case 1: return (control & 0x02) != 0;
        case 2: return (control & 0x04) != 0 && (videoControl & 0x04) != 0;
        case 3: return (control & 0x08) != 0 && (videoControl & 0x08) != 0;
        default: return false;
        }
    };
    const unsigned order[3] = {l0, l1, l2};
    for (int n = 0; n < 3; n++) {
        if (order[n] && on(order[n])) {
            DrawScroll(static_cast<int>(order[n]) - 1, rasterLine, layers, videoRam, options, width, pens, priority,
                       static_cast<uint8_t>(1u << n), scrollExtra);
        }
    }
    // (holes filled from the scroll screens alone, before the objects)
    if (options.fillHoles && options.side > 0) {
        FillHoles(pens, width, options.side);
    }
    // (the marks of the objects' pixels: the panel's, those in front of the fighters)
    uint8_t marks[kMaxWidth];
    const bool marking = (options.markObjects && options.panelObject) || options.markFront;
    if (marking) {
        memset(marks, 0, sizeof(marks));
    }
    if (options.layerMask & 1) {
        DrawObjects(rasterLine, layers, options, width, pens, priority, masks, marking ? marks : nullptr);
    }
    for (int x = 0; x < width; x++) {
        out[x] = m_colours[pens[x]];
    }
    if (options.markFront) {
        // (in front: an object marked so, or a scroll screen's pixel an object of the fighters' level is behind)
        const uint32_t behind = options.frontLevel ? masks[options.frontLevel & 7] : 0;
        for (int x = 0; x < width; x++) {
            const bool front = priority[x] == 31 ? (marks[x] & kFrontPixel) != 0 : ((behind >> (priority[x] & 7)) & 1) != 0;
            out[x] |= front ? 0x40000000u : 0;
        }
    }
    if (options.markObjects) {
        // (an object's pixels, and those of the scroll screens marked, have the top byte set: 0x80 and the colour set)
        unsigned marked = 0;
        for (int n = 0; n < 3; n++) {
            if (order[n] && (options.markLayers & (1u << order[n]))) {
                marked |= 1u << n;
            }
        }
        for (int x = 0; x < width; x++) {
            const bool layer = priority[x] != 31 && (priority[x] & marked) != 0;
            const bool mark = priority[x] == 31 || layer;
            const bool panel = layer || (priority[x] == 31 && marking && (marks[x] & kPanelPixel));
            out[x] |= mark ? 0x80000000u | (static_cast<uint32_t>(pens[x] >> 4) & 0x1F) << 24 | (panel && options.panelObject ? 0x40000000u : 0) : 0;
        }
    }
}

} // namespace cps2recomp::native_video
