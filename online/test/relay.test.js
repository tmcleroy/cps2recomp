// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
// GET /v1/relay: Cloudflare's TURN credentials, with the network stood in for.

import assert from "node:assert/strict";
import test from "node:test";
import worker, { kRelayTtl } from "../src/index.js";

const ask = (env) => worker.fetch(new Request("https://service/v1/relay?game=sf2hf", { headers: { "cf-connecting-ip": "203.0.113.7" } }), env, { waitUntil() {} });

test("no TURN key: no relay", async () => {
  const answer = await ask({});
  assert.equal(answer.status, 404);
});

test("credentials from the TURN key, the UDP server's address", async (t) => {
  const asked = [];
  t.mock.method(globalThis, "fetch", async (url, init) => {
    asked.push({ url, init });
    return Response.json({ iceServers: [{ urls: ["stun:stun.cloudflare.com:3478"] },
      { urls: ["turn:turn.cloudflare.com:3478?transport=udp", "turn:turn.cloudflare.com:3478?transport=tcp", "turns:turn.cloudflare.com:5349?transport=tcp"],
        username: "u1", credential: "c1" }] });
  });
  const answer = await ask({ TURN_KEY_ID: "key", TURN_KEY_API_TOKEN: "token" });
  assert.equal(answer.status, 200);
  assert.deepEqual(await answer.json(), { server: "turn.cloudflare.com:3478", username: "u1", credential: "c1", ttl: kRelayTtl });
  assert.equal(asked.length, 1);
  assert.match(asked[0].url, /\/v1\/turn\/keys\/key\/credentials\/generate-ice-servers$/);
  assert.equal(asked[0].init.headers.authorization, "Bearer token");
  assert.deepEqual(JSON.parse(asked[0].init.body), { ttl: kRelayTtl });
});

test("the older answer, one object", async (t) => {
  t.mock.method(globalThis, "fetch", async () => Response.json({ iceServers: { urls: ["turn:turn.cloudflare.com:3478?transport=udp"], username: "u2", credential: "c2" } }));
  const answer = await ask({ TURN_KEY_ID: "key", TURN_KEY_API_TOKEN: "token" });
  assert.equal((await answer.json()).username, "u2");
});

test("Cloudflare refusing: 502", async (t) => {
  t.mock.method(globalThis, "fetch", async () => new Response("no", { status: 401 }));
  const answer = await ask({ TURN_KEY_ID: "key", TURN_KEY_API_TOKEN: "token" });
  assert.equal(answer.status, 502);
});

test("rate limited by the asker's IP", async (t) => {
  t.mock.method(globalThis, "fetch", async () => assert.fail("asked Cloudflare past the limit"));
  const keys = [];
  const answer = await ask({ TURN_KEY_ID: "key", TURN_KEY_API_TOKEN: "token", RELAY_LIMIT: { limit: async ({ key }) => (keys.push(key), { success: false }) } });
  assert.equal(answer.status, 429);
  assert.deepEqual(keys, ["203.0.113.7"]);
});
