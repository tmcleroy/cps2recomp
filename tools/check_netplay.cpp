// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
// ROM-free protocol regressions. Compile the production translation unit, so these exercise the same packet,
// socket and state-transition code as the game. Unused game-dependent sections are removed by the linker.
#include "../port/netplay.cpp"
#include "../port/hud.h"
#include "../recomp-common/text.h"
#include "../port/online_pad_queue.h"
#include "../port/keyboard_input.h"
#include "../recomp-common/menu_sdl.h"
#include "../fight/tag.h"

#include <iostream>
#include <stdexcept>

namespace {
void Require(bool condition, const std::string &message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}
}

// These dependencies belong to introduction branches the tests do not enter. Fail explicitly if a test starts
// consulting game features or discovering external interfaces; no game state or service is needed here.
namespace cps2recomp {
Game &TheGame() { static Game game; return game; }
int testTagMode = -1, testTeamOrder = 0;
int FeatureValue(const char *name) {
    if (!strcmp(name, tag::kFeature) && testTagMode >= 0) return testTagMode;
    if (!strcmp(name, tag::kTeamOrder)) return testTeamOrder;
    throw std::runtime_error("unexpected game feature lookup");
}
}
namespace launcher {
std::vector<LocalAddress> local_addresses() {
    throw std::runtime_error("unexpected network interface discovery");
}
}

namespace port {
struct NetplayTest {
    using Packet = std::vector<uint8_t>;

    static Netplay::Settings Settings(bool hosting) {
        Netplay::Settings settings;
        settings.hosting = hosting;
        settings.people = 1;
        settings.name = hosting ? "Host" : "Guest";
        settings.service = "off";
        return settings;
    }

    static void ConnectReceiver(Netplay &sender, UdpSocket &receiver) {
        std::string error;
        Require(receiver.Open(0, error), "receiver socket: " + error);
        Require(sender.socket.Open(0, error), "sender socket: " + error);
        sender.links.resize(1);
        Require(UdpSocket::Resolve("127.0.0.1", receiver.LocalPort(), sender.links[0].address, error),
                "loopback address: " + error);
        sender.links[0].name = "Host";
        sender.machines = 4;
    }

    static std::vector<Packet> Receive(UdpSocket &socket, size_t expected) {
        std::vector<Packet> packets;
        const uint64_t deadline = NowMs() + 2000;
        while (packets.size() < expected && NowMs() < deadline) {
            uint8_t data[2048];
            NetAddress from;
            const int size = socket.Receive(data, sizeof(data), from);
            if (size >= 0) {
                packets.emplace_back(data, data + size);
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
        Require(packets.size() == expected, "missing loopback packets: expected " + std::to_string(expected) +
                ", received " + std::to_string(packets.size()));
        for (const Packet &packet : packets) {
            Require(packet.size() >= 5 && !memcmp(packet.data(), kMagic, 4), "invalid packet header");
        }
        return packets;
    }

    static void Feed(Netplay &client, const Packet &packet) {
        if (client.links.empty()) {
            client.links.resize(1);
            client.links[0].name = "Host";
        }
        client.machines = 4;
        client.Take(client.links[0], packet.data(), packet.size());
    }

    static void ReopeningPackets() {
        Netplay host(Settings(true));
        UdpSocket receiver;
        ConnectReceiver(host, receiver);
        // The normal lifecycle: End broadcasts a reconnect, then BackToSolo(true) cleans up this connection.
        host.End("Peer3 left.", false, nullptr);
        host.Leave(true);
        const auto packets = Receive(receiver, 6);
        for (const Packet &packet : packets) {
            Require(packet[4] == kOver, "a reopening host emitted terminal kBye");
            Reader reader(packet.data(), packet.size());
            Require(reader.Text() == "Peer3 left." && reader.ok, "reopening lost its original reason");
        }
        for (bool reverse : {false, true}) {
            Netplay client(Settings(false));
            // UDP can reorder or duplicate either batch. Both must leave the client eligible to rejoin.
            for (size_t n = 0; n < packets.size() * 2; n++) {
                Feed(client, packets[reverse ? packets.size() - 1 - n % packets.size() : n % packets.size()]);
            }
            Require(client.lost && !client.left && !client.failed && client.ended == "Peer3 left.",
                    "reordered/duplicate reopening packets prevented rejoin");
        }
        // Even if every End packet was lost, receiving only cleanup must still request a reconnect.
        Netplay client(Settings(false));
        Feed(client, packets.back());
        Require(client.lost && !client.left, "cleanup alone prevented rejoin");
    }

    static void LeaveKinds() {
        for (bool hosting : {false, true}) {
            for (bool reopening : {false, true}) {
                Netplay sender(Settings(hosting));
                UdpSocket receiver;
                ConnectReceiver(sender, receiver);
                sender.Leave(reopening);
                for (const Packet &packet : Receive(receiver, 3)) {
                    const bool reconnect = hosting && reopening;
                    Require(packet[4] == (reconnect ? kOver : kBye), "incorrect leave packet kind");
                    Netplay client(Settings(false));
                    Feed(client, packet);
                    Require(client.lost == reconnect && client.left == !reconnect,
                            "leave packet produced incorrect client disposition");
                    if (reconnect) {
                        Require(client.ended == "The match is restarting.", "missing reconnect fallback reason");
                    }
                }
            }
        }
    }

    static void EndOrdering() {
        Writer over(kOver), bye(kBye), refuse(kRefuse);
        over.Text("Peer3 left.");
        refuse.Text("The games have come apart.");
        // Once a reason is accepted, later cleanup cannot change a reconnect into a departure (or mask a desync).
        Netplay reconnect(Settings(false));
        for (const Packet *packet : {&over.out, &bye.out, &over.out, &bye.out}) {
            Feed(reconnect, *packet);
        }
        Require(reconnect.lost && !reconnect.left && reconnect.ended == "Peer3 left.",
                "late goodbye overrode reconnect");
        Netplay departed(Settings(false));
        for (const Packet *packet : {&bye.out, &over.out, &bye.out}) {
            Feed(departed, *packet);
        }
        Require(departed.left && !departed.lost && departed.ended == "Host left.",
                "terminal departure was overwritten");
        Netplay failed(Settings(false));
        for (const Packet *packet : {&refuse.out, &bye.out, &over.out}) {
            Feed(failed, *packet);
        }
        Require(failed.failed && !failed.lost && !failed.left && failed.ended == "The games have come apart.",
                "cleanup masked a desync");
        Netplay host(Settings(true));
        Feed(host, over.out);
        Require(host.ended.empty() && !host.lost, "host accepted a client-only reconnect instruction");
    }

    static void PollState(Netplay &host) {
        host.intro = Netplay::Intro::Polling;
        host.code = "ABC-DEF";
        host.needed = host.toldPlayers = 4;
        host.people = host.toldHere = 1;
        host.codeAt = host.askedAt = NowMs();
    }

    static void Answer(Netplay &host, int status) {
        std::promise<Netplay::HttpAnswer> answer;
        host.asking = answer.get_future();
        answer.set_value({status, "{\"joiners\":[]}"});
        std::string error;
        Require(host.Introduce(host.askedAt, error), "poll failed: " + error);
        Require(!host.asking.valid(), "completed answer was not consumed");
        Require(host.polledGone.empty(), "completed poll retained its in-flight snapshot");
    }

    static void DepartureAcknowledgments() {
        for (const int status : {0, 500, 503}) {
            Netplay host(Settings(true));
            PollState(host);
            host.gone = {7, 8};
            host.polledGone = {7}; // 8 departed after this request began.
            Answer(host, status);
            Require(host.gone == std::vector<uint32_t>({7, 8}), "failed poll discarded pending departures");
            host.polledGone = host.gone; // the next request resends both.
            Answer(host, 200);
            Require(host.gone.empty(), "successful retry did not acknowledge departures");
        }
        Netplay host(Settings(true));
        PollState(host);
        host.gone = {7, 8};
        host.polledGone = {7};
        Answer(host, 200);
        Require(host.gone == std::vector<uint32_t>({8}), "acknowledgment lost a newer in-flight departure");
        Answer(host, 200);
        Require(host.gone == std::vector<uint32_t>({8}), "empty acknowledgment removed unsent departures");

        // An unfinished future must not consume either the pending set or its request snapshot.
        std::promise<Netplay::HttpAnswer> pending;
        host.asking = pending.get_future();
        host.polledGone = {8};
        std::string error;
        Require(host.Introduce(host.askedAt, error), "in-flight poll failed");
        Require(host.gone == std::vector<uint32_t>({8}) && host.polledGone == host.gone,
                "in-flight poll discarded departures");
        pending.set_value({200, "{\"joiners\":[]}"});
        Require(host.Introduce(host.askedAt, error) && host.gone.empty() && host.polledGone.empty(),
                "in-flight acknowledgment did not complete");

        Netplay expired(Settings(true));
        PollState(expired);
        expired.secret = "test-secret";
        expired.gone = {7, 8};
        expired.polledGone = {7};
        expired.tickets[9] = expired.askedAt; // an old-generation reservation still waiting for its hello.
        expired.tickets[10] = Netplay::kCame;
        expired.links.resize(1);
        expired.links[0].ticket = 10; // an established connection must shed its old reservation identity too.
        Answer(expired, 404);
        Require(expired.gone.empty() && expired.intro == Netplay::Intro::Stun,
                "expired code carried removals into a new ticket generation");
        Require(expired.tickets.empty() && expired.links.size() == 1 && expired.links[0].ticket == 0,
                "old reservation timeout or link departure could remove a renewed ticket");
        Require(expired.settings.renewCode == "ABCDEF" && expired.settings.renewSecret == "test-secret",
                "expired code lost its renewal identity");
    }

    static void DepartureTransmission() {
        // Reserve a loopback TCP port without listening. Real production Http gets a connection refusal, with
        // no external service or game involved; meanwhile we inspect the actual request's pending snapshot.
        struct ClosedService {
            SocketHandle socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            ~ClosedService() { if (socket != static_cast<SocketHandle>(-1)) { closesocket(socket); } }
        } service;
        Require(service.socket != static_cast<SocketHandle>(-1), "cannot create failed-service socket");
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        Require(bind(service.socket, reinterpret_cast<sockaddr *>(&address), sizeof(address)) == 0,
                "cannot reserve failed-service port");
        socklen_t size = sizeof(address);
        Require(getsockname(service.socket, reinterpret_cast<sockaddr *>(&address), &size) == 0,
                "cannot read failed-service port");
        Netplay host(Settings(true));
        PollState(host);
        host.settings.service = "http://127.0.0.1:" + std::to_string(ntohs(address.sin_port));
        host.gone = {7};
        std::string error;
        for (int attempt = 0; attempt < 2; attempt++) {
            const auto pending = host.gone;
            const uint64_t now = host.askedAt + 2001;
            Require(host.Introduce(now, error) && host.asking.valid(), "poll request did not start");
            Require(host.gone == pending && host.polledGone == pending, "request discarded unacknowledged departures");
            if (!attempt) {
                host.gone.push_back(8); // another player leaves while the HTTP request is in flight.
            }
            Require(host.asking.wait_for(std::chrono::seconds(12)) == std::future_status::ready,
                    "failed-service request did not complete");
            Require(host.Introduce(now, error), "failed request ended introduction");
            Require(!host.asking.valid() && host.polledGone.empty() && host.gone == std::vector<uint32_t>({7, 8}),
                    "failed request or retry lost pending departures");
        }
    }

    using RoomLayouts = std::vector<std::vector<std::vector<int>>>;
    static void CheckRosters(const RoomLayouts &expected) {
        for (const auto &seats : expected) {
            Netplay host(Settings(true));
            host.needed = 4;
            host.people = static_cast<int>(seats[0].size());
            host.links.resize(seats.size() - 1);
            for (size_t i = 0; i < host.links.size(); i++) {
                host.links[i].people = static_cast<int>(seats[i + 1].size());
                host.links[i].name = "Guest" + std::to_string(i + 1);
            }
            host.Assign();
            Require(host.Machines() == static_cast<int>(seats.size()), "incorrect roster machine count");
            std::array<int, 4> seen{};
            for (int machine = 0; machine < host.Machines(); machine++) {
                Require(host.People(machine) == static_cast<int>(seats[machine].size()), "incorrect people count");
                for (int person = 0; person < host.People(machine); person++) {
                    const int player = host.Player(machine, person);
                    Require(player == seats[machine][person], "incorrect team/seat assignment");
                    Require(host.MachineOf(player) == machine, "incorrect reverse player ownership");
                    seen[player]++;
                }
            }
            Require(seen == std::array<int, 4>{1, 1, 1, 1}, "roster duplicated or omitted a player");
            Require(host.MachineOf(4) == -1, "out-of-roster player has an owner");
            for (int recipient = 1; recipient < host.Machines(); recipient++) {
                const auto start = host.Start(recipient);
                Require(start[4] == kStart, "incorrect start packet kind");
                Reader reader(start.data(), start.size());
                Require(reader.U8() == host.Machines() && reader.U8() == recipient, "incorrect start recipient");
                for (int machine = 0; machine < host.Machines(); machine++) {
                    Require(reader.U8() == host.People(machine), "serialized people count differs");
                    for (int person = 0; person < host.People(machine); person++) {
                        Require(reader.U8() == host.Player(machine, person), "serialized player ownership differs");
                    }
                    Require(reader.Text() == host.roster[machine].name, "serialized machine name differs");
                    Require(reader.U8() == host.DelayOf(machine), "serialized machine delay differs");
                    const int ahead = reader.U8();
                    Require((ahead == 255 ? -1 : ahead) == host.AheadOf(machine), "serialized machine run-ahead differs");
                }
                Require(reader.ok && reader.at == reader.end, "malformed start roster");
            }
        }
    }

    static void Rosters() {
        CheckRosters({{{0, 2}, {1}, {3}}, {{0}, {1, 3}, {2}}, {{0}, {1}, {2, 3}},
                      {{0}, {1}, {2}, {3}}, {{0, 2}, {1, 3}}});
    }

    static void AlternateTeamOrder() {
        const auto previous = TheGame();
        tag::Description description;
        description.teamOrder = description.threePlayers = true;
        tag::Register(description);
        testTagMode = tag::kVersus;
        testTeamOrder = 1;
        CheckRosters({{{0}, {1}, {2}, {3}}, {{0, 1}, {2, 3}}, {{0, 1}, {2}, {3}},
                      {{0}, {2, 3}, {1}}, {{0}, {1}, {2, 3}}});

        Netplay host(Settings(true));
        host.needed = 4;
        UdpSocket socket;
        ConnectReceiver(host, socket);
        auto options = Settings(false);
        options.watching = true;
        Netplay viewer(options);
        viewer.Hello();
        auto hello = viewer.greeting;
        hello[5] = Netplay::kProtocol - 1; // the same game, version and signature on the previous protocol
        const auto &from = host.links[0].address;
        for (int order : {1, 0}) {
            testTeamOrder = order;
            host.WatchPacket(hello.data(), hello.size(), from);
            Require(host.viewers.empty() && Receive(socket, 1)[0][4] == kRefuse,
                    "a previous-protocol viewer entered the room");
            host.WatchPacket(viewer.greeting.data(), viewer.greeting.size(), from);
            Require(host.viewers.size() == 1, "matching-protocol viewer refused");
            host.viewers.clear();
        }

        testTagMode = -1;
        TheGame() = previous;
    }

    static void AsymmetricRosters() {
        tag::Description description;
        description.threePlayers = true;
        tag::Register(description);
        for (const auto mode : {tag::kTwoVsOne, tag::kOneVsTwo}) {
            testTagMode = mode;
            const int split = mode == tag::kTwoVsOne ? 0 : 1;
            const int solo = 1 - split;
            Require(TheGame().matchPlayers() == 3, "wrong human count for asymmetric match");
            Require(tag::Person(solo, 0) == solo && tag::Person(solo, 1) == solo,
                    "solo player's picks do not share a controller");
            Require(tag::Person(split, 0) == split && tag::Person(split, 1) == 2,
                    "teammates do not have distinct controllers");
            const std::vector<std::vector<std::vector<int>>> layouts{
                {{0}, {1}, {2}}, {{split, 2}, {solo}}, {{solo}, {split, 2}}};
            for (const auto &layout : layouts) {
                Netplay host(Settings(true));
                host.needed = host.MatchPlayers();
                host.people = static_cast<int>(layout[0].size());
                host.links.resize(layout.size() - 1);
                for (size_t m = 1; m < layout.size(); ++m)
                    host.links[m - 1].people = static_cast<int>(layout[m].size());
                Require(host.Wanted() == 0, "three people should fill the match");
                host.Assign();
                for (size_t m = 0; m < layout.size(); ++m) {
                    for (size_t p = 0; p < layout[m].size(); ++p) {
                        Require(host.roster[m].players[p] == layout[m][p], "incorrect asymmetric roster");
                    }
                }
                Require(host.MachineOf(3) == -1, "phantom fourth player");
            }
            // A partial three-person roster can have two one-seat machines before it fills.
            Netplay partial(Settings(true));
            partial.needed = 3;
            partial.people = 1;
            partial.links.resize(1);
            partial.links[0].people = 1;
            partial.Assign();
            Require(partial.Player(0, 0) != partial.Player(1, 0), "the solo seat was assigned twice");
        }
        testTagMode = -1;
        TheGame().matchOwner = nullptr;
        TheGame().matchPlayers = nullptr;
    }

    static uint16_t InputAt(int machine, int frame) {
        return static_cast<uint16_t>((machine + 1) * 0x1000 + frame * 17 + 1);
    }

    static Packet EmitControls(Netplay &sender, Netplay::Link &link, UdpSocket &receiver) {
        sender.SendInputs(link);
        auto packets = Receive(receiver, 1);
        Require(packets[0][4] == kControls, "expected production controls packet");
        return std::move(packets[0]);
    }

    static void FourStreamControls() {
        Netplay host(Settings(true));
        host.needed = host.machines = 4;
        host.links.resize(3);
        host.Assign();
        UdpSocket receiver;
        std::string error;
        Require(receiver.Open(0, error) && host.socket.Open(0, error), "controls sockets: " + error);
        NetAddress destination;
        Require(UdpSocket::Resolve("127.0.0.1", receiver.LocalPort(), destination, error), "controls address: " + error);
        std::array<std::unique_ptr<Netplay>, 4> clients;
        std::array<Packet, 4> early, late;
        host.upTo[0] = 6;
        for (int frame = 0; frame < 6; frame++) {
            host.inputs[0][frame][0] = InputAt(0, frame);
        }
        for (int machine = 1; machine < 4; machine++) {
            auto &link = host.links[machine - 1];
            link.address = destination;
            link.lastHeard = NowMs();
            link.started = true;
            clients[machine] = std::make_unique<Netplay>(Settings(false));
            Netplay &client = *clients[machine];
            client.local = machine;
            client.machines = 4;
            client.roster = host.roster;
            client.links.resize(1);
            client.links[0].address = destination;
            Require(client.socket.Open(0, error), "client controls socket: " + error);
            for (int frame = 0; frame < 6; frame++) {
                client.inputs[machine][frame][0] = InputAt(machine, frame);
            }
            client.current = client.upTo[machine] = 2;
            early[machine] = EmitControls(client, client.links[0], receiver);
            // No acknowledgment arrived: the next real packet repeats all unacknowledged controls.
            client.current = client.upTo[machine] = 6;
            late[machine] = EmitControls(client, client.links[0], receiver);
        }
        const auto take = [&](int machine, const Packet &packet) {
            host.Take(host.links[machine - 1], packet.data(), packet.size());
        };
        take(1, late[1]);
        const auto accepted = host.inputs[1];
        take(1, early[1]); // reordering: an older cumulative packet arrives after the newer one.
        take(1, late[1]); // duplication must neither regress the cursor nor replace accepted history.
        Require(host.upTo[1] == 6 && host.inputs[1] == accepted, "reordering/duplication changed accepted controls");
        take(2, early[2]); // this machine's newer packet is delayed/lost.
        take(3, late[3]);
        Require(host.upTo == std::array<uint64_t, 4>{6, 6, 2, 6}, "one machine advanced another's input cursor");
        Require(host.Slowest() == 2 && host.confirmed <= host.Slowest(), "missing controls advanced the confirmation bound");
        Require(host.Ready(2 + Netplay::kMostAhead - 1), "prediction stopped before its permitted boundary");
        Require(!host.Ready(2 + Netplay::kMostAhead) && host.ended.empty(), "prediction passed a missing player's limit");
        Receive(receiver, 3); // Ready repeats controls to every link while stalled.

        // A packet from one joining machine cannot claim another machine's controls.
        const auto before = host.inputs;
        const auto cursors = host.upTo;
        take(2, late[3]);
        Require(host.inputs == before && host.upTo == cursors, "wrong-link packet changed another player's controls");
        take(2, late[2]); // the sender's cumulative retransmission repairs every missing frame.
        Require(host.Slowest() == 6 && host.Ready(2 + Netplay::kMostAhead), "late controls did not resume the stalled match");
        for (int machine = 0; machine < 4; machine++) {
            for (int frame = 0; frame < 6; frame++) {
                Require(host.inputs[machine][frame][0] == InputAt(machine, frame), "repaired input stream has a gap or wrong owner");
            }
        }
        Require(host.confirmed == 0, "packet receipt confirmed frames without running the simulation");

        // The host relays its own and both other peers' streams. The recipient keeps its local stream intact.
        const Packet relay = EmitControls(host, host.links[1], receiver);
        Netplay &recipient = *clients[2];
        recipient.Take(recipient.links[0], relay.data(), relay.size());
        Require(recipient.upTo == std::array<uint64_t, 4>{6, 6, 6, 6}, "host relay omitted a remote stream");
        for (int machine = 0; machine < 4; machine++) {
            Require(recipient.inputs[machine] == host.inputs[machine], "relay mixed distinct controller histories");
        }
        const auto relayed = recipient.inputs;
        recipient.Take(recipient.links[0], relay.data(), relay.size());
        Require(recipient.inputs == relayed && recipient.Slowest() == 6, "duplicate relay changed history or confirmation bound");
        Require(recipient.Ready(6 + Netplay::kMostAhead - 1), "recipient prediction stopped early");
        Require(!recipient.Ready(6 + Netplay::kMostAhead) && recipient.ended.empty(), "recipient exceeded prediction limit");
        Receive(receiver, 1);
    }

    static void TimingSamples() {
        Netplay peer(Settings(false));
        auto &link = peer.links.emplace_back();
        peer.machines = 2;
        peer.local = 1;
        // An older timing sample can carry a useful cumulative acknowledgement.
        auto deliver = [&](uint32_t timestamp, uint8_t ack, int ahead) {
            Writer w(kControls);
            w.U32(7); w.U8(0); // simulation frame and number of control blocks
            w.U8(2); w.U32(0); w.U32(ack);
            w.U16(ahead * 100); w.U32(0); w.U32(0);
            w.U32(timestamp); w.U32(0); w.U32(0); w.U16(40 + ahead);
            peer.Take(link, w.out.data(), w.out.size());
        };
        deliver(UINT32_MAX - 3, 1, 1);
        deliver(2, 2, 2); // wraps forward six milliseconds
        deliver(UINT32_MAX - 1, 3, 7); // old packet arriving late
        Require(link.theirAhead == 2 && link.farthest == 42 && link.echo == 2 && link.has[1] == 3,
                "late timing sample reset pacing or dropped a useful acknowledgement");
        deliver(2, 3, 7); // duplicate timestamp
        Require(link.theirAhead == 2, "duplicate timing sample changed the pacing estimate");

        // Give a later datagram a deliberately shorter simulated transit time.
        peer.settings.fakeLag = 2000;
        peer.SendTo({}, {1});
        peer.settings.fakeLag = 1;
        peer.SendTo({}, {2});
        Require(peer.late.size() == 2 && peer.late.front().data.front() == 2,
                "simulator hid reordering behind the oldest queued packet");
    }

    static void FourPeerPacing() {
        Netplay host(Settings(true)), guest(Settings(false));
        host.needed = guest.needed = host.machines = guest.machines = 4;
        host.links.resize(3);
        host.Assign();
        guest.local = 3;
        guest.links.resize(1);
        const uint64_t start = 60 * 60 * 4;
        // The host sees three guests; this guest sees only the host. Other guest links stay level.
        const auto tick = [&](Netplay &peer, uint64_t frame, double estimatedLead, int latestLead) {
            peer.upTo.fill(frame + 1);
            for (auto &link : peer.links) {
                const bool paired = link.machine == (peer.Hosting() ? 3 : 0);
                link.started = true;
                link.lastHeard = NowMs();
                const uint64_t reported = paired ? static_cast<uint64_t>(static_cast<int64_t>(frame) - latestLead) : frame;
                link.frame = std::max(link.frame, reported);
                link.ahead = paired ? estimatedLead : 0;
                link.theirAhead = -link.ahead;
            }
            if (peer.Ready(frame)) return 0;
            Require(peer.Ready(frame), "clock correction repeatedly blocked the same frame");
            return 1;
        };
        // A consistent 1.5-frame lead with bounded noise, then both level with occasional single-frame
        // bursts of noise inside the window, then the other peer ahead. Count pauses on both machines.
        const double jitter[] = {-0.20, 0.15, 0.25, -0.10, 0.05, -0.25, 0.20};
        const std::array<std::array<int, 2>, 3> expected = {{{5, 0}, {0, 0}, {0, 5}}};
        for (int phase = 0; phase < 3; ++phase) {
            std::array<int, 2> pauses{};
            for (int i = 0; i < 300; ++i) {
                const uint64_t frame = start + phase * 300 + i;
                const double by = (phase == 0 ? 1.5 : phase == 2 ? -1.5 :
                                   i % 60 == 20 ? 1.8 : i % 60 == 40 ? -1.8 : 0.0) + jitter[i % 7];
                const int latest = by > 1 ? 2 : by < -1 ? -2 : 0;
                pauses[0] += tick(host, frame, by, latest);
                pauses[1] += tick(guest, frame, -by, -latest);
            }
            std::cout << "four-peer clock window " << phase + 1 << " (300 frames): host " << pauses[0]
                      << ", guest " << pauses[1] << " pacing waits\n";
            Require(pauses == expected[phase], "a fractional lead was ignored or the two clocks chased jitter");
        }

        // Stop spending the eight-tick budget as soon as the latest peer frame catches up. A fresh
        // lead in the same second gets a new budget only at the following 60-frame boundary.
        const uint64_t recovery = start + 960;
        for (int i = 0; i <= 60; ++i) {
            const int latest = i < 3 || i >= 8 ? 8 : 0;
            const int waited = tick(host, recovery + i, 8, latest);
            Require(waited == (i < 3 || i == 60), "fixed budget failed to stop at catch-up or refilled too soon");
        }
        // Prediction uses the actual remote input, independently of the softer clock correction.
        const uint64_t boundary = recovery + 61;
        host.upTo.fill(boundary + 1);
        host.upTo[3] = boundary - Netplay::kMostAhead;
        Require(!host.Ready(boundary) && host.predictionWaits == 1, "timing bypassed the hard prediction limit");
        ++host.upTo[3];
        host.links[2].frame = boundary;
        Require(host.Ready(boundary), "new real input failed to release the prediction stall");
    }

    static void TopHudNames() {
        using Labels = std::vector<std::pair<std::string, bool>>;
        using namespace recomp::text;
        const Labels names{{"P1 ABCDEFGHIJKLMNOP", true}, {"P3 ÉéÇçÑßÉéÇçÑßÉéÇç", false}};
        for (int width : {224, 384, 512}) {
            for (int scorePixels : {0, 24, 36}) { // half-width of the reserved casual / FT5 / FT10 strip
                for (int wins : {0, 2, 125, 1234567890}) {
                    Labels row = names;
                    if (wins) row.push_back({"STREAK " + std::to_string(wins), true});
                    const int budget = width / 2 - scorePixels - 14;
                    const int gap = FitHudNames(row, wins, budget);
                    int drawn = (static_cast<int>(row.size()) - 1) * gap;
                    for (const auto &part : row) drawn += static_cast<int>(TextWidth(part.first.c_str(), 1));
                    Require(drawn <= budget, "top name panel covers the other team or the first-to score");
                    for (size_t i = 0; i < names.size(); ++i) {
                        const auto &original = names[i].first;
                        const auto &shown = row[i].first;
                        const size_t chars = Count(shown.data(), shown.size());
                        Require(shown.starts_with(original.substr(0, 2)) && original.starts_with(shown) &&
                                Bytes(original.data(), original.size(), chars) == shown.size() && row[i].second == names[i].second,
                                "top name lost the controller number, a UTF-8 character, or the point indicator");
                    }
                }
            }
        }
        Labels shortNames{{"P1 AB", true}, {"P3 CD", false}, {"STREAK 2", true}};
        const auto original = shortNames;
        Require(FitHudNames(shortNames, 2, 180) == 2 * kCharWidth && shortNames == original,
                "a short row lost its original labels or spacing");
    }

    static void WindowStallInputs() {
        using Pads = OnlinePadQueue::Pads;
        Require(SDL_Init(SDL_INIT_GAMEPAD), "SDL gamepad init");
        SDL_VirtualJoystickDesc desc{};
        SDL_INIT_INTERFACE(&desc);
        desc.name = "online input sample";
        desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
        desc.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
        desc.naxes = SDL_GAMEPAD_AXIS_COUNT;
        std::array<SDL_JoystickID, 2> ids{};
        std::array<SDL_Gamepad *, 2> physical{};
        for (size_t i = 0; i < ids.size(); ++i) {
            ids[i] = SDL_AttachVirtualJoystick(&desc);
            physical[i] = SDL_OpenGamepad(ids[i]);
            Require(physical[i], "open virtual pad");
        }
        Netplay peer(Settings(false));
        peer.local = 1;
        auto &host = peer.links.emplace_back();
        host.started = true;
        uint64_t frame = 200;
        OnlinePadQueue queue;
        const auto poll = [&](Pads held, uint64_t ms, bool stalled, Pads expected) {
            Pads mapped{};
            for (size_t i = 0; i < physical.size(); ++i) {
                SDL_Joystick *joy = SDL_GetGamepadJoystick(physical[i]);
                Require(SDL_SetJoystickVirtualButton(joy, SDL_GAMEPAD_BUTTON_DPAD_LEFT, held[i] & kLeft), "left");
                Require(SDL_SetJoystickVirtualButton(joy, SDL_GAMEPAD_BUTTON_SOUTH, held[i] & kPunch1), "south");
                Require(SDL_SetJoystickVirtualButton(joy, SDL_GAMEPAD_BUTTON_EAST, held[i] & kPunch2), "east");
            }
            SDL_UpdateGamepads();
            for (size_t i = 0; i < physical.size(); ++i) {
                if (SDL_GetGamepadButton(physical[i], SDL_GAMEPAD_BUTTON_DPAD_LEFT)) mapped[i] |= kLeft;
                if (SDL_GetGamepadButton(physical[i], SDL_GAMEPAD_BUTTON_SOUTH)) mapped[i] |= kPunch1;
                if (SDL_GetGamepadButton(physical[i], SDL_GAMEPAD_BUTTON_EAST)) mapped[i] |= kPunch2;
            }
            Require(mapped == held, "SDL lost a sample before the pacing test");
            auto supplied = queue.Poll(mapped, ms);
            host.lastHeard = NowMs();
            peer.upTo[0] = stalled ? frame - Netplay::kMostAhead : frame;
            Require(peer.Ready(frame) == !stalled, "unexpected prediction gate");
            if (!stalled) {
                Require(supplied == expected, "lost, combined, or transferred a stalled owner's input");
                queue.Accepted();
                ++frame;
            }
        };
        poll({}, 0, false, {});
        poll({kPunch1, kPunch2}, 10, true, {});
        poll({0, kPunch1 | kPunch2}, 20, true, {});
        poll({kPunch2, 0}, 30, false, {kPunch1, kPunch2});
        poll({kPunch2, 0}, 47, false, {0, kPunch1 | kPunch2});
        poll({kPunch2, 0}, 64, false, {kPunch2, 0});
        poll({}, 81, false, {});

        // Hold back throughout two separate LP taps. Count all four press/release transitions
        // across the wait and let unchanged polls drain them one game frame at a time.
        const Pads back{kLeft, 0}, jab{kLeft | kPunch1, 0};
        poll(back, 82, false, back);
        poll(jab, 90, true, {});
        poll(back, 98, true, {});
        poll(jab, 106, true, {});
        poll(back, 114, true, {});
        poll(back, 121, false, jab);
        poll(back, 138, false, back);
        poll(back, 155, false, jab);
        poll(back, 172, false, back);
        poll(back, 189, false, back);

        // MP starts while LP is still down. Only the sampled overlap forms a chord. The
        // guest's local second controller holds its own attack independently of seat one.
        const Pads lp{kPunch1, kPunch2}, chord{kPunch1 | kPunch2, kPunch2}, mp{kPunch2, kPunch2};
        poll(lp, 200, true, {});
        poll(chord, 205, true, {});
        poll(mp, 210, false, lp);
        poll(mp, 224, false, chord);
        poll(mp, 241, false, mp);
        poll({}, 258, false, {});
        for (size_t i = 0; i < ids.size(); ++i) {
            SDL_CloseGamepad(physical[i]);
            SDL_DetachVirtualJoystick(ids[i]);
        }
        SDL_Quit();

        queue.Clear();
        queue.Poll({kLeft, kPunch1}, 100);
        queue.Poll({kRight, kPunch1}, 101);
        queue.Poll({kRight, kPunch2}, 102);
        queue.ReleaseSeat(1); // local seat 0's two directions survive loss of its teammate
        Require(queue.Poll({kRight, 0}, 103) == Pads{kLeft, 0}, "disconnect changed another seat's input");
        queue.Accepted();
        Require(queue.Poll({kRight, 0}, 104) == Pads{kRight, 0}, "opposite directions were combined");
        queue.Clear(); // reconnect/menu boundary: no old directional edge
        Require(queue.Poll({}, 105) == Pads{}, "controls survived session reset");

        queue.Poll({kPunch1, 0}, 110);
        Require(queue.Poll({}, 210) == Pads{}, "expired tap survived a prolonged network wait");
        queue.Poll({kPunch1, 0}, 211);
        Require(queue.Poll({0, kPunch2}, 212) == Pads{0, kPunch2}, "expired queue did not stay live until resume");
        queue.Accepted();
        for (int i = 0; i < 4; ++i) queue.Poll({static_cast<uint16_t>(i + 1), 0}, 220 + i);
        Require(queue.Poll({}, 224) == Pads{}, "overflow replayed old controls or lost a release");
        Require(queue.Poll({0, kPunch1}, 225) == Pads{0, kPunch1}, "overflow did not respect current controls");
        queue.Accepted();
    }

    static void StalledRetries() {
        Netplay host(Settings(true));
        host.needed = host.machines = 4;
        host.links.resize(3);
        host.Assign();
        host.settings.fakeLag = 4000; // hold the real outgoing packets for inspection
        // A 320 ms input gap, polled each millisecond. Advancing the wall clock alone should resend
        // periodically. It must not generate one duplicate per poll to each of the three guests.
        for (uint64_t ms = 0; ms < 320; ++ms) host.SendInputs(ms);
        Require(host.late.size() == 30, "stalled input retries scaled with poll rate");
        host.upTo[2] = 1; // new input relayed from guest 2 while guest 3 still stalls the match
        host.inputs[2][0][0] = 0x12;
        host.SendInputs(319);
        Require(host.late.size() == 33, "fresh relayed data was suppressed during a prediction stall");
        host.upTo[1] = 1;
        host.SendInputs(321); // fresh cumulative acknowledgement, no simulation frame elapsed
        Require(host.late.size() == 36, "newly received stream waited for the duplicate retry timer");
        host.SendInputs(322);
        Require(host.late.size() == 36, "repeated input sent before the retry deadline");
        host.current = 1;
        host.SendInputs(323);
        Require(host.late.size() == 39, "simulation frame advance waited for the retry deadline");
        host.late.clear();
    }

    static void FirstToConsensus() {
        const auto final = [](Netplay &match) {
            match.set.Load({5, 0, 5, 2, 0, 0, 0, 0, 1, 5000});
            for (auto &link : match.links) {
                link.lastHeard = NowMs();
                link.started = true;
            }
        };
        Writer done(kSetDone);
        done.U8(5); done.U8(5); done.U8(2); done.U8(0);
        Netplay host(Settings(true));
        host.needed = 4;
        host.links.resize(3);
        host.Assign();
        final(host);
        host.Take(host.links[0], done.out.data(), done.out.size());
        Require(!host.SetAgrees(), "one peer completed a four-machine set");
        host.Take(host.links[1], done.out.data(), done.out.size());
        Require(!host.SetAgrees(), "a missing final-score peer was ignored");
        host.Take(host.links[2], done.out.data(), done.out.size());
        host.viewers.resize(2); // viewers never vote on a player set
        Require(host.SetAgrees(), "all player acknowledgments failed to complete the set");
        host.setAgreed = NowMs() - 2001;
        Require(!host.Ready(5001) && host.SetFinished() && !host.failed && !host.lost && !host.left,
                "agreed host set failed to finish normally");
        Require(host.ended == "Left team wins the FT5 set (5-2).", "winning team was reported as a single player");

        Netplay client(Settings(false));
        client.links.resize(1);
        final(client);
        client.Take(client.links[0], done.out.data(), done.out.size());
        Require(!client.SetAgrees(), "client finished before the host heard all players");
        done.out.back() = 1;
        client.Take(client.links[0], done.out.data(), done.out.size());
        Require(client.SetAgrees(), "host consensus did not release the client");
        done.out.back() = 0; // an earlier host packet can arrive after the ready packet, even just before goodbye
        client.Take(client.links[0], done.out.data(), done.out.size());
        client.Take(client.links[0], done.out.data(), done.out.size());
        Require(client.SetAgrees(), "reordered pre-consensus packet revoked established host agreement");
        Writer bye(kBye);
        client.Take(client.links[0], bye.out.data(), bye.out.size());
        Require(client.SetFinished() && !client.lost && !client.failed, "host departure masked an agreed set result");

        for (int people : {2, 3, 4}) {
            Netplay right(Settings(false));
            right.needed = people;
            right.set.Load({5, 1, 2, 5, 0, 0, 0, 0, 1, 5000});
            right.FinishSet();
            const std::string expected = people == 2 ? "Player 2" : "Right team";
            Require(right.ended == expected + " wins the FT5 set (2-5).", "right-hand winner was attributed to the wrong player");
        }

        // Agreement belongs to this link's exact score and this Netplay session, not any later result.
        Netplay changed(Settings(false));
        changed.links.resize(1);
        final(changed);
        done.out.back() = 1;
        changed.Take(changed.links[0], done.out.data(), done.out.size());
        Require(changed.SetAgrees(), "fresh client did not accept host consensus");
        auto wrongTarget = done.out;
        wrongTarget[5] = wrongTarget[6] = 10;
        changed.Take(changed.links[0], wrongTarget.data(), wrongTarget.size());
        Require(changed.SetAgrees() && changed.links[0].setTarget == 5,
                "a different set target replaced the agreed result");
        done.out[7] = 3;
        done.out.back() = 0;
        changed.Take(changed.links[0], done.out.data(), done.out.size());
        Require(!changed.SetAgrees() && !changed.links[0].setReady,
                "a changed final score inherited another result's agreement");
        done.out[7] = 2;
        changed.Take(changed.links[0], done.out.data(), done.out.size());
        Require(!changed.SetAgrees(), "an unready replacement score revived old agreement");
        Netplay restarted(Settings(false));
        restarted.links.resize(1);
        final(restarted);
        restarted.Take(restarted.links[0], done.out.data(), done.out.size());
        Require(!restarted.SetAgrees(), "a new session inherited a completed session's agreement");

        Netplay mismatch(Settings(true));
        mismatch.links.resize(3);
        mismatch.needed = 4;
        mismatch.Assign();
        final(mismatch);
        done.out[7] = 3; // a different losing score, still a syntactically valid completed set
        mismatch.Take(mismatch.links[2], done.out.data(), done.out.size());
        Require(!mismatch.Ready(5001) && mismatch.failed && !mismatch.SetFinished() &&
                mismatch.ended == "The games disagree on the final set score.", "disagreed set score was accepted");
    }

    static void ModePickOrder() {
        OnlinePickHistory host(1), guest(2);
        const OnlinePickHistory::Position mode{60, 0, 12};
        host.Record({60, 0, 0}, 3);
        Require(host.Before(mode) == 3 && guest.Before(mode) == 2, "mode missed a preceding pick");
        host.Record({60, 0, 22}, 9);
        guest.Record({60, 1, 0}, 7);
        guest.Record({61, 1, 0}, 5);
        Require(host.Before(mode) == 3 && guest.Before(mode) == 2,
                "replaying mode consumed a later command or a later sender in the same frame");
        Require(host.Before({60, 0, 0}) == 1 && guest.Before({61, 0, 0}) == 7 &&
                    host.Before({62, 0, 0}) == 9 && guest.Before({62, 0, 0}) == 5,
                "mode at another boundary restored the wrong fighter");
    }

    static void ModeEvents() {
        Netplay host(Settings(true)), client(Settings(false));
        host.links.resize(1);
        host.Assign();
        client.local = 1;
        client.roster = host.roster;
        client.links.resize(1);
        UdpSocket receiver;
        std::string error;
        Require(receiver.Open(0, error) && host.socket.Open(0, error) && client.socket.Open(0, error), "event sockets: " + error);
        NetAddress address;
        Require(UdpSocket::Resolve("127.0.0.1", receiver.LocalPort(), address, error), "event address: " + error);
        host.links[0].address = client.links[0].address = address;
        client.upTo[1] = 3;
        client.current = 3;
        client.events[1][1] = "mode,2";
        const auto packet = EmitControls(client, client.links[0], receiver);
        host.Take(host.links[0], packet.data(), packet.size());
        Require(host.upTo[1] == 3 && host.events[1][1] == "mode,2" && host.watchModeChanged,
                "controls acknowledged without their mode event");
        host.Take(host.links[0], packet.data(), packet.size());
        const auto notices = host.TakeEvents();
        Require(notices.size() == 1 && notices[0] == std::make_pair(1, std::string("mode,2")),
                "retransmission duplicated an event or changed its machine owner");
        host.confirmed = 3;
        host.upTo[0] = 3;
        host.events[0][1] = "mode,1";
        host.PruneEvents();
        Require(host.events[0].count(1) == 1, "local confirmation discarded an unacknowledged event");
        const auto retry = EmitControls(host, host.links[0], receiver);
        client.Take(client.links[0], retry.data(), retry.size());
        Require(client.events[0][1] == "mode,1" && client.upTo[0] == 3,
                "unacknowledged event was missing from retransmitted controls");
        host.links[0].has[0] = 3;
        host.PruneEvents();
        Require(host.events[0].empty(), "acknowledged confirmed event was not retired");

        Netplay four(Settings(true));
        four.needed = four.machines = 4;
        four.links.resize(3);
        four.Assign();
        four.Command("mode,2");
        Require(four.commanded.empty(), "four-human roster accepted a two-human mode switch");
        four.Take(four.links[0], packet.data(), packet.size());
        Require(four.upTo[1] == 0 && four.events[1].empty(), "four-human match accepted a remote mode event");

        Netplay gap(Settings(true));
        gap.links.resize(1);
        gap.Assign();
        auto skipped = packet;
        skipped[11] = 1; // first frame becomes 1, leaving frame 0 absent
        gap.Take(gap.links[0], skipped.data(), skipped.size());
        Require(gap.upTo[1] == 0 && gap.events[1].empty(), "a later block skipped missing input/event history");
        gap.Take(gap.links[0], packet.data(), packet.size());
        Require(gap.upTo[1] == 3 && gap.events[1][1] == "mode,2", "cumulative input did not repair the missing prefix");
    }

    // The relay's TURN client (port/turn.h): its digests against their standards' examples, the addresses it keeps off
    // the relay, and an allocation against a scripted server: the first request unsigned, the 401's realm and nonce,
    // the request again signed with MD5(user:realm:password), the relayed address, a permission, a peer's packet
    static void TurnClient() {
        const auto hex = [](const uint8_t *data, size_t size) {
            static const char *digits = "0123456789abcdef";
            std::string out;
            for (size_t i = 0; i < size; ++i) out += digits[data[i] >> 4], out += digits[data[i] & 15];
            return out;
        };
        const auto md5 = turn::Md5("abc");
        Require(hex(md5.data(), 16) == "900150983cd24fb0" "d6963f7d28e17f72", "MD5 of abc");
        const std::string jefe = "Jefe", what = "what do ya want for nothing?";
        const auto mac = turn::HmacSha1({jefe.begin(), jefe.end()}, {what.begin(), what.end()});
        Require(hex(mac.data(), 20) == "effcdf6ae5eb2fa2d274" "16d5f184df9c259a7c79", "HMAC-SHA1 (RFC 2202, case 2)");
        const std::string nine = "123456789";
        Require(turn::Crc32(reinterpret_cast<const uint8_t *>(nine.data()), 9) == 0xCBF43926u, "CRC-32 of 123456789");
        const uint32_t cloudflare = 0x8D654B01; // (141.101.75.1: a relay on the internet)
        Require(turn::Unrelayable(0xC0A80105, cloudflare) && turn::Unrelayable(0x0A000001, cloudflare) &&
                    turn::Unrelayable(0x7F000001, cloudflare) && turn::Unrelayable(0x64400001, cloudflare) &&
                    !turn::Unrelayable(0xCB007107, cloudflare),
                "private addresses kept off an internet relay, public ones not");
        Require(!turn::Unrelayable(0xC0A80122, 0xC0A80105) && turn::Unrelayable(0x0A000001, 0xC0A80105),
                "a test relay on a private network reaches only that network");

        turn::Client client;
        client.Start("user", "pass", 1000, 7);
        auto out = client.Tick(1000);
        turn::Parsed first;
        Require(out.size() == 1 && turn::Parse(out[0].data(), out[0].size(), first) && first.type == turn::kAllocate &&
                    first.Find(turn::kTransport) && !first.Find(turn::kIntegrity) && first.Find(turn::kFingerprint),
                "the first Allocate was not one unsigned request with its transport and fingerprint");
        const auto answer = [](uint16_t type, const std::array<uint8_t, 12> &id) { return turn::Writer(type, id); };
        turn::Writer challenge = answer(turn::kAllocateError, first.id);
        const uint8_t unauthorized[4] = {0, 0, 4, 1};
        challenge.Attribute(turn::kErrorCode, unauthorized, 4);
        challenge.Text(turn::kRealm, "example.org");
        challenge.Text(turn::kNonce, "n0nce");
        std::vector<uint8_t> payload;
        uint32_t ip = 0;
        int port = 0;
        Require(client.Take(challenge.out.data(), challenge.out.size(), payload, ip, port, 1010) == turn::Client::Took::Ours,
                "the 401 was not taken");
        out = client.Tick(1010);
        turn::Parsed signedAsk;
        Require(out.size() == 1 && turn::Parse(out[0].data(), out[0].size(), signedAsk) && signedAsk.type == turn::kAllocate &&
                    signedAsk.Text(turn::kUsername) == "user" && signedAsk.Text(turn::kRealm) == "example.org" &&
                    signedAsk.Text(turn::kNonce) == "n0nce",
                "the Allocate again did not carry the username, realm and nonce");
        // (its integrity: the HMAC, by MD5(user:realm:pass), of the message up to it with the length counting it)
        const turn::Parsed::Item *integrity = signedAsk.Find(turn::kIntegrity);
        Require(integrity && integrity->size == 20, "the Allocate again was not signed");
        const size_t upTo = static_cast<size_t>(integrity->value - 4 - out[0].data());
        std::vector<uint8_t> covered(out[0].begin(), out[0].begin() + static_cast<long>(upTo));
        covered[2] = static_cast<uint8_t>((upTo - 20 + 24) >> 8), covered[3] = static_cast<uint8_t>(upTo - 20 + 24);
        const auto key = turn::Md5("user:example.org:pass");
        const auto expected = turn::HmacSha1({key.begin(), key.end()}, covered);
        Require(memcmp(expected.data(), integrity->value, 20) == 0, "the Allocate's MESSAGE-INTEGRITY was wrong");
        turn::Writer granted = answer(turn::kAllocateOk, signedAsk.id);
        granted.Address(turn::kRelayed, 0xCB007107, 50123);
        Require(client.Take(granted.out.data(), granted.out.size(), payload, ip, port, 1020) == turn::Client::Took::Ours &&
                    client.Ready() && client.RelayIp() == 0xCB007107 && client.RelayPort() == 50123,
                "the relayed address was not taken");
        client.Permit(0xC6336401); // (198.51.100.1)
        client.Permit(0xC0A80105); // (a LAN's: not asked for)
        out = client.Tick(1030);
        turn::Parsed permission;
        Require(out.size() == 1 && turn::Parse(out[0].data(), out[0].size(), permission) && permission.type == turn::kPermission &&
                    std::count_if(permission.items.begin(), permission.items.end(),
                                  [](const turn::Parsed::Item &item) { return item.type == turn::kPeer; }) == 1,
                "the CreatePermission did not ask for the public peer alone");
        turn::Writer data = answer(turn::kData, {});
        data.Address(turn::kPeer, 0xC6336401, 7845);
        const uint8_t game[3] = {'C', 'P', 'S'};
        data.Attribute(turn::kDataAttr, game, 3);
        Require(client.Take(data.out.data(), data.out.size(), payload, ip, port, 1040) == turn::Client::Took::Data && ip == 0xC6336401 &&
                    port == 7845 && payload == std::vector<uint8_t>(game, game + 3),
                "a peer's packet through the relay was not given out");
        const auto wrapped = client.Wrap(0xC6336401, 7845, game, 3);
        turn::Parsed sent;
        Require(turn::Parse(wrapped.data(), wrapped.size(), sent) && sent.type == turn::kSend && sent.Address(turn::kPeer, ip, port) &&
                    ip == 0xC6336401 && sent.Find(turn::kDataAttr) && sent.Find(turn::kDataAttr)->size == 3,
                "a packet for a peer was not a Send indication");
        // (a request unanswered: sent again, and after 5 seconds the relay is given up)
        turn::Client silent;
        silent.Start("user", "pass", 0, 9);
        Require(silent.Tick(0).size() == 1 && silent.Tick(600).size() == 1 && !silent.Failed(), "an unanswered request was not sent again");
        silent.Tick(5600);
        Require(silent.Failed(), "a server that never answered was not given up");
    }

    // A strict network (a symmetric or carrier-grade NAT): the two STUN servers see this socket at two ports; one that
    // keeps one address shows the same to both, and an answer from neither server, or for another request, is not counted
    static void StrictNetwork() {
        const auto answer = [](const std::array<uint8_t, 12> &id, uint32_t ip, int port) {
            std::vector<uint8_t> p = {0x01, 0x01, 0x00, 0x0C, 0x21, 0x12, 0xA4, 0x42};
            p.insert(p.end(), id.begin(), id.end());
            const int xport = port ^ 0x2112;
            const uint32_t xip = ip ^ 0x2112A442u;
            for (int b : {0x00, 0x20, 0x00, 0x08, 0x00, 0x01, xport >> 8, xport & 0xFF, static_cast<int>(xip >> 24),
                          static_cast<int>((xip >> 16) & 0xFF), static_cast<int>((xip >> 8) & 0xFF), static_cast<int>(xip & 0xFF)}) {
                p.push_back(static_cast<uint8_t>(b));
            }
            return p;
        };
        for (const bool strict : {true, false}) {
            Netplay net(Settings(false));
            NetAddress one, two, other;
            std::string error;
            Require(UdpSocket::Resolve("127.0.0.1", 3478, one, error) && UdpSocket::Resolve("127.0.0.2", 19302, two, error) &&
                        UdpSocket::Resolve("127.0.0.3", 3478, other, error),
                    "stun addresses: " + error);
            net.stunServers = {one, two};
            net.stunId = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
            const uint32_t ip = 0xCB007107; // (203.0.113.7)
            Require(net.StrictNetwork() < 0, "a network was judged before STUN answered");
            auto first = answer(net.stunId, ip, 40000);
            Require(net.TakeStun(first.data(), static_cast<int>(first.size()), one) && net.seenAs == "203.0.113.7:40000",
                    "the first STUN answer was not read");
            auto stray = answer(net.stunId, ip, 50000);
            net.TakeStun(stray.data(), static_cast<int>(stray.size()), other);
            auto stale = answer({}, ip, 50000);
            net.TakeStun(stale.data(), static_cast<int>(stale.size()), two);
            Require(net.StrictNetwork() < 0, "an answer from no server, or for another request, judged the network");
            auto second = answer(net.stunId, ip, strict ? 40001 : 40000);
            net.TakeStun(second.data(), static_cast<int>(second.size()), two);
            Require(net.StrictNetwork() == (strict ? 1 : 0), strict ? "a strict network was not seen" : "a network keeping one address was called strict");
            Require(net.seenAs == "203.0.113.7:40000", "the address told the others changed with the second answer");
        }
    }

    // Issue #448: the host moves the people between the sides in a match of four, as an event on its own stream, and
    // every machine plays the frames from the event's on with the new seats; nobody else can, and a malformed change is
    // nothing
    static void SeatChanges() {
        Netplay host(Settings(true));
        host.needed = host.machines = 4;
        host.links.resize(3);
        host.Assign();
        Require(host.CanSeat(), "a four-person host cannot change the seats");
        Netplay::Layout swapped = host.SeatLayout();
        for (int m = 0; m < 4; ++m) swapped[m][0] ^= 1; // (players 1<->2, 3<->4: the sides swapped)
        host.Seat(swapped);
        Require(host.commanded.size() == 1 && host.commanded[0] == "seats=1.0.3.2", "the host's swap was not one seat event");
        host.commanded.clear();
        host.Seat(host.SeatLayout());
        Require(host.commanded.empty(), "a change to the seats as they are was sent");
        host.Command("mode=1:3");
        Require(host.commanded.empty(), "a four-person match took a two-person mode event");
        Netplay::Layout parsed{};
        for (const char *bad : {"seats=1.0.3", "seats=1.1.3.2", "seats=1.0.3.4", "seats=10.3.2", "seats=1.0.3.2.", "seats="}) {
            Require(!host.SeatEvent(bad, parsed), std::string("a malformed seat change was taken: ") + bad);
        }
        Require(host.SeatEvent("ready=1,seats=2.3.0.1", parsed) && parsed[0][0] == 2 && parsed[3][0] == 1,
                "a seat change after another item was not read");

        // The host's event reaches a joining machine with its controls, frame and all; a frame already run goes back
        UdpSocket receiver;
        std::string error;
        Require(receiver.Open(0, error) && host.socket.Open(0, error), "seat sockets: " + error);
        NetAddress destination;
        Require(UdpSocket::Resolve("127.0.0.1", receiver.LocalPort(), destination, error), "seat address: " + error);
        for (auto &link : host.links) link.address = destination, link.lastHeard = NowMs(), link.started = true;
        host.upTo[0] = 8;
        host.events[0][5] = "seats=1.0.3.2";
        host.NoteSeats(0, 5, host.events[0][5]);
        Netplay client(Settings(false));
        client.local = 2;
        client.needed = client.machines = 4;
        client.roster = host.roster;
        client.links.resize(1);
        client.links[0].address = destination;
        for (int m = 0; m < 4; ++m) client.seatBase[m] = client.roster[m].players;
        client.seatBaseSet = true;
        client.current = 7;
        Require(!client.CanSeat(), "a joining machine can change the seats");
        const Packet packet = EmitControls(host, host.links[1], receiver);
        client.Take(client.links[0], packet.data(), packet.size());
        Require(client.seatChanges.size() == 1 && client.seatChanges.count(5), "the seat change did not come with its frame");
        Require(client.SeatsAt(4)[0][0] == 0 && client.SeatsAt(5)[0][0] == 1 && client.SeatsAt(9)[2][0] == 3,
                "the frames before and after the change were not played with their own seats");
        Require(client.eventFrom == 5, "a seat change for a frame already run did not roll back to it");
        Require(client.TakeEvents().size() == 1, "the seat change was not said on the screen");

        // A joining machine's own seat event, or any other event in a match of four, is refused with its packet
        Netplay other(Settings(true));
        other.needed = other.machines = 4;
        other.links.resize(3);
        other.Assign();
        client.events[2][6] = "seats=0.1.2.3";
        client.inputs[2][6][0] = 1;
        client.upTo[2] = 7;
        Require(client.socket.Open(0, error), "seat reply socket: " + error);
        const Packet theirs = EmitControls(client, client.links[0], receiver);
        other.Take(other.links[1], theirs.data(), theirs.size());
        Require(other.upTo[2] == 0 && other.seatChanges.empty(), "a joining machine's seat change was taken");

        // A match of two: the host swaps the sides (Swap sides), its modes' events still go; the other player cannot
        Netplay duo(Settings(true));
        duo.links.resize(1);
        duo.Assign();
        Require(duo.needed == 2 && duo.CanSeat(), "a host of two cannot swap the sides");
        Netplay::Layout sides = duo.SeatLayout();
        std::swap(sides[0][0], sides[1][0]);
        duo.Seat(sides);
        Require(duo.commanded.size() == 1 && duo.commanded[0] == "seats=1.0", "the host's swap of two was not one seat event");
        duo.Command("mode=1:3");
        Require(duo.commanded.size() == 2, "a match of two lost its mode events");
        for (const char *bad : {"seats=0.0", "seats=1", "seats=2.0", "seats=1.0.2"}) {
            Require(!duo.SeatEvent(bad, parsed), std::string("a malformed seat change of two was taken: ") + bad);
        }
        duo.NoteSeats(0, 9, "court_one=2,seats=1.0");
        Require(duo.SeatsAt(8)[0][0] == 0 && duo.SeatsAt(9)[0][0] == 1 && duo.SeatsAt(9)[1][0] == 0,
                "a swap of two was not played from its frame");
        duo.NoteSeats(1, 10, "seats=0.1");
        Require(duo.seatChanges.size() == 1, "the other player's seat change was taken");
        Netplay guest(Settings(false));
        guest.needed = guest.machines = 2;
        guest.local = 1;
        Require(!guest.CanSeat(), "the other player of two can swap the sides");
        guest.Command("seats=1.0");
        Require(guest.commanded.empty(), "the other player of two sent a seat change");

        // A match of three (2 vs 1, 1 vs 2) at two machines, a pair at the host's: the host moves who plays alone
        Netplay trio(Settings(true));
        trio.needed = 3;
        trio.people = 2;
        trio.links.resize(1);
        trio.links[0].people = 1;
        trio.Assign();
        Require(trio.CanSeat(), "a host of three cannot change the seats");
        Netplay::Layout moved = trio.SeatLayout();
        std::swap(moved[0][0], moved[1][0]); // (the host's first person and the other machine's change places)
        trio.Seat(moved);
        Require(trio.commanded.size() == 1 && trio.SeatEvent(trio.commanded[0], parsed) && parsed == moved,
                "the host's change of three was not one seat event");
        Require(!trio.SeatEvent("seats=03.1", parsed) && !trio.SeatEvent("seats=0.1.2", parsed),
                "a malformed seat change of three was taken");
        trio.seatBase = trio.SeatLayout();
        trio.NoteSeats(0, 4, trio.commanded[0]);
        Require(trio.SeatsAt(3) == trio.SeatLayout() && trio.SeatsAt(4) == moved, "a change of three was not played from its frame");

        // The set's score starts again, keeping its format
        FirstTo set;
        set.Begin(1);
        set.Restart();
        Require(set.Target() == 3 && set.Counting() && set.Score(0) == 0 && set.Winner() < 0, "a restarted set lost its format");
    }

    static void SpectatorControls() {
        Netplay host(Settings(true));
        UdpSocket receiver;
        ConnectReceiver(host, receiver);
        host.needed = host.machines = 4;
        host.viewers.resize(1);
        auto &viewer = host.viewers[0];
        viewer.address = host.links[0].address;
        viewer.token = "view-only";
        viewer.welcome = {1};
        viewer.state = {1};
        viewer.pieces = 1;
        viewer.start = viewer.ack = 10000;
        viewer.heard = NowMs();
        host.confirmed = 10005;
        for (uint32_t frame = viewer.start; frame < host.confirmed; ++frame) {
            auto &input = host.watchInputs[frame % Netplay::kWatchFrames];
            for (int player = 0; player < 4; ++player) input.players[player] = InputAt(player, frame - viewer.start);
            input.hash = frame * 7;
        }
        host.WatchSend();
        const auto packet = Receive(receiver, 1)[0];
        Require(packet[4] == kWatchControls, "spectator did not receive confirmed inputs");
        auto settings = Settings(false);
        settings.watching = true;
        Netplay watch(settings);
        watch.links.resize(1);
        watch.needed = watch.machines = 4;
        watch.current = watch.watchUpTo = 10000;
        watch.watchToken = "wrong";
        watch.Take(watch.links[0], packet.data(), packet.size());
        Require(watch.watchUpTo == 10000, "spectator accepted another viewer's input stream");
        watch.watchToken = viewer.token;
        watch.Take(watch.links[0], packet.data(), packet.size());
        Require(watch.watchUpTo == host.confirmed, "late spectator did not advance to confirmed frame");
        for (uint32_t frame = viewer.start; frame < host.confirmed; ++frame) {
            const auto &actual = watch.watchInputs[frame % Netplay::kWatchFrames];
            const auto &expected = host.watchInputs[frame % Netplay::kWatchFrames];
            Require(actual.players == expected.players && actual.hash == expected.hash,
                    "spectator dropped or swapped a controller stream");
        }
        watch.Command("mode,2");
        Require(watch.commanded.empty(), "viewer entered player controls");
        // (a spectator's chat goes to the host only, marked a spectator's, under its name)
        watch.settings.name = "Watcher";
        watch.Say("hello from the seats");
        Require(watch.links[0].unheard.size() == 1 && watch.links[0].unheard[0].from == Netplay::kSpectator &&
                    watch.links[0].unheard[0].name == "Watcher",
                "viewer's chat line not queued to the host as a spectator's");
        Writer controls(kControls);
        watch.Take(watch.links[0], controls.out.data(), controls.out.size());
        Require(watch.upTo == std::array<uint64_t, 4>{}, "viewer consumed a player input packet");
        const auto cursors = host.upTo;
        Writer ack(kWatchAck);
        ack.Text(viewer.token); ack.U32(1); ack.U32(10005);
        host.WatchPacket(ack.out.data(), ack.out.size(), viewer.address);
        Require(host.viewers[0].ack == 10005 && host.upTo == cursors && host.confirmed == 10005,
                "viewer acknowledgment changed player confirmation");
        host.WatchPacket(controls.out.data(), controls.out.size(), viewer.address);
        Require(host.upTo == cursors, "viewer packet injected player controls");
    }

    static void KeyboardShortcuts() {
        using Shortcut = KeyboardInput::Shortcut;
        using Pads = OnlinePadQueue::Pads;
        const BoundTable &table = MakeBoundTable(2, "", true);
        Bindings bindings = recomp::DefaultBindings(table.table);
        recomp::ApplyBinding(bindings, table.table, "keyboard1", "mp", "Left Alt");
        recomp::ApplyBinding(bindings, table.table, "keyboard2", "lp", "B");
        KeyboardInput input(bindings, table);
        std::array<bool, SDL_SCANCODE_COUNT> keys{};
        const auto sample = [&](bool captured = false) {
            const bool *polled = input.Poll(keys.data(), keys.size());
            return Pads{input.Read(polled, 0, false, captured), input.Read(polled, 1, false, captured)};
        };
        OnlinePadQueue queue;
        keys[SDL_SCANCODE_B] = true;
        keys[SDL_SCANCODE_LALT] = true;
        Require(sample() == Pads{kPunch2, kPunch1}, "bound Alt masked a keyboard seat");
        Require(sample(true) == Pads{}, "captured keyboard sent gameplay");
        queue.Poll(sample(), 10);
        keys[SDL_SCANCODE_LALT] = false;
        Require(queue.Poll(sample(), 20) == Pads{kPunch2, kPunch1}, "Alt press lost during a wait");
        queue.Accepted();
        Require(queue.Poll(sample(), 25) == Pads{0, kPunch1}, "Alt release lost during a wait");

        keys[SDL_SCANCODE_RALT] = keys[SDL_SCANCODE_RETURN] = true;
        Require(input.KeyDown(SDL_SCANCODE_RETURN, SDL_KMOD_RALT, false) == Shortcut::Fullscreen &&
                sample() == Pads{0, kPunch1}, "unbound Alt+Enter leaked Start or masked the second seat");
        keys[SDL_SCANCODE_RALT] = false;
        auto menu = [&]() { return recomp::sdl::MenuHeld(input.Poll(keys.data(), keys.size()), nullptr, 0); };
        Require(sample() == Pads{0, kPunch1} && !(menu() & recomp::kMenuConfirm),
                "releasing shortcut Alt before Enter leaked Start or menu Confirm");
        keys[SDL_SCANCODE_SPACE] = true;
        Require(menu() & recomp::kMenuConfirm, "fullscreen swallowed another menu key");
        keys[SDL_SCANCODE_SPACE] = keys[SDL_SCANCODE_RETURN] = false;
        (void)sample();
        keys[SDL_SCANCODE_RETURN] = true;
        Require(input.KeyDown(SDL_SCANCODE_RETURN, SDL_KMOD_NONE, false) == Shortcut::None &&
                (menu() & recomp::kMenuConfirm) && sample() == Pads{cps2recomp::kStart, kPunch1},
                "fresh Enter remained captured after the shortcut was released");
        keys[SDL_SCANCODE_LALT] = true;
        Require(input.KeyDown(SDL_SCANCODE_RETURN, SDL_KMOD_LALT, false) == Shortcut::None &&
                sample() == Pads{kPunch2 | cps2recomp::kStart, kPunch1}, "gameplay Alt+Enter toggled fullscreen");
        Require(input.KeyDown(SDL_SCANCODE_F11, SDL_KMOD_NONE, false) == Shortcut::Fullscreen &&
                input.KeyDown(SDL_SCANCODE_F12, SDL_KMOD_RALT, false) == Shortcut::Record &&
                input.KeyDown(SDL_SCANCODE_F12, SDL_KMOD_RALT | SDL_KMOD_LSHIFT, false) == Shortcut::Replay &&
                input.KeyDown(SDL_SCANCODE_F12, SDL_KMOD_LSHIFT, false) == Shortcut::Scene &&
                input.KeyDown(SDL_SCANCODE_F12, SDL_KMOD_LCTRL, false) == Shortcut::Clip &&
                input.KeyDown(SDL_SCANCODE_F12, SDL_KMOD_NONE, false) == Shortcut::Screenshot,
                "F12 shortcuts did not keep their distinct actions");
        recomp::ApplyBinding(bindings, table.table, "keyboard1", "mp", "Right Alt");
        keys[SDL_SCANCODE_RETURN] = keys[SDL_SCANCODE_LALT] = false;
        keys[SDL_SCANCODE_RALT] = true;
        Require(sample() == Pads{kPunch2, kPunch1} && input.ShortcutAlt(SDL_KMOD_LALT) && !input.ShortcutAlt(SDL_KMOD_RALT),
                "right Alt binding took the wrong physical key");
    }

    static void Run() {
        for (const auto &[name, run] : std::initializer_list<std::pair<const char *, void (*)()>>{
                 {"reopening host packets (loopback UDP)", ReopeningPackets},
                 {"intentional leave and reconnect fallback", LeaveKinds},
                 {"reordered/repeated end messages", EndOrdering},
                 {"departure acknowledgment and retry", DepartureAcknowledgments},
                 {"departure transmission and failed-service retry", DepartureTransmission},
                 {"five mixed-machine rosters and start packets", Rosters},
                 {"both three-human formats across all three machine layouts", AsymmetricRosters},
                 {"alternate four-player teams, paired seats and spectator protocol", AlternateTeamOrder},
                 {"four input streams, retransmission, relay and bounded prediction", FourStreamControls},
                 {"reordered timing samples, clock wrap and simulated delivery order", TimingSamples},
                 {"four-peer clock correction across lag and recovery", FourPeerPacing},
                 {"top name, streak and first-to layout bounds", TopHudNames},
                 {"bounded stalled retries with immediate fresh controls", StalledRetries},
                 {"SDL taps through network stalls, two local seats and stale-input expiry", WindowStallInputs},
                 {"keyboard Alt bindings, UI shortcuts and held inputs across waits", KeyboardShortcuts},
                 {"all-machine first-to consensus and score mismatch", FirstToConsensus},
                 {"reliable mode events and four-human mode guard", ModeEvents},
                 {"fighter picks before same-frame mode commands under rollback", ModePickOrder},
                 {"four-stream confirmed spectators and read-only controls", SpectatorControls},
                 {"host seat changes in a match of four, three or two, frame-exact and rolled back to", SeatChanges},
                 {"a strict network from two STUN servers' answers", StrictNetwork},
                 {"the relay's TURN client: digests, private addresses, an allocation", TurnClient}}) {
            run();
            std::cout << "PASS: " << name << '\n';
        }
    }
};
}

int main() {
    try {
        port::NetplayTest::Run();
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
