// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
//
// The native interface's records (docs/NATIVE_VIDEO.md; native.h has how they are made and compared)
#pragma once

#include "../machine/romset.h"

#include <cstdint>
#include <cstdio>

namespace cps2recomp {

namespace native_video {

    // A pixel's marks (PictureOptions: markObjects with panelObject, markFront): the panel's, in front of the fighters
    constexpr uint8_t kPanelPixel = 1, kFrontPixel = 2;

    // The palette has six pages of 512 colours: objects, scroll screens 1-3, and two for stars
    constexpr int kPages = 6, kPageColours = 0x200;

    // An upload of the palette: the board takes these pages of colours (a colour is 4 bits each of brightness,
    // red, green, blue) from the game's buffer; the pages not taken keep the colours they had
    struct PaletteUpload {
        uint8_t pages = 0;   // bit n: page n is taken
        uint32_t source = 0; // where the buffer is (the 68000's address)
        uint16_t colours[kPages][kPageColours] = {};
        bool operator==(const PaletteUpload &) const = default;
        // (what of it differs from another, for a report)
        void Describe(const PaletteUpload &other, FILE *out) const;
    };

    // The object list the board draws a frame from: 1,024 entries of four words (x, with the entry's priority in
    // its top bits; y; tile; attributes), as the game's code put them, and what the game's code said of all of them:
    // the priority control and the offsets across and down
    constexpr int kObjectWords = 0x1000;
    struct ObjectList {
        uint16_t words[kObjectWords] = {};
        uint16_t priority = 0, xOffset = 0, yOffset = 0;
        // (a game with another set's graphics, Vampire Savior Definitive's: which entries are drawn from them,
        // Machine::objectsFromExtra)
        uint8_t fromExtra[kObjectWords / 4] = {};
        bool operator==(const ObjectList &) const = default;
        void Describe(const ObjectList &other, FILE *out) const;
    };

    // What the CPS-2's picture is drawn from on a line, but for the palette (taken at its upload) and the object list
    // (taken as the frame starts): where the three scroll screens' maps are and where each is scrolled to; the row
    // scroll's table, the place in it and whether it is on (the video control); the layers' order and which are on
    // (the layer control); and the objects' offsets. Its words are the registers'.
    struct Layers {
        uint16_t maps[3] = {}, rows = 0, scroll[3][2] = {}, rowOffset = 0, videoControl = 0, layerControl = 0;
        uint16_t xOffset = 0, yOffset = 0;
        uint16_t masks[4] = {}; // (a CPS-1's: its four priority masks, the pens of a screen in front of the objects)
        bool operator==(const Layers &) const = default;
        void Describe(const Layers &other, FILE *out) const;
    };

    // What a game's sources set on the picture (Video's members of the same names, video.h: a wider picture, its
    // screens and objects moved, tiles beyond the board's columns, holes filled, objects hidden or changed, marks, the
    // layers drawn), which the picture drawn from the records follows as video.cpp does
    struct PictureOptions {
        int side = 0;
        int scroll[3] = {0, 0, 0};
        const int16_t *rows = nullptr;
        int (*object)(uint32_t tile, uint16_t attributes, int entry) = nullptr;
        bool (*tile)(int which, int column, int row, int at, uint16_t &code, uint16_t &attributes) = nullptr;
        bool fillHoles = false;
        bool signedCps1X = false; // keep software sprite positions beyond the edges of the 512-column raster
        bool (*hideObject)(uint32_t tile, uint16_t attributes) = nullptr;
        void (*objectsTaken)(uint16_t *list, int &last) = nullptr;
        bool markObjects = false;
        unsigned markLayers = 0;
        unsigned layerMask = 0xF;
        bool (*panelObject)(uint32_t tile, uint16_t attributes) = nullptr;
        bool markFront = false;
        unsigned frontLevel = 0;
        // (the scene export's, port/scene.h: each pixel's front object, 1 + its entry (0 none), lines of the picture's width)
        uint16_t *objectEntries = nullptr;
    };

    // The board's side: the upload the palette chip makes now, from its registers and video RAM
    // (as Video::CopyPalette reads them)
    PaletteUpload DecodePalette(const uint8_t *videoRam, const uint16_t *cpsA, const uint16_t *cpsB, const Board &board);
    // ... the object list it takes, from the bank of object memory it latches and the object registers
    ObjectList DecodeObjects(const uint8_t *bank, const uint16_t *objectRegs, const uint8_t *fromExtra = nullptr);
    // ... the layers a line is drawn with, from CPS-A's and CPS-B's registers and the object registers (the game's side
    // makes them the same way, from what the game said to each)
    Layers DecodeLayers(const uint16_t *cpsA, const uint16_t *cpsB, const uint16_t *objectRegs, uint8_t layerControl,
                        const uint8_t *masks = nullptr);
    // A CPS-1's object list: 256 entries in video RAM where CPS-A's first register says (Video::LatchObjectsCps1); the
    // rest of the record's words 0
    ObjectList DecodeObjectsCps1(const uint8_t *videoRam, const uint16_t *cpsA);

    // The game's side: a buffer of colours in video RAM as the game left it
    // (`pages` and the buffer's place as the game's code works them out)
    PaletteUpload GamePalette(const uint8_t *videoRam, uint32_t source, uint8_t pages);

    // The registers the game's code talks to the video through: the object registers (400000-40000B), the object
    // bank (8040E0), CPS-A and CPS-B (at 8001xx, and again at 8041xx)
    bool IsVideoRegister(uint32_t address);

} // namespace native_video

} // namespace cps2recomp
