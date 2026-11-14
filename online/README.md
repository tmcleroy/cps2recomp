# The online service

Online play (`port/netplay.h`) is copies of the game sending each other their players' controls directly: two, or up to
four in a match of four (the tag-team games' 2 vs 2), whose joining machines each reach the host. This service only
introduces them: a host asks it for a code; each other player gives the code, and the host and they are told where the
other can be reached. Nothing of a match goes through it. It is a [Cloudflare Worker](https://developers.cloudflare.com/workers/)
with a [Durable Object](https://developers.cloudflare.com/durable-objects/) for each match (`src/index.js`), within
Cloudflare's free plan.

How the two meet:

1. Each game asks a public STUN server (Cloudflare's, or Google's) what address and port its UDP socket is seen from
   on the internet: the router's, for a machine behind one.
2. The host sends that and its addresses on its own networks to `POST /v1/host`, with the players its match is for
   (`players`: 2, or 4) and how many of them play at its machine (`here`: 1, or 2), and is given a code (`K7Q-M2X`,
   six letters and digits; no 0, O, 1, I or L). It asks `GET /v1/host/CODE` every 2 seconds, every 5 after two
   minutes and every 10 after ten (a joining player waits 30 seconds to hear from it), until everyone has come.
3. Each other player sends the code, its own addresses and the people at its machine (`people`: 1, or 2 in a match of
   four) to `POST /v1/join/CODE`, and is given the host's addresses and a number of its own (`ticket`), which it says
   to the host in its hello. The host's next question gives it each who joined (`joiners`: "TICKET ADDRESS
   ADDRESS..."), and says back (`gone=TICKET,...`) those who went before the match began, or never came in 30
   seconds: their places are free again. A code lasts 30 minutes (a host still waiting then has the same code again),
   and can be joined while its match has room (once, for a match of two); a code for another game, or for another
   build or wire protocol of the game, is turned away. (A host without `players` has a match of two, and is told the
   one who joined as `addresses`, as it always was.)
4. Each side sends to every address of the other's. The host's packets open its router to the other's, whose hellos
   then come through (UDP hole punching); the first address that answers is the match's. In a match of four the host
   does so for each who joins, and they never need to reach each other: the host passes on what each sends.

A host that asks for it (`--net-list`, with `--net-name`) is also in the **lobby**: a Durable Object keeping every
game's listed matches whose host has asked about them in the last 25 seconds, while they have room or are running
with spectators allowed, in memory only (a match tells it at most every 10 seconds).
`GET /v1/lobby?protocol=N` lists them all, each with its game (the launcher's Online page); `GET /v1/lobby/GAME?protocol=N`
one game's (a game page's Online card).

The **waiting room** is the players whose launcher has its Online page open: a Durable Object (Room) they check in at
every 10 seconds, or 20 while the launcher isn't the window in front (`POST /v1/room`, with a name, the games they
have and whether they can be invited; with `lobby: true` the answer has every game's open matches too, so the Online
page asks once), gone 30 seconds after the last check-in, and kept in memory only (a room let go is made again by the next check-ins). Each is
known by an id the service makes from a secret only their launcher has, and shown by name only. A host can name one
to invite (`invite` and `from` in `POST /v1/host`): the Match gives the invitation to the room while the host asks
about its match, and takes it back when the match is full; a player declines with `POST /v1/room/decline`, which the
host's next question says (`invite: "declined"`, or `"away"` if it couldn't be given). Players block each other with
ids their launchers keep, sent with each check-in.

Two routers that each give every destination a port of its own ("symmetric" NAT, as some mobile networks do) can't be
met this way; a relay would be needed, which the service doesn't have yet. Then the host's address, with its port
forwarded, or a VPN such as Tailscale, still works.

**Stats** for whoever runs the service: the Stats Durable Object counts matches hosted and joined per UTC day, by
game (90 days kept), and knows the hosts asking about a match in the last 30 seconds; nothing about who. `GET
/v1/stats?key=KEY` shows them, with how many have the Online page open, if the Worker has the secret `STATS_KEY`
(without it the page is closed).

## Running it

### Spectator development

The spectator protocol adds `POST /v1/watch/CODE`. Only a live match whose host opted in is watchable.
The viewer supplies its game, build, version, protocol and addresses and receives the host's addresses and
an admission token. Authenticated host polls with `active=1` keep the running lobby entry alive and deliver
viewer addresses and tokens for UDP introduction. Ordinary joins reserve player seats (one or two people per joining
machine in a four-player match); spectators have a separate eight-viewer limit and never consume those seats.
An active match rejects additional player joins even if some players connected directly without service tickets.
The host can send `gone` removals and `active=1` in the same poll without losing either update.
The lobby says `watching: true` for a running match and never publishes addresses or tokens.

Run `node online/test-spectators.mjs` for service tests. `node online/dev-service.mjs` starts a loopback-only
in-memory development server; it is not a replacement for a production deployment. With it running, set
`SERVICE=http://127.0.0.1:8787` and run `python tools/check_spectators.py` from the repository root after
building Marvel vs. Capcom. With no SERVICE, that test connects by local address instead. It checks a late
viewer against confirmed hashes and verifies that viewer input and departure do not affect the players.

Watch from the lobby needs this service deployed (below); a copy of the game on an older service still hosts and
joins as before.

```sh
cd online
npx wrangler login     # once: Cloudflare's tool, signed in to the account in a browser
npx wrangler deploy    # https://cps2recomp-online.<the account's subdomain>.workers.dev
npx wrangler secret put STATS_KEY   # once, for the stats page
npx wrangler dev       # or on this machine, at http://localhost:8787, for tests
```

The games ask `Netplay::kService` (`port/netplay.h`) unless started with `--net-service URL` (or `off`: no codes,
addresses only).

## Checking it

With Node.js 22 or later, from the repository's root:

```sh
node --test online/test/*.test.js
```

Or run `npm test` from `online`. No dependencies, game files, account or network are needed. The tests import the
actual Worker and Durable Object handlers, using in-memory storage and bindings. They cover two-player compatibility,
every ordered one-/two-person placement in a match of four, capacity errors, ticket removal and retries after a lost
request or answer, wrong credentials/build/game/protocol, lobby visibility, rehosting and expiry. Retrying a removal
must not evict a replacement that received a newer ticket. Live rehosting preserves the ticket counter so a delayed
old removal cannot delete a new arrival; a record recreated after expiry has no such retained history and would
need a separate generation token to distinguish arbitrarily delayed old requests. The same suite exercises spectator
admission to full and direct-address four-player matches, token retries and limits, privacy, host/viewer expiry, rehosting, and combined
active/removal polling. A known mismatched protocol is rejected before reserving a seat; missing protocol fields
retain the legacy admission behavior. UDP handshakes still enforce the actual protocol.

These are serial service tests. They do not establish Cloudflare scheduling/storage behavior, UDP hole punching,
or a match between separate networks. The game's retry queue and online rollback need their own checks too.
