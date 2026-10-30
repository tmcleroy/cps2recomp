// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
//
// The check of the `roms` folder, as tools/validate_cps2_roms.py does it: every zip is read the way the games read
// a set (machine/romset.cpp), its files looked up by CRC32 in the zip chosen and then in the zips of the set, its
// parent and its other regions beside it; the region read is the one the zip is named for, else the set's own,
// else the first other region whose files are all there. Files of a zip that no region of its game uses are
// reported as extras (the game ignores them); nothing is ever removed or rewritten. The folder is only read.
#include "romcheck.h"

#include <zlib.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <map>
#include <set>

namespace romcheck {

namespace {

uint32_t Le32(const unsigned char* p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) | (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

uint16_t Le16(const unsigned char* p) {
    return static_cast<uint16_t>(p[0] | (p[1] << 8));
}

std::string Lower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

// A file's name inside a zip, without the folders it is in
std::string BaseName(const std::string& inside) {
    const size_t slash = inside.find_last_of("/\\");
    return slash == std::string::npos ? inside : inside.substr(slash + 1);
}

// A file's name without its folders and its last extension ("roms/msh.zip" is "msh")
std::string Stem(const std::string& path) {
    const std::string base = BaseName(path);
    const size_t dot = base.find_last_of('.');
    return dot == std::string::npos || dot == 0 ? base : base.substr(0, dot);
}

struct Entry {
    std::string name;
    uint32_t crc = 0, size = 0, packed = 0, offset = 0;
    uint16_t method = 0;
};

// A zip file: its list of files (its central directory, read once) and, when a set needs one, the file unpacked
struct Zip {
    std::string path, name;
    std::vector<Entry> entries;
    bool ok = false;
    std::string error;

    bool Open(const std::string& file) {
        path = file;
        name = BaseName(file);
        FILE* f = std::fopen(file.c_str(), "rb");
        if (!f) {
            error = "cannot be opened";
            return false;
        }
        std::fseek(f, 0, SEEK_END);
        const long size = std::ftell(f);
        if (size < 22) {
            std::fclose(f);
            error = "is not a zip file";
            return false;
        }
        // (the directory is at the end, after a record that says where it is)
        const long tail_size = std::min<long>(size, 65536 + 22);
        std::vector<unsigned char> tail(static_cast<size_t>(tail_size));
        std::fseek(f, size - tail_size, SEEK_SET);
        bool good = std::fread(tail.data(), 1, tail.size(), f) == tail.size();
        long end = -1;
        for (long at = tail_size - 22; good && at >= 0; at--) {
            if (Le32(&tail[static_cast<size_t>(at)]) == 0x06054B50) {
                end = at;
                break;
            }
        }
        if (end < 0) {
            std::fclose(f);
            error = "is not a zip file";
            return false;
        }
        const size_t at_end = static_cast<size_t>(end);
        const uint32_t count = Le16(&tail[at_end + 10]);
        const uint32_t dir_size = Le32(&tail[at_end + 12]);
        const uint32_t dir_at = Le32(&tail[at_end + 16]);
        // (a zip of more than 4 GB, or with more than 65535 files, keeps its list of files in a record the launcher
        // does not read: its set cannot be taken from it. Nor can one whose list is not inside the file.)
        if (count == 0xFFFF || dir_size == 0xFFFFFFFFu || dir_at == 0xFFFFFFFFu ||
            static_cast<long long>(dir_at) + static_cast<long long>(dir_size) > static_cast<long long>(size)) {
            std::fclose(f);
            error = "is too large for the launcher to read (a zip64 file)";
            return false;
        }
        std::vector<unsigned char> dir(dir_size);
        std::fseek(f, dir_at, SEEK_SET);
        good = dir.empty() || std::fread(dir.data(), 1, dir.size(), f) == dir.size();
        std::fclose(f);
        if (!good) {
            error = "has no list of files the launcher can read";
            return false;
        }
        size_t at = 0;
        for (uint32_t n = 0; n < count && at + 46 <= dir.size() && Le32(&dir[at]) == 0x02014B50; n++) {
            Entry entry;
            entry.method = Le16(&dir[at + 10]);
            entry.crc = Le32(&dir[at + 16]);
            entry.packed = Le32(&dir[at + 20]);
            entry.size = Le32(&dir[at + 24]);
            entry.offset = Le32(&dir[at + 42]);
            const size_t name_len = Le16(&dir[at + 28]);
            if (at + 46 + name_len > dir.size()) {
                break;
            }
            entry.name.assign(reinterpret_cast<const char*>(&dir[at + 46]), name_len);
            if (!entry.name.empty() && entry.name.back() != '/') {  // (a folder is not a file)
                entries.push_back(entry);
            }
            at += 46 + name_len + Le16(&dir[at + 30]) + Le16(&dir[at + 32]);
        }
        ok = true;
        return true;
    }

    // The file unpacks, its CRC32 as the zip says ("" when it does; else why not)
    std::string Verify(const Entry& entry) const {
        if (entry.method != 0 && entry.method != 8) {
            return "uses zip compression method " + std::to_string(entry.method) + ", which the launcher cannot read (re-zip it with Deflate or Store)";
        }
        FILE* f = std::fopen(path.c_str(), "rb");
        if (!f) {
            return "cannot be opened";
        }
        unsigned char local[30];
        std::fseek(f, static_cast<long>(entry.offset), SEEK_SET);
        bool good = std::fread(local, 1, sizeof local, f) == sizeof local && Le32(local) == 0x04034B50;
        std::vector<unsigned char> packed(entry.packed);
        if (good) {
            std::fseek(f, static_cast<long>(entry.offset + 30 + Le16(&local[26]) + Le16(&local[28])), SEEK_SET);
            good = packed.empty() || std::fread(packed.data(), 1, packed.size(), f) == packed.size();
        }
        std::fclose(f);
        if (!good) {
            return "cannot be unpacked or is corrupt";
        }
        uint32_t crc = 0;
        uint64_t total = 0;
        if (entry.method == 0) {
            crc = packed.empty() ? 0u : static_cast<uint32_t>(::crc32(0, packed.data(), static_cast<uInt>(packed.size())));
            total = packed.size();
        } else {
            z_stream z = {};
            if (inflateInit2(&z, -MAX_WBITS) != Z_OK) {
                return "cannot be unpacked or is corrupt";
            }
            z.next_in = packed.data();
            z.avail_in = static_cast<uInt>(packed.size());
            unsigned char out[65536];
            int result = Z_OK;
            while (result != Z_STREAM_END) {
                z.next_out = out;
                z.avail_out = static_cast<uInt>(sizeof out);
                result = inflate(&z, Z_NO_FLUSH);
                if (result != Z_OK && result != Z_STREAM_END) {
                    break;
                }
                const size_t got = sizeof out - z.avail_out;
                if (got) {
                    crc = static_cast<uint32_t>(::crc32(crc, out, static_cast<uInt>(got)));
                    total += got;
                }
                if (result == Z_OK && z.avail_in == 0) {
                    break;  // (truncated)
                }
            }
            inflateEnd(&z);
            if (result != Z_STREAM_END) {
                return "cannot be unpacked or is corrupt";
            }
        }
        if (total != entry.size || crc != entry.crc) {
            return "is corrupt (its CRC32 is not the zip's)";
        }
        return {};
    }
};

// A zip of the folder, opened once and kept (a set may be read from several)
Zip* Opened(const std::string& path, std::map<std::string, Zip>& cache) {
    auto found = cache.find(path);
    if (found != cache.end()) {
        return found->second.ok ? &found->second : nullptr;
    }
    Zip& zip = cache[path];
    zip.Open(path);
    return zip.ok ? &zip : nullptr;
}

std::string ZipError(const std::string& path, std::map<std::string, Zip>& cache) {
    auto found = cache.find(path);
    return found == cache.end() ? std::string("cannot be read") : found->second.error;
}

// Every file of one region looked up by CRC32 in the zips of `chain`; the zips it was found in (`used`) when all of
// them are there. Every file found is unpacked too, as the game will.
std::vector<std::string> CheckFiles(const Region& region, const std::vector<const Zip*>& chain, std::vector<std::string>& used) {
    std::vector<std::string> problems;
    struct Hit {
        const Zip* zip = nullptr;
        const Entry* entry = nullptr;
    };
    std::vector<Hit> hits;
    for (const Need& need : region.files) {
        Hit hit;
        for (const Zip* zip : chain) {
            for (const Entry& entry : zip->entries) {
                if (entry.crc == need.crc) {
                    hit = {zip, &entry};
                    break;
                }
            }
            if (hit.entry) {
                break;
            }
        }
        if (!hit.entry) {
            // (a file of that name with another checksum is another version of the game: say so)
            const Entry* near = nullptr;
            for (const Zip* zip : chain) {
                for (const Entry& entry : zip->entries) {
                    if (Lower(BaseName(entry.name)) == Lower(need.name)) {
                        near = &entry;
                        break;
                    }
                }
                if (near) {
                    break;
                }
            }
            char expected[16], found[16];
            std::snprintf(expected, sizeof expected, "%08x", need.crc);
            if (near) {
                std::snprintf(found, sizeof found, "%08x", near->crc);
                problems.push_back(need.name + ": wrong CRC32 (the zip has " + found + ", " + expected + " is expected)");
            } else {
                problems.push_back(need.name + ": missing (CRC32 " + expected + ")");
            }
            continue;
        }
        hits.push_back(hit);
    }
    if (!problems.empty()) {
        return problems;
    }
    for (size_t n = 0; n < hits.size(); n++) {
        const std::string why = hits[n].zip->Verify(*hits[n].entry);
        if (!why.empty()) {
            problems.push_back(region.files[n].name + ": " + BaseName(hits[n].entry->name) + " in " + hits[n].zip->name + " " + why);
        }
    }
    if (problems.empty()) {
        for (const Hit& hit : hits) {
            if (std::find(used.begin(), used.end(), hit.zip->name) == used.end()) {
                used.push_back(hit.zip->name);
            }
        }
    }
    return problems;
}

// One zip tried as a game's set, as cps2recomp::LoadRomSet would read it
struct Candidate {
    bool ok = false;
    std::string region;                 // which region of the game was read
    std::vector<std::string> problems;  // why none could be (the first region tried)
    std::vector<std::string> extras;    // files of the zip no region of the game uses
    std::vector<std::string> used;      // the zips the files were found in
};

Candidate CheckCandidate(const std::string& path, const Game& game, const std::map<std::string, std::string>& listing,
                         std::map<std::string, Zip>& cache) {
    Candidate out;
    const std::string name = BaseName(path);
    const Zip* first = Opened(path, cache);
    if (!first) {
        out.problems.push_back(name + " " + ZipError(path, cache));
        return out;
    }
    // (the set's zip, its parent's and its other regions': a collection keeps the files a set shares there)
    std::vector<const Zip*> chain = {first};
    std::vector<std::string> beside = {game.set, game.parent};
    for (const Region& region : game.regions) {
        beside.push_back(region.id);
    }
    for (const std::string& one : beside) {
        if (one.empty()) {
            continue;
        }
        auto found = listing.find(Lower(one));
        if (found == listing.end()) {
            continue;
        }
        const Zip* other = Opened(found->second, cache);
        if (other && std::find(chain.begin(), chain.end(), other) == chain.end()) {
            chain.push_back(other);
        }
    }
    // (the region the zip is named for is read first, then the set's own, then the rest)
    const std::string chosen = Lower(Stem(path));
    std::vector<size_t> order;
    for (size_t n = 1; n < game.regions.size(); n++) {
        if (Lower(game.regions[n].id) == chosen) {
            order.push_back(n);
        }
    }
    if (!game.regions.empty()) {
        order.push_back(0);
    }
    for (size_t n = 1; n < game.regions.size(); n++) {
        if (Lower(game.regions[n].id) != chosen) {
            order.push_back(n);
        }
    }
    // (the other regions' own files are not extras: they are ROMs the game plays too)
    std::set<uint32_t> known;
    for (const Region& region : game.regions) {
        for (const Need& need : region.files) {
            known.insert(need.crc);
        }
    }
    for (const Entry& entry : first->entries) {
        if (!known.count(entry.crc)) {
            out.extras.push_back(BaseName(entry.name));
        }
    }
    for (const size_t index : order) {
        std::vector<std::string> used;
        std::vector<std::string> problems = CheckFiles(game.regions[index], chain, used);
        if (problems.empty()) {
            out.ok = true;
            out.region = game.regions[index].name;
            out.used = used;
            out.problems.clear();
            return out;
        }
        if (out.problems.empty()) {
            out.problems = problems;
        }
    }
    return out;
}

}  // namespace

namespace {

// scan's work, so that one zip the launcher cannot look into is a report and not the end of the launcher
void ScanInto(Report& report, const std::filesystem::path& folder, const std::vector<Game>& games) {
    std::error_code error;
    report.present = std::filesystem::is_directory(folder, error);
    if (!report.present) {
        return;
    }
    std::map<std::string, std::string> listing;  // (the set's name, in lowercase, and the zip's path)
    std::vector<std::string> names;              // (the zips' names, as they are on disk)
    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(folder, error)) {
        if (error || !entry.is_regular_file(error)) {
            continue;
        }
        if (Lower(entry.path().extension().u8string()) != ".zip") {
            continue;
        }
        // (a path with / for its folders on every system: it is kept as the game's file, and the games read it)
        listing[Lower(entry.path().stem().u8string())] = entry.path().generic_u8string();
        names.push_back(entry.path().filename().u8string());
    }
    std::sort(names.begin(), names.end());
    report.empty = names.empty();
    std::map<std::string, Zip> cache;
    std::set<std::string> used;  // (the zips a game's set was read from)
    for (const Game& game : games) {
        Result result;
        result.id = game.id;
        result.game = game.name;
        // (the zips that can hold it, by the set's name, then any other: a renamed zip is still found by its files)
        std::vector<std::string> tried;
        auto add = [&](const std::string& one) {
            if (one.empty()) {
                return;
            }
            const std::string key = Lower(one);
            if (listing.count(key) && std::find(tried.begin(), tried.end(), key) == tried.end()) {
                tried.push_back(key);
            }
        };
        add(game.set);
        add(game.parent);
        for (const Region& region : game.regions) {
            add(region.id);
        }
        for (const std::string& one : names) {
            add(Stem(one));
        }
        if (tried.empty()) {
            result.problems.push_back("no " + game.set + ".zip in this folder");
        }
        for (const std::string& key : tried) {
            Candidate candidate = CheckCandidate(listing[key], game, listing, cache);
            if (candidate.ok) {
                result.ok = true;
                result.zip = BaseName(listing[key]);
                result.path = listing[key];
                result.region = candidate.region;
                result.extras = candidate.extras;
                for (const std::string& one : candidate.used) {
                    used.insert(Lower(Stem(one)));
                }
                result.problems.clear();
                break;
            }
            if (result.problems.empty()) {
                result.problems = candidate.problems;
            }
        }
        if (result.ok) {
            report.ok++;
        } else {
            report.failed++;
        }
        report.games.push_back(std::move(result));
    }
    // (a zip that is named for no game's set, and that no game's set was read from: another game's, or something
    // else altogether. A zip named for a game's set is not one of these, even when it could not be read)
    std::set<std::string> known_names;
    for (const Game& game : games) {
        for (const std::string& one : {game.set, game.parent}) {
            if (!one.empty()) {
                known_names.insert(Lower(one));
            }
        }
        for (const Region& region : game.regions) {
            known_names.insert(Lower(region.id));
        }
    }
    for (const std::string& one : names) {
        const std::string key = Lower(Stem(one));
        if (!used.count(key) && !known_names.count(key)) {
            report.unused.push_back(one);
        }
    }
}

}  // namespace

Report scan(const std::filesystem::path& folder, const std::vector<Game>& games) {
    Report report;
    report.folder = folder;
    try {
        ScanInto(report, folder, games);
    } catch (...) {
        // (a zip whose list of files is not what a zip's is, a name the system will not take: said, not thrown out of
        // the launcher's window)
        report.unreadable = true;
    }
    return report;
}

}  // namespace romcheck
