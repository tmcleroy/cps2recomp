# Online reliability checks

The local checks need no deployed introduction service. Run the service checks without ROM data:

```sh
node --test online/test/*.test.js
```

These use the production Worker with in-memory Durable Object substitutes. They check admission and reservation
semantics, not Cloudflare deployment or real routers. The protocol checks exercise the production C++ networking
code with synthetic state and loopback packets:

```sh
tools/check_netplay.sh
```

SDL 3 development files, zlib, a C++20 compiler, Python 3, curl and pkg-config are required. Use `SDL_STATIC=1` when
linking the static SDL build. `make check-online-service` and `make check-netplay` run these same checks. They must
preserve departures when a poll fails, acknowledge only those included in the successful poll, and allow repeats. A host
reopening a match must send reconnect messages even if earlier end messages are lost or reordered. An intentional
host exit must still end the clients' sessions.

The C++ suite also sends actual cumulative input packets through loopback sockets: four different streams,
reordering, duplicates, a delayed peer, retransmission, host forwarding and rejection of a stream sent by the wrong
peer. It checks that prediction stalls at `kMostAhead` and resumes when missing controls arrive. Frame simulation
and confirmed hashes are checked separately by the gameplay runs below.

`make check-crossfever-reset` checks the production MVC gauge hook with a synthetic memory bus: boot clears every
prior counter phase, ordinary match steps retain it, and save/load restores it. It needs no ROM data and is a
boundary regression, not a substitute for playing through reset and rematch with the game.

## Gameplay and player placement

Build the game first, with your own ROM set. Each layout lists people per machine in arrival order, host first:

```sh
for layout in 1,1 2,2 2,1,1 1,2,1 1,1,2 1,1,1,1; do
    PEOPLE="$layout" GAME=mvsc tools/check_online.sh 3000 || exit 1
done
```

The merged online features can be combined with each layout. For example:

```sh
WATCH=1 FIRST_TO=1 PEOPLE=1,1,1,1 GAME=mvsc tools/check_online.sh 3000
```

`FIRST_TO=0|1|2` checks Casual/FT5/FT10 negotiation: every joining machine asks for a different format, and every
playing machine must use the host's and agree on any set-score events through the shared confirmed frame. A short
run may contain no scored game; it does not establish completion of a whole FT5/FT10 set. `WATCH=1` joins a viewer
after play begins, compares at least 100 of its absolute frame hashes against the host, and verifies its attempted
chat never reaches the players. The viewer uses synthetic inputs too; its playback must still match the match.

`--watch CODE` (or a direct address) now starts a spectator. Memory-write diagnostics use
`--watch-writes LOW[-HIGH]` instead, with hexadecimal 68000 addresses. Update older diagnostic commands that
used `--watch` for memory writes; that spelling now belongs to online viewing.

Each run compares confirmed state hashes between the machines and against replay of the recorded inputs. It also
checks controller ownership, names and chat delivery with injected delay, jitter and packet loss. The four-person
runs enable Cross Fever. With layout `1,1,2`, the final pair receives P3 and P4, because P1 and P2 were already
assigned; teams remain P1/P3 versus P2/P4. With `1,2,1`, the joining pair receives P2/P4 and the last arrival P3.

Also repeat after menu-hosted disconnects and rejoining, reset/rehost, and ordinary rematches. Check joining and
all six attack buttons for each owner, tags, simultaneous Duo, expiry and KO. Synthetic inputs and local processes
do not establish physical-controller or real internet acceptance.

## Internet acceptance still required

Record build, platform, people per machine, joining order, connection method, result, ping, stalls and desyncs.
Use four actual machines and cover:

| Network arrangement | Cases |
|---|---|
| One home network | LAN addresses; public-code joining through the same router |
| Separate home networks | Four independent connections; rotate the host |
| Restrictive/mobile network | Direct connection attempt; forwarded-port or VPN control |
| Uneven links | One slow peer, asymmetric delay, burst loss, packet reordering and duplication |
| Recovery | Every joining peer leaves in turn; reconnect and replace a peer; intentional host exit |

A pass includes a complete match and rematch, correct controller ownership, and matching confirmed state hashes.
Keep failure-stage evidence separate: lobby admission, address discovery, direct UDP handshake, game transfer,
simulation. Connection success and simulation correctness are different checks.

## Research guiding the next tests

- [Large-Scale Measurement of NAT Traversal (2026)](https://arxiv.org/abs/2604.12484) measures a different P2P system.
  Its separation of connection stages is useful here; its success rates are not measurements of this port.
- [Peer-to-Peer Communication Across NATs](https://arxiv.org/abs/cs/0603074) explains why address discovery and hole
  punching depend on router behavior. Use the network matrix above before making connectivity claims.
- [Formalizing Rollback Netcodes (OPODIS 2025)](https://drops.dagstuhl.de/entities/document/10.4230/LIPIcs.OPODIS.2025.11)
  studies a client-server model. Applying its reasoning to this host-forwarded simulation suggests tests that
  confirmed frames contain all required real inputs, prediction stops at its limit, and replay never needs a state
  older than the retained history. These are proposed applications, not a proof supplied by that paper.

Relay fallback remains a separate design: establish direct-connection failure reasons first, then evaluate relay
latency, capacity and operating cost. Neither a relay nor real internet acceptance is provided by the local tests.

## Targeted verification, November 14, 2026

These checks cover the networking fixes through `b394926`, including `6dff752`. They do not rerun or replace
`471f7f7`'s gameplay results below.

- **Set-result reordering:** the production C++ regression previously failed when `ready=true` was followed by
  a delayed `ready=false` for the same target and score. The later host goodbye then looked like a departure.
  Readiness now stays established for that exact result. Duplicate unready packets, changed scores, a different
  target, final goodbye and a new in-memory session are covered; all **10 protocol groups pass**. This adds no
  wire-session nonce and does not establish protection against packets replayed across sessions.
- **Invitation RPC interleaving:** five new regressions fail on the previous Worker and pass with the fix.
  Deferred external Room calls expose ticket rollback during rehost, loss of a concurrent admission during poll,
  same-millisecond replacement overwrite, expired-record resurrection and a decline overwritten by an away reply.
  The host stores its new generation before calling Room; a reply merges only into fresh, live, matching state.
  The generation is internal storage metadata, not a new HTTP or UDP field. Full Worker suite: **39/39 pass**;
  the separate spectator service suite also passes. These substitutes test controlled external-await boundaries,
  not Cloudflare's complete scheduler or a deployed service.

Reproduction uses the two ROM-free commands at the top of this document and
`node online/test-spectators.mjs`. On this arm64 Mac, the C++ check used Apple Clang 21 with
`CXXFLAGS='-isysroot /Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk'`; the installed SDL library still
warns that it targets a newer SDK. Whitespace and repository audit checks pass.

A fully deleted or expired service record can still restart its ticket counter; the stored generation protects
late invitation work, not old client removal tickets across record deletion. Four-machine internet/rematch,
physical-controller, hosted CI and production Worker deployment checks remain outstanding.

## Local verification, November 13, 2026

The merge with upstream `eea67d1` was checked on an arm64 Mac with the MVC interpreter build, Apple Clang 21,
SDL 3.4.20 and Node 25.9.0. The combined wire protocol is 10; new save states use version 12.

- The MVC build and `make check` passed, including the repository audit, shared-source check, first-to scoring,
  example build and launcher smoke checks.
- 34 Worker tests, the service's separate spectator suite and 10 production C++ protocol groups passed. Added
  cases cover same-code rehost ticket retention, protocol rejection before reservation, all-peer set agreement,
  reliable mode-event delivery, four-person mode restrictions and four-stream/read-only spectators.
- The Cross Fever reset/state suite passed. It also checks the marked MVC parts of both version-11 layouts,
  preserves the next section when legacy parts are absent, and rejects missing required version-12 parts. These
  focused section tests do not promise that arbitrary old whole-game saves load across different builds.
- All six layouts passed 3,000-frame runs with `WATCH=1 FIRST_TO=1`, 50 ms simulated delay, 25 ms jitter and 5%
  packet loss. Assignments, names, chat, host-selected FT5 and offline replay passed. The four-person runs enabled
  Cross Fever. Counts below are common confirmed hashes, not total frames requested:

| People per machine | Playing machines' matching frames | Viewer's matching host frames |
|---|---:|---:|
| `1,1` | 2997 | 2405 |
| `2,2` | 2998 | 2585 |
| `2,1,1` | 2987 | 2961 |
| `1,2,1` | 2990 | 2972 |
| `1,1,2` | 2989 | 2972 |
| `1,1,1,1` | 2988 | 2969 |

- A separate 6,700-frame player run used the production Worker through its loopback-only test service. The viewer
  joined by code at confirmed frame 4,576 (first hash 4,577), beyond the 4,096-frame history window, and matched
  1,124 host hashes. Its synthetic controls and exit did not affect the players; their hashes matched too.
- A 120-frame headless `--watch-writes FF0000-FF0001` run emitted memory-write diagnostics and exited normally,
  instead of attempting to spectate. Shell syntax and whitespace checks passed.

These runs establish local simulation, negotiation and replay behavior, not completion of a whole FT5/FT10 set.
All-peer set completion is covered by the production protocol test with synthetic scores; whole-set gameplay,
physical controllers, real-router/NAT and four-machine internet tests remain separate. The Worker was not deployed.
Windows/Linux runtime checks and hosted CI results are not implied by these macOS results. Build warnings included
existing deprecated codecvt use and the installed SDL library targeting a newer macOS SDK.

## Local verification, November 11, 2026

These results belong to commit `4b578bf`, before the later upstream merge. They do not validate changes added by
that merge; the expanded set/spectator checks above must be rerun on the merged build.

On an arm64 Mac, with the interpreter build of MVC, Apple Clang 21, SDL 3.4.20 and Node 25.9.0:

- `make check`: repository audit, shared-source check, example build, and launcher startup/screenshots passed.
- 22 Worker tests and seven production C++ protocol test groups passed. Temporarily restoring either early removal
  clearing or the reopening host's terminal goodbye made its regression test fail.
- All six layouts above passed 3,000-frame runs on the final build, with matching common confirmed-frame hashes,
  offline replay, assignments, names and chat. These were local processes with simulated network faults.
- The Cross Fever reset test passed every counter phase, disabled modes, ordinary-match continuity and save/load.
  A separate game-level boundary comparison replayed the existing Cross Fever movie from two pre-boot states
  differing only in the saved gauge phase: all 4,500 frame hashes and meter samples matched after the fix. Before
  the fix they diverged; the patched cold start also matched the original cold start. This is a modified serialized
  counter fixture, not natural menu-reset or remote-play acceptance, and it does not establish a prior peer desync.

Other games, physical controllers, real internet matches and Windows/Linux runtime execution remain separate
validation. CI is configured to run the service/reset tests and Linux protocol tests; the local results above do
not stand in for a completed CI run or a performance benchmark.
