// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Morphus56K and contributors. Part of cps2recomp.
// The actual Worker and Durable Object handlers, with storage and delivery stood in for in memory.
// Most requests are serial; deferred RPC cases explicitly interleave handlers at external-await boundaries.
// This does not model Cloudflare's complete event scheduling or the game's UDP transport.

import assert from "node:assert/strict";
import test from "node:test";
import worker, { Lobby, Match, Room, Stats } from "../src/index.js";

class Storage {
  values = new Map();
  alarm = null;
  async get(key) { return structuredClone(this.values.get(key)); }
  async put(key, value) { this.values.set(key, structuredClone(value)); }
  async deleteAll() { this.values.clear(); }
  async setAlarm(when) { this.alarm = when; }
}

class Namespace {
  objects = new Map();
  constructor(make) { this.make = make; }
  idFromName(name) { return name; }
  get(id) {
    if (!this.objects.has(id)) {
      const state = { storage: new Storage() };
      const handler = this.make(state);
      this.objects.set(id, { state, handler, fetch: (url, init) => handler.fetch(new Request(url, init)) });
    }
    return this.objects.get(id);
  }
}

function fixture(t) {
  let now = Date.UTC(2026, 0, 1);
  t.mock.method(Date, "now", () => now);
  const env = {};
  env.MATCHES = new Namespace((state) => new Match(state, env));
  env.LOBBIES = new Namespace((state) => new Lobby(state));
  env.ROOMS = new Namespace((state) => new Room(state, env));
  env.STATS = new Namespace((state) => new Stats(state));
  const about = { game: "mvsc", title: "Marvel vs. Capcom", build: "test-build", version: "test-version", protocol: 10 };
  async function request(path, body) {
    const pending = [];
    const response = await worker.fetch(new Request("https://online.test" + path,
      body === undefined ? {} : { method: "POST", body: JSON.stringify(body) }), env,
      { waitUntil: (promise) => pending.push(promise) });
    await Promise.all(pending);
    return { status: response.status, body: await response.json() };
  }
  const host = (overrides = {}) => request("/v1/host", {
    ...about, addresses: ["127.0.0.1:7000"], listed: true, name: "Host", ...overrides,
  });
  const join = (code, port, overrides = {}) => request("/v1/join/" + code, {
    ...about, addresses: ["127.0.0.1:" + port], people: 1, ...overrides,
  });
  const poll = (host, gone = "", active = false) => request("/v1/host/" + host.code + "?secret=" +
    encodeURIComponent(host.secret) + (gone ? "&gone=" + encodeURIComponent(gone) : "") + (active ? "&active=1" : ""));
  const watch = (code, overrides = {}) => request("/v1/watch/" + code, {
    ...about, addresses: ["127.0.0.1:9000"], ...overrides,
  });
  return { env, about, request, host, join, poll, watch, advance: (ms) => { now += ms; } };
}

async function start(f, overrides = {}) {
  const answer = await f.host(overrides);
  assert.equal(answer.status, 200);
  assert.match(answer.body.code, /^[23456789ABCDEFGHJKMNPQRSTUVWXYZ]{6}$/);
  assert.equal(typeof answer.body.secret, "string");
  return answer.body;
}

async function four(f, overrides = {}) {
  const host = await start(f, { players: 4, here: 1, ...overrides });
  for (let i = 1; i <= 3; ++i) {
    const joined = await f.join(host.code, 7000 + i);
    assert.equal(joined.status, 200);
    assert.equal(joined.body.ticket, i);
  }
  return host;
}

// Pause one invitation RPC, where Durable Objects may run another request before this one resumes.
function deferInvite(t, f, reply = { given: false }) {
  const room = f.env.ROOMS.get("all"), original = room.fetch;
  let entered, release, paused = false;
  const waiting = new Promise((resolve) => { entered = resolve; });
  const held = new Promise((resolve) => { release = resolve; });
  room.fetch = async (url, init) => {
    if (!paused && new URL(url).pathname === "/invite") {
      paused = true;
      entered();
      await held;
      return Response.json(reply);
    }
    return original(url, init);
  };
  t.after(() => { release(); room.fetch = original; });
  return { waiting, release };
}

async function invitedHost(f) {
  // Only the external Room response is stood in for: all match mutations use the production handler.
  f.env.ROOMS.get("all").fetch = async () => Response.json({ given: true });
  return start(f, { players: 4, here: 1, invite: "abcdef123456" });
}

test("rehosting cannot roll back tickets while its invitation RPC is in flight", async (t) => {
  const f = fixture(t);
  const host = await start(f, { players: 4, here: 1 });
  assert.equal((await f.join(host.code, 7001)).body.ticket, 1);
  const rpc = deferInvite(t, f);
  const rehosting = f.host({ ...host, players: 4, here: 1, invite: "abcdef123456" });
  await rpc.waiting;
  const during = await f.join(host.code, 7002);
  assert.equal(during.status, 200);
  rpc.release();
  assert.equal((await rehosting).status, 200);
  const after = await f.join(host.code, 7003);
  assert.equal(after.status, 200);
  assert.ok(after.body.ticket > during.body.ticket, "rehosting must not reuse the concurrently issued ticket");
  assert.deepEqual((await f.poll(host, String(during.body.ticket))).body.joiners,
    [`${after.body.ticket} 127.0.0.1:7003`], "the earlier ticket must not evict the next player");
});

test("an away invitation response preserves a join admitted during the poll", async (t) => {
  const f = fixture(t), host = await invitedHost(f);
  const rpc = deferInvite(t, f);
  const polling = f.poll(host);
  await rpc.waiting;
  const joined = await f.join(host.code, 7001);
  assert.equal(joined.status, 200);
  rpc.release();
  const answer = await polling;
  assert.equal(answer.status, 200);
  assert.deepEqual(answer.body.joiners, [`${joined.body.ticket} 127.0.0.1:7001`]);
  const saved = await f.env.MATCHES.get(host.code).state.storage.get("match");
  assert.equal(saved.tickets, joined.body.ticket);
  assert.equal(saved.joiners.length, 1);
  assert.equal(saved.invited, "away");
});

test("a stale invitation response cannot change a replacement match from the same millisecond", async (t) => {
  const f = fixture(t), host = await invitedHost(f);
  const rpc = deferInvite(t, f);
  const polling = f.poll(host);
  await rpc.waiting;
  assert.equal((await f.host({ ...host, players: 4, here: 2 })).status, 200);
  await f.join(host.code, 7002, { people: 2 });
  const storage = f.env.MATCHES.get(host.code).state.storage;
  const replacement = await storage.get("match");
  rpc.release();
  await polling;
  assert.deepEqual(await storage.get("match"), replacement);
});

test("a stale invitation response cannot resurrect an expired match", async (t) => {
  const f = fixture(t), host = await invitedHost(f);
  const rpc = deferInvite(t, f);
  const polling = f.poll(host);
  await rpc.waiting;
  f.advance(30 * 60 * 1000 + 1);
  assert.equal((await f.request("/v1/code/" + host.code)).status, 404);
  rpc.release();
  await polling;
  assert.equal(await f.env.MATCHES.get(host.code).state.storage.get("match"), undefined);
});

test("a delayed away response does not overwrite the invited player's decline", async (t) => {
  const f = fixture(t), host = await invitedHost(f);
  const rpc = deferInvite(t, f);
  const polling = f.poll(host);
  await rpc.waiting;
  const match = f.env.MATCHES.get(host.code);
  assert.equal((await match.fetch("https://match/declined")).status, 200);
  rpc.release();
  const answer = await polling;
  assert.equal(answer.body.invite, "declined");
  assert.equal((await match.state.storage.get("match")).invited, "declined");
});

test("a legacy host is two players, ignoring each machine's larger people setting", async (t) => {
  const f = fixture(t);
  const host = await start(f, { protocol: 7, here: 2 });
  const info = await f.request("/v1/code/" + host.code);
  assert.equal(info.body.players, 2);
  assert.equal(info.body.room, 1);
  assert.equal((await f.join(host.code, 7001, { people: 2, protocol: 7 })).status, 200);
  const poll = await f.poll(host);
  assert.equal(poll.body.joined, true);
  assert.deepEqual(poll.body.addresses, ["127.0.0.1:7001"]);
  const extra = await f.join(host.code, 7002, { protocol: 7 });
  assert.equal(extra.status, 409);
  assert.equal(extra.body.error, "Someone has joined that match already.");
});

// Every ordered partition of four people with one or two at the host and one or two at each joiner.
for (const [here, joiners] of [[1, [1, 1, 1]], [1, [1, 2]], [1, [2, 1]], [2, [1, 1]], [2, [2]]]) {
  test("four people admitted in arrival order: " + [here, ...joiners].join(" + "), async (t) => {
    const f = fixture(t);
    const host = await start(f, { players: 4, here });
    let left = 4 - here;
    const expected = [];
    for (let i = 0; i < joiners.length; ++i) {
      const info = await f.request("/v1/code/" + host.code);
      assert.equal(info.status, 200);
      assert.equal(info.body.players, 4);
      assert.equal(info.body.room, left);
      const joined = await f.join(host.code, 7001 + i, { people: joiners[i] });
      assert.equal(joined.status, 200);
      assert.equal(joined.body.ticket, i + 1);
      assert.deepEqual(joined.body.addresses, ["127.0.0.1:7000"]);
      expected.push(`${i + 1} 127.0.0.1:${7001 + i}`);
      assert.deepEqual((await f.poll(host)).body.joiners, expected);
      left -= joiners[i];
    }
    assert.equal(left, 0);
    assert.equal((await f.request("/v1/code/" + host.code)).status, 409);
    const extra = await f.join(host.code, 7010);
    assert.equal(extra.status, 409);
    assert.equal(extra.body.error, "That match is full.");
    assert.deepEqual((await f.poll(host)).body.joiners, expected);
  });
}

test("a two-person joiner cannot consume the last single seat or a ticket", async (t) => {
  const f = fixture(t);
  const host = await start(f, { players: 4, here: 1 });
  assert.equal((await f.join(host.code, 7001, { people: 2 })).body.ticket, 1);
  const refused = await f.join(host.code, 7002, { people: 2 });
  assert.equal(refused.status, 409);
  assert.match(refused.body.error, /room for one more player/);
  assert.equal((await f.request("/v1/code/" + host.code)).body.room, 1);
  assert.equal((await f.join(host.code, 7003)).body.ticket, 2);
});

for (const [field, value, message] of [["game", "another-game", /another game/],
                                     ["build", "another-build", /different builds/],
                                     ["version", "another-version", /same version/],
                                     ["protocol", 9, /different online protocols/]]) {
  test("wrong " + field + " does not reserve a place", async (t) => {
    const f = fixture(t);
    const host = await start(f, { players: 4, here: 1 });
    const refused = await f.join(host.code, 7001, { [field]: value });
    assert.equal(refused.status, 409);
    assert.match(refused.body.error, message);
    assert.equal((await f.request("/v1/code/" + host.code)).body.room, 3);
    assert.equal((await f.join(host.code, 7002)).body.ticket, 1);
  });
}

test("wrong host secret cannot release someone else's reservation", async (t) => {
  const f = fixture(t);
  const host = await four(f);
  const refused = await f.request("/v1/host/" + host.code + "?secret=wrong&gone=2");
  assert.equal(refused.status, 404);
  assert.equal((await f.poll(host)).body.joiners.length, 3);
  assert.equal((await f.join(host.code, 7010)).status, 409);
});

test("gone before delivery: retrying the removal opens the blocked seat", async (t) => {
  const f = fixture(t);
  const host = await four(f);
  // The first HTTP request never reaches the Worker. Ordinary polls must not silently evict someone.
  for (let i = 0; i < 3; ++i) {
    assert.equal((await f.poll(host)).body.joiners.length, 3);
  }
  assert.equal((await f.join(host.code, 7010)).status, 409);
  const acknowledged = await f.poll(host, "2");
  assert.equal(acknowledged.status, 200);
  assert.deepEqual(acknowledged.body.joiners, ["1 127.0.0.1:7001", "3 127.0.0.1:7003"]);
  assert.equal((await f.join(host.code, 7010)).body.ticket, 4);
});

test("gone response lost: retry is idempotent and cannot evict the replacement", async (t) => {
  const f = fixture(t);
  const host = await four(f);
  // The Worker commits the removal, but its answer is lost before the host receives it.
  await f.poll(host, "2");
  assert.equal((await f.join(host.code, 7010)).body.ticket, 4);
  const retried = await f.poll(host, "2,2");
  assert.equal(retried.status, 200);
  assert.deepEqual(retried.body.joiners, ["1 127.0.0.1:7001", "3 127.0.0.1:7003", "4 127.0.0.1:7010"]);
  assert.equal((await f.join(host.code, 7011)).status, 409);
});

test("gone can release two people without removing other joiners", async (t) => {
  const f = fixture(t);
  const host = await start(f, { players: 4, here: 1 });
  await f.join(host.code, 7001, { people: 2 });
  await f.join(host.code, 7002);
  await f.poll(host, "1");
  assert.equal((await f.request("/v1/code/" + host.code)).body.room, 2);
  const replacement = await f.join(host.code, 7003, { people: 2 });
  assert.equal(replacement.status, 200);
  assert.equal(replacement.body.ticket, 3);
  assert.deepEqual((await f.poll(host)).body.joiners, ["2 127.0.0.1:7002", "3 127.0.0.1:7003"]);
});

test("unknown and invalid gone tickets do not change the roster", async (t) => {
  const f = fixture(t);
  const host = await four(f);
  const before = (await f.poll(host)).body.joiners;
  const after = await f.poll(host, "0,-1,1.5,not-a-ticket,999");
  assert.equal(after.status, 200);
  assert.deepEqual(after.body.joiners, before);
  assert.equal((await f.join(host.code, 7010)).status, 409);
});

test("a partial match remains listed, a full one leaves, and gone allows relisting", async (t) => {
  const f = fixture(t);
  const host = await start(f, { players: 4, here: 1 });
  const listed = async () => (await f.request("/v1/lobby/mvsc?protocol=10")).body.matches.map((m) => m.code);
  assert.deepEqual(await listed(), [host.code]);
  assert.deepEqual((await f.request("/v1/lobby/mvsc?protocol=7")).body.matches, []);
  await f.join(host.code, 7001);
  await f.join(host.code, 7002);
  assert.deepEqual(await listed(), [host.code]);
  await f.join(host.code, 7003);
  assert.deepEqual(await listed(), []);
  f.advance(10001);
  await f.poll(host, "2");
  assert.deepEqual(await listed(), [host.code]);
});

test("the same host can reopen its full code and start a new admission roster", async (t) => {
  const f = fixture(t);
  const host = await four(f);
  const again = await f.host({ ...host, players: 4, here: 2 });
  assert.equal(again.status, 200);
  assert.deepEqual(again.body, host);
  assert.deepEqual((await f.poll(host)).body, { joined: false });
  assert.equal((await f.request("/v1/code/" + host.code)).body.room, 2);
  assert.equal((await f.join(host.code, 7010, { people: 2 })).status, 200);
});

test("late gone requests from earlier live rehosts cannot evict a replacement player", async (t) => {
  const f = fixture(t);
  const host = await start(f, { players: 4, here: 2 });
  let oldTicket = (await f.join(host.code, 7001, { people: 2 })).body.ticket;
  const stale = [];
  for (let round = 0; round < 3; round++) {
    stale.push(oldTicket);
    assert.equal((await f.host({ ...host, players: 4, here: 2 })).status, 200);
    const port = 7010 + round;
    const joined = await f.join(host.code, port, { people: 2 });
    assert.equal(joined.status, 200);
    // The old authenticated request arrives after /host reopened the same code and a new player joined it.
    const poll = await f.poll(host, stale.join(","));
    assert.deepEqual(poll.body.joiners, [`${joined.body.ticket} 127.0.0.1:${port}`],
      "a delayed removal from an earlier match evicted the new player");
    assert.ok(joined.body.ticket > oldTicket, "live rehosting must not reuse player ticket numbers");
    oldTicket = joined.body.ticket;
  }
});

test("a different host cannot replace the existing code's roster", async (t) => {
  const f = fixture(t);
  const host = await four(f);
  const other = await f.host({ code: host.code, secret: "wrong", players: 4, here: 2 });
  assert.equal(other.status, 200);
  assert.notEqual(other.body.code, host.code);
  assert.equal((await f.poll(host)).body.joiners.length, 3);
  assert.equal((await f.join(host.code, 7010)).status, 409);
});

test("a 30-minute code expires and its host can renew it with the same secret", async (t) => {
  const f = fixture(t);
  const host = await start(f, { players: 4, here: 1 });
  f.advance(30 * 60 * 1000 + 1);
  assert.equal((await f.request("/v1/code/" + host.code)).status, 404);
  assert.equal((await f.poll(host)).status, 404);
  const renewed = await f.host({ ...host, players: 4, here: 1 });
  assert.equal(renewed.status, 200);
  assert.deepEqual(renewed.body, host);
  assert.equal((await f.request("/v1/code/" + host.code)).body.room, 3);
  assert.equal((await f.join(host.code, 7001)).status, 200);
});

test("the expiry alarm removes both the saved match and its lobby entry", async (t) => {
  const f = fixture(t);
  const host = await start(f, { players: 4, here: 1 });
  const object = f.env.MATCHES.get(host.code);
  assert.equal(object.state.storage.alarm, Date.now() + 30 * 60 * 1000);
  await object.handler.alarm();
  assert.equal((await f.request("/v1/code/" + host.code)).status, 404);
  assert.deepEqual((await f.request("/v1/lobby?protocol=10")).body.matches, []);
});

test("a stored pre-upgrade match still exposes the one joiner's old addresses field", async (t) => {
  const f = fixture(t);
  const host = await start(f, { protocol: 7 });
  const storage = f.env.MATCHES.get(host.code).state.storage;
  const legacy = await storage.get("match");
  for (const key of ["players", "here", "joiners", "tickets"]) delete legacy[key];
  legacy.joiner = ["127.0.0.1:7001"];
  await storage.put("match", legacy);
  const poll = await f.poll(host);
  assert.equal(poll.status, 200);
  assert.equal(poll.body.joined, true);
  assert.deepEqual(poll.body.addresses, legacy.joiner);
  assert.equal((await f.request("/v1/code/" + host.code)).status, 409);
  assert.equal((await f.join(host.code, 7002, { protocol: 7 })).status, 409);
});

test("an empty stored pre-upgrade match accepts one new joiner", async (t) => {
  const f = fixture(t);
  const host = await start(f, { protocol: 7 });
  const storage = f.env.MATCHES.get(host.code).state.storage;
  const legacy = await storage.get("match");
  for (const key of ["players", "here", "joiners", "tickets"]) delete legacy[key];
  legacy.joiner = null;
  await storage.put("match", legacy);
  const joined = await f.join(host.code, 7001, { people: 2, protocol: 7 });
  assert.equal(joined.status, 200);
  assert.equal(joined.body.ticket, 1);
  assert.equal((await f.join(host.code, 7002, { protocol: 7 })).status, 409);
});

test("missing protocol retains pre-protocol admission behavior", async (t) => {
  const f = fixture(t);
  const oldHost = await start(f, { protocol: undefined });
  assert.equal((await f.join(oldHost.code, 7001)).status, 200);
  const currentHost = await start(f);
  assert.equal((await f.join(currentHost.code, 7002, { protocol: undefined })).status, 200);
});

test("four-player spectators do not reserve seats or expose private admission data in the lobby", async (t) => {
  const f = fixture(t);
  const host = await start(f, { players: 4, here: 1, spectators: true });
  assert.equal((await f.watch(host.code)).status, 409);
  const joiners = [];
  for (let player = 1; player <= 3; player++) {
    assert.equal((await f.join(host.code, 7000 + player)).body.ticket, player);
    joiners.push(`${player} 127.0.0.1:${7000 + player}`);
  }
  assert.equal((await f.watch(host.code)).status, 409, "full reservations do not imply the match started");
  f.advance(10001);
  await f.poll(host, "", true);
  const viewer = await f.watch(host.code);
  assert.equal(viewer.status, 200);
  assert.equal(typeof viewer.body.token, "string");
  assert.deepEqual(viewer.body.addresses, ["127.0.0.1:7000"]);
  const poll = await f.poll(host, "", true);
  assert.deepEqual(poll.body.joiners, joiners);
  assert.deepEqual(poll.body.addresses, ["127.0.0.1:7001"], "legacy first-joiner address remains available");
  assert.deepEqual(poll.body.viewers, [{ token: viewer.body.token, addresses: ["127.0.0.1:9000"] }]);
  assert.deepEqual(poll.body.watchers, [viewer.body.token + "@127.0.0.1:9000"]);
  assert.equal((await f.join(host.code, 7010)).status, 409);
  const lobby = await f.request("/v1/lobby?protocol=10");
  assert.equal(lobby.body.matches.length, 1);
  assert.equal(lobby.body.matches[0].watching, true);
  const publicText = JSON.stringify(lobby.body);
  assert.equal(publicText.includes("127.0.0.1"), false);
  assert.equal(publicText.includes(viewer.body.token), false);
  assert.equal(publicText.includes(host.secret), false);
});

test("active direct-address four-player matches reject joins despite unused service reservations", async (t) => {
  const f = fixture(t);
  const host = await start(f, { players: 4, here: 1, spectators: true });
  await f.poll(host, "", true);
  assert.equal((await f.request("/v1/code/" + host.code)).status, 409);
  assert.equal((await f.join(host.code, 7001)).status, 409);
  assert.equal((await f.watch(host.code)).status, 200);
  const poll = await f.poll(host, "", true);
  assert.deepEqual(poll.body.joiners, []);
  assert.deepEqual(poll.body.addresses, []);
});

test("one host poll carries both gone acknowledgments and active spectator introductions", async (t) => {
  const f = fixture(t);
  const host = await four(f, { spectators: true });
  await f.poll(host, "", true);
  const viewer = await f.watch(host.code);
  const poll = await f.poll(host, "2", true);
  assert.deepEqual(poll.body.joiners, ["1 127.0.0.1:7001", "3 127.0.0.1:7003"]);
  assert.equal(poll.body.viewers[0].token, viewer.body.token);
  assert.equal((await f.join(host.code, 7010)).status, 409, "running match cannot refill a released reservation");
  await f.poll(host, "2");
  assert.equal((await f.watch(host.code)).status, 409);
  assert.equal((await f.request("/v1/code/" + host.code)).body.room, 1);
  assert.equal((await f.join(host.code, 7010)).body.ticket, 4);
});

test("unauthenticated polls cannot enable watching or evict a player", async (t) => {
  const f = fixture(t);
  const host = await four(f, { spectators: true });
  const forged = await f.request("/v1/host/" + host.code + "?secret=wrong&active=1&gone=2");
  assert.equal(forged.status, 404);
  assert.equal((await f.watch(host.code)).status, 409);
  assert.equal((await f.poll(host)).body.joiners.length, 3);
});

test("spectators require host opt-in and matching game, version, build and protocol", async (t) => {
  const f = fixture(t);
  const closed = await four(f);
  await f.poll(closed, "", true);
  assert.equal((await f.watch(closed.code)).status, 409);
  const host = await four(f, { spectators: true });
  await f.poll(host, "", true);
  for (const overrides of [{ game: "other" }, { version: "other" }, { build: "other" }, { protocol: 9 }]) {
    assert.equal((await f.watch(host.code, overrides)).status, 409);
  }
  assert.equal((await f.watch(host.code, { addresses: [] })).status, 400);
  assert.deepEqual((await f.poll(host, "", true)).body.viewers, []);
});

test("viewer admission is bounded, token retries update the address, and four player tickets stay intact", async (t) => {
  const f = fixture(t);
  const host = await four(f, { spectators: true });
  const roster = (await f.poll(host, "", true)).body.joiners;
  const first = await f.watch(host.code);
  const retry = await f.watch(host.code, { token: first.body.token, addresses: ["127.0.0.1:9001"] });
  assert.equal(retry.body.token, first.body.token);
  for (let i = 1; i < 8; i++) assert.equal((await f.watch(host.code)).status, 200);
  assert.equal((await f.watch(host.code)).status, 429);
  const poll = await f.poll(host, "", true);
  assert.equal(poll.body.viewers.length, 8);
  assert.deepEqual(poll.body.viewers[0].addresses, ["127.0.0.1:9001"]);
  assert.deepEqual(poll.body.joiners, roster);
});

test("stale host heartbeats refuse watchers and expired viewer reservations are reclaimed", async (t) => {
  const f = fixture(t);
  const host = await four(f, { spectators: true });
  await f.poll(host, "", true);
  const old = await f.watch(host.code);
  f.advance(25001);
  assert.equal((await f.watch(host.code)).status, 409);
  f.advance(5000);
  assert.deepEqual((await f.poll(host, "", true)).body.viewers, []);
  const next = await f.watch(host.code, { token: old.body.token });
  assert.equal(next.status, 200);
  assert.notEqual(next.body.token, old.body.token);
});

test("rehosting clears viewers and player reservations before the next match", async (t) => {
  const f = fixture(t);
  const host = await four(f, { spectators: true });
  await f.poll(host, "", true);
  const old = await f.watch(host.code);
  assert.equal((await f.host({ ...host, players: 4, here: 2, spectators: true })).status, 200);
  assert.equal((await f.watch(host.code, { token: old.body.token })).status, 409);
  assert.deepEqual((await f.poll(host)).body, { joined: false });
  const lobby = await f.request("/v1/lobby?protocol=10");
  assert.equal(lobby.body.matches[0].watching, false);
  await f.join(host.code, 7010, { people: 2 });
  await f.poll(host, "", true);
  const next = await f.watch(host.code, { token: old.body.token });
  assert.notEqual(next.body.token, old.body.token);
  const poll = await f.poll(host, "", true);
  assert.equal(poll.body.joiners.length, 1);
  assert.equal(poll.body.viewers.length, 1);
});

test("a running spectator-enabled match extends its lifetime on authenticated heartbeat", async (t) => {
  const f = fixture(t);
  const host = await four(f, { spectators: true });
  f.advance(30 * 60 * 1000 - 1);
  assert.equal((await f.poll(host, "", true)).status, 200);
  f.advance(2);
  assert.equal((await f.watch(host.code)).status, 200);
  f.advance(30 * 60 * 1000 + 1);
  assert.equal((await f.poll(host, "", true)).status, 404);
});
