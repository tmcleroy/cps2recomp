// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
//
// A TURN client (RFC 8656, over UDP), for online play's relay (port/netplay.h, RelaySocket): when a router stops a
// direct connection (a symmetric or carrier-grade NAT, Netplay::StrictNetwork), each side can ask a TURN server for an
// address of its own there (the relayed address), which the other players are told with the rest. Their packets to it
// come to this machine wrapped (Data indications), and this machine's to them go wrapped to the server (Send
// indications), which sends them on from the relayed address. The server lets in only the addresses given leave
// (CreatePermission, by IP), and forgets the address unless asked again (Refresh). Its requests are signed with the
// long-term credential (a username and password from the online service, which has them from Cloudflare's TURN keys).
//
// Only the messages: no sockets here, nor any library (netplay.cpp's RelaySocket sends and receives them). IPv4 only: addresses are an
// IP in host order and a port.
#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace port::turn {

// MD5 (RFC 1321): the long-term credential's key is MD5(username:realm:password)
inline std::array<uint8_t, 16> Md5(const std::string &text) {
    static const uint32_t k[64] = {
        0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501, 0x698098d8, 0x8b44f7af,
        0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821, 0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
        0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8, 0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8,
        0x676f02d9, 0x8d2a4c8a, 0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
        0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665, 0xf4292244, 0x432aff97,
        0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1, 0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
        0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391};
    static const int r[64] = {7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 5, 9,  14, 20, 5, 9,
                              14, 20, 5, 9,  14, 20, 5, 9,  14, 20, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
                              4, 11, 16, 23, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21};
    std::vector<uint8_t> m(text.begin(), text.end());
    const uint64_t bits = static_cast<uint64_t>(m.size()) * 8;
    m.push_back(0x80);
    while (m.size() % 64 != 56) m.push_back(0);
    for (int i = 0; i < 8; ++i) m.push_back(static_cast<uint8_t>(bits >> (8 * i)));
    uint32_t h[4] = {0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476};
    for (size_t block = 0; block < m.size(); block += 64) {
        uint32_t w[16];
        for (int i = 0; i < 16; ++i) {
            const uint8_t *p = &m[block + 4 * i];
            w[i] = p[0] | p[1] << 8 | p[2] << 16 | static_cast<uint32_t>(p[3]) << 24;
        }
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
        for (int i = 0; i < 64; ++i) {
            uint32_t f;
            int g;
            if (i < 16) f = (b & c) | (~b & d), g = i;
            else if (i < 32) f = (d & b) | (~d & c), g = (5 * i + 1) % 16;
            else if (i < 48) f = b ^ c ^ d, g = (3 * i + 5) % 16;
            else f = c ^ (b | ~d), g = (7 * i) % 16;
            const uint32_t next = d;
            d = c;
            c = b;
            const uint32_t sum = a + f + k[i] + w[g];
            b = b + (sum << r[i] | sum >> (32 - r[i]));
            a = next;
        }
        h[0] += a, h[1] += b, h[2] += c, h[3] += d;
    }
    std::array<uint8_t, 16> out{};
    for (int i = 0; i < 16; ++i) out[i] = static_cast<uint8_t>(h[i / 4] >> (8 * (i % 4)));
    return out;
}

// SHA-1 (RFC 3174), for HMAC-SHA1: a STUN message's MESSAGE-INTEGRITY
inline std::array<uint8_t, 20> Sha1(const std::vector<uint8_t> &data) {
    std::vector<uint8_t> m = data;
    const uint64_t bits = static_cast<uint64_t>(m.size()) * 8;
    m.push_back(0x80);
    while (m.size() % 64 != 56) m.push_back(0);
    for (int i = 7; i >= 0; --i) m.push_back(static_cast<uint8_t>(bits >> (8 * i)));
    uint32_t h[5] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};
    const auto rotl = [](uint32_t x, int n) { return x << n | x >> (32 - n); };
    for (size_t block = 0; block < m.size(); block += 64) {
        uint32_t w[80];
        for (int i = 0; i < 16; ++i) {
            const uint8_t *p = &m[block + 4 * i];
            w[i] = static_cast<uint32_t>(p[0]) << 24 | p[1] << 16 | p[2] << 8 | p[3];
        }
        for (int i = 16; i < 80; ++i) w[i] = rotl(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
        for (int i = 0; i < 80; ++i) {
            uint32_t f, k;
            if (i < 20) f = (b & c) | (~b & d), k = 0x5A827999;
            else if (i < 40) f = b ^ c ^ d, k = 0x6ED9EBA1;
            else if (i < 60) f = (b & c) | (b & d) | (c & d), k = 0x8F1BBCDC;
            else f = b ^ c ^ d, k = 0xCA62C1D6;
            const uint32_t t = rotl(a, 5) + f + e + k + w[i];
            e = d, d = c, c = rotl(b, 30), b = a, a = t;
        }
        h[0] += a, h[1] += b, h[2] += c, h[3] += d, h[4] += e;
    }
    std::array<uint8_t, 20> out{};
    for (int i = 0; i < 20; ++i) out[i] = static_cast<uint8_t>(h[i / 4] >> (24 - 8 * (i % 4)));
    return out;
}

inline std::array<uint8_t, 20> HmacSha1(const std::vector<uint8_t> &key, const std::vector<uint8_t> &message) {
    std::vector<uint8_t> k = key;
    if (k.size() > 64) {
        const auto hashed = Sha1(k);
        k.assign(hashed.begin(), hashed.end());
    }
    k.resize(64, 0);
    std::vector<uint8_t> inner(64), outer(64);
    for (int i = 0; i < 64; ++i) inner[i] = k[i] ^ 0x36, outer[i] = k[i] ^ 0x5c;
    inner.insert(inner.end(), message.begin(), message.end());
    const auto first = Sha1(inner);
    outer.insert(outer.end(), first.begin(), first.end());
    return Sha1(outer);
}

// CRC-32 (as zlib's): the FINGERPRINT attribute's
inline uint32_t Crc32(const uint8_t *data, size_t size) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < size; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit) crc = crc >> 1 ^ (0xEDB88320 & (0 - (crc & 1)));
    }
    return ~crc;
}

constexpr uint32_t kCookie = 0x2112A442;
// The message types used: (method | class) as the wire has them
enum : uint16_t {
    kAllocate = 0x0003, kAllocateOk = 0x0103, kAllocateError = 0x0113,
    kRefresh = 0x0004, kRefreshOk = 0x0104, kRefreshError = 0x0114,
    kPermission = 0x0008, kPermissionOk = 0x0108, kPermissionError = 0x0118,
    kSend = 0x0016, kData = 0x0017,
};
// The attributes used
enum : uint16_t {
    kUsername = 0x0006, kIntegrity = 0x0008, kErrorCode = 0x0009, kLifetime = 0x000D, kPeer = 0x0012, kDataAttr = 0x0013,
    kRealm = 0x0014, kNonce = 0x0015, kRelayed = 0x0016, kTransport = 0x0019, kFingerprint = 0x8028,
};

// A message being made: a type and a transaction id, then its attributes; Finish signs it (with the key, when there
// is one) and adds the fingerprint
struct Writer {
    std::vector<uint8_t> out;
    Writer(uint16_t type, const std::array<uint8_t, 12> &id) {
        out = {static_cast<uint8_t>(type >> 8), static_cast<uint8_t>(type), 0, 0, 0x21, 0x12, 0xA4, 0x42};
        out.insert(out.end(), id.begin(), id.end());
    }
    void Attribute(uint16_t type, const uint8_t *data, size_t size) {
        out.push_back(static_cast<uint8_t>(type >> 8));
        out.push_back(static_cast<uint8_t>(type));
        out.push_back(static_cast<uint8_t>(size >> 8));
        out.push_back(static_cast<uint8_t>(size));
        out.insert(out.end(), data, data + size);
        while (out.size() % 4) out.push_back(0);
        Length(out.size() - 20);
    }
    void Text(uint16_t type, const std::string &text) { Attribute(type, reinterpret_cast<const uint8_t *>(text.data()), text.size()); }
    void U32(uint16_t type, uint32_t value) {
        const uint8_t v[4] = {static_cast<uint8_t>(value >> 24), static_cast<uint8_t>(value >> 16), static_cast<uint8_t>(value >> 8),
                              static_cast<uint8_t>(value)};
        Attribute(type, v, 4);
    }
    // (an IPv4 address and port, XORed with the cookie: XOR-PEER-ADDRESS and the like)
    void Address(uint16_t type, uint32_t ip, int port) {
        const uint16_t xport = static_cast<uint16_t>(port ^ (kCookie >> 16));
        const uint32_t xip = ip ^ kCookie;
        const uint8_t v[8] = {0, 0x01, static_cast<uint8_t>(xport >> 8), static_cast<uint8_t>(xport), static_cast<uint8_t>(xip >> 24),
                              static_cast<uint8_t>(xip >> 16), static_cast<uint8_t>(xip >> 8), static_cast<uint8_t>(xip)};
        Attribute(type, v, 8);
    }
    void Length(size_t length) {
        out[2] = static_cast<uint8_t>(length >> 8);
        out[3] = static_cast<uint8_t>(length);
    }
    std::vector<uint8_t> Finish(const std::vector<uint8_t> &key) {
        if (!key.empty()) {
            // (the HMAC over the message as it is, its length already counting the integrity attribute)
            Length(out.size() - 20 + 24);
            const auto mac = HmacSha1(key, out);
            Attribute(kIntegrity, mac.data(), mac.size());
        }
        Length(out.size() - 20 + 8);
        const uint32_t crc = Crc32(out.data(), out.size()) ^ 0x5354554E;
        U32(kFingerprint, crc);
        return out;
    }
};

// A message that came: its type, transaction id and attributes (each one's type, and where its value is)
struct Parsed {
    uint16_t type = 0;
    std::array<uint8_t, 12> id{};
    struct Item {
        uint16_t type;
        const uint8_t *value;
        size_t size;
    };
    std::vector<Item> items;
    const Item *Find(uint16_t wanted) const {
        for (const Item &item : items)
            if (item.type == wanted) return &item;
        return nullptr;
    }
    std::string Text(uint16_t wanted) const {
        const Item *item = Find(wanted);
        return item ? std::string(reinterpret_cast<const char *>(item->value), item->size) : std::string();
    }
    bool Address(uint16_t wanted, uint32_t &ip, int &port) const {
        const Item *item = Find(wanted);
        if (!item || item->size < 8 || item->value[1] != 0x01) return false;
        port = (item->value[2] << 8 | item->value[3]) ^ (kCookie >> 16);
        ip = (static_cast<uint32_t>(item->value[4]) << 24 | item->value[5] << 16 | item->value[6] << 8 | item->value[7]) ^ kCookie;
        return true;
    }
    int Error() const { // (ERROR-CODE: 401, 438 ...; 0: none)
        const Item *item = Find(kErrorCode);
        return item && item->size >= 4 ? (item->value[2] & 7) * 100 + item->value[3] : 0;
    }
};

// A STUN message (the cookie, a whole number of attributes), or false
inline bool Parse(const uint8_t *data, size_t size, Parsed &parsed) {
    if (size < 20 || (data[0] & 0xC0) || data[4] != 0x21 || data[5] != 0x12 || data[6] != 0xA4 || data[7] != 0x42) return false;
    const size_t length = static_cast<size_t>(data[2] << 8 | data[3]);
    if (length + 20 > size || length % 4) return false;
    parsed = Parsed{};
    parsed.type = static_cast<uint16_t>(data[0] << 8 | data[1]);
    memcpy(parsed.id.data(), data + 8, 12);
    for (size_t at = 20; at + 4 <= 20 + length;) {
        const uint16_t type = static_cast<uint16_t>(data[at] << 8 | data[at + 1]);
        const size_t itemSize = static_cast<size_t>(data[at + 2] << 8 | data[at + 3]);
        if (at + 4 + itemSize > 20 + length) return false;
        parsed.items.push_back({type, data + at + 4, itemSize});
        at += 4 + ((itemSize + 3) & ~size_t{3});
    }
    return true;
}

// An address a relay on the internet can't send to (a private network's, the machine's own, link-local, a carrier's
// shared range): such a peer is reached directly, and is not asked to be let in (a server may refuse the whole
// permission for one of them). A relay that is itself on such a network (a test's) may send within it.
inline bool Private(uint32_t ip) {
    const uint32_t a = ip >> 24, b = (ip >> 16) & 0xFF;
    return a == 0 || a == 10 || a == 127 || (a == 172 && b >= 16 && b < 32) || (a == 192 && b == 168) || (a == 169 && b == 254) ||
           (a == 100 && b >= 64 && b < 128) || a >= 224;
}
inline bool Unrelayable(uint32_t ip, uint32_t relayIp) {
    return Private(ip) && !(Private(relayIp) && (ip >> 16) == (relayIp >> 16));
}

// The client: an allocation on one server, kept while the match goes on, with the permissions for the other players
class Client {
public:
    // (the server's username and password; the time now, in milliseconds; a seed for the transaction ids)
    void Start(const std::string &user, const std::string &password, uint64_t now, uint64_t seed) {
        username = user;
        secret = password;
        random = seed | 1;
        started = now;
        state = State::Asking;
        Ask(kAllocate, now);
    }
    bool Started() const { return state != State::Off; }
    bool Ready() const { return state == State::Ready; }
    bool Failed() const { return state == State::Failed; }
    const std::string &Why() const { return why; }
    uint32_t RelayIp() const { return relayIp; }
    int RelayPort() const { return relayPort; }
    // (a peer's IP the server is to let through, from now on)
    void Permit(uint32_t ip) {
        if (ip && std::find(permitted.begin(), permitted.end(), ip) == permitted.end()) {
            permitted.push_back(ip);
            permitsDue = true;
        }
    }
    bool Permitted(uint32_t ip) const { return std::find(permitted.begin(), permitted.end(), ip) != permitted.end(); }
    bool Reaches(uint32_t ip) const { return Ready() && !Unrelayable(ip, relayIp); }
    // What to send the server now: a request again (one not answered in half a second, for 5 seconds), the allocation
    // refreshed (every 5 minutes, for 10 more), the permissions (when there are new ones, and every 4 minutes: they
    // last 5)
    std::vector<std::vector<uint8_t>> Tick(uint64_t now) {
        std::vector<std::vector<uint8_t>> out;
        out.swap(queued);
        if (state == State::Off || state == State::Failed) return out;
        for (Pending &p : pending) {
            if (now - p.sent >= 500) {
                if (now - p.first > 5000) {
                    Fail(p.type == kAllocate ? "the relay server did not answer" : "the relay server stopped answering");
                    return {};
                }
                p.sent = now;
                out.push_back(p.message);
            }
        }
        if (state == State::Ready) {
            if (now - refreshedAt > 300000 && !Waiting(kRefresh)) Ask(kRefresh, now);
            if ((permitsDue || (now - permittedAt > 240000 && !permitted.empty())) && !Waiting(kPermission)) {
                if (std::any_of(permitted.begin(), permitted.end(), [&](uint32_t ip) { return !Unrelayable(ip, relayIp); })) {
                    Ask(kPermission, now);
                } else {
                    permitsDue = false;
                }
            }
            std::vector<std::vector<uint8_t>> more;
            more.swap(queued);
            for (auto &m : more) out.push_back(std::move(m));
        }
        return out;
    }
    enum class Took { NotOurs, Ours, Data };
    // A packet from the server: one of our answers (Ours), a peer's packet (Data: `payload`, from `peerIp`:`peerPort`),
    // or anything else (NotOurs: a STUN server's answer that shares the address, say)
    Took Take(const uint8_t *data, size_t size, std::vector<uint8_t> &payload, uint32_t &peerIp, int &peerPort, uint64_t now) {
        Parsed m;
        if (!Parse(data, size, m)) return Took::NotOurs;
        if (m.type == kData) {
            const Parsed::Item *item = m.Find(kDataAttr);
            if (!item || !m.Address(kPeer, peerIp, peerPort)) return Took::Ours;
            payload.assign(item->value, item->value + item->size);
            return Took::Data;
        }
        auto it = std::find_if(pending.begin(), pending.end(), [&](const Pending &p) { return p.id == m.id; });
        if (it == pending.end()) return Took::NotOurs;
        const Pending was = *it;
        pending.erase(it);
        const int error = m.Error();
        if (error == 401 || error == 438) {
            // (unauthenticated, or a stale nonce: the realm and nonce given, and the request again, signed)
            const std::string newRealm = m.Text(kRealm), newNonce = m.Text(kNonce);
            if (newNonce.empty() || ++challenges > 4 || (error == 401 && !nonce.empty() && newNonce == nonce && was.signedWith == nonce)) {
                Fail("the relay server refused the credentials");
                return Took::Ours;
            }
            if (!newRealm.empty()) realm = newRealm;
            nonce = newNonce;
            const auto md5 = Md5(username + ":" + realm + ":" + secret);
            key.assign(md5.begin(), md5.end());
            Ask(was.type, now);
            return Took::Ours;
        }
        if (was.type == kAllocate) {
            if (m.type == kAllocateOk && m.Address(kRelayed, relayIp, relayPort)) {
                state = State::Ready;
                challenges = 0;
                refreshedAt = now;
            } else {
                Fail("the relay server would not give an address (" + std::to_string(error) + ")");
            }
        } else if (was.type == kRefresh) {
            if (m.type == kRefreshOk) refreshedAt = now, challenges = 0;
            else Fail("the relay server dropped the address (" + std::to_string(error) + ")");
        } else if (was.type == kPermission) {
            if (m.type == kPermissionOk) permittedAt = now, challenges = 0;
        }
        return Took::Ours;
    }
    // A packet for a peer, as a Send indication for the server
    std::vector<uint8_t> Wrap(uint32_t ip, int port, const uint8_t *data, size_t size) {
        Writer w(kSend, NewId());
        w.Address(kPeer, ip, port);
        w.Attribute(kDataAttr, data, size);
        w.Length(w.out.size() - 20);
        return w.out; // (indications are not signed)
    }

private:
    enum class State { Off, Asking, Ready, Failed } state = State::Off;
    std::string username, secret, realm, nonce, why;
    std::vector<uint8_t> key;
    uint64_t random = 1, started = 0, refreshedAt = 0, permittedAt = 0;
    uint32_t relayIp = 0;
    int relayPort = 0;
    std::vector<uint32_t> permitted;
    bool permitsDue = false;
    int challenges = 0; // (the server's 401s and 438s answered: a few, or the credentials are wrong)
    struct Pending {
        uint16_t type;
        std::array<uint8_t, 12> id;
        std::vector<uint8_t> message;
        uint64_t first, sent;
        std::string signedWith;
    };
    std::vector<Pending> pending;
    std::vector<std::vector<uint8_t>> queued;
    std::array<uint8_t, 12> NewId() {
        std::array<uint8_t, 12> id{};
        for (uint8_t &b : id) b = static_cast<uint8_t>((random = random * 6364136223846793005ull + 1442695040888963407ull) >> 56);
        return id;
    }
    bool Waiting(uint16_t type) const {
        return std::any_of(pending.begin(), pending.end(), [&](const Pending &p) { return p.type == type; });
    }
    void Fail(const std::string &reason) {
        state = State::Failed;
        why = reason;
        pending.clear();
        queued.clear();
    }
    void Ask(uint16_t type, uint64_t now) {
        const auto id = NewId();
        Writer w(type, id);
        if (type == kAllocate) {
            const uint8_t udp[4] = {17, 0, 0, 0};
            w.Attribute(kTransport, udp, 4);
        } else if (type == kRefresh) {
            w.U32(kLifetime, 600);
        } else if (type == kPermission) {
            for (const uint32_t ip : permitted)
                if (!Unrelayable(ip, relayIp)) w.Address(kPeer, ip, 0);
            permitsDue = false;
        }
        if (!nonce.empty()) {
            w.Text(kUsername, username);
            w.Text(kRealm, realm);
            w.Text(kNonce, nonce);
        }
        const std::vector<uint8_t> message = w.Finish(nonce.empty() ? std::vector<uint8_t>{} : key);
        pending.push_back({type, id, message, now, now, nonce});
        queued.push_back(message);
    }
};

} // namespace port::turn
