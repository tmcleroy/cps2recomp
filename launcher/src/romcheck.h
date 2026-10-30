// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
//
// The check of a folder of ROM sets, done by the launcher itself when it opens (the `roms` folder beside it).
// It is tools/validate_cps2_roms.py's check, in the launcher: every zip of the folder is read the way the games
// read a set (machine/romset.cpp) — its files looked up by CRC32, never by name — against every game's game.cfg,
// so that the player is told which of their sets are the ones their games need, which are another version's, and
// which games have no set at all. Nothing is ever written: the folder is only read.
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace romcheck {

// One file a set needs: its name in ROM collections (for a message) and the CRC32 it must have
struct Need {
    std::string name;
    uint32_t crc = 0;
};

// One way a game can be read: its own set, or another region of it (game.cfg's other_set: the same code, with its
// own program files and key)
struct Region {
    std::string id;             // the set's name in ROM collections (mvsc, mvscu...)
    std::string name;           // as the player reads it
    std::vector<Need> files;    // everything it needs, its key file too (a CPS-1 game has none)
};

// A game's sets, from its game.cfg
struct Game {
    std::string id;                 // its short name (a folder under games/)
    std::string name;               // as the player reads it
    std::string set;                // its set's name in ROM collections
    std::string parent;             // the set it shares files with ("" when none)
    std::vector<Region> regions;    // its own set first, then its other regions
};

// What the folder had to say about one game
struct Result {
    std::string id;                         // its short name (a folder under games/): the launcher's game with it
    std::string game;                       // as the player reads it
    bool ok = false;
    std::string zip;                        // the zip it was read from, by name ("" when none could be)
    std::string path;                       // ... and where it is (what the launcher keeps as the game's file)
    std::string region;                     // which of its regions was read ("" when none could be)
    std::vector<std::string> problems;      // why it could not be read (the first set tried)
    std::vector<std::string> extras;        // files of the zip that no region of the game uses
};

// The folder, and every game's set in it
struct Report {
    std::filesystem::path folder;
    bool present = false;                   // the folder is there
    bool empty = true;                      // ... and has no zip in it
    bool unreadable = false;                // ... and could not be read at all (a zip the launcher cannot look into)
    std::vector<Result> games;              // every game of this build, in its table's order
    std::vector<std::string> unused;        // the folder's zips that are no game's set
    int ok = 0;                             // how many games' sets were found
    int failed = 0;                         // ... and how many were not
};

// Every game's set looked for in the folder (its zips read once)
Report scan(const std::filesystem::path& folder, const std::vector<Game>& games);

// The games this launcher knows, from their game.cfg (launcher/src/description.cpp)
const std::vector<Game>& games();

}  // namespace romcheck
