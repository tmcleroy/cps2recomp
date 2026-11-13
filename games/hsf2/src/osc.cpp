// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Hyper Street Fighter II: OSC output (the Display page's "OSC output"). Each frame, while the feature is on, the
// players' health and the round clock go out as an OSC bundle over UDP, for TouchDesigner, DMX lighting and the like
// to make something of the fight (a lamp that fades green to red as player one's health falls, a nervous flicker as
// the clock runs down). It reads the game's RAM only and changes nothing of the play.
//
// The values (hsf2.h): each player's health (kHealth, +0x2A in the record, 0x90 full, at or below nothing a knockout),
// sent as 0..1; the round clock (kRoundClock, a BCD byte of seconds, and kClockFrames the frames left of the second);
// and whether a round is being fought now (Fought()). A ready-made colour per player (yellow full, red empty, as the
// game's health bar fades) is sent too, so a light works without any mapping in the receiver.
//
// Where it goes: 127.0.0.1:9000 (TouchDesigner's default), or the address in the environment variable HSF2_OSC, as
// "ip:port" (an IP, not a host name). Nothing is sent when the feature is off; the socket is opened on the first frame
// it is on.
//
// OSC on the wire (no library): a bundle is "#bundle", a time tag (1, "immediately"), then each message with its byte
// length before it. A message is its address string, then its type tag string (",f", ",fff", ",i"), then the
// arguments, big-endian; strings are null-terminated and padded with nulls to a multiple of four bytes.
#include "hsf2.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace cps2recomp {

namespace {
    using namespace hsf2;

#ifdef _WIN32
    using Sock = SOCKET;
    constexpr Sock kNoSock = INVALID_SOCKET;
#else
    using Sock = int;
    constexpr Sock kNoSock = -1;
#endif

    // The socket and its destination, opened once on the first frame the feature is on
    Sock s_sock = kNoSock;
    sockaddr_in s_dst{};
    bool s_tried = false; // tried to open (so a failure is not retried every frame)

    void OpenSocket() {
        if (s_tried) {
            return;
        }
        s_tried = true;
#ifdef _WIN32
        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa); // (harmless if netplay has already done it; each start wants a matching cleanup, which the OS does at exit)
#endif
        std::string host = "127.0.0.1";
        int port = 9000;
        if (const char *given = std::getenv("HSF2_OSC")) {
            const std::string value = given;
            const size_t colon = value.rfind(':');
            if (colon != std::string::npos) {
                host = value.substr(0, colon);
                port = std::atoi(value.c_str() + colon + 1);
            } else {
                host = value;
            }
        }
        s_sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (s_sock == kNoSock) {
            fprintf(stderr, "hsf2: OSC output: no socket\n");
            return;
        }
        s_dst.sin_family = AF_INET;
        s_dst.sin_port = htons(static_cast<uint16_t>(port));
        if (inet_pton(AF_INET, host.c_str(), &s_dst.sin_addr) != 1) {
            fprintf(stderr, "hsf2: OSC output: %s is not an IP address\n", host.c_str());
#ifdef _WIN32
            closesocket(s_sock);
#else
            close(s_sock);
#endif
            s_sock = kNoSock;
            return;
        }
        fprintf(stderr, "hsf2: OSC output to %s:%d\n", host.c_str(), port);
    }

    void Send(const std::vector<uint8_t> &packet) {
        if (s_sock == kNoSock) {
            return;
        }
        sendto(s_sock, reinterpret_cast<const char *>(packet.data()), static_cast<int>(packet.size()), 0,
               reinterpret_cast<const sockaddr *>(&s_dst), sizeof(s_dst));
    }

    // --- building the packet ---

    void PutU32(std::vector<uint8_t> &out, uint32_t value) { // big-endian
        out.push_back(static_cast<uint8_t>(value >> 24));
        out.push_back(static_cast<uint8_t>(value >> 16));
        out.push_back(static_cast<uint8_t>(value >> 8));
        out.push_back(static_cast<uint8_t>(value));
    }
    void PutString(std::vector<uint8_t> &out, const char *s) {
        for (const char *p = s; *p; ++p) {
            out.push_back(static_cast<uint8_t>(*p));
        }
        out.push_back(0);
        while (out.size() % 4 != 0) {
            out.push_back(0);
        }
    }
    uint32_t FloatBits(float f) {
        uint32_t bits;
        std::memcpy(&bits, &f, sizeof(bits));
        return bits;
    }
    float Clamp01(float f) {
        return f < 0.f ? 0.f : f > 1.f ? 1.f : f;
    }

    // A message, built on its own (its strings padded from its own start, so its length is a multiple of four and the
    // bundle stays aligned when it is put in). Up to three floats, then an optional int.
    std::vector<uint8_t> Message(const char *address, int floats, const float *values, bool hasInt, int32_t intValue) {
        std::vector<uint8_t> m;
        PutString(m, address);
        std::string types = ",";
        types.append(static_cast<size_t>(floats), 'f');
        if (hasInt) {
            types.push_back('i');
        }
        PutString(m, types.c_str());
        for (int i = 0; i < floats; i++) {
            PutU32(m, FloatBits(values[i]));
        }
        if (hasInt) {
            PutU32(m, static_cast<uint32_t>(intValue));
        }
        return m;
    }

    void Add(std::vector<uint8_t> &bundle, const std::vector<uint8_t> &message) {
        PutU32(bundle, static_cast<uint32_t>(message.size()));
        bundle.insert(bundle.end(), message.begin(), message.end());
    }
    void AddFloat(std::vector<uint8_t> &bundle, const char *address, float v) {
        Add(bundle, Message(address, 1, &v, false, 0));
    }
    void AddColour(std::vector<uint8_t> &bundle, const char *address, float health) {
        // Yellow full, red empty, as the game's health bar fades (a light works with no mapping in the receiver):
        // red always on, green fades out with the health
        const float rgb[3] = {1.f, Clamp01(health), 0.f};
        Add(bundle, Message(address, 3, rgb, false, 0));
    }
    void AddInt(std::vector<uint8_t> &bundle, const char *address, int32_t v) {
        Add(bundle, Message(address, 0, nullptr, true, v));
    }

    // --- reading the game ---

    constexpr float kRoundSeconds = 99.f; // the clock's full count (for the 0..1 reading; the receiver can rescale)
    constexpr float kFramesPerSecond = 64.f; // kClockFrames counts down 0x40 to 0 within each second

    float HealthOf(int player) {
        const int16_t health = static_cast<int16_t>(Ram16(Player(player) + kHealth));
        if (health <= 0) {
            return 0.f;
        }
        const float f = static_cast<float>(health) / static_cast<float>(kHealthFull);
        return f > 1.f ? 1.f : f;
    }
    // The clock as a smooth count of seconds left (the whole seconds plus the fraction of the one running down)
    float ClockSeconds() {
        const uint8_t bcd = Ram8(kRoundClock);
        const int seconds = (bcd >> 4) * 10 + (bcd & 0x0F);
        const uint8_t frames = Ram8(kClockFrames);
        const float fraction = frames > 0 && frames <= static_cast<uint8_t>(kFramesPerSecond)
                                   ? static_cast<float>(frames) / kFramesPerSecond
                                   : 0.f;
        return static_cast<float>(seconds) + fraction;
    }
    // Any fight on the screen now, the attract-mode demonstration too: both players in a fight (kFighting == 2),
    // whichever task runs it. Fought() means only a game after a coin, in the game task; this does not look at the task,
    // so the lamp can play along with the demonstration the cabinet shows itself.
    bool AnyFight() {
        return Ram8(Player(0) + kFighting) == 2 && Ram8(Player(1) + kFighting) == 2;
    }
} // namespace

void OscFrame(Machine &) {
    if (!FeatureOn("osc_output")) {
        return;
    }
    OpenSocket();
    if (s_sock == kNoSock) {
        return;
    }

    const float h1 = HealthOf(0), h2 = HealthOf(1);
    const float seconds = ClockSeconds();

    std::vector<uint8_t> bundle;
    PutString(bundle, "#bundle");
    PutU32(bundle, 0); // time tag: 0, 1 means "immediately"
    PutU32(bundle, 1);

    AddFloat(bundle, "/hsf2/p1/health", h1);
    AddFloat(bundle, "/hsf2/p2/health", h2);
    AddColour(bundle, "/hsf2/p1/color", h1);
    AddColour(bundle, "/hsf2/p2/color", h2);
    AddFloat(bundle, "/hsf2/timer", seconds);
    AddFloat(bundle, "/hsf2/timer01", Clamp01(seconds / kRoundSeconds));
    AddInt(bundle, "/hsf2/active", Fought() ? 1 : 0);
    AddInt(bundle, "/hsf2/fight", AnyFight() ? 1 : 0);

    Send(bundle);
}

void RegisterOsc() {
    TheGame().features.push_back(
        {"osc_output", "Display", "OSC output",
         "Sends the players' health and the round clock out as OSC over UDP each frame, for TouchDesigner, DMX "
         "lighting and the like. The address is 127.0.0.1:9000, or set HSF2_OSC to \"ip:port\". It changes nothing of "
         "the game.",
         0});
}

} // namespace cps2recomp
