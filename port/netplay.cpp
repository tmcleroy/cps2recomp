// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
//
// Online play: the socket, the handshake, and rollback over the network (netplay.h)
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include "netplay.h"

#include "game_info.h"
#include "../fight/battle.h"
#include "../recomp-common/launcher/src/online.h"
#include "../recomp-common/launcher/src/run_program.h"
#include "../runtime/game.h"
#include "../fight/tag.h"
#include "port.h"

#include <SDL3/SDL.h>

#include <zlib.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <thread>

using namespace cps2recomp;

namespace port {

namespace {
    // The pages of the modes that can be played online (fight/court.cpp's, fight/battle.cpp's)
    constexpr const char *kBallBattle = "Ball battle", *kTagTeamBattle = "Tag team battle";
    bool HasFeature(const char *name) {
        for (const Feature &feature : TheGame().features) {
            if (std::string(feature.name) == name) {
                return true;
            }
        }
        return false;
    }

    uint64_t NowMs() {
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
    }

    // The packets: "C2NP", what kind, then the kind's fields, little-endian
    constexpr uint8_t kMagic[4] = {'C', '2', 'N', 'P'};
    constexpr uint16_t kProtocol = Netplay::kProtocol;
    enum Kind : uint8_t {
        kHello = 1,   // joining -> host: the protocol, the game and its build, the people at the machine
        kWelcome = 2, // host -> joining: the delay, the board settings, the features that change play
        kRefuse = 3,  // either: why not (and the match is over)
        kControls = 4, // either: controls, and the rest of what each frame says (Netplay::SendInputs)
        kBye = 5,     // either: this machine has left
        kPunch = 6,   // host -> joining, before the hello: nothing, but it opens the host's router to them
        kPiece = 7,   // host -> joining: a piece of the game's state as the match begins (Netplay::state)
        kPieceAck = 8, // joining -> host: how many pieces have come, all before that one
        kChat = 9,    // either: a line of the chat, by its number, and the machine that said it
        kChatAck = 10, // either: how many lines of the chat have come, all before that one
        kWaiting = 11, // host -> joining, the state had: how many more players the match waits for
        kStart = 12,  // host -> joining: the match begins: the machines, each one's players and name, and which this is
        kSetDone = 14,
        kWatchHello = 15,
        kWatchControls = 16,
        kWatchAck = 17,
        kOver = 13,   // host -> joining: the match is over, another machine having gone (the host waits for all again)
    };

    // A name or a line of the chat as the screen's font has it (ASCII's letters, digits and marks), at most `most` of
    // them, spaces at its ends taken off
    std::string Printable(const std::string &text, size_t most) {
        std::string out;
        for (const char c : text) {
            if (out.size() < most && c >= 32 && c < 127) {
                out += c;
            }
        }
        const size_t start = out.find_first_not_of(' ');
        return start == std::string::npos ? std::string() : out.substr(start, out.find_last_not_of(' ') - start + 1);
    }

    struct Writer {
        std::vector<uint8_t> out;
        explicit Writer(Kind kind) {
            out.assign(kMagic, kMagic + 4);
            out.push_back(kind);
        }
        void U8(uint8_t v) { out.push_back(v); }
        void U16(uint16_t v) { U8(static_cast<uint8_t>(v)); U8(static_cast<uint8_t>(v >> 8)); }
        void U32(uint32_t v) { U16(static_cast<uint16_t>(v)); U16(static_cast<uint16_t>(v >> 16)); }
        void Text(const std::string &s) {
            U16(static_cast<uint16_t>(s.size()));
            out.insert(out.end(), s.begin(), s.end());
        }
        void Bytes(const void *p, size_t n) {
            U16(static_cast<uint16_t>(n));
            out.insert(out.end(), static_cast<const uint8_t *>(p), static_cast<const uint8_t *>(p) + n);
        }
    };
    struct Reader {
        const uint8_t *at, *end;
        bool ok = true;
        Reader(const uint8_t *data, size_t size, size_t from = 5) : at(data + from), end(data + size) {} // (after the magic and kind)
        bool Has(size_t n) {
            ok = ok && static_cast<size_t>(end - at) >= n;
            return ok;
        }
        uint8_t U8() { return Has(1) ? *at++ : 0; }
        uint16_t U16() { const uint16_t lo = U8(); return static_cast<uint16_t>(lo | U8() << 8); }
        uint32_t U32() { const uint32_t lo = U16(); return lo | static_cast<uint32_t>(U16()) << 16; }
        std::string Text() {
            const size_t n = U16();
            if (!Has(n)) {
                return "";
            }
            std::string s(reinterpret_cast<const char *>(at), n);
            at += n;
            return s;
        }
    };

    // Which features the joining side takes from the host: those that change how the game plays, and the stage (the
    // menu's Stage page). Each player's
    // own: how the picture is shown and the sound. Off for both: the cheats (a fair match), and the rest (the
    // alternative modes, training, the viewers: kept apart from the save state, or not worked out for two machines
    // yet), and run-ahead (rollback does what it does). A mode marked for matches (kFeatureMatch: tag team) is the host's.
    enum class Share { Host, Own, Off, Keep };
    Share Shared(const Feature &feature) {
        const std::string category = feature.category, name = feature.name;
        if (name == "run_ahead" || name == "custom_colours") {
            return Share::Off; // (custom colours: each player's own library, which the other side has not)
        }
        if (feature.flags & kFeatureMatch) {
            return Share::Host;
        }
        if (name == "online_set" || name.rfind("widescreen", 0) == 0 || category == "Game" || category == "Board") {
            return Share::Host;
        }
        if (category == "Stage") {
            // (the stage is the game's own choice made for it, the same on both machines or they part: the host's.
            // Another game's stage is drawn by each side that has its ROM set; the fight is the same without)
            return Share::Host;
        }
        // (the modes played online, Port::OnlineEvent: their pages as the host has them, each player's own fighters
        // then sent by them; how the court is looked at, each player's own)
        if (category == std::string(kBallBattle) || category == std::string(kTagTeamBattle)) {
            return name == "court_camera" ? Share::Own : Share::Host;
        }
        if (name.size() > 9 && name.compare(name.size() - 9, 9, "_hitboxes") == 0) {
            return Share::Own; // (a shooting game's hit boxes, on its Cheats page: drawn over the picture only)
        }
        if (category == "Cheats") {
            return Share::Off; // (a fair match: no damage for the host's player, say, is not)
        }
        if (category == "Display" || category == "Sound" || category == "Online") {
            return Share::Own; // (how each player sees and hears it, and what their screen says of the match)
        }
        if (feature.flags & kFeatureOwn) {
            return Share::Keep; // (a record or a choice kept for a mode: left as it is, and the mode is off)
        }
        return Share::Off;
    }

    // What both sides must have the same of: the game as this build is made (the set its code was translated from)
    // and the sound program. The program itself may be another region's set's (game.cfg's other_set), which differs
    // only in its online_words: the host's are sent in the welcome, so the match is played as the host's region
    uint32_t BuildHash(const Port &port) {
        uLong h = crc32(0, nullptr, 0);
        h = crc32(h, reinterpret_cast<const Bytef *>(GAME_PROGRAM), static_cast<uInt>(strlen(GAME_PROGRAM)));
        h = crc32(h, port.roms.soundProgram.data(), static_cast<uInt>(port.roms.soundProgram.size()));
        return static_cast<uint32_t>(h);
    }
#ifndef GAME_ONLINE_WORDS
#define GAME_ONLINE_WORDS ""
#endif
    // The program's words two sets of the game have differently (game.cfg's online_words: the region, the checksum
    // and the like), in both views (the rest of a set's program differs only where the game never reads it: the
    // cipher's other view)
    std::vector<uint32_t> OnlineWords() {
        std::vector<uint32_t> words;
        for (const char *at = GAME_ONLINE_WORDS; *at;) {
            char *end;
            const unsigned long word = strtoul(at, &end, 16);
            if (end == at) {
                at++; // (" || " between the lines of them)
                continue;
            }
            words.push_back(static_cast<uint32_t>(word));
            at = end;
        }
        return words;
    }
    std::string SetName(const Port &port) {
        return port.roms.name.empty() ? port.roms.set : port.roms.set + " (" + port.roms.name + ")";
    }
    // A machine's players as said: "player 1", or "players 1 and 3"
    std::string MachinePlayers(const Netplay &net, int machine) {
        return net.People(machine) > 1 ? "players " + std::to_string(net.Player(machine, 0) + 1) + " and " + std::to_string(net.Player(machine, 1) + 1)
                                       : "player " + std::to_string(net.Player(machine, 0) + 1);
    }
    // A match's code as the service keeps it ("K7QM2X"), from what was typed ("k7q-m2x"), or "" if it is not one
    // (an address has a dot or a colon)
    std::string CodeOf(const std::string &text) {
        static const std::string kAlphabet = "23456789ABCDEFGHJKMNPQRSTUVWXYZ"; // (online/src/index.js)
        if (text.find_first_of(".:") != std::string::npos) {
            return "";
        }
        std::string code;
        for (const char c : text) {
            if (c == '-' || c == ' ') {
                continue;
            }
            const char upper = static_cast<char>(toupper(static_cast<unsigned char>(c)));
            if (kAlphabet.find(upper) == std::string::npos) {
                return "";
            }
            code += upper;
        }
        return code.size() == 6 ? code : "";
    }
    // ... as said to the player: "K7Q-M2X"
    std::string Spaced(const std::string &code) {
        return code.size() == 6 ? code.substr(0, 3) + "-" + code.substr(3) : code;
    }

    // The service is asked with the system's curl (as the launcher's updates are fetched: macOS, Windows 10 and
    // later, and Linux have it; Android, through the game's activity), on another thread; the answer's last line is
    // its HTTP status
    std::string CurlProgram() {
#ifdef _WIN32
        char system[MAX_PATH];
        const UINT n = GetSystemDirectoryA(system, MAX_PATH);
        return n > 0 && n < MAX_PATH ? std::string(system) + "\\curl.exe" : "curl.exe";
#else
        std::error_code error;
        return std::filesystem::exists("/usr/bin/curl", error) ? "/usr/bin/curl" : "curl";
#endif
    }

    // A text field of a JSON answer ("" if none), a list of texts, and whether a field is true (the service's
    // answers are small and of known shapes: online/src/index.js)
    std::string JsonText(const std::string &json, const std::string &key) {
        const size_t at = json.find("\"" + key + "\":\"");
        if (at == std::string::npos) {
            return "";
        }
        std::string out;
        for (size_t i = at + key.size() + 4; i < json.size() && json[i] != '"'; i++) {
            if (json[i] == '\\' && i + 1 < json.size()) {
                i++;
            }
            out += json[i];
        }
        return out;
    }
    std::vector<std::string> JsonList(const std::string &json, const std::string &key) {
        std::vector<std::string> out;
        const size_t at = json.find("\"" + key + "\":[");
        if (at == std::string::npos) {
            return out;
        }
        // (each text to its closing quote, so that the brackets of an IPv6 address in one don't end the list)
        for (size_t i = at + key.size() + 4; i < json.size() && json[i] != ']'; i++) {
            if (json[i] != '"') {
                continue;
            }
            std::string text;
            for (i++; i < json.size() && json[i] != '"'; i++) {
                if (json[i] == '\\' && i + 1 < json.size()) {
                    i++;
                }
                text += json[i];
            }
            out.push_back(text);
        }
        return out;
    }
    bool JsonTrue(const std::string &json, const std::string &key) {
        return json.find("\"" + key + "\":true") != std::string::npos;
    }
    uint32_t JsonNumber(const std::string &json, const std::string &key) {
        const size_t at = json.find("\"" + key + "\":");
        return at == std::string::npos ? 0 : static_cast<uint32_t>(strtoul(json.c_str() + at + key.size() + 3, nullptr, 10));
    }

    // The STUN binding request (RFC 5389): what address and port a server sees this socket's packets come from
    constexpr uint32_t kStunCookie = 0x2112A442;
} // namespace

// The socket

#ifdef _WIN32
using SocketHandle = SOCKET;
#else
using SocketHandle = int;
#define closesocket close
#endif

UdpSocket::~UdpSocket() {
    if (handle != -1) {
        closesocket(static_cast<SocketHandle>(handle));
    }
}

bool UdpSocket::Open(int port, std::string &error) {
#ifdef _WIN32
    static bool started = false;
    if (!started) {
        WSADATA data;
        if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
            error = "cannot start Windows sockets";
            return false;
        }
        started = true;
    }
#endif
    // (IPv6 with IPv4 too, where the system has it; or IPv4)
    SocketHandle s = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
    six = true;
    if (s != static_cast<SocketHandle>(-1)) {
        int off = 0;
        setsockopt(s, IPPROTO_IPV6, IPV6_V6ONLY, reinterpret_cast<const char *>(&off), sizeof(off));
        sockaddr_in6 any{};
        any.sin6_family = AF_INET6;
        any.sin6_port = htons(static_cast<uint16_t>(port));
        any.sin6_addr = in6addr_any;
        if (bind(s, reinterpret_cast<sockaddr *>(&any), sizeof(any)) != 0) {
            closesocket(s);
            s = static_cast<SocketHandle>(-1);
        }
    }
    if (s == static_cast<SocketHandle>(-1)) {
        six = false;
        s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        sockaddr_in any{};
        any.sin_family = AF_INET;
        any.sin_port = htons(static_cast<uint16_t>(port));
        any.sin_addr.s_addr = htonl(INADDR_ANY);
        if (s == static_cast<SocketHandle>(-1) || bind(s, reinterpret_cast<sockaddr *>(&any), sizeof(any)) != 0) {
            error = "cannot use UDP port " + std::to_string(port) + " (is another copy of the game using it?)";
            if (s != static_cast<SocketHandle>(-1)) {
                closesocket(s);
            }
            return false;
        }
    }
#ifdef _WIN32
    u_long nonBlocking = 1;
    ioctlsocket(s, FIONBIO, &nonBlocking);
#else
    fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0) | O_NONBLOCK);
#endif
    handle = static_cast<intptr_t>(s);
    return true;
}

bool UdpSocket::Resolve(const std::string &host, int port, NetAddress &address, std::string &error, bool fourOnly) {
    addrinfo hints{}, *found = nullptr;
    hints.ai_family = fourOnly ? AF_INET : AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    if (getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &found) != 0 || !found) {
        error = "cannot find " + host;
        return false;
    }
    // (an IPv4 address as IPv6 sees it, for the socket that is both)
    const addrinfo *use = found;
    if (use->ai_family == AF_INET) {
        sockaddr_in6 mapped{};
        mapped.sin6_family = AF_INET6;
#ifdef SIN6_LEN
        mapped.sin6_len = sizeof(mapped); // (as received addresses have it, so the two compare equal)
#endif
        mapped.sin6_port = reinterpret_cast<const sockaddr_in *>(use->ai_addr)->sin_port;
        mapped.sin6_addr.s6_addr[10] = 0xFF;
        mapped.sin6_addr.s6_addr[11] = 0xFF;
        memcpy(&mapped.sin6_addr.s6_addr[12], &reinterpret_cast<const sockaddr_in *>(use->ai_addr)->sin_addr, 4);
        memcpy(address.storage.data(), &mapped, sizeof(mapped));
        address.length = sizeof(mapped);
        memcpy(address.four.data(), use->ai_addr, sizeof(sockaddr_in));
        address.hasFour = true;
    } else {
        memcpy(address.storage.data(), use->ai_addr, use->ai_addrlen);
        address.length = static_cast<int>(use->ai_addrlen);
    }
    freeaddrinfo(found);
    return true;
}

void UdpSocket::Split(const std::string &text, std::string &host, int &port) {
    host = text;
    const size_t colon = host.rfind(':');
    if (colon != std::string::npos && host.find(':') == colon) { // (ADDRESS:PORT; an IPv6 address alone has several)
        port = atoi(host.c_str() + colon + 1);
        host = host.substr(0, colon);
    } else if (host.size() > 2 && host[0] == '[') { // ([IPv6]:PORT)
        const size_t close = host.find(']');
        if (close != std::string::npos && close + 1 < host.size() && host[close + 1] == ':') {
            port = atoi(host.c_str() + close + 2);
        }
        host = host.substr(1, close == std::string::npos ? std::string::npos : close - 1);
    }
}

int UdpSocket::LocalPort() const {
    sockaddr_storage address{};
    socklen_t length = sizeof(address);
    if (getsockname(static_cast<SocketHandle>(handle), reinterpret_cast<sockaddr *>(&address), &length) != 0) {
        return 0;
    }
    return ntohs(address.ss_family == AF_INET6 ? reinterpret_cast<sockaddr_in6 *>(&address)->sin6_port
                                               : reinterpret_cast<sockaddr_in *>(&address)->sin_port);
}

void UdpSocket::Send(const NetAddress &to, const uint8_t *data, size_t size) {
    const bool asFour = !six && to.hasFour;
    sendto(static_cast<SocketHandle>(handle), reinterpret_cast<const char *>(data), static_cast<int>(size), 0,
           reinterpret_cast<const sockaddr *>(asFour ? to.four.data() : to.storage.data()),
           static_cast<socklen_t>(asFour ? sizeof(sockaddr_in) : to.length));
}

int UdpSocket::Receive(uint8_t *data, size_t size, NetAddress &from) {
    sockaddr_storage address{};
    socklen_t length = sizeof(address);
    const int got = static_cast<int>(recvfrom(static_cast<SocketHandle>(handle), reinterpret_cast<char *>(data), static_cast<int>(size), 0,
                                              reinterpret_cast<sockaddr *>(&address), &length));
    if (got < 0) {
        return -1;
    }
    from = NetAddress{};
    memcpy(from.storage.data(), &address, std::min<size_t>(length, sizeof(from.storage)));
    from.length = static_cast<int>(length);
    return got;
}

// The session

void Netplay::SendTo(const NetAddress &to, const std::vector<uint8_t> &packet) {
    if (settings.fakeLoss && static_cast<int>((random = random * 6364136223846793005ull + 1442695040888963407ull) >> 33) % 100 < settings.fakeLoss) {
        return;
    }
    if (settings.fakeLag || settings.fakeJitter) {
        random = random * 6364136223846793005ull + 1442695040888963407ull;
        const int jitter = settings.fakeJitter ? static_cast<int>((random >> 33) % (2 * settings.fakeJitter + 1)) - settings.fakeJitter : 0;
        late.push_back({NowMs() + static_cast<uint64_t>(std::max(0, settings.fakeLag + jitter)), to, packet});
        return;
    }
    socket.Send(to, packet.data(), packet.size());
}

void Netplay::SendAll(const std::vector<uint8_t> &packet, const Link *except) {
    for (const Link &link : links) {
        if (&link != except) {
            SendTo(link.address, packet);
        }
    }
}

Netplay::Link *Netplay::LinkFrom(const NetAddress &from) {
    for (Link &link : links) {
        if (link.address == from) {
            return &link;
        }
    }
    return nullptr;
}

namespace {
    // The features as the match has them: the joining side takes the host's (Share::Host); both turn off the rest
    void Settle(bool hosting, const std::map<std::string, int> &host) {
        for (const Feature &feature : TheGame().features) {
            switch (Shared(feature)) {
            case Share::Host:
                if (!hosting) {
                    const auto it = host.find(feature.name);
                    SetFeatureForNow(feature.name, it == host.end() ? 0 : it->second);
                }
                break;
            case Share::Off: SetFeatureForNow(feature.name, 0); break;
            default: break;
            }
        }
    }
} // namespace

bool Netplay::MatchDecides(const Feature &feature) {
    const Share share = Shared(feature);
    return share == Share::Host || share == Share::Off;
}

void Netplay::StartAt(int frames) {
    delay = std::clamp(frames, 0, kMostAhead); // (the first `delay` frames: nobody's controls)
    upTo.fill(static_cast<uint64_t>(delay));
    for (auto &stream : events) stream.clear();
    eventFrom = UINT64_MAX;
}

// The players the match is for, as the host's features have it: four in tag team's 2 vs 2, two otherwise (the
// alternative modes that would keep tag team from routing the controls are off in a match: Settle)
int Netplay::MatchPlayers() const {
    return tag::Chosen() == tag::kVersus ? 4 : 2;
}

// The people at this machine in a match of four: --net-people, or the Online page's People at this machine
int Netplay::PeopleHere() const {
    return std::clamp(settings.people ? settings.people : FeatureValue("online_people") + 1, 1, kMostSeats);
}

// (hosting) how many more players the match waits for
int Netplay::Wanted() const {
    int wanted = needed - people;
    for (const Link &link : links) {
        wanted -= link.people;
    }
    return std::max(0, wanted);
}

// (hosting) the wait as said: for the other player, or for how many more of a match of four
void Netplay::SayWaiting() {
    const int wanted = Wanted();
    waiting = needed <= 2 ? "Waiting for the other player"
              : wanted == 1 ? "Waiting for 1 more player"
                            : "Waiting for " + std::to_string(wanted) + " more players";
    if (code.empty()) {
        waiting += " (UDP port " + std::to_string(settings.port) + ")";
    }
}

int Netplay::MachineOf(int player) const {
    for (int machine = 0; machine < machines; machine++) {
        for (int person = 0; person < roster[machine].people; person++) {
            if (roster[machine].players[person] == player) {
                return machine;
            }
        }
    }
    return -1;
}

// The players handed out as the match begins: the host's first, then the joining machines' in the order they came;
// a machine of two takes a whole team where one is free (players 1 and 3, or 2 and 4), or else the two left first
void Netplay::Assign() {
    machines = 1 + static_cast<int>(links.size());
    bool taken[Machine::kMaxPlayers] = {};
    const auto give = [&](Seats &seats, int count, const std::string &name) {
        seats.people = count;
        seats.name = name;
        int n = 0;
        for (int team = 0; count == 2 && n == 0 && team < 2; team++) {
            if (!taken[team] && !taken[team + 2]) {
                seats.players = {team, team + 2};
                taken[team] = taken[team + 2] = true;
                n = 2;
            }
        }
        for (int player = 0; player < needed && n < count; player++) {
            if (!taken[player]) {
                seats.players[n++] = player;
                taken[player] = true;
            }
        }
    };
    give(roster[0], people, settings.name);
    for (size_t i = 0; i < links.size(); i++) {
        links[i].machine = static_cast<int>(i) + 1;
        give(roster[i + 1], links[i].people, links[i].name);
    }
}

// (hosting) the start for a joining machine: the machines, each one's players and name, and which it is
std::vector<uint8_t> Netplay::Start(int machine) const {
    Writer start(kStart);
    start.U8(static_cast<uint8_t>(machines));
    start.U8(static_cast<uint8_t>(machine));
    for (int m = 0; m < machines; m++) {
        start.U8(static_cast<uint8_t>(roster[m].people));
        for (int person = 0; person < roster[m].people; person++) {
            start.U8(static_cast<uint8_t>(roster[m].players[person]));
        }
        start.Text(roster[m].name);
    }
    return start.out;
}

// The welcome for those who come: the delay, the board settings, the features that change play, and the game as it
// is now (the board's reset, or, with --net-arcade, wherever the host's play has got to); the same for each until the
// match begins
void Netplay::Welcome(Port &port, const std::vector<uint8_t> *snapshot, uint32_t start) {
    if (!snapshot) {
        needed = MatchPlayers();
        people = needed > 2 ? PeopleHere() : 1;
    }
    Writer welcome(kWelcome);
    welcome.U16(kProtocol);
    welcome.U8(static_cast<uint8_t>(delay));
    welcome.Bytes(port.machine->eeprom.words, sizeof(port.machine->eeprom.words));
    std::string features;
    for (const Feature &feature : TheGame().features) {
        if (Shared(feature) == Share::Host && FeatureSetting(feature.name)) {
            features += std::string(feature.name) + "=" + std::to_string(FeatureSetting(feature.name)) + ",";
        }
    }
    welcome.Text(features);
    // (the game as it is now: the state the match begins from, sent in pieces, after this side's program's words
    // where another set's may differ, which the other side takes: their count, then each one's address and both views)
    std::vector<uint8_t> raw;
    if (snapshot) raw = *snapshot;
    else port.KeepState(raw);
    const std::vector<uint32_t> words = OnlineWords();
    Writer program(kWelcome);
    program.U16(static_cast<uint16_t>(words.size()));
    for (const uint32_t word : words) {
        const size_t at = std::min<size_t>(word, port.roms.program.size() - 2);
        program.U32(word);
        for (const uint8_t b : {port.roms.program[at], port.roms.program[at + 1], port.roms.opcodes[at], port.roms.opcodes[at + 1]}) {
            program.U8(b);
        }
    }
    raw.insert(raw.begin(), program.out.begin() + 5, program.out.end());
    uLongf packed = compressBound(static_cast<uLong>(raw.size()));
    state.resize(packed);
    compress2(state.data(), &packed, raw.data(), static_cast<uLong>(raw.size()), 6);
    state.resize(packed);
    stateSize = raw.size();
    chunks = static_cast<uint32_t>((state.size() + kChunk - 1) / kChunk);
    welcome.U32(static_cast<uint32_t>(stateSize));
    welcome.U32(static_cast<uint32_t>(state.size()));
    // (flags: 1, this side's player is already in the game, which was played while it waited)
    welcome.U8(port.cameIn ? 1 : 0);
    // (this side's set: the other side says so when it has another region's)
    welcome.Text(SetName(port));
    welcome.Text(settings.name);
    welcome.U8(static_cast<uint8_t>(needed)); // (the players the match is for)
    welcome.U32(start);
    // Viewers receive the final roster with their snapshot; players get it later in kStart.
    welcome.U8(snapshot ? static_cast<uint8_t>(machines) : 0);
    if (snapshot) {
        for (int m = 0; m < machines; ++m) {
            welcome.U8(static_cast<uint8_t>(roster[m].people));
            for (int person = 0; person < roster[m].people; ++person)
                welcome.U8(static_cast<uint8_t>(roster[m].players[person]));
            welcome.Text(roster[m].name);
        }
    }
    for (uint32_t value : set.Saved()) welcome.U32(value);
    greeting = welcome.out;
}

void Netplay::Hello() {
    Writer hello(settings.watching ? kWatchHello : kHello);
    hello.U16(kProtocol);
    hello.Text(GAME_ID);
    hello.U32(build);
    hello.Text(RECOMP_VERSION);
    hello.Text(settings.name);
    if (settings.watching) {
        if (watchToken.empty()) watchToken = std::to_string(NowMs()) + "-" + std::to_string(random);
        hello.Text(watchToken);
    } else {
        hello.U8(static_cast<uint8_t>(PeopleHere()));
        hello.U32(ticket);
    }
    greeting = hello.out;
}

bool Netplay::Begin(Port &port, std::string &error) {
    if (!socket.Open(settings.hosting ? settings.port : 0, error)) {
        return false;
    }
    local = settings.hosting ? 0 : 1; // (joining: which, the host says as the match begins)
    build = BuildHash(port);
    began = NowMs();
    if (settings.hosting) {
        StartAt(settings.delay);
        if (!settings.solo) {
            Settle(true, {}); // (solo: when someone comes)
        }
        needed = MatchPlayers();
        people = needed > 2 ? PeopleHere() : 1;
        SayWaiting();
    } else {
        Hello();
        // A match's code (K7Q-M2X: six letters and digits), or the host's address
        joinCode = CodeOf(settings.address);
        if (!joinCode.empty()) {
            if (settings.service == "off" || settings.service.empty()) {
                error = "Joining by a code needs the online service, which --net-service off turned off.";
                return false;
            }
            intro = Intro::Stun;
            waiting = "Joining match " + Spaced(joinCode);
        } else {
            std::string host;
            int hostPort = settings.port;
            UdpSocket::Split(settings.address, host, hostPort);
            NetAddress address;
            if (!UdpSocket::Resolve(host, hostPort, address, error)) {
                return false;
            }
            candidates = {address};
            joinPort = hostPort;
            waiting = "Joining " + host + " (UDP port " + std::to_string(hostPort) + ")";
        }
    }
    if (settings.hosting && settings.service != "off" && !settings.service.empty()) {
        intro = Intro::Stun;
    }
    if (intro == Intro::Stun) {
        // (the STUN servers' addresses looked up on another thread: a name can take a while, or fail, offline)
        stunLookup = std::async(std::launch::async, [] {
            std::vector<NetAddress> found;
            for (const auto &[host, port] : {std::pair<const char *, int>{"stun.cloudflare.com", 3478}, {"stun.l.google.com", 19302}}) {
                NetAddress address;
                std::string ignored;
                // (IPv4: the address a router changes, which the other side must be told)
                if (UdpSocket::Resolve(host, port, address, ignored, true)) {
                    found.push_back(address);
                }
            }
            return found;
        });
        stunBegan = NowMs();
    }
    fprintf(stderr, "online: %s\n", waiting.c_str());
    return true;
}

namespace {
    // The service asked (method, its path after the service's address, a JSON body or none): the status (0: no
    // answer) and the body, with curl, which is started with no window of its own
    std::pair<int, std::string> Http(const std::string &method, const std::string &url, const std::string &body) {
        std::vector<std::string> args = {CurlProgram(), "-sS", "--max-time", "10", "-X", method, "-H", "content-type: application/json",
                                         "-w", "\n%{http_code}"};
        if (!body.empty()) {
            args.push_back("--data-binary");
            args.push_back(body);
        }
        args.push_back(url);
#if defined(__ANDROID__)
        // (Android has no curl to start: the game's activity makes the request, as the launcher's are made there:
        // recomp-common's run_program, GameActivity.curl)
        std::string text;
        const int exitCode = launcher::run_program(launcher::curl_program(), std::vector<std::string>(args.begin() + 1, args.end()), &text);
#elif defined(_WIN32)
        // (Windows: curl, a console program, started with no console window; its output through a pipe)
        std::string command;
        for (const std::string &arg : args) {
            std::string quoted = "\"";
            for (size_t i = 0; i < arg.size(); i++) {
                size_t slashes = 0;
                while (i < arg.size() && arg[i] == '\\') {
                    slashes++, i++;
                }
                if (i == arg.size()) {
                    quoted.append(slashes * 2, '\\');
                    break;
                }
                quoted.append(arg[i] == '"' ? slashes * 2 + 1 : slashes, '\\');
                quoted += arg[i];
            }
            command += (command.empty() ? "" : " ") + quoted + "\"";
        }
        SECURITY_ATTRIBUTES inherit{sizeof(inherit), nullptr, TRUE};
        HANDLE readEnd = nullptr, writeEnd = nullptr;
        if (!CreatePipe(&readEnd, &writeEnd, &inherit, 0)) {
            return {0, ""};
        }
        SetHandleInformation(readEnd, HANDLE_FLAG_INHERIT, 0);
        STARTUPINFOA startup{};
        startup.cb = sizeof(startup);
        startup.dwFlags = STARTF_USESTDHANDLES;
        startup.hStdOutput = writeEnd;
        startup.hStdError = nullptr;
        startup.hStdInput = nullptr;
        PROCESS_INFORMATION info{};
        std::vector<char> commandLine(command.begin(), command.end());
        commandLine.push_back('\0');
        const bool started = CreateProcessA(nullptr, commandLine.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &info);
        CloseHandle(writeEnd);
        std::string text;
        int exitCode = -1;
        if (started) {
            char buffer[4096];
            DWORD got = 0;
            while (ReadFile(readEnd, buffer, sizeof(buffer), &got, nullptr) && got > 0) {
                text.append(buffer, got);
            }
            WaitForSingleObject(info.hProcess, INFINITE);
            DWORD code = 0;
            GetExitCodeProcess(info.hProcess, &code);
            exitCode = static_cast<int>(code);
            CloseHandle(info.hProcess);
            CloseHandle(info.hThread);
        }
        CloseHandle(readEnd);
#else
        std::vector<const char *> argv;
        for (const std::string &arg : args) {
            argv.push_back(arg.c_str());
        }
        argv.push_back(nullptr);
        const SDL_PropertiesID properties = SDL_CreateProperties();
        SDL_SetPointerProperty(properties, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, argv.data());
        SDL_SetNumberProperty(properties, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, SDL_PROCESS_STDIO_APP);
        SDL_SetNumberProperty(properties, SDL_PROP_PROCESS_CREATE_STDERR_NUMBER, SDL_PROCESS_STDIO_NULL);
        SDL_Process *process = SDL_CreateProcessWithProperties(properties);
        SDL_DestroyProperties(properties);
        if (!process) {
            return {0, ""};
        }
        size_t size = 0;
        int exitCode = 0;
        char *out = static_cast<char *>(SDL_ReadProcess(process, &size, &exitCode));
        SDL_DestroyProcess(process);
        std::string text = out ? std::string(out, size) : "";
        SDL_free(out);
#endif
        if (getenv("CPS2RECOMP_NET_DEBUG")) {
            fprintf(stderr, "online: %s %s: curl %d: %s\n", method.c_str(), url.c_str(), exitCode, text.c_str());
        }
        const size_t line = text.rfind('\n');
        if (exitCode != 0 || line == std::string::npos) {
            return {0, ""};
        }
        return {atoi(text.c_str() + line + 1), text.substr(0, line)};
    }
} // namespace

std::string Netplay::Addresses() const {
    // (where the internet saw this socket, and its address on each of this machine's networks)
    std::vector<std::string> list;
    if (!seenAs.empty()) {
        list.push_back(seenAs);
    }
    for (const launcher::LocalAddress &here : launcher::local_addresses()) {
        list.push_back(here.address + ":" + std::to_string(socket.LocalPort()));
    }
    std::string json = "[";
    for (size_t i = 0; i < list.size() && i < 8; i++) {
        json += (i ? ",\"" : "\"") + list[i] + "\"";
    }
    return json + "]";
}

bool Netplay::TakeStun(const uint8_t *data, int size) {
    // A binding success (0x0101) for our request: its XOR-MAPPED-ADDRESS (0x0020), IPv4
    if (size < 20 || data[0] != 0x01 || data[1] != 0x01 || memcmp(data + 8, stunId.data(), 12) != 0) {
        return false;
    }
    for (int at = 20; at + 4 <= size;) {
        const int type = data[at] << 8 | data[at + 1], length = data[at + 2] << 8 | data[at + 3];
        if (type == 0x0020 && length >= 8 && at + 12 <= size && data[at + 5] == 0x01) {
            const int port = (data[at + 6] << 8 | data[at + 7]) ^ (kStunCookie >> 16);
            const uint32_t ip = (static_cast<uint32_t>(data[at + 8]) << 24 | data[at + 9] << 16 | data[at + 10] << 8 | data[at + 11]) ^ kStunCookie;
            seenAs = std::to_string(ip >> 24) + "." + std::to_string((ip >> 16) & 0xFF) + "." + std::to_string((ip >> 8) & 0xFF) + "." +
                     std::to_string(ip & 0xFF) + ":" + std::to_string(port);
            return true;
        }
        at += 4 + ((length + 3) & ~3);
    }
    return true;
}

// A step of the introduction (see Intro); false and `error` when joining by a code cannot go on
bool Netplay::Introduce(uint64_t now, std::string &error) {
    const std::string service = settings.service;
    const auto ask = [&](const std::string &method, const std::string &path, const std::string &body) {
        askedAt = now;
        asking = std::async(std::launch::async, [method, url = service + path, body] {
            const auto [status, text] = Http(method, url, body);
            return HttpAnswer{status, text};
        });
    };
    const auto answered = [&] { return asking.valid() && asking.wait_for(std::chrono::seconds(0)) == std::future_status::ready; };
    const std::string about = "\"game\":\"" GAME_ID "\",\"title\":\"" GAME_NAME "\",\"version\":\"" RECOMP_VERSION "\",\"protocol\":" +
                              std::to_string(kProtocol) +
                              ",\"build\":\"" + std::to_string(build) + "\"";
    // (an address the service said, as one this socket can send to)
    const auto address = [](const std::string &text, NetAddress &to) {
        std::string host, ignored;
        int port = 0;
        UdpSocket::Split(text, host, port);
        return port && UdpSocket::Resolve(host, port, to, ignored);
    };
    if (getenv("CPS2RECOMP_NET_DEBUG")) {
        static int last = -1;
        if (last != static_cast<int>(intro)) {
            fprintf(stderr, "online: intro %d, stun servers %zu, seen as '%s'\n", static_cast<int>(intro), stunServers.size(), seenAs.c_str());
            last = static_cast<int>(intro);
        }
    }
    switch (intro) {
    case Intro::Off:
    case Intro::Done: return true;
    case Intro::Stun:
        if (stunServers.empty() && stunLookup.valid() && stunLookup.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            stunServers = stunLookup.get();
        }
        if (seenAs.empty() && now - stunBegan < 3000) {
            if (!stunServers.empty() && now - stunSent > 400) {
                // (a binding request: a type, no length, the cookie and a transaction id of our own)
                for (uint8_t &b : stunId) {
                    b = static_cast<uint8_t>((random = random * 6364136223846793005ull + 1442695040888963407ull) >> 56);
                }
                std::vector<uint8_t> request = {0x00, 0x01, 0x00, 0x00, 0x21, 0x12, 0xA4, 0x42};
                request.insert(request.end(), stunId.begin(), stunId.end());
                for (const NetAddress &server : stunServers) {
                    socket.Send(server, request.data(), request.size());
                }
                stunSent = now;
            }
            return true;
        }
        // (with where the internet sees us, or without: then only the addresses of this machine's networks)
        if (settings.hosting) {
            const std::string again = settings.renewCode.empty() ? ""
                                      : "\"code\":\"" + settings.renewCode + "\",\"secret\":\"" + settings.renewSecret + "\",";
            std::string listed;
            if (settings.listed || !settings.invite.empty()) {
                // (the name, as JSON text: quotes and backslashes escaped, control characters left out)
                std::string name;
                for (const char c : settings.name) {
                    if (c == '"' || c == '\\') {
                        name += '\\';
                    }
                    if (static_cast<unsigned char>(c) >= 0x20) {
                        name += c;
                    }
                }
                listed = std::string(settings.listed ? ",\"listed\":true" : "") + ",\"name\":\"" + name + "\"";
            }
            // (a player invited from the waiting room: ids, hex digits)
            if (!settings.invite.empty()) {
                listed += ",\"invite\":\"" + settings.invite + "\",\"from\":\"" + settings.from + "\"";
            }
            // (the players the match is for, and the host's: the service lets in as many more as there is room for)
            toldPlayers = needed;
            toldHere = people;
            const std::string players = ",\"players\":" + std::to_string(needed) + ",\"here\":" + std::to_string(people);
            ask("POST", "/v1/host", "{" + again + about + listed + players + (settings.spectators ? ",\"spectators\":true" : "") + ",\"addresses\":" + Addresses() + "}");
        } else {
            ask("POST", std::string(settings.watching ? "/v1/watch/" : "/v1/join/") + joinCode, "{" + about + ",\"people\":" + std::to_string(PeopleHere()) + ",\"addresses\":" + Addresses() + "}");
        }
        intro = Intro::Asking;
        return true;
    case Intro::Asking: {
        if (!answered()) {
            return true;
        }
        const HttpAnswer answer = asking.get();
        if (settings.hosting) {
            if (answer.status == 200 && !JsonText(answer.body, "code").empty()) {
                code = Spaced(JsonText(answer.body, "code"));
                secret = JsonText(answer.body, "secret");
                SayWaiting(); // (the code said under it, and the lobby: Port::Tick)
                fprintf(stderr, "online: the match's code is %s%s\n", code.c_str(), seenAs.empty() ? "" : (" (seen at " + seenAs + ")").c_str());
                codeAt = now;
                intro = Intro::Polling;
            } else {
                fprintf(stderr, "online: no code: the online service did not answer (%d)\n", answer.status);
                intro = Intro::Off; // (the host's address still works)
            }
            return true;
        }
        if (answer.status == 200) {
            if (settings.watching) {
                watchToken = JsonText(answer.body, "token");
                Writer hello(kWatchHello);
                hello.U16(kProtocol); hello.Text(GAME_ID); hello.U32(build);
                hello.Text(RECOMP_VERSION); hello.Text(settings.name); hello.Text(watchToken);
                greeting = hello.out;
            }
            for (const std::string &text : JsonList(answer.body, "addresses")) {
                NetAddress to;
                if (address(text, to)) {
                    candidates.push_back(to);
                }
            }
            if (candidates.empty()) {
                error = "The host of that match can't be reached.";
                return false;
            }
            // (the service's number for this machine, which the host says back when it goes before the match begins)
            ticket = JsonNumber(answer.body, "ticket");
            Hello();
            fprintf(stderr, "online: match %s: trying %zu of the host's addresses\n", Spaced(joinCode).c_str(), candidates.size());
            began = now; // (30 seconds from here to hear from the host)
            intro = Intro::Done;
            return true;
        }
        error = answer.status == 404   ? "There's no match with the code " + Spaced(joinCode) + " (codes last 30 minutes)."
                : answer.status == 409 ? JsonText(answer.body, "error")
                                       : "Couldn't reach the online service (is this machine on the internet?).";
        return false;
    }
    case Intro::Polling: {
        // (hosting: asked until everyone has joined, every 2 seconds, then 5 after two minutes and 10 after ten, the
        // most a joining player's 30 seconds of waiting allow; their addresses are then sent to. Each question says
        // who joined by the code and went before the match began, or never came: the service has room for others)
        if (asking.valid()) {
            if (!answered()) {
                return true;
            }
            const HttpAnswer answer = asking.get();
            if (answer.status == 200) {
                // A failed request (or a lost answer) must not strand a reservation at the service. Retire only
                // the departures in this poll: others may have arrived while it was in flight. Repeating them is safe.
                std::erase_if(gone, [&](uint32_t number) {
                    return std::find(polledGone.begin(), polledGone.end(), number) != polledGone.end();
                });
                // (each who joined: "TICKET ADDRESS ADDRESS..."; or, from a service before these, the one who joined)
                std::vector<std::string> joined = JsonList(answer.body, "joiners");
                if (joined.empty() && JsonTrue(answer.body, "joined")) {
                    std::string one = "0";
                    for (const std::string &text : JsonList(answer.body, "addresses")) {
                        one += " " + text;
                    }
                    joined.push_back(one);
                }
                for (const std::string &line : joined) {
                    size_t at = line.find(' ');
                    const uint32_t number = static_cast<uint32_t>(strtoul(line.c_str(), nullptr, 10));
                    if (number && !tickets.count(number)) {
                        tickets[number] = now; // (its hello waited for)
                    }
                    while (at != std::string::npos) {
                        const size_t next = line.find(' ', at + 1);
                        const std::string text = line.substr(at + 1, next == std::string::npos ? std::string::npos : next - at - 1);
                        NetAddress to;
                        if (!text.empty() && std::find(told.begin(), told.end(), text) == told.end() && address(text, to)) {
                            told.push_back(text);
                            candidates.push_back(to);
                        }
                        at = next;
                    }
                }
                if (invitation.empty()) {
                    invitation = JsonText(answer.body, "invite");
                }
            }
            polledGone.clear();
            if (answer.status == 404) {
                // (the code has run out (30 minutes): the same one had again, for whoever was given it, and asked on)
                fprintf(stderr, "online: the code ran out: asking for it again\n");
                // The renewed match allocates tickets anew; old removals must not remove its new arrivals.
                gone.clear();
                tickets.clear();
                for (Link &link : links) {
                    link.ticket = 0;
                }
                settings.renewCode = code.substr(0, 3) + code.substr(4);
                settings.renewSecret = secret;
                intro = Intro::Stun;
                return true;
            }
        }
        // (the match's players or the host's changed while nobody had come (hosting while playing): the service told
        // again, with the same code)
        if (links.empty() && (toldPlayers != needed || toldHere != people)) {
            settings.renewCode = code.substr(0, 3) + code.substr(4);
            settings.renewSecret = secret;
            tickets.clear();
            gone.clear();
            polledGone.clear();
            intro = Intro::Stun;
            return true;
        }
        const uint64_t waited = now - codeAt;
        if (now - askedAt > (waited < 120000 ? 2000u : waited < 600000 ? 5000u : 10000u)) {
            std::string left;
            polledGone = gone;
            for (const uint32_t number : polledGone) {
                left += (left.empty() ? "&gone=" : ",") + std::to_string(number);
            }
            ask("GET", "/v1/host/" + code.substr(0, 3) + code.substr(4) + "?secret=" + secret + left, "");
        }
        return true;
    }
    }
    return true;
}

int Netplay::Waiting(Port &port, std::string &error) {
    Pump();
    uint8_t packet[2048];
    NetAddress from;
    const uint64_t now = NowMs();
    if (settings.hosting && links.empty()) {
        // (nobody here: the match as the features have it now, which may change while the host plays meanwhile)
        const int players = MatchPlayers(), here = players > 2 ? PeopleHere() : 1;
        if (players != needed || here != people) {
            needed = players;
            people = here;
            SayWaiting();
        }
    }
    if (!Introduce(now, error)) {
        return -1;
    }
    if (settings.hosting) {
        int got;
        while ((got = socket.Receive(packet, sizeof(packet), from)) >= 0) {
            if (TakeStun(packet, got) || got < 5 || memcmp(packet, kMagic, 4)) {
                continue;
            }
            Reader in(packet, static_cast<size_t>(got));
            Link *link = LinkFrom(from);
            if (packet[4] == kHello) {
                const uint16_t protocol = in.U16();
                const std::string game = in.Text();
                const uint32_t theirs = in.U32();
                const std::string version = in.Text();
                const std::string name = in.Text();
                const int theirPeople = std::clamp<int>(in.U8(), 1, kMostSeats);
                const uint32_t theirTicket = in.U32();
                // (in a match of two, one at each machine, whatever the other machine's People at this machine says)
                const int bringing = needed > 2 ? theirPeople : 1;
                std::string why;
                if (protocol == kProtocol && version != RECOMP_VERSION) {
                    why = "The other player has version " + version + " and this one " RECOMP_VERSION ": both need the same version.";
                } else if (protocol != kProtocol) {
                    why = "The two copies of the game are different versions: update both.";
                } else if (game != GAME_ID) {
                    why = "The other player started " + game + ", this side " GAME_ID ".";
                } else if (theirs != build) {
                    why = "The two copies of the game are different builds: use the same release on both.";
                } else if (!link && Wanted() == 0) {
                    why = needed > 2 ? "That match is full." : "Someone has joined that match already.";
                } else if (!link && bringing > Wanted()) {
                    why = "That match has room for one more player, and this machine has two (People at this machine, on the Online page).";
                }
                if (!why.empty()) {
                    Writer refuse(kRefuse);
                    refuse.Text(why);
                    socket.Send(from, refuse.out.data(), refuse.out.size());
                    fprintf(stderr, "online: turned away a player: %s\n", why.c_str());
                    continue;
                }
                if (!link) {
                    if (links.empty()) {
                        if (settings.solo) {
                            // (the player's own game until now: the match from the board's reset, with the match's features)
                            port.MatchComing();
                            Settle(true, {});
                        }
                        Welcome(port);
                    }
                    links.emplace_back();
                    link = &links.back();
                    link->address = from;
                    link->name = name;
                    link->people = bringing;
                    link->ticket = theirTicket;
                    if (theirTicket) {
                        tickets[theirTicket] = kCame;
                    }
                    if (needed > 2) {
                        fprintf(stderr, "online: %s is here (%d of %d players)\n", name.empty() ? "a player" : Printable(name, kNameMost).c_str(),
                                needed - Wanted(), needed);
                    } else {
                        fprintf(stderr, "online: the other player is here\n");
                    }
                    SayWaiting();
                }
                link->lastHeard = now;
                if (link->chunksHad == 0) {
                    SendTo(link->address, greeting);
                    link->greeted = now;
                }
            } else if (link && packet[4] == kPieceAck) {
                link->chunksHad = std::max(link->chunksHad, in.U32());
                link->lastHeard = now;
            } else if (link && packet[4] == kBye) {
                // (they left before it began)
                fprintf(stderr, "online: %s left before the match began\n", link->name.empty() ? "a player" : Printable(link->name, kNameMost).c_str());
                if (link->ticket) {
                    gone.push_back(link->ticket);
                    tickets[link->ticket] = kWent;
                }
                links.erase(links.begin() + (link - links.data()));
                SayWaiting();
            }
        }
        // (those who went quiet before it began: waited for again, and the game, if it is played meanwhile, goes on
        // when nobody is left)
        for (auto it = links.begin(); it != links.end();) {
            if (now - it->lastHeard > 5000) {
                fprintf(stderr, "online: %s went quiet before the match began\n",
                        needed > 2 ? (it->name.empty() ? "a player" : Printable(it->name, kNameMost).c_str()) : "the other player");
                if (it->ticket) {
                    gone.push_back(it->ticket);
                    tickets[it->ticket] = kWent;
                }
                it = links.erase(it);
                SayWaiting();
            } else {
                ++it;
            }
        }
        // (and those who joined by the code but never came, in 30 seconds: the service has room for others again)
        for (auto &[number, seen] : tickets) {
            if (seen < kCame && now - seen > 30000) {
                gone.push_back(number);
                seen = kWent;
            }
        }
        // Everyone here, with the whole of the state: the match begins, each joining machine told which it is and
        // who plays where
        bool all = !links.empty() && Wanted() == 0;
        for (const Link &link : links) {
            all = all && link.chunksHad >= chunks;
        }
        if (all) {
            Assign();
            for (Link &link : links) {
                SendTo(link.address, Start(link.machine));
                link.greeted = now;
                link.lastHeard = now;
            }
            intro = intro == Intro::Off ? Intro::Off : Intro::Done; // (nobody more to ask about)
            fprintf(stderr, "online: the match is on: you are %s (%d frame%s of delay)\n", MachinePlayers(*this, local).c_str(), delay,
                    delay == 1 ? "" : "s");
            return 1;
        }
        for (Link &link : links) {
            if (link.chunksHad == 0 && now - link.greeted > 250) {
                SendTo(link.address, greeting);
                link.greeted = now;
            }
        }
        // (the state, in pieces: those each hasn't said it has, a window at a time, again until it has them)
        if (now - chunkSent > 40) {
            for (Link &link : links) {
                for (uint32_t n = link.chunksHad; n < chunks && n < link.chunksHad + 32; n++) {
                    Writer piece(kPiece);
                    piece.U32(n);
                    const size_t at = static_cast<size_t>(n) * kChunk;
                    piece.Bytes(state.data() + at, std::min(kChunk, state.size() - at));
                    SendTo(link.address, piece.out);
                }
            }
            chunkSent = now;
        }
        // (those who have it all: how many more the match waits for, now and then, which tells them the host is there)
        if (now - sent > 250) {
            Writer status(kWaiting);
            status.U8(static_cast<uint8_t>(Wanted()));
            for (const Link &link : links) {
                if (link.chunksHad >= chunks) {
                    SendTo(link.address, status.out);
                }
            }
            sent = now;
        }
        // (someone joined by the code: packets their way, which open this side's router to their hello)
        if (Wanted() > 0 && !candidates.empty() && now - punchedAt > 250) {
            Writer punch(kPunch);
            for (const NetAddress &to : candidates) {
                if (!LinkFrom(to)) {
                    SendTo(to, punch.out);
                }
            }
            punchedAt = now;
        }
        return 0;
    }
    // Joining: the hello every quarter second to each of the host's addresses until the welcome comes (for 30 seconds)
    if (intro != Intro::Off && intro != Intro::Done) {
        int got;
        while ((got = socket.Receive(packet, sizeof(packet), from)) >= 0) {
            TakeStun(packet, got);
        }
        return 0;
    }
    if (!welcomed && now - began > 30000) {
        error = joinCode.empty() ? "The host did not answer. Is it waiting, and can this machine reach it on UDP port " + std::to_string(joinPort) + "?"
                                 : "The host of match " + Spaced(joinCode) + " couldn't be reached through the two networks' routers.";
        return -1;
    }
    if (welcomed && now - links[0].lastHeard > 10000) {
        error = "Lost the connection to the host.";
        return -1;
    }
    if (now - sent > (welcomed ? 100 : 250)) {
        if (welcomed) {
            // (how many pieces have come, and, once all have, that this machine is ready and still here)
            Writer ack(settings.watching ? kWatchAck : kPieceAck);
            if (settings.watching) ack.Text(watchToken);
            ack.U32(chunksHad);
            if (settings.watching) ack.U32(static_cast<uint32_t>(watchUpTo));
            SendTo(links[0].address, ack.out);
        } else {
            for (const NetAddress &to : candidates) {
                SendTo(to, greeting);
            }
        }
        sent = now;
    }
    int got;
    while ((got = socket.Receive(packet, sizeof(packet), from)) >= 0) {
        if (TakeStun(packet, got) || got < 5 || memcmp(packet, kMagic, 4)) {
            continue;
        }
        if (settings.watching && std::none_of(candidates.begin(), candidates.end(), [&](const NetAddress &a) { return a == from; })) continue;
        Reader in(packet, static_cast<size_t>(got));
        if (packet[4] == kRefuse) {
            error = in.Text();
            return -1;
        }
        if (welcomed && from == links[0].address) {
            links[0].lastHeard = now;
            if (packet[4] == kPiece && !stateHad) {
                const uint32_t n = in.U32();
                const size_t size = in.U16();
                const size_t at = static_cast<size_t>(n) * kChunk;
                if (n < chunks && in.Has(size) && at + size <= state.size()) {
                    memcpy(state.data() + at, in.at, size);
                    had[n] = true;
                    while (chunksHad < chunks && had[chunksHad]) {
                        chunksHad++;
                    }
                }
                if (chunksHad == chunks) {
                    // (all here: the host's game, as this side's)
                    std::vector<uint8_t> raw(stateSize);
                    uLongf size = static_cast<uLongf>(stateSize);
                    if (uncompress(raw.data(), &size, state.data(), static_cast<uLong>(state.size())) != Z_OK || size != stateSize) {
                        error = "The host's game did not come whole.";
                        return -1;
                    }
                    // (the host's program's words first: where this side's set differs, the host's are taken in)
                    Reader program(raw.data(), raw.size(), 0);
                    std::vector<std::pair<uint32_t, std::array<uint8_t, 4>>> words(program.U16());
                    for (auto &[word, bytes] : words) {
                        word = program.U32();
                        for (uint8_t &b : bytes) {
                            b = program.U8();
                        }
                    }
                    if (!program.ok) {
                        error = "The host's game did not come whole.";
                        return -1;
                    }
                    for (const auto &[word, bytes] : words) {
                        if (word + 2 <= port.roms.program.size()) {
                            memcpy(&port.roms.program[word], &bytes[0], 2);
                            memcpy(&port.roms.opcodes[word], &bytes[2], 2);
                        }
                    }
                    raw.erase(raw.begin(), raw.begin() + (program.at - raw.data()));
                    port.PutBackState(raw);
                    stateHad = true;
                    if (settings.watching) {
                        current = watchUpTo = watchStart;
                        fprintf(stderr, "online: spectating from confirmed frame %llu\n", static_cast<unsigned long long>(watchStart));
                        return 1;
                    }
                    sent = 0; // (said at once: the host begins when everyone has it)
                }
            } else if (packet[4] == kWaiting && stateHad) {
                // (the match waits for more: said, with the code to give them)
                const int wanted = in.U8();
                if (in.ok && wanted > 0) {
                    waiting = wanted == 1 ? "Waiting for 1 more player" : "Waiting for " + std::to_string(wanted) + " more players";
                    code = Spaced(joinCode);
                }
            } else if (packet[4] == kStart && stateHad) {
                // The match begins: the machines, each one's players and name, and which this is
                const int count = in.U8(), which = in.U8();
                std::array<Seats, kMostMachines> seats{};
                bool taken[Machine::kMaxPlayers] = {};
                bool sense = count >= 2 && count <= kMostMachines && which >= 1 && which < count;
                for (int m = 0; sense && m < count; m++) {
                    seats[m].people = in.U8();
                    sense = seats[m].people >= 1 && seats[m].people <= kMostSeats;
                    for (int person = 0; sense && person < seats[m].people; person++) {
                        const int player = in.U8();
                        sense = player < Machine::kMaxPlayers && !taken[player];
                        if (sense) {
                            seats[m].players[person] = player;
                            taken[player] = true;
                        }
                    }
                    seats[m].name = in.Text();
                }
                if (!sense || !in.ok) {
                    error = "The host's start did not make sense.";
                    return -1;
                }
                machines = count;
                local = which;
                roster = seats;
                code.clear();
                links[0].lastHeard = now;
                fprintf(stderr, "online: the match is on: you are %s (%d frame%s of delay)\n", MachinePlayers(*this, local).c_str(), delay,
                        delay == 1 ? "" : "s");
                return 1;
            }
            continue;
        }
        if (packet[4] != kWelcome || welcomed) {
            continue;
        }
        links.assign(1, Link{});
        links[0].address = from;
        links[0].lastHeard = now;
        if (in.U16() != kProtocol) {
            error = "The two copies of the game are different versions: update both.";
            return -1;
        }
        StartAt(in.U8());
        const size_t words = in.U16();
        if (!in.Has(words) || words != sizeof(port.machine->eeprom.words)) {
            error = "The host's welcome did not make sense.";
            return -1;
        }
        memcpy(port.machine->eeprom.words, in.at, words); // (the host's board settings, for this match only)
        in.at += words;
        std::map<std::string, int> host;
        const std::string features = in.Text();
        for (size_t at = 0; at < features.size();) {
            const size_t comma = std::min(features.find(',', at), features.size());
            const std::string item = features.substr(at, comma - at);
            const size_t eq = item.find('=');
            if (eq != std::string::npos) {
                host[item.substr(0, eq)] = atoi(item.c_str() + eq + 1);
            }
            at = comma + 1;
        }
        // (this player's own fighters for player 2's side of the modes played online, as they had them: sent as this
        // side's first event, after the host's pages are taken)
        std::string mine;
        for (const char *row : {"court_two", "team_2_1", "team_2_2", "team_2_3"}) {
            if (HasFeature(row)) {
                mine += (mine.empty() ? "" : ",") + std::string(row) + "=" + std::to_string(FeatureSetting(row));
            }
        }
        Settle(false, host);
        Command("hello," + mine); // ("hello": not said on the screen, Port::HearEvents)
        stateSize = in.U32();
        state.assign(in.U32(), 0);
        if (!in.ok || stateSize == 0 || state.empty() || state.size() > (16u << 20)) {
            error = "The host's welcome did not make sense.";
            return -1;
        }
        hostIn = (in.U8() & 1) != 0;
        // (the host's set: where this side's is another region's, the match is played as the host's)
        const std::string hostSet = in.Text();
        links[0].name = in.Text();
        needed = in.U8(); // (the players the match is for: the host's choice)
        watchStart = in.U32();
        const int count = in.U8();
        bool rosterOk = settings.watching ? count >= 2 && count <= kMostMachines : count == 0;
        std::array<bool, 4> assigned{};
        int total = 0;
        for (int m = 0; m < count && m < kMostMachines; ++m) {
            Seats seats;
            seats.people = in.U8();
            if (seats.people < 1 || seats.people > kMostSeats) { rosterOk = false; break; }
            for (int person = 0; person < seats.people; ++person) {
                const int player = in.U8();
                if (player >= needed || player >= 4 || assigned[player]) { rosterOk = false; break; }
                assigned[player] = true;
                seats.players[person] = player;
                total++;
            }
            seats.name = in.Text();
            roster[m] = seats;
        }
        std::array<uint32_t, FirstTo::kSaved> saved{};
        for (auto &value : saved) value = in.U32();
        if (settings.watching) {
            machines = count;
            current = watchUpTo = watchStart;
            rosterOk = rosterOk && total == needed;
            set.Load(saved);
        }
        if (!in.ok || !rosterOk || (needed != 2 && needed != 4)) {
            error = "The host's welcome did not make sense.";
            return -1;
        }
        if (hostSet != SetName(port)) {
            fprintf(stderr, "online: playing the host's set, %s\n", hostSet.c_str());
        }
        chunks = static_cast<uint32_t>((state.size() + kChunk - 1) / kChunk);
        had.assign(chunks, false);
        welcomed = true;
        sent = 0; // (the first ack at once)
    }
    return 0;
}

std::string Netplay::MachineName(int machine) const {
    if (machine < 0 || machine >= machines) {
        return "";
    }
    return Printable(!settings.watching && machine == local ? settings.name : roster[machine].name, kNameMost);
}

std::string Netplay::Name(int player) const {
    return MachineName(MachineOf(player));
}

// A line of the chat, to each link but the one it came from, by the link's own count
void Netplay::Queue(int from, const std::string &text, const Link *except) {
    for (Link &link : links) {
        if (&link != except) {
            link.unheard.push_back({link.said++, from, text});
        }
    }
    SendChat(0);
}

void Netplay::Say(const std::string &text) {
    if (settings.watching) return;
    const std::string line = Printable(text, kChatMost);
    if (!line.empty()) {
        Queue(local, line, nullptr);
    }
}

std::vector<std::pair<int, std::string>> Netplay::TakeHeard() {
    std::vector<std::pair<int, std::string>> taken;
    taken.swap(heard);
    return taken;
}

// The lines each link hasn't said it has, in order, now and then until it has (a line is a packet: what the chat
// sends is small beside the controls, sent every frame)
void Netplay::SendChat(uint64_t now) {
    for (Link &link : links) {
        if (link.unheard.empty() || now - link.saidAt < 250) {
            continue;
        }
        for (const Line &line : link.unheard) {
            Writer out(kChat);
            out.U32(line.number);
            out.U8(static_cast<uint8_t>(line.from));
            out.Text(line.text);
            SendTo(link.address, out.out);
        }
        link.saidAt = NowMs();
    }
}

int Netplay::LinkPing(const Link &link) const {
    return std::max(0, static_cast<int>(std::lround(link.ping - 1000.0 / Machine::kFramesPerSecond)));
}

int Netplay::Ping() const {
    int most = 0;
    for (const Link &link : links) {
        most = std::max(most, LinkPing(link) + (settings.hosting ? 0 : link.farthest));
    }
    return most;
}

uint64_t Netplay::Silent() const {
    const uint64_t now = NowMs();
    uint64_t most = 0;
    for (const Link &link : links) {
        most = std::max(most, now - link.lastHeard);
    }
    return most;
}

bool Netplay::Connect(Port &port, std::string &error) {
    if (!Begin(port, error)) {
        return false;
    }
    int state;
    while ((state = Waiting(port, error)) == 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return state > 0;
}

uint64_t Netplay::Slowest() const {
    uint64_t least = UINT64_MAX;
    for (int machine = 0; machine < machines; machine++) {
        if (machine != local) {
            least = std::min(least, upTo[machine]);
        }
    }
    return least;
}

void Netplay::SendInputs() {
    if (settings.watching) return;
    for (Link &link : links) {
        SendInputs(link);
    }
}

// What a frame says to a link: the frame this machine is on; the controls the link hasn't said it has, this machine's
// and (hosting) the other joining machines', each machine's from where the link has them; how far this machine has
// each machine's; how far ahead of the link it is; its last hash; the times for the ping; and (hosting) the longest
// ping of the other joining machines to it
void Netplay::SendInputs(Link &link) {
    Writer out(kControls);
    out.U32(static_cast<uint32_t>(current));
    const auto first = [&](int machine) {
        return std::max<uint64_t>(link.has[machine], upTo[machine] > 64 ? upTo[machine] - 64 : 0);
    };
    int carried[kMostMachines], count = 0;
    for (int machine = 0; machine < machines; machine++) {
        if (machine != link.machine && (settings.hosting || machine == local) && first(machine) < upTo[machine]) {
            carried[count++] = machine;
        }
    }
    out.U8(static_cast<uint8_t>(count));
    for (int i = 0; i < count; i++) {
        const int machine = carried[i];
        const uint64_t from = first(machine);
        out.U8(static_cast<uint8_t>(machine));
        out.U32(static_cast<uint32_t>(from));
        uint64_t to = from;
        size_t bytes = 0;
        while (to < upTo[machine] && to - from < 64) {
            const auto event = events[machine].find(to);
            const size_t size = 2 * roster[machine].people +
                (event == events[machine].end() ? 0 : 6 + event->second.size());
            if (bytes + size > 1200) break;
            bytes += size;
            ++to;
        }
        out.U8(static_cast<uint8_t>(to - from));
        for (uint64_t at = from; at < to; at++) {
            for (int person = 0; person < roster[machine].people; person++) {
                out.U16(inputs[machine][at % kInputs][person]);
            }
        }
        auto event = events[machine].lower_bound(from);
        const auto end = events[machine].lower_bound(to);
        out.U8(static_cast<uint8_t>(std::distance(event, end)));
        for (; event != end; ++event) {
            out.U32(static_cast<uint32_t>(event->first));
            out.Text(event->second);
        }
    }
    out.U8(static_cast<uint8_t>(machines));
    for (int machine = 0; machine < machines; machine++) {
        out.U32(static_cast<uint32_t>(upTo[machine]));
    }
    out.U16(static_cast<uint16_t>(static_cast<int16_t>(std::lround(std::clamp(link.ahead, -100.0, 100.0) * 100))));
    out.U32(static_cast<uint32_t>(lastHashFrame));
    out.U32(lastHash);
    const uint64_t now = NowMs();
    out.U32(static_cast<uint32_t>(now));
    out.U32(link.echo);
    out.U32(static_cast<uint32_t>(link.echoAt ? now - link.echoAt : 0));
    int farthest = 0;
    for (const Link &other : links) {
        if (settings.hosting && &other != &link) {
            farthest = std::max(farthest, LinkPing(other));
        }
    }
    out.U16(static_cast<uint16_t>(std::min(farthest, 65535)));
    SendTo(link.address, out.out);
}

// The match over on this machine: why, whether it is a failure, and the link that said so (nullptr: this machine
// found it). The host tells the other joining machines: a desync's reason as theirs too, or that a machine went,
// which they come back from (`lost`) to the host, which waits for everyone again
void Netplay::End(const std::string &why, bool failure, const Link *from) {
    if (!ended.empty()) {
        return; // (the first reason is the one: a desync is followed by a goodbye)
    }
    ended = why;
    failed = failure;
    if (settings.hosting && !links.empty()) {
        // (a desync goes to the others as theirs; a machine gone, as the end they come back from; three times, as a
        // goodbye goes)
        Writer tell(failure && !lost ? kRefuse : kOver);
        tell.Text(why);
        for (int n = 0; n < 3; n++) {
            SendAll(tell.out, from);
        }
        Pump();
    }
}

void Netplay::Take(Link &link, const uint8_t *data, size_t size) {
    Reader in(data, size);
    const uint64_t now = NowMs();
    if (settings.watching && data[4] == kWatchControls) {
        const std::string token = in.Text();
        const uint64_t first = in.U32();
        const unsigned count = in.U8();
        std::vector<WatchInput> frames(count);
        for (auto &f : frames) {
            for (auto &value : f.players) value = in.U16();
            f.hash = in.U32();
        }
        if (!in.ok || token != watchToken || first > watchUpTo || first + count > current + kWatchFrames) return;
        for (uint64_t at = watchUpTo; at < first + count; ++at) watchInputs[at % kWatchFrames] = frames[at - first];
        watchUpTo = std::max(watchUpTo, first + count);
        link.lastHeard = now;
        return;
    }
    if (settings.watching && data[4] != kBye && data[4] != kRefuse) return;
    switch (data[4]) {
    case kSetDone: {
        const int target = in.U8(), first = in.U8(), second = in.U8();
        const bool ready = in.U8() != 0;
        if (in.ok && size == 9 && target == set.Target() &&
            ((first == target && second < target) || (second == target && first < target))) {
            // UDP can deliver the host's pre-consensus score after its ready score. Agreement is
            // monotonic for that exact result; a different result must establish its own agreement.
            const bool alreadyReady = link.setReady && link.setTarget == target &&
                link.setScore == std::array<int, 2>{first, second};
            link.setTarget = target;
            link.setScore = {first, second};
            link.setReady = !settings.hosting && (ready || alreadyReady);
            link.lastHeard = now;
        }
        return;
    }
    case kBye:
        if (!settings.watching && ended.empty() && set.Winner() >= 0 && SetAgrees()) {
            FinishSet();
            return;
        }
        // (two machines: the other player; more: who it was)
        if (ended.empty()) {
            left = true;
            End(settings.watching ? "The match ended." : machines <= 2 ? "The other player left."
                : link.name.empty() ? (settings.hosting ? "A player left." : "The host left.")
                                    : Printable(link.name, kNameMost) + " left.",
                false, &link);
        }
        return;
    case kRefuse: {
        // (a desync, said by the machine that found it)
        const std::string why = in.Text();
        End(why, true, &link);
        return;
    }
    case kOver:
        // (from the host: another machine went; this one goes to the host again, which waits for everyone again)
        if (!settings.hosting && ended.empty()) {
            ended = in.Text();
            lost = true;
        }
        return;
    case kChat: {
        // (the next line, or one again that is already here: either way, how many have come is said back. From the
        // host, the machine that said it; the host passes a joining machine's on to the others)
        const uint32_t n = in.U32();
        const int from = in.U8();
        const std::string text = Printable(in.Text(), kChatMost);
        if (in.ok && n == link.heardUpTo) {
            link.heardUpTo++;
            const int machine = settings.hosting ? link.machine : from;
            if (!text.empty() && machine >= 0 && machine < machines && machine != local) {
                heard.push_back({machine, text});
                if (settings.hosting) {
                    Queue(machine, text, &link);
                }
            }
        }
        Writer ack(kChatAck);
        ack.U32(link.heardUpTo);
        SendTo(link.address, ack.out);
        return;
    }
    case kChatAck: {
        const uint32_t upToLine = in.U32();
        while (in.ok && !link.unheard.empty() && link.unheard.front().number < upToLine) {
            link.unheard.pop_front();
        }
        return;
    }
    case kControls: break;
    default: return;
    }
    const uint64_t frame = in.U32();
    const unsigned count = in.U8();
    if (count > kMostMachines) {
        return;
    }
    struct Block {
        int machine = 0;
        uint64_t first = 0;
        unsigned count = 0;
        Controls inputs[256];
        std::vector<std::pair<uint64_t, std::string>> events;
    } blocks[kMostMachines];
    for (unsigned b = 0; b < count; b++) {
        Block &block = blocks[b];
        block.machine = in.U8();
        block.first = in.U32();
        block.count = in.U8();
        // (a machine of the match's but this one; from a joining machine, its own only)
        if (block.machine >= machines || block.machine == local || (settings.hosting && block.machine != link.machine)) {
            return;
        }
        for (unsigned n = 0; n < block.count; n++) {
            block.inputs[n] = {};
            for (int person = 0; person < roster[block.machine].people; person++) {
                block.inputs[n][person] = in.U16();
            }
        }
        const unsigned eventCount = in.U8();
        if (eventCount && needed != 2) return;
        for (unsigned n = 0; n < eventCount; ++n) {
            const uint64_t at = in.U32();
            const std::string text = in.Text();
            if (at < block.first || at >= block.first + block.count || text.size() > 512) return;
            block.events.push_back({at, text});
        }
    }
    const unsigned acks = in.U8();
    std::array<uint64_t, kMostMachines> has{};
    for (unsigned machine = 0; machine < acks; machine++) {
        const uint64_t upToFrame = in.U32();
        if (machine < kMostMachines) {
            has[machine] = upToFrame;
        }
    }
    const double theirs = static_cast<int16_t>(in.U16()) / 100.0;
    const uint64_t hashFrame = in.U32();
    const uint32_t hash = in.U32();
    const uint32_t sentAt = in.U32(), echoed = in.U32(), held = in.U32();
    const int farthest = in.U16();
    if (!in.ok) {
        return;
    }
    link.started = true;
    link.lastHeard = now;
    link.frame = std::max(link.frame, frame);
    link.theirAhead = theirs;
    for (int machine = 0; machine < machines && machine < static_cast<int>(acks); machine++) {
        link.has[machine] = std::max(link.has[machine], has[machine]);
    }
    for (unsigned b = 0; b < count; b++) {
        const Block &block = blocks[b];
        uint64_t &known = upTo[block.machine];
        // A later block cannot silently skip missing input/events; cumulative retransmission repairs the gap.
        if (block.first > known) continue;
        for (const auto &[at, text] : block.events) {
            if (at >= confirmed && at < confirmed + kInputs && !events[block.machine].count(at)) {
                events[block.machine][at] = text;
                eventNotices.push_back({block.machine, text});
                watchModeChanged = true;
                if (at <= current) eventFrom = std::min(eventFrom, at);
            }
        }
        for (uint64_t at = known; at < block.first + block.count; at++) {
            if (at >= block.first && at < confirmed + kInputs) {
                inputs[block.machine][at % kInputs] = block.inputs[at - block.first];
                known = at + 1;
            }
        }
    }
    if (hashFrame) {
        link.hashes[hashFrame] = hash;
    }
    link.echo = sentAt;
    link.echoAt = now;
    if (echoed) {
        const double rtt = static_cast<double>(static_cast<uint32_t>(static_cast<uint32_t>(now) - echoed - held));
        if (rtt < 10000) {
            link.ping = link.ping ? link.ping * 0.9 + rtt * 0.1 : rtt;
        }
    }
    if (!settings.hosting) {
        link.farthest = farthest;
    }
}

void Netplay::Pump() {
    const uint64_t now = NowMs();
    while (!late.empty() && late.front().due <= now) {
        socket.Send(late.front().to, late.front().data.data(), late.front().data.size());
        late.pop_front();
    }
}

bool Netplay::Ready(uint64_t frame) {
    Pump();
    WatchService();
    uint8_t packet[2048];
    NetAddress from;
    int got;
    uint64_t now = NowMs();
    while ((got = socket.Receive(packet, sizeof(packet), from)) >= 0) {
        if (got < 5 || memcmp(packet, kMagic, 4)) {
            continue;
        }
        Link *link = LinkFrom(from);
        if (!link) {
            if (settings.hosting && settings.spectators &&
                (packet[4] == kWatchHello || packet[4] == kWatchAck || packet[4] == kBye)) {
                WatchPacket(packet, static_cast<size_t>(got), from);
                continue;
            }
            if (settings.hosting && packet[4] == kHello) {
                // (someone else, now the match is on)
                Writer refuse(kRefuse);
                refuse.Text(machines > 2 ? "That match is full." : "Someone has joined that match already.");
                socket.Send(from, refuse.out.data(), refuse.out.size());
            }
            continue;
        }
        if (packet[4] == kPieceAck) {
            link->lastHeard = NowMs(); // (the start not there yet: sent again below)
            continue;
        }
        Take(*link, packet, static_cast<size_t>(got));
    }
    now = NowMs(); // (after what came, each heard at its own time)
    // (hosting: the start, again, to those whose controls haven't come since)
    for (Link &link : links) {
        if (settings.hosting && !link.started && now - link.greeted > 100) {
            SendTo(link.address, Start(link.machine));
            link.greeted = now;
        }
    }
    WatchSend();
    Compare();
    SendChat(now);
    if (ended.empty() && set.Winner() >= 0 && !settings.watching) {
        if (!setSent || now - setSent >= 100) {
            Writer done(kSetDone);
            done.U8(static_cast<uint8_t>(set.Target()));
            done.U8(static_cast<uint8_t>(set.Score(0)));
            done.U8(static_cast<uint8_t>(set.Score(1)));
            done.U8(settings.hosting && SetAgrees());
            SendAll(done.out);
            setSent = now;
        }
        for (const Link &link : links) {
            if (link.setTarget == set.Target() &&
                (link.setScore[0] != set.Score(0) || link.setScore[1] != set.Score(1))) {
                End("The games disagree on the final set score.", true, nullptr);
            }
        }
        if (ended.empty() && SetAgrees()) {
            if (!setAgreed) setAgreed = now;
            SendInputs();
            if (now - setAgreed >= 2000) FinishSet();
            return false;
        }
    }
    for (const Link &link : links) {
        if (ended.empty() && now - link.lastHeard > 5000) {
            lost = set.Winner() < 0 && !settings.watching;
            End(machines <= 2                ? "Lost the connection to the other player."
                : !settings.hosting          ? "Lost the connection to the host."
                : link.name.empty()          ? "Lost the connection to a player."
                                             : "Lost the connection to " + Printable(link.name, kNameMost) + ".",
                true, nullptr);
        }
    }
    if (!ended.empty()) {
        return false;
    }
    current = frame;
    if (settings.watching) {
        if (now - watchSent >= 30 && !links.empty()) {
            Writer ack(kWatchAck);
            ack.Text(watchToken);
            ack.U32(chunksHad);
            ack.U32(static_cast<uint32_t>(watchUpTo));
            SendTo(links[0].address, ack.out);
            watchSent = now;
        }
        return frame < watchUpTo;
    }
    // Too far ahead of another machine's controls: wait for them
    if (frame >= Slowest() + kMostAhead) {
        waits++;
        SendInputs();
        return false;
    }
    // Ahead of another machine in time (each one's own idea of how far ahead it is, compared): a tick waited now and
    // then, until even (the host keeps time with the slowest of the joining machines, each of them with the host)
    if (frame % 60 == 0 && frame != waitedAt && waitTicks == 0) {
        double by = 0;
        for (const Link &link : links) {
            by = std::max(by, (link.ahead - link.theirAhead) / 2);
        }
        waitTicks = by >= 1 ? std::min(static_cast<int>(by), 8) : 0;
    }
    if (waitTicks > 0 && frame != waitedAt) {
        waitedAt = frame;
        waitTicks--;
        waits++;
        SendInputs();
        return false;
    }
    return true;
}

void Netplay::Run(Port &port, uint64_t at, bool drawing) {
    Kept &k = kept[at % kKept];
    uint16_t players[Machine::kMaxPlayers] = {};
    for (int machine = 0; machine < machines; machine++) {
        // (this machine's own; another's, or guessed as the last that came)
        const uint64_t known = upTo[machine];
        k.used[machine] = machine == local || at < known ? inputs[machine][at % kInputs]
                          : known                         ? inputs[machine][(known - 1) % kInputs]
                                                          : Controls{};
        for (int person = 0; person < roster[machine].people; person++) {
            players[roster[machine].players[person]] = k.used[machine][person];
        }
    }
    for (int machine = 0; needed == 2 && machine < machines; ++machine) {
        const auto event = events[machine].find(at);
        if (event != events[machine].end()) port.OnlineEvent(machine, event->second);
    }
    const bool versus = set.Target() && battle::TwoPeopleNow();
    port.machine->drawing = drawing;
    port.Step(players);
    port.machine->drawing = true;
    if (set.Target()) {
        k.result = {battle::RoundResultNow(), battle::RoundsToWin(), versus};
    }
    if ((at + 1) % kHashEvery == 0 || port.hashFile) {
        k.hash = port.StateHash();
    }
}

void Netplay::Frame(Port &port, const uint16_t *controls) {
    if (settings.watching) {
        current = port.frame;
        const WatchInput &f = watchInputs[port.frame % kWatchFrames];
        uint16_t players[Machine::kMaxPlayers] = {};
        std::copy(f.players.begin(), f.players.end(), players);
        const bool versus = set.Target() && battle::TwoPeopleNow(); // (as Run reads it: before the frame)
        port.Step(players);
        if (set.Target()) {
            // (the viewer's frames are all confirmed ones: the set's score counted as the players count it)
            if (set.Confirm(port.frame, {battle::RoundResultNow(), battle::RoundsToWin(), versus}) >= 0) {
                fprintf(stderr, "online: set FT%d frame %llu score %d-%d%s\n", set.Target(),
                        static_cast<unsigned long long>(port.frame + 1), set.Score(0), set.Score(1),
                        set.Winner() >= 0 ? " complete" : "");
            }
        }
        if (f.hash || port.hashFile) {
            const uint32_t hash = port.StateHash();
            if (port.hashFile) fprintf(port.hashFile, "%llu %08x\n", static_cast<unsigned long long>(port.frame + 1), hash);
            if (f.hash && hash != f.hash) { ended = "Spectator playback came apart from the match."; failed = true; }
        }
        return;
    }
    const auto began = std::chrono::steady_clock::now();
    current = port.frame;
    const uint64_t frame = port.frame;
    for (int person = 0; person < roster[local].people; person++) {
        inputs[local][(frame + delay) % kInputs][person] = controls[person];
    }
    upTo[local] = frame + delay + 1;
    if (needed == 2 && !commanded.empty()) {
        std::string &line = events[local][frame + delay];
        for (const std::string &text : commanded) {
            line += (line.empty() ? "" : ",") + text;
            eventNotices.push_back({local, text});
        }
        commanded.clear();
        watchModeChanged = true;
    }
    // How far ahead of each link this machine is: the frame it is on, over what the link said last
    for (Link &link : links) {
        link.ahead = link.ahead * 0.95 + (static_cast<double>(frame) - static_cast<double>(link.frame)) * 0.05;
    }
    SendInputs();
    // A wrong guess since the last frame that had every machine's real controls (the first frame where any machine's
    // real ones differ from those it was run with): back to it, and on to here again, unseen and unheard (the sound
    // of those frames has been heard)
    uint64_t wrong = frame;
    for (int machine = 0; machine < machines; machine++) {
        if (machine == local) {
            continue;
        }
        for (uint64_t at = confirmed; at < std::min(upTo[machine], wrong); at++) {
            if (kept[at % kKept].used[machine] != inputs[machine][at % kInputs]) {
                wrong = at;
                break;
            }
        }
    }
    if (eventFrom >= confirmed && eventFrom < frame) wrong = std::min(wrong, eventFrom);
    eventFrom = UINT64_MAX;
    if (wrong < frame) {
        const size_t sound = port.machine->audio.size();
        port.PutBackState(kept[wrong % kKept].state);
        for (uint64_t at = wrong; at < frame; at++) {
            if (at != wrong) {
                port.KeepState(kept[at % kKept].state);
            }
            Run(port, at, false);
        }
        port.machine->audio.resize(sound);
        rollbacks++;
        rerun += frame - wrong;
        mostRerun = std::max(mostRerun, static_cast<int>(frame - wrong));
    }
    // The frames that now have every machine's real controls: their hashes, to compare (and to the hash log)
    const uint64_t upToAll = std::min(Slowest(), frame);
    for (uint64_t at = confirmed; at < upToAll; at++) {
        const Kept &k = kept[at % kKept];
        if (settings.hosting && settings.spectators) {
            WatchInput &watch = watchInputs[at % kWatchFrames];
            watch = {};
            for (int m = 0; m < machines; ++m)
                for (int person = 0; person < roster[m].people; ++person)
                    watch.players[roster[m].players[person]] = inputs[m][at % kInputs][person];
            watch.hash = (at + 1) % kHashEvery == 0 ? k.hash : 0;
        }
        const int won = set.Confirm(at, k.result);
        if (won >= 0) {
            fprintf(stderr, "online: set FT%d frame %llu score %d-%d%s\n", set.Target(),
                    static_cast<unsigned long long>(at + 1), set.Score(0), set.Score(1),
                    set.Winner() >= 0 ? " complete" : "");
        }
        if (!port.opt.movieRecord.empty()) {
            // (--record: what every player held, as the frame had them, for the frames that have the real ones)
            Movie::Pads pads{};
            for (int machine = 0; machine < machines; machine++) {
                for (int person = 0; person < roster[machine].people; person++) {
                    pads[roster[machine].players[person]] = inputs[machine][at % kInputs][person];
                }
            }
            port.recMovie.changes[at] = pads;
        }
        if (port.hashFile) {
            fprintf(port.hashFile, "%llu %08x\n", static_cast<unsigned long long>(at + 1), k.hash);
        }
        if ((at + 1) % kHashEvery == 0) {
            ourHashes[at + 1] = {k.hash, 0};
            lastHashFrame = at + 1;
            lastHash = k.hash;
        }
    }
    confirmed = upToAll;
    PruneEvents();
    Compare();
    // The frame itself
    port.KeepState(kept[frame % kKept].state);
    WatchCapture(port);
    Run(port, frame, true);
    const double took = std::chrono::duration<double>(std::chrono::steady_clock::now() - began).count();
    worst = std::max(worst, took);
    over += took > 1 / Machine::kFramesPerSecond;
    Report();
}

void Netplay::PruneEvents() {
    for (int machine = 0; machine < machines; ++machine) {
        uint64_t before = confirmed;
        // Simulation confirmation does not prove that every recipient got this event. Retain it until
        // each link that needs this stream acknowledges the controls carrying it.
        for (const Link &link : links) {
            if (machine != link.machine && (settings.hosting || machine == local)) before = std::min(before, link.has[machine]);
        }
        auto &stream = events[machine];
        stream.erase(stream.begin(), stream.lower_bound(before));
    }
}

void Netplay::Command(const std::string &text) {
    size_t length = text.size();
    for (const auto &pending : commanded) length += pending.size() + 1;
    if (!text.empty() && length <= 512 && !settings.watching && needed == 2) {
        commanded.push_back(text);
    }
}

std::vector<std::pair<int, std::string>> Netplay::TakeEvents() {
    std::vector<std::pair<int, std::string>> out;
    out.swap(eventNotices);
    return out;
}

void Netplay::BeginSet() {
    if (settings.watching) { // (a viewer has the host's set as of where it came in: Waiting)
        fprintf(stderr, "online: set format %s, %d-%d\n", set.Target() == 5 ? "FT5" : set.Target() == 10 ? "FT10" : "Casual",
                set.Score(0), set.Score(1));
        return;
    }
    set.Begin(battle::OnlineSets() ? FeatureSetting("online_set") : 0);
    fprintf(stderr, "online: set format %s\n", set.Target() == 5 ? "FT5" : set.Target() == 10 ? "FT10" : "Casual");
}

bool Netplay::SetAgrees() const {
    if (!set.Target() || set.Winner() < 0 || links.empty()) return false;
    for (const Link &link : links) {
        if (link.setTarget != set.Target() || link.setScore[0] != set.Score(0) || link.setScore[1] != set.Score(1) ||
            (!settings.hosting && !link.setReady)) return false;
    }
    return true;
}

void Netplay::FinishSet() {
    setFinished = true;
    left = lost = false;
    ended = "Player " + std::to_string(set.Winner() + 1) + " wins the FT" + std::to_string(set.Target()) +
            " set (" + std::to_string(set.Score(0)) + "-" + std::to_string(set.Score(1)) + ").";
}

// Our hashes against each link's (the host's against every joining machine's, a joining machine's against the
// host's: all alike if each is like the host's)
void Netplay::Compare() {
    const unsigned everyone = (1u << links.size()) - 1;
    for (auto it = ourHashes.begin(); it != ourHashes.end();) {
        for (size_t i = 0; i < links.size(); i++) {
            if (it->second.compared & (1u << i)) {
                continue;
            }
            const auto theirs = links[i].hashes.find(it->first);
            if (theirs == links[i].hashes.end()) {
                continue;
            }
            if (theirs->second != it->second.hash && ended.empty()) {
                const std::string at = std::to_string(it->first);
                End(machines <= 2 ? "The two games came apart (a desync at frame " + at + "): the match is stopped."
                                  : "The games came apart (a desync at frame " + at + "): the match is stopped.",
                    true, nullptr);
                if (!settings.hosting) {
                    Writer refuse(kRefuse);
                    refuse.Text(ended);
                    SendAll(refuse.out);
                    Pump();
                }
            }
            it->second.compared |= 1u << i;
            links[i].hashes.erase(theirs);
        }
        if (it->second.compared == everyone || it->first + kHashEvery * 20 < confirmed) {
            it = ourHashes.erase(it); // (compared with all, or theirs never came: lost)
        } else {
            ++it;
        }
    }
    for (Link &link : links) {
        while (!link.hashes.empty() && link.hashes.begin()->first + kHashEvery * 20 < confirmed) {
            link.hashes.erase(link.hashes.begin());
        }
    }
}

// Viewers have their own state transfer and confirmed-input cursor. Their acknowledgements never
// change the player's acknowledgement, rollback window, or timing.
void Netplay::WatchPacket(const uint8_t *data, size_t size, const NetAddress &from) {
    Reader in(data, size);
    auto foundViewer = std::find_if(viewers.begin(), viewers.end(), [&](const Viewer &v) { return v.address == from; });
    if (data[4] == kWatchHello) {
        const unsigned protocol = in.U16();
        const std::string game = in.Text();
        const uint32_t theirs = in.U32();
        const std::string version = in.Text();
        in.Text(); // viewers do not take a player name or enter player chat
        const std::string token = in.Text();
        const bool invited = settings.service == "off" || std::any_of(invitedViewers.begin(), invitedViewers.end(),
            [&](const auto &v) { return v.first == token && v.second == from; });
        if (!in.ok || !invited || token.empty() || token.size() > 64) return;
        if (protocol != kProtocol || game != GAME_ID || theirs != build || version != RECOMP_VERSION) {
            Writer refuse(kRefuse); refuse.Text("Watching needs the same game and build as the players.");
            SendTo(from, refuse.out); return;
        }
        if (foundViewer == viewers.end()) {
            if (viewers.size() >= 8) return;
            Viewer v; v.address = from; v.token = token; v.heard = NowMs();
            viewers.push_back(std::move(v));
            fprintf(stderr, "online: spectator connected (%zu)\n", viewers.size());
        } else if (foundViewer->token == token) foundViewer->heard = NowMs();
        return;
    }
    if (foundViewer == viewers.end()) return;
    if (data[4] == kBye) { viewers.erase(foundViewer); return; }
    if (data[4] != kWatchAck) return;
    const std::string token = in.Text();
    const uint32_t pieces = in.U32(), ack = in.U32();
    Viewer &v = *foundViewer;
    if (!in.ok || token != v.token || v.welcome.empty() || pieces > (v.state.size() + kChunk - 1) / kChunk ||
        ack < v.start || ack > confirmed) return;
    v.pieces = std::max(v.pieces, pieces);
    v.ack = std::max(v.ack, ack);
    v.heard = NowMs();
}

void Netplay::WatchCapture(Port &port) {
    if (!settings.hosting || !settings.spectators) return;
    bool pending = watchModeChanged;
    watchModeChanged = false;
    for (const auto &stream : events) pending |= stream.lower_bound(confirmed) != stream.end();
    if (pending) {
        Writer stop(kRefuse);
        stop.Text("The online mode changed. Choose Watch to reconnect.");
        for (const Viewer &viewer : viewers) SendTo(viewer.address, stop.out);
        viewers.clear();
        return;
    }
    for (Viewer &v : viewers) {
        if (!v.welcome.empty()) continue;
        Netplay copy(settings);
        copy.delay = delay; copy.set = set; copy.needed = needed; copy.people = people; copy.machines = machines; copy.roster = roster;
        copy.Welcome(port, &kept[confirmed % kKept].state, static_cast<uint32_t>(confirmed));
        v.welcome = std::move(copy.greeting); v.state = std::move(copy.state);
        v.start = v.ack = static_cast<uint32_t>(confirmed);
        break; // bound snapshot compression to one viewer per game frame
    }
}

void Netplay::WatchSend() {
    const uint64_t now = NowMs();
    for (auto it = viewers.begin(); it != viewers.end();) {
        Viewer &v = *it;
        // Admission happens in Ready(), before Frame() assigns a confirmed starting snapshot.
        // An uninitialized viewer's zero cursor is not a claim to replay from the match's start.
        if (now - v.heard > 10000 || (!v.welcome.empty() && confirmed > uint64_t(v.ack) + kWatchFrames)) {
            Writer bye(kRefuse); bye.Text("Spectator connection fell behind. Choose Watch to reconnect.");
            SendTo(v.address, bye.out);
            it = viewers.erase(it); continue;
        }
        ++it;
        if (v.welcome.empty() || now - v.sent < 40) continue;
        v.sent = now;
        const uint32_t pieces = static_cast<uint32_t>((v.state.size() + kChunk - 1) / kChunk);
        if (v.pieces < pieces) {
            if (!v.pieces) SendTo(v.address, v.welcome);
            for (uint32_t n = v.pieces; n < pieces && n < v.pieces + 4; ++n) {
                Writer piece(kPiece); piece.U32(n);
                const size_t at = size_t(n) * kChunk;
                piece.Bytes(v.state.data() + at, std::min(kChunk, v.state.size() - at));
                SendTo(v.address, piece.out);
            }
        } else {
            Writer inputs(kWatchControls); inputs.Text(v.token); inputs.U32(v.ack);
            const uint32_t count = static_cast<uint32_t>(std::min<uint64_t>(64, confirmed - v.ack));
            inputs.U8(static_cast<uint8_t>(count));
            for (uint32_t n = 0; n < count; ++n) {
                const auto &f = watchInputs[(v.ack + n) % kWatchFrames];
                for (uint16_t value : f.players) inputs.U16(value);
                inputs.U32(f.hash);
            }
            SendTo(v.address, inputs.out);
        }
    }
}

void Netplay::WatchService() {
    if (!settings.hosting || !settings.spectators || code.empty() || secret.empty()) return;
    const uint64_t now = NowMs();
    if (watchAsking.valid()) {
        if (watchAsking.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
        const auto answer = watchAsking.get();
        invitedViewers.clear();
        if (answer.status == 200) {
            for (const std::string &item : JsonList(answer.body, "watchers")) {
                const size_t split = item.find('@');
                if (split == std::string::npos || split > 64) continue;
                std::string host, ignored; int port = 0; NetAddress address;
                UdpSocket::Split(item.substr(split + 1), host, port);
                if (port && UdpSocket::Resolve(host, port, address, ignored)) {
                    invitedViewers.push_back({item.substr(0, split), address});
                    Writer punch(kPunch); SendTo(address, punch.out);
                }
            }
        }
    }
    if (now - watchAsked < 2000) return;
    watchAsked = now;
    const std::string url = settings.service + "/v1/host/" + code.substr(0, 3) + code.substr(4) + "?secret=" + secret + "&active=1";
    watchAsking = std::async(std::launch::async, [url] {
        const auto [status, body] = Http("GET", url, ""); return HttpAnswer{status, body};
    });
}

// (what tests hold back goes before the match does: a machine told the match is over hears why)
Netplay::~Netplay() {
    while (!late.empty()) {
        socket.Send(late.front().to, late.front().data.data(), late.front().data.size());
        late.pop_front();
    }
}

void Netplay::Leave(bool reopening) {
    while (!late.empty()) { // (... and before the goodbye)
        socket.Send(late.front().to, late.front().data.data(), late.front().data.size());
        late.pop_front();
    }
    // A host reopening the match must repeat End's reconnect message, not contradict it with a terminal goodbye.
    // UDP may deliver this before End's packets, or lose those packets altogether.
    const bool again = settings.hosting && reopening;
    Writer bye(again ? kOver : kBye);
    if (again) {
        bye.Text(ended.empty() ? "The match is restarting." : ended);
    }
    Writer viewerBye(kBye);
    for (const Viewer &viewer : viewers) socket.Send(viewer.address, viewerBye.out.data(), viewerBye.out.size());
    for (const Link &link : links) {
        for (int n = 0; n < 3; n++) {
            socket.Send(link.address, bye.out.data(), bye.out.size());
        }
    }
}

void Netplay::Report(bool last) {
    const uint64_t now = NowMs();
    if (!last && now - lastReport < 10000) {
        return;
    }
    if (!lastReport && !last) {
        lastReport = now;
        return;
    }
    lastReport = now;
    double ping = 0;
    for (const Link &link : links) {
        ping = std::max(ping, link.ping);
    }
    fprintf(stderr, "online: frame %llu, ping %.0f ms, %llu rollbacks (%llu since the last report), %llu frames run again, at most %d at once; "
                    "%llu ticks waited; a frame at most %.1f ms, %llu over a frame's time\n",
            static_cast<unsigned long long>(current), ping, static_cast<unsigned long long>(rollbacks),
            static_cast<unsigned long long>(rollbacks - reportedRollbacks), static_cast<unsigned long long>(rerun), mostRerun,
            static_cast<unsigned long long>(waits), worst * 1000, static_cast<unsigned long long>(over));
    reportedRollbacks = rollbacks;
}

} // namespace port
