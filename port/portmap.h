// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
//
// The router's port forward, asked for by the game (online play, port/netplay.h): as Fightcade's and most games'
// online play does, the home router is asked to send what comes to one of its own ports to the game's UDP port, so
// the other player can reach this machine directly even where hole punching can't (a router that lets in only
// what answers something sent, from exactly where it was sent). Nobody opens anything in the router's settings: the
// router must only have UPnP or NAT-PMP on, as most home routers do.
//
// Two ways, both tried at once, the first that works used:
// - NAT-PMP (RFC 6886: Apple's routers, many others): one UDP request to the router's port 5351.
// - UPnP IGD (most other routers): an SSDP search on the home network finds the router's description; its
//   WANIPConnection (or WANPPPConnection) service is asked over HTTP (SOAP) for AddPortMapping and
//   GetExternalIPAddress.
// The forward lasts kLease seconds unless Close takes it away sooner (the match's end). IPv4 only: the address a
// router changes.
//
// Blocking: Open waits up to its time for answers, so it runs on another thread (Netplay::Begin).
#pragma once

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace port::portmap {

constexpr int kLease = 7200;           // (seconds: two hours, longer than a lobby's wait and a long set)
constexpr int kNatPmpPort = 5351;      // (the router's NAT-PMP port)
constexpr const char *kName = "CPS2 Recomp online";

// What came of Open: the forward made (`ok`, `external` "IP:PORT" the other player is told), or why not (`why`, what
// each way said). `made` (with `how` and the fields after) when the router holds a forward Close must take away,
// even one that is no use (`ok` false: a private address outside, another router in front)
struct Mapping {
    bool ok = false, made = false;
    std::string how;      // "UPnP" or "NAT-PMP"
    std::string router;   // (its IP on the home network)
    std::string external; // "IP:PORT"
    std::string why;
    std::string local; // (this machine's IP the router forwards to)
    int localPort = 0, externalPort = 0;
    std::string control, service; // (UPnP: the service's control URL and type)
};

// The pieces (each without sockets, for tools/check_netplay.cpp)

inline std::string Ip(uint32_t ip) { // (host order)
    return std::to_string(ip >> 24) + "." + std::to_string((ip >> 16) & 0xFF) + "." + std::to_string((ip >> 8) & 0xFF) + "." +
           std::to_string(ip & 0xFF);
}
inline uint32_t ParseIp(const std::string &text) { // (0: not an IPv4 address)
    unsigned a, b, c, d;
    char rest;
    if (sscanf(text.c_str(), "%u.%u.%u.%u%c", &a, &b, &c, &d, &rest) != 4 || a > 255 || b > 255 || c > 255 || d > 255) return 0;
    return a << 24 | b << 16 | c << 8 | d;
}
// An address no other player can reach: a home network's, the provider's NAT (100.64/10), loopback, link-local
inline bool Private(uint32_t ip) {
    return (ip >> 24) == 10 || (ip >> 20) == (172u << 4 | 1) || (ip >> 16) == (192u << 8 | 168) || (ip >> 22) == (100u << 2 | 1) ||
           (ip >> 24) == 127 || (ip >> 16) == (169u << 8 | 254) || ip == 0;
}

// The text between <tag> and </tag> (any namespace prefix on the tag), from `from`; "" if there is none
inline std::string Tag(const std::string &xml, const std::string &tag, size_t from = 0, size_t *end = nullptr) {
    for (size_t at = from; (at = xml.find('<', at)) != std::string::npos; ++at) {
        size_t name = at + 1;
        if (name < xml.size() && (xml[name] == '/' || xml[name] == '?' || xml[name] == '!')) continue;
        size_t close = xml.find_first_of(" >/", name);
        if (close == std::string::npos) return "";
        std::string found = xml.substr(name, close - name);
        const size_t colon = found.find(':');
        if (colon != std::string::npos) found = found.substr(colon + 1);
        if (found != tag) continue;
        const size_t open = xml.find('>', close);
        if (open == std::string::npos) return "";
        if (xml[open - 1] == '/') { // (<tag/>)
            if (end) *end = open + 1;
            return "";
        }
        // (the closing tag, with the same prefix the opening had)
        const std::string closing = "</" + xml.substr(name, close - name) + ">";
        const size_t stop = xml.find(closing, open + 1);
        if (stop == std::string::npos) return "";
        if (end) *end = stop + closing.size();
        std::string text = xml.substr(open + 1, stop - open - 1);
        const size_t first = text.find_first_not_of(" \t\r\n"), last = text.find_last_not_of(" \t\r\n");
        return first == std::string::npos ? "" : text.substr(first, last - first + 1);
    }
    return "";
}

// "http://HOST:PORT/PATH" split; false if it isn't one
inline bool SplitUrl(const std::string &url, std::string &host, int &port, std::string &path) {
    if (url.compare(0, 7, "http://") != 0) return false;
    const size_t slash = url.find('/', 7);
    std::string where = url.substr(7, slash == std::string::npos ? std::string::npos : slash - 7);
    path = slash == std::string::npos ? "/" : url.substr(slash);
    port = 80;
    const size_t colon = where.find(':');
    if (colon != std::string::npos) {
        port = atoi(where.c_str() + colon + 1);
        where = where.substr(0, colon);
    }
    host = where;
    return !host.empty() && port > 0 && port < 65536;
}

// An SSDP answer's LOCATION (the router's description), "" if it has none
inline std::string SsdpLocation(const std::string &answer) {
    for (size_t at = 0; at < answer.size();) {
        size_t end = answer.find('\n', at);
        if (end == std::string::npos) end = answer.size();
        std::string line = answer.substr(at, end - at);
        at = end + 1;
        std::string name = line.substr(0, 9);
        for (char &c : name) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
        if (line.size() > 9 && name == "location:") {
            line = line.substr(9);
            const size_t first = line.find_first_not_of(" \t"), last = line.find_last_not_of(" \t\r");
            return first == std::string::npos ? "" : line.substr(first, last - first + 1);
        }
    }
    return "";
}

// The router's services that make forwards, from its description at `location`: (control URL, service type), the
// WANIPConnection ones first
inline std::vector<std::pair<std::string, std::string>> ForwardServices(const std::string &xml, const std::string &location) {
    std::string base = Tag(xml, "URLBase");
    if (base.empty()) {
        std::string host, path;
        int port = 0;
        if (SplitUrl(location, host, port, path)) base = "http://" + host + ":" + std::to_string(port);
    }
    while (!base.empty() && base.back() == '/') base.pop_back();
    std::vector<std::pair<std::string, std::string>> ip, ppp;
    size_t at = 0, end = 0;
    for (;;) {
        const std::string service = Tag(xml, "service", at, &end);
        if (end <= at) break;
        at = end;
        const std::string type = Tag(service, "serviceType"), control = Tag(service, "controlURL");
        if (control.empty()) continue;
        const std::string url = control.compare(0, 7, "http://") == 0 ? control : base + (control[0] == '/' ? "" : "/") + control;
        if (type.find(":WANIPConnection:") != std::string::npos) ip.push_back({url, type});
        else if (type.find(":WANPPPConnection:") != std::string::npos) ppp.push_back({url, type});
    }
    ip.insert(ip.end(), ppp.begin(), ppp.end());
    return ip;
}

// A SOAP request's body: `action` of `service`, with its arguments
inline std::string Soap(const std::string &service, const std::string &action,
                        const std::vector<std::pair<std::string, std::string>> &arguments) {
    std::string body = "<?xml version=\"1.0\"?>\r\n<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
                       "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\"><s:Body><u:" +
                       action + " xmlns:u=\"" + service + "\">";
    for (const auto &[name, value] : arguments) body += "<" + name + ">" + value + "</" + name + ">";
    return body + "</u:" + action + "></s:Body></s:Envelope>\r\n";
}

// NAT-PMP's requests: the router's address outside (op 0), a UDP forward (op 1; lifetime 0 takes it away)
inline std::vector<uint8_t> NatPmpAddressRequest() { return {0, 0}; }
inline std::vector<uint8_t> NatPmpMapRequest(int localPort, int externalPort, uint32_t lifetime) {
    return {0, 1, 0, 0, static_cast<uint8_t>(localPort >> 8), static_cast<uint8_t>(localPort), static_cast<uint8_t>(externalPort >> 8),
            static_cast<uint8_t>(externalPort), static_cast<uint8_t>(lifetime >> 24), static_cast<uint8_t>(lifetime >> 16),
            static_cast<uint8_t>(lifetime >> 8), static_cast<uint8_t>(lifetime)};
}
// An answer: its op (128 address, 129 the UDP forward; -1 not NAT-PMP's), result code, and the address (op 128) or
// the outside port (op 129)
struct NatPmpAnswer {
    int op = -1, result = -1;
    uint32_t address = 0;
    int localPort = 0, externalPort = 0;
};
inline NatPmpAnswer NatPmpParse(const uint8_t *data, int size) {
    NatPmpAnswer answer;
    if (size < 8 || data[0] != 0) return answer;
    answer.result = data[2] << 8 | data[3];
    if (data[1] == 128 && size >= 12) {
        answer.op = 128;
        answer.address = static_cast<uint32_t>(data[8]) << 24 | data[9] << 16 | data[10] << 8 | data[11];
    } else if (data[1] == 129 && size >= 16) {
        answer.op = 129;
        answer.localPort = data[8] << 8 | data[9];
        answer.externalPort = data[10] << 8 | data[11];
    }
    return answer;
}

// The sockets

namespace detail {
#ifdef _WIN32
using Handle = SOCKET;
constexpr Handle kNone = INVALID_SOCKET;
inline void Shut(Handle s) { closesocket(s); }
inline void NonBlocking(Handle s) {
    u_long on = 1;
    ioctlsocket(s, FIONBIO, &on);
}
#else
using Handle = int;
constexpr Handle kNone = -1;
inline void Shut(Handle s) { close(s); }
inline void NonBlocking(Handle s) { fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0) | O_NONBLOCK); }
#endif

inline uint64_t Now() {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
}
inline sockaddr_in Four(uint32_t ip, int port) {
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port = htons(static_cast<uint16_t>(port));
    a.sin_addr.s_addr = htonl(ip);
    return a;
}
// Whether `s` can be read (or written) within `ms`
inline bool Ready(Handle s, int ms, bool write = false) {
    fd_set set;
    FD_ZERO(&set);
    FD_SET(s, &set);
    timeval wait{ms / 1000, (ms % 1000) * 1000};
    return select(static_cast<int>(s) + 1, write ? nullptr : &set, write ? &set : nullptr, nullptr, &wait) > 0;
}

// This machine's IP on the network that leads out (a UDP socket "connected" outside: nothing is sent); 0 offline
inline uint32_t LocalIp() {
    Handle s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == kNone) return 0;
    sockaddr_in out = Four(0x08080808, 53), self{};
    socklen_t length = sizeof(self);
    uint32_t ip = 0;
    if (connect(s, reinterpret_cast<sockaddr *>(&out), sizeof(out)) == 0 &&
        getsockname(s, reinterpret_cast<sockaddr *>(&self), &length) == 0) {
        ip = ntohl(self.sin_addr.s_addr);
    }
    Shut(s);
    return ip;
}

// An HTTP request to the router (HTTP/1.1, the connection closed after): the status (0: no answer) and the body,
// `local` this machine's IP that reached it. Within `ms`
inline int Http(const std::string &url, const std::string &method, const std::string &headers, const std::string &body,
                int ms, std::string &answer, uint32_t *local = nullptr) {
    std::string host, path;
    int port = 0;
    if (!SplitUrl(url, host, port, path)) return 0;
    const uint32_t ip = ParseIp(host);
    if (!ip) return 0; // (a router gives its address as numbers)
    const uint64_t until = Now() + ms;
    Handle s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == kNone) return 0;
    NonBlocking(s);
    sockaddr_in to = Four(ip, port);
    connect(s, reinterpret_cast<sockaddr *>(&to), sizeof(to));
    int status = 0;
    if (Ready(s, ms, true)) {
        int error = 0;
        socklen_t size = sizeof(error);
        getsockopt(s, SOL_SOCKET, SO_ERROR, reinterpret_cast<char *>(&error), &size);
        if (error == 0) {
            if (local) {
                sockaddr_in self{};
                socklen_t length = sizeof(self);
                if (getsockname(s, reinterpret_cast<sockaddr *>(&self), &length) == 0) *local = ntohl(self.sin_addr.s_addr);
            }
            const std::string request = method + " " + path + " HTTP/1.1\r\nHost: " + host + ":" + std::to_string(port) +
                                        "\r\nConnection: close\r\n" + headers + "Content-Length: " + std::to_string(body.size()) +
                                        "\r\n\r\n" + body;
            size_t sent = 0;
            while (sent < request.size() && Now() < until) {
                const int n = static_cast<int>(send(s, request.data() + sent, static_cast<int>(request.size() - sent), 0));
                if (n > 0) sent += n;
                else if (!Ready(s, static_cast<int>(until - Now()), true)) break;
            }
            std::string text;
            char buffer[4096];
            while (sent == request.size() && Now() < until) {
                if (!Ready(s, static_cast<int>(until - Now()))) break;
                const int n = static_cast<int>(recv(s, buffer, sizeof(buffer), 0));
                if (n <= 0) break;
                text.append(buffer, n);
                if (text.size() > (1 << 20)) break;
            }
            const size_t split = text.find("\r\n\r\n");
            if (text.compare(0, 5, "HTTP/") == 0 && split != std::string::npos) {
                status = atoi(text.c_str() + text.find(' ') + 1);
                const std::string head = text.substr(0, split);
                answer = text.substr(split + 4);
                // (chunked: the chunks' sizes taken out)
                std::string lower = head;
                for (char &c : lower) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
                if (lower.find("transfer-encoding: chunked") != std::string::npos) {
                    std::string whole;
                    for (size_t at = 0; at < answer.size();) {
                        const size_t line = answer.find("\r\n", at);
                        if (line == std::string::npos) break;
                        const size_t chunk = strtoul(answer.c_str() + at, nullptr, 16);
                        if (chunk == 0 || line + 2 + chunk > answer.size()) break;
                        whole += answer.substr(line + 2, chunk);
                        at = line + 2 + chunk + 2;
                    }
                    answer = whole;
                }
            }
        }
    }
    Shut(s);
    return status;
}

// A UPnP action on the service: the HTTP status and the answer
inline int Act(const std::string &control, const std::string &service, const std::string &action,
               const std::vector<std::pair<std::string, std::string>> &arguments, int ms, std::string &answer,
               uint32_t *local = nullptr) {
    const std::string headers = "Content-Type: text/xml; charset=\"utf-8\"\r\nSOAPAction: \"" + service + "#" + action + "\"\r\n";
    return Http(control, "POST", headers, Soap(service, action, arguments), ms, answer, local);
}
} // namespace detail

// What Open may be told instead of looking (tools/check_netplay.cpp's routers on this machine)
struct Where {
    std::string location;  // (UPnP: the description's URL, without the SSDP search)
    uint32_t natPmp = 0;   // (NAT-PMP: the router's IP...)
    int natPmpPort = kNatPmpPort; // (... and port)
    uint32_t local = 0;    // (this machine's IP)
    bool search = true;    // (false: neither looked for beyond what is given)
};

// UPnP: the forward from the router's description at `location`, within `ms`
inline Mapping OpenUpnp(const std::string &location, int localPort, int ms, uint32_t localIp) {
    Mapping map;
    map.how = "UPnP";
    const uint64_t until = detail::Now() + ms;
    const auto left = [&] { return static_cast<int>(until > detail::Now() ? until - detail::Now() : 0); };
    std::string host, path;
    int port = 0;
    if (SplitUrl(location, host, port, path)) map.router = host;
    std::string description;
    uint32_t local = 0;
    const int status = detail::Http(location, "GET", "", "", left(), description, &local);
    if (status != 200) {
        map.why = "UPnP: the router's description " + (status ? "said HTTP " + std::to_string(status) : std::string("did not come"));
        return map;
    }
    if (!local) local = localIp;
    const auto services = ForwardServices(description, location);
    if (services.empty()) {
        map.why = "UPnP: the router has no service that forwards ports";
        return map;
    }
    map.local = Ip(local);
    map.localPort = localPort;
    for (const auto &[control, service] : services) {
        // (the same port outside where it's free, else others; a lease, or a lasting one where the router takes no
        // other (725, OnlyPermanentLeasesSupported))
        int lease = kLease;
        for (int attempt = 0; attempt < 4 && left() > 0; ++attempt) {
            const int outside = attempt == 0 ? localPort : 20000 + static_cast<int>((detail::Now() * 7919 + attempt * 104729) % 40000);
            std::string answer;
            const int code = detail::Act(control, service, "AddPortMapping",
                                         {{"NewRemoteHost", ""},
                                          {"NewExternalPort", std::to_string(outside)},
                                          {"NewProtocol", "UDP"},
                                          {"NewInternalPort", std::to_string(localPort)},
                                          {"NewInternalClient", map.local},
                                          {"NewEnabled", "1"},
                                          {"NewPortMappingDescription", kName},
                                          {"NewLeaseDuration", std::to_string(lease)}},
                                         left(), answer);
            if (code == 200) {
                map.made = true;
                map.externalPort = outside;
                map.control = control;
                map.service = service;
                break;
            }
            const std::string error = Tag(answer, "errorCode");
            if (error == "725" && lease) {
                lease = 0;
                --attempt; // (the same port again)
                continue;
            }
            map.why = "UPnP: AddPortMapping said " + (error.empty() ? "HTTP " + std::to_string(code) : "error " + error) +
                      (error == "718" ? " (the port is taken)" : error == "606" ? " (not allowed: the router's UPnP is read-only)" : "");
            if (error != "718") break; // (only a taken port is worth another)
        }
        if (map.made) break;
    }
    if (!map.made) return map;
    std::string answer;
    const int code = detail::Act(map.control, map.service, "GetExternalIPAddress", {}, left(), answer);
    const uint32_t outside = code == 200 ? ParseIp(Tag(answer, "NewExternalIPAddress")) : 0;
    if (!outside) {
        map.why = "UPnP: the forward was made, but the router did not say its address outside";
        return map;
    }
    map.external = Ip(outside) + ":" + std::to_string(map.externalPort);
    if (Private(outside)) {
        map.why = "UPnP: the router's own address outside is " + Ip(outside) +
                  ", a private one: another router (or the provider's NAT) is in front of it, and the forward doesn't reach past that";
        return map;
    }
    map.ok = true;
    return map;
}

// The forward for this machine's UDP port `localPort`: NAT-PMP and UPnP tried at once, within `ms`
inline Mapping Open(int localPort, int ms, const Where &where = {}) {
    using namespace detail;
    Mapping result;
    const uint64_t began = Now(), until = began + ms;
    const auto left = [&] { return static_cast<int>(until > Now() ? until - Now() : 0); };
    const uint32_t local = where.local ? where.local : LocalIp();
    if (!local) {
        result.why = "no network leads out";
        return result;
    }
    // (NAT-PMP: to the router, by default the network's .1, as most home networks have it; and to whoever answers the
    // SSDP search, once known)
    Handle pmp = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP), ssdp = where.search ? socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP) : kNone;
    if (pmp != kNone) NonBlocking(pmp);
    if (ssdp != kNone) {
        NonBlocking(ssdp);
        sockaddr_in self = Four(local, 0);
        bind(ssdp, reinterpret_cast<sockaddr *>(&self), sizeof(self));
        in_addr via{};
        via.s_addr = htonl(local);
        setsockopt(ssdp, IPPROTO_IP, IP_MULTICAST_IF, reinterpret_cast<const char *>(&via), sizeof(via));
        const unsigned char ttl = 2;
        setsockopt(ssdp, IPPROTO_IP, IP_MULTICAST_TTL, reinterpret_cast<const char *>(&ttl), sizeof(ttl));
    }
    std::vector<uint32_t> routers;
    if (where.natPmp) routers.push_back(where.natPmp);
    else if (where.search) routers.push_back((local & 0xFFFFFF00u) | 1);
    std::string location = where.location;
    uint32_t pmpOutside = 0, pmpRouter = 0;
    int pmpPort = 0, pmpResult = -1;
    bool pmpHeard = false;
    uint64_t sentAt = 0;
    int sends = 0;
    std::string upnpWhy;
    while (left() > 0) {
        // (NAT-PMP's requests again at 0.25, 0.5, 1 seconds...; the search twice)
        if (sends < 6 && Now() - sentAt >= static_cast<uint64_t>(250 << (sends > 2 ? 2 : sends))) {
            for (uint32_t router : routers) {
                sockaddr_in to = Four(router, where.natPmpPort);
                const auto ask = NatPmpAddressRequest(), map = NatPmpMapRequest(localPort, localPort, kLease);
                if (!pmpOutside) sendto(pmp, reinterpret_cast<const char *>(ask.data()), static_cast<int>(ask.size()), 0, reinterpret_cast<sockaddr *>(&to), sizeof(to));
                sendto(pmp, reinterpret_cast<const char *>(map.data()), static_cast<int>(map.size()), 0, reinterpret_cast<sockaddr *>(&to), sizeof(to));
            }
            if (ssdp != kNone && location.empty() && sends < 2) {
                sockaddr_in group = Four(0xEFFFFFFA, 1900); // (239.255.255.250)
                for (const char *target : {"urn:schemas-upnp-org:device:InternetGatewayDevice:1", "urn:schemas-upnp-org:service:WANIPConnection:1",
                                           "urn:schemas-upnp-org:device:InternetGatewayDevice:2"}) {
                    const std::string search = std::string("M-SEARCH * HTTP/1.1\r\nHOST: 239.255.255.250:1900\r\nMAN: \"ssdp:discover\"\r\nMX: 1\r\nST: ") +
                                               target + "\r\n\r\n";
                    sendto(ssdp, search.data(), static_cast<int>(search.size()), 0, reinterpret_cast<sockaddr *>(&group), sizeof(group));
                }
            }
            sentAt = Now();
            ++sends;
        }
        // (what came)
        uint8_t packet[2048];
        sockaddr_in from{};
        socklen_t length = sizeof(from);
        int got;
        while (pmp != kNone && (got = static_cast<int>(recvfrom(pmp, reinterpret_cast<char *>(packet), sizeof(packet), 0, reinterpret_cast<sockaddr *>(&from), &length))) > 0) {
            const NatPmpAnswer answer = NatPmpParse(packet, got);
            if (answer.op < 0) continue;
            pmpHeard = true;
            pmpRouter = ntohl(from.sin_addr.s_addr);
            if (answer.op == 128 && answer.result == 0) pmpOutside = answer.address;
            if (answer.op == 129) {
                pmpResult = answer.result;
                if (answer.result == 0) pmpPort = answer.externalPort;
            }
            length = sizeof(from);
        }
        if (pmpPort && pmpOutside) {
            result = {};
            result.how = "NAT-PMP";
            result.made = true;
            result.router = Ip(pmpRouter);
            result.local = Ip(local);
            result.localPort = localPort;
            result.externalPort = pmpPort;
            result.external = Ip(pmpOutside) + ":" + std::to_string(pmpPort);
            result.ok = !Private(pmpOutside);
            if (!result.ok) {
                result.why = "NAT-PMP: the router's own address outside is " + Ip(pmpOutside) +
                             ", a private one: another router (or the provider's NAT) is in front of it, and the forward doesn't reach past that";
            }
            break;
        }
        length = sizeof(from);
        while (ssdp != kNone && location.empty() &&
               (got = static_cast<int>(recvfrom(ssdp, reinterpret_cast<char *>(packet), sizeof(packet) - 1, 0, reinterpret_cast<sockaddr *>(&from), &length))) > 0) {
            location = SsdpLocation(std::string(reinterpret_cast<char *>(packet), got));
            const uint32_t answered = ntohl(from.sin_addr.s_addr);
            if (!location.empty() && std::find(routers.begin(), routers.end(), answered) == routers.end()) {
                routers.push_back(answered); // (NAT-PMP too, at the router that answered)
                sends = std::min(sends, 3);
            }
            length = sizeof(from);
        }
        if (!location.empty() && upnpWhy.empty()) {
            // (UPnP's steps, with what time is left, NAT-PMP's answer looked at after)
            Mapping upnp = OpenUpnp(location, localPort, left(), local);
            if (upnp.made) {
                result = upnp;
                if (upnp.ok) break;
            }
            upnpWhy = upnp.why.empty() ? "UPnP: no use" : upnp.why;
            if (!where.search && !where.natPmp) break;
        }
        // (nothing more to wait for: no answer to NAT-PMP after 1.5 seconds (or a no), and UPnP's search unanswered
        // or its forward failed)
        if (Now() - began > 1500 && (!pmpHeard || pmpResult > 0) && (location.empty() || !upnpWhy.empty())) break;
        // (a moment for what comes next)
        fd_set set;
        FD_ZERO(&set);
        Handle top = 0;
        for (Handle s : {pmp, ssdp}) {
            if (s != kNone) {
                FD_SET(s, &set);
                if (s > top) top = s;
            }
        }
        timeval wait{0, std::min(left(), 50) * 1000};
        select(static_cast<int>(top) + 1, &set, nullptr, nullptr, &wait);
    }
    if (pmp != kNone) Shut(pmp);
    if (ssdp != kNone) Shut(ssdp);
    if (!result.ok) {
        // (why not, from each way)
        std::string why = result.why;
        const auto add = [&](const std::string &part) {
            if (!part.empty() && why.find(part) == std::string::npos) why += (why.empty() ? "" : "; ") + part;
        };
        add(upnpWhy.empty() ? (where.search || !where.location.empty() ? "UPnP: no router answered the search" : "") : upnpWhy);
        add(!pmpHeard ? (routers.empty() ? "" : "NAT-PMP: no answer") :
            pmpResult > 0 ? "NAT-PMP: the router said no (result " + std::to_string(pmpResult) + ")" : "");
        result.why = why;
    }
    return result;
}

// The forward taken away (the match is over), within `ms`
inline void Close(const Mapping &map, int ms = 800) {
    using namespace detail;
    if (!map.made) return;
    if (map.how == "UPnP") {
        std::string answer;
        Act(map.control, map.service, "DeletePortMapping",
            {{"NewRemoteHost", ""}, {"NewExternalPort", std::to_string(map.externalPort)}, {"NewProtocol", "UDP"}}, ms, answer);
        return;
    }
    const uint32_t router = ParseIp(map.router);
    Handle s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == kNone || !router) return;
    sockaddr_in to = Four(router, kNatPmpPort);
    const auto request = NatPmpMapRequest(map.localPort, 0, 0);
    sendto(s, reinterpret_cast<const char *>(request.data()), static_cast<int>(request.size()), 0, reinterpret_cast<sockaddr *>(&to), sizeof(to));
    Ready(s, std::min(ms, 250)); // (its answer not needed: a moment for it)
    Shut(s);
}

} // namespace port::portmap
