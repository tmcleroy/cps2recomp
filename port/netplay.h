// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
//
// Online play (issue #70): a copy of the game on each player's machine, each running the whole game with every
// player's controls. Each machine sends the others its own players' controls, frame by frame, over UDP. A frame
// does not wait for the others': until they come they are guessed (as they last were), and when they come and a
// guess was wrong, the game is put back to that frame and the frames since are run again with the real ones
// (rollback, as --rollback-test tries it: port.h, RollbackTest). No machine is ahead of another's controls by more
// than kMostAhead frames: past that, it waits.
//
// --host [PORT] waits for the other players; --join ADDRESS[:PORT] goes to the host. The host is player 1. Before the
// first frame they make sure they are the same game, and the joining machines take the host's board settings (the
// settings memory) and the features that change play (Game and Board, and widescreen, and the modes marked for
// matches: tag team); the cheats and the rest of the alternative modes, training, run-ahead and the like stay off.
// Every so often each sends a hash of the game as it was at a frame all have the real controls for; if two differ,
// the games have come apart (a desync), and the match stops, saying so.
//
// A match of four (tag team's 2 vs 2 on the host) is four people, at two, three or four machines: each machine says
// how many of them play at it (one, or two with its first two controllers: Settings::people, the Online page's People
// at this machine), and the match begins when all four are there. The host's are players 1 (and 3), and the joining
// machines' the rest, in the order they came, a machine of two taking a whole team where one is free (2 and 4). The
// host is between the joining machines: each sends the host its controls, and the host sends each the others' with
// its own (the hashes and the chat go the same way). A match of two is two machines, one player at each.
#pragma once

#include "first_to.h"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <future>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace cps2recomp {
struct Feature;
}

namespace port {

struct Port;

// A UDP socket, IPv6 and IPv4 both; the other side's address
struct NetAddress {
    std::array<uint8_t, 128> storage{}; // (a sockaddr_storage)
    int length = 0;
    // (an IPv4 address: as IPv4 too, for a socket that has no IPv6)
    std::array<uint8_t, 16> four{};
    bool hasFour = false;
    bool operator==(const NetAddress &other) const {
        return length == other.length && storage == other.storage;
    }
};
class UdpSocket {
public:
    ~UdpSocket();
    bool Open(int port, std::string &error); // (0: any port)
    int LocalPort() const; // (the port it was given)
    static bool Resolve(const std::string &host, int port, NetAddress &address, std::string &error, bool fourOnly = false);
    // "ADDRESS", "ADDRESS:PORT", an IPv6 address, or "[IPv6]:PORT": the address and the port (`port` unless said)
    static void Split(const std::string &text, std::string &host, int &port);
    void Send(const NetAddress &to, const uint8_t *data, size_t size);
    // A packet that has come (its size), or -1 for none yet
    int Receive(uint8_t *data, size_t size, NetAddress &from);

private:
    intptr_t handle = -1;
    bool six = false; // (IPv6, and IPv4 through it)
};

class Netplay {
public:
    static constexpr int kDefaultPort = 7845;
    // The packets' layout (2: the host's state sent at the start); two copies must have the same to play, and the
    // lobby shows only matches of this one (the launcher asks with it too)
    static constexpr int kProtocol = 10; // (3: the version said in the hello and to the service; 4: the welcome's flags;
                                        // 5: the welcome's program words, so two regions' sets play together;
                                        // 6: each player's name in the hello and the welcome, and the chat;
                                        // 7: two players at each machine, as the welcome says, and each one's controls;
                                        // 8: up to four machines, the host between them: the people at each in the
                                        // hello, the machines and their players as the match begins, each machine's
                                        // controls and the chat's lines passed on by the host;
                                        // 10: combined with upstream first-to, spectators and mode events (upstream 9))
    static constexpr int kMostSeats = 2;    // players at one machine
    static constexpr int kMostMachines = 4; // machines in a match: the host's, and up to three joining
    static constexpr int kMostAhead = 8;    // frames run on guessed controls at most
    static constexpr int kKept = 16;        // states kept (more than the most a rollback goes back)
    static constexpr int kInputs = 128;     // controls kept, each machine's
    static constexpr int kHashEvery = 60;   // frames between the hashes compared
    static constexpr int kDefaultDelay = 2; // frames a player's own controls are held back (fewer rollbacks)
    // The introduction service (online/): a host gets a code from it, and a player joining by the code is told where
    // the host is, and the host where they are; the two then reach each other directly
    static constexpr const char *kService = "https://cps2recomp-online.tmcleroy.workers.dev";

    static const char *Service() {
        const char *value = std::getenv("CPS2RECOMP_NET_SERVICE");
        return value && *value ? value : kService;
    }

    // What --host, --join and the --net-* options asked for
    struct Settings {
        bool hosting = false;
        bool watching = false, spectators = false;
        std::string address; // (joining: the host's)
        int port = kDefaultPort;
        int delay = kDefaultDelay;
        std::string service = Service(); // (--net-service URL; "off": none, the host's address only)
        std::string renewCode, renewSecret; // (hosting again after a player left: the code they know, kept)
        bool listed = false;  // (--net-list) the match in the lobby: on the Online card's list of open matches
        std::string name;     // (--net-name) ... under this name
        // (--net-invite TO:FROM, --net-invite-name NAME) hosting: a player in the launcher's waiting room (online/)
        // invited to the match, by their id, from this player's (named `name`); the invited player's name, as said
        std::string invite, from, inviteName;
        // (--net-arcade) hosting: the game played meanwhile, alone, as offline (with the match's features); the match
        // begins from the game as it is when the other player comes, who presses Start to challenge
        bool arcade = false;
        // (the in-game menu's Host a match) hosting while the game is the player's own, offline, features and all;
        // when someone comes the match begins from the board's reset with the match's (Port::MatchComing)
        bool solo = false;
        // (--net-people N) the people who play at this machine in a match of four, 1 or 2 (0: as the Online page's People
        // at this machine has it); in a match of two, one
        int people = 0;
        // (tests: each packet sent late by this many ms, give or take `jitter`, and this many in a hundred lost)
        int fakeLag = 0, fakeJitter = 0, fakeLoss = 0;
    };
    explicit Netplay(const Settings &settings) : settings(settings) {}
    ~Netplay();
    // The socket, the other machines found and agreed with (blocks until then; false and `error` if it did not go)
    bool Connect(Port &port, std::string &error);
    // ... or a step at a time, for a window that shows the wait: Begin, then Waiting each tick until it is not 0 (1:
    // the match is on; -1: it did not go, as `error` says)
    bool Begin(Port &port, std::string &error);
    int Waiting(Port &port, std::string &error);
    std::string waiting; // what is being waited for ("Waiting for the other player...")
    std::string code;    // (hosting) the match's code, once the service has given one, as said ("K7Q-M2X")
    const std::string &Secret() const {
        return secret;
    }
    bool Watching() const { return settings.watching; }
    uint64_t WatchStart() const { return watchStart; }
    bool Hosting() const {
        return settings.hosting;
    }
    // Whether the match decides this feature for every player (the host's setting, or off for all: Settle), so that a
    // player changing it during the match would leave the games apart; the in-game menu then leaves it as it is
    static bool MatchDecides(const cps2recomp::Feature &feature);
    bool Listed() const {
        return settings.listed;
    }
    // (hosting, --net-invite) the invited player, by name, and what came of it: "" (waiting for them), "declined", or
    // "away" (they couldn't be given it: gone from the waiting room)
    const std::string &Invited() const {
        return settings.inviteName;
    }
    const std::string &Invitation() const {
        return invitation;
    }
    bool Arcade() const {
        return settings.hosting && settings.arcade;
    }
    // (hosting) someone has come, and is being sent the game: the game waits for them now
    bool Found() const {
        return !links.empty();
    }
    // (joining) the host's player has already pressed Start: the host was playing while it waited (--net-arcade)
    bool HostIn() const {
        return hostIn;
    }
    // The time a packet takes there and back (ms; the frame's wait for it to be read taken off): to the other player,
    // or to the farthest in a match of more machines (a joining machine's to the host and on to the farthest of the
    // others); and how long since a machine this one plays with was last heard (the longest)
    int Ping() const;
    int Delay() const {
        return delay;
    }
    uint64_t Silent() const;
    // The machines in the match (2-4), and this one (0: the host's)
    int Machines() const {
        return machines;
    }
    int Players() const { return needed; }
    int Local() const {
        return local;
    }
    // The people at a machine (1, or 2: its first and its second controllers), and which of the board's players (0-3)
    // each is; the board's player a machine has (-1: none, the computer's)
    int People(int machine) const {
        return machine >= 0 && machine < machines ? roster[machine].people : 0;
    }
    int Player(int machine, int person) const {
        return roster[machine].players[person];
    }
    int MachineOf(int player) const;
    // Each tick: what has come in, sent again what has not been taken; false if the next frame must wait (another
    // machine's controls are too far behind, or this machine is ahead of another in time)
    bool Ready(uint64_t frame);
    // The frame, with this machine's players' controls (People(Local()) of them, its first player's first): rollback
    // first if a guess was wrong, then the frame itself
    void Frame(Port &port, const uint16_t *controls);
    void BeginSet();
    const FirstTo &Set() const { return set; }
    bool SetFinished() const { return setFinished; }
    // Why the match is over ("" while it goes on); and whether that is a failure (a desync, a machine lost)
    std::string ended;
    bool failed = false;
    // (... because the other side left, or went silent: a window waits for them again. A joining machine told by the
    // host that another machine went is `lost` too: it goes to the host again, which waits for everyone again)
    bool left = false, lost = false;
    // The match is over on this machine: the others are told. A reopening host asks them to reconnect instead.
    void Leave(bool reopening = false);
    // A line on how it is going (ping, rollbacks), every few seconds, to stderr
    void Report(bool last = false);
    // The players' names (--net-name; the others' from the hello, or as the match begins), as the screen shows them:
    // at most kNameMost letters ("" for none given): a machine's, and a board player's machine's
    static constexpr size_t kNameMost = 16;
    std::string MachineName(int machine) const;
    std::string Name(int player) const;
    // The chat (issue #196): a line said to the other players, straight to them (never through the service; through
    // the host to the other joining machines), sent again until they have it; and the lines that have come since this
    // was last asked, in order, each with the machine that said it
    static constexpr size_t kChatMost = 60;
    void Say(const std::string &text);
    std::vector<std::pair<int, std::string>> TakeHeard();
    void Command(const std::string &text);
    std::vector<std::pair<int, std::string>> TakeEvents();

private:
    friend struct NetplayTest; // ROM-free protocol regressions exercise the production transitions
    Settings settings;
    FirstTo set;
    uint64_t setSent = 0, setAgreed = 0;
    bool setFinished = false;
    void FinishSet();
    void PruneEvents();
    bool SetAgrees() const;
    static constexpr size_t kWatchFrames = 4096;
    struct WatchInput { std::array<uint16_t, 4> players{}; uint32_t hash = 0; };
    struct Viewer {
        NetAddress address;
        std::string token;
        std::vector<uint8_t> welcome, state;
        uint32_t pieces = 0, start = 0, ack = 0;
        uint64_t heard = 0, sent = 0;
    };
    std::vector<Viewer> viewers;
    std::vector<std::pair<std::string, NetAddress>> invitedViewers;
    std::array<WatchInput, kWatchFrames> watchInputs{};
    uint64_t watchStart = 0, watchUpTo = 0, watchSent = 0;
    std::string watchToken;
    bool watchModeChanged = false;

    uint64_t watchAsked = 0;
    void WatchPacket(const uint8_t *data, size_t size, const NetAddress &from);
    void WatchSend();
    void WatchCapture(Port &port);
    void WatchService();
    UdpSocket socket;
    // (the handshake: the hello or the welcome, sent again until answered)
    std::vector<uint8_t> greeting;
    uint32_t build = 0;
    uint64_t began = 0, sent = 0;
    bool hostIn = false;
    void Hello();             // (joining: the hello, with the people here and the service's number for this machine)
    void Welcome(Port &port, const std::vector<uint8_t> *snapshot = nullptr, uint32_t start = 0); // (hosting: the welcome, and the game as it is, for those who come)
    int joinPort = 0;
    void StartAt(int frames);
    // The game as the match begins: the host's, sent to each joining machine in pieces (compressed), which it takes as
    // its own; so all begin exactly alike, the first time and after a reconnect
    static constexpr size_t kChunk = 1024;
    std::vector<uint8_t> state;   // (compressed)
    size_t stateSize = 0;         // (as it was)
    uint32_t chunks = 0, chunksHad = 0; // its pieces, and (joining) how many this machine has
    uint64_t chunkSent = 0;
    bool welcomed = false;        // (joining: the welcome has come, the state is coming)
    bool stateHad = false;        // (joining: ... and has come: the start is waited for)
    std::vector<bool> had;        // (joining: the pieces that have come)
    // The introduction (online/): this side's address as the internet sees it (asked of STUN servers, from the
    // socket the match uses), then the service asked for a code (hosting) or the host's addresses (joining by code),
    // then (hosting) asked until everyone has joined; then each side sends to the other's addresses (a host's
    // packets open the way through its router for the others' hellos)
    struct HttpAnswer {
        int status = 0; // (0: no answer)
        std::string body;
    };
    enum class Intro { Off, Stun, Asking, Polling, Done } intro = Intro::Off;
    std::string joinCode; // (joining by a code: it, as typed)
    uint32_t ticket = 0;  // (joining by a code: the service's number for this machine, said in the hello)
    std::future<std::vector<NetAddress>> stunLookup;
    std::vector<NetAddress> stunServers;
    std::array<uint8_t, 12> stunId{};
    uint64_t stunBegan = 0, stunSent = 0;
    std::string seenAs; // ("IP:PORT")
    std::future<HttpAnswer> asking, watchAsking;
    uint64_t askedAt = 0;
    uint64_t codeAt = 0; // (hosting: when the code was had, for how often it is asked about: Introduce)
    std::string secret;
    std::string invitation; // (hosting, --net-invite) what the service said came of it: Invitation()
    std::vector<NetAddress> candidates; // (joining: the host's addresses; hosting: those of each who joined by the code)
    std::vector<std::string> told;      // (hosting: the addresses of those, as the service said them)
    // (hosting: the service's numbers for those who joined by the code, and when each was first said, or that it came
    // or went; those that went before the match began, or never came, to be said to the service, which then has room
    // for others)
    static constexpr uint64_t kCame = UINT64_MAX - 1, kWent = UINT64_MAX;
    std::map<uint32_t, uint64_t> tickets;
    std::vector<uint32_t> gone;
    std::vector<uint32_t> polledGone; // departures in the outstanding poll; retained in gone until acknowledged
    int toldPlayers = 0, toldHere = 0; // (hosting: the match's players and the host's, as the service was told them)
    uint64_t punchedAt = 0;
    bool Introduce(uint64_t now, std::string &error);
    bool TakeStun(const uint8_t *data, int size);
    std::string Addresses() const; // (this side's, as JSON)

    // The match: the players it is for (4 in tag team's 2 vs 2), and each machine's people and players
    int needed = 2;
    int people = 1; // (this machine's)
    int MatchPlayers() const;
    int PeopleHere() const;
    int machines = 2, local = 0;
    struct Seats {
        int people = 1;
        std::array<int, kMostSeats> players{};
        std::string name;
    };
    std::array<Seats, kMostMachines> roster{};
    void Assign(); // (hosting: the players handed out, as the match begins)
    std::vector<uint8_t> Start(int machine) const; // (hosting: the start, for a joining machine)
    int delay = kDefaultDelay;
    // A machine's controls for a frame: each of its players'
    using Controls = std::array<uint16_t, kMostSeats>;
    std::array<std::array<Controls, kInputs>, kMostMachines> inputs{};
    std::array<uint64_t, kMostMachines> upTo{}; // each machine's controls are known for the frames before this
    uint64_t Slowest() const; // (the least of the other machines')
    // A machine this one talks to: for the host, each joining machine; for a joining machine, the host
    struct Line {
        uint32_t number;
        int from; // (the machine that said it)
        std::string text;
    };
    struct Link {
        NetAddress address;
        int machine = 0;
        std::string name;
        int people = 1;
        uint32_t ticket = 0;
        uint64_t lastHeard = 0, greeted = 0;
        uint32_t chunksHad = 0;  // (hosting: the pieces of the state it has)
        int setTarget = 0;
        std::array<int, 2> setScore{};
        bool setReady = false;
        bool started = false;    // (hosting: its controls have come since the match began)
        std::array<uint64_t, kMostMachines> has{}; // how far it has each machine's controls (the frames before)
        uint64_t frame = 0;      // the frame it was on, last heard
        double ahead = 0, theirAhead = 0; // (keeping time with it: how far ahead of it this machine is, and its own idea)
        std::map<uint64_t, uint32_t> hashes;
        uint32_t echo = 0;       // its time on its last packet
        uint64_t echoAt = 0;     // ... and when it came, ours
        double ping = 0;
        int farthest = 0;        // (from the host) the longest ping of the other joining machines to it, ms
        // (the chat to it: the lines it hasn't said it has, and when they were last sent; and the lines from it, all
        // those before this number)
        std::deque<Line> unheard;
        uint32_t said = 0, heardUpTo = 0;
        uint64_t saidAt = 0;
    };
    std::vector<Link> links;
    Link *LinkFrom(const NetAddress &from);
    int LinkPing(const Link &link) const;
    // (hosting: those still waited for: how many more players)
    int Wanted() const;
    void SayWaiting();
    struct Kept {
        std::vector<uint8_t> state; // the game before the frame
        std::array<Controls, kMostMachines> used{}; // each machine's controls it was run with
        uint32_t hash = 0;          // the game after it, last run (when hashed)
        FirstTo::Frame result;     // replaced on rollback; scored only when this frame is confirmed
    };
    std::array<Kept, kKept> kept;
    uint64_t confirmed = 0;  // the frames before this were run with every machine's real controls
    uint64_t current = 0;    // the frame running now
    struct OurHash {
        uint32_t hash = 0;
        unsigned compared = 0; // (the links it has been compared with)
    };
    std::map<uint64_t, OurHash> ourHashes;
    uint64_t lastHashFrame = 0; uint32_t lastHash = 0; // (ours, sent with every packet)
    // Keeping time with the others: a tick waited now and then, until even
    int waitTicks = 0;
    uint64_t waitedAt = UINT64_MAX;
    // (for the report)
    uint64_t rollbacks = 0, rerun = 0, waits = 0, lastReport = 0, reportedRollbacks = 0;
    int mostRerun = 0;
    double worst = 0;  // the longest a frame took, its rollback with it (seconds)
    uint64_t over = 0; // ... and how many took longer than a frame's time

    // (tests: packets held back, and when they go)
    struct Late { uint64_t due; NetAddress to; std::vector<uint8_t> data; };
    std::deque<Late> late;
    uint64_t random = 0x2545F4914F6CDD1Dull;

    // (the chat: this machine's lines and those passed on, to each link; the lines heard, not taken yet)
    std::vector<std::pair<int, std::string>> heard;
    void Queue(int from, const std::string &text, const Link *except);
    void SendChat(uint64_t now);
    // (events: each player's, by the frame they happen before; this side's waiting for its next frame; the earliest
    // frame an event came for that has been run already; those to say)
    std::map<uint64_t, std::string> events[kMostMachines];
    std::vector<std::string> commanded;
    uint64_t eventFrom = UINT64_MAX;
    std::vector<std::pair<int, std::string>> eventNotices;

    void SendTo(const NetAddress &to, const std::vector<uint8_t> &packet);
    void SendAll(const std::vector<uint8_t> &packet, const Link *except = nullptr);
    void SendInputs();
    void SendInputs(Link &link);
    void Pump();
    void Take(Link &link, const uint8_t *data, size_t size);
    void End(const std::string &why, bool failure, const Link *from);
    void Run(Port &port, uint64_t at, bool drawing);
    void Compare();
};

} // namespace port
