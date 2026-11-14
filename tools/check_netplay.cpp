// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
// ROM-free protocol regressions. Compile the production translation unit, so these exercise the same packet,
// socket and state-transition code as the game. Unused game-dependent sections are removed by the linker.
#include "../port/netplay.cpp"

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
int FeatureValue(const char *) {
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

    static void Rosters() {
        const std::vector<std::vector<std::vector<int>>> expected = {
            {{0, 2}, {1}, {3}}, {{0}, {1, 3}, {2}}, {{0}, {1}, {2, 3}},
            {{0}, {1}, {2}, {3}}, {{0, 2}, {1, 3}},
        };
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
                }
                Require(reader.ok && reader.at == reader.end, "malformed start roster");
            }
        }
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
        watch.Say("spectator cannot play");
        Require(watch.commanded.empty() && watch.links[0].unheard.empty(), "viewer entered player controls/chat");
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

    static void Run() {
        for (const auto &[name, run] : std::initializer_list<std::pair<const char *, void (*)()>>{
                 {"reopening host packets (loopback UDP)", ReopeningPackets},
                 {"intentional leave and reconnect fallback", LeaveKinds},
                 {"reordered/repeated end messages", EndOrdering},
                 {"departure acknowledgment and retry", DepartureAcknowledgments},
                 {"departure transmission and failed-service retry", DepartureTransmission},
                 {"five mixed-machine rosters and start packets", Rosters},
                 {"four input streams, retransmission, relay and bounded prediction", FourStreamControls},
                 {"all-machine first-to consensus and score mismatch", FirstToConsensus},
                 {"reliable mode events and four-human mode guard", ModeEvents},
                 {"four-stream confirmed spectators and read-only controls", SpectatorControls}}) {
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
