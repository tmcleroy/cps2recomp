# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
"""Three local processes: a late viewer matches confirmed hashes and leaves without ending play."""
import os
from pathlib import Path
import re
import subprocess
import time
import json
from urllib.request import urlopen

game = os.environ.get("GAME", "mvsc")
work = Path("work") / game / "spectators"
work.mkdir(parents=True, exist_ok=True)
exe = Path("build") / game / (game + (".exe" if os.name == "nt" else ""))
port = 10000 + os.getpid() % 20000
service = os.environ.get("SERVICE", "off")
# The service filters by the client protocol; keep this test paired with the build's wire version.
protocol_header = Path(__file__).resolve().parents[1] / "port" / "netplay.h"
protocol_match = re.search(r"\bkProtocol\s*=\s*(\d+)\s*;", protocol_header.read_text())
if protocol_match is None:
    raise RuntimeError("cannot read the client protocol from port/netplay.h")
protocol = int(protocol_match.group(1))
join_frame = int(os.environ.get("JOIN_FRAME", "4300"))
player_frames = join_frame + 2400
# FIRST_TO=1|2: the players play a whole FT5 (FT10) set; the viewer, coming in partway, must count the same games
first_to = os.environ.get("FIRST_TO", "")
if first_to:
    join_frame = int(os.environ.get("JOIN_FRAME", "20000"))
    player_frames = 200000
processes = []
files = []

def start(name, args, frames):
    settings = work / (name + ".ini")
    settings.write_text("")
    eeprom = work / (name + ".eeprom")
    eeprom.unlink(missing_ok=True)
    (work / (name + ".hash")).unlink(missing_ok=True)
    log = (work / (name + ".log")).open("w")
    files.append(log)
    p = subprocess.Popen([str(exe), "--rom", os.environ.get("ROM", "rom/" + game), "--headless",
        *(["--unthrottled"] if first_to else []),
        "--frames", str(frames), "--settings", str(settings), "--eeprom", str(eeprom),
        "--net-service", service, "--hash-no-picture", "--hash-no-sound", "--hash-log", str(work / (name + ".hash")),
        "--net-fake", "20:10:5", *args], stdout=log, stderr=log)
    processes.append(p)
    return p

def hashes(name):
    return dict(line.split() for line in (work / (name + ".hash")).read_text().splitlines())

try:
    set_features = ["--features", "online_set=" + first_to + ",free_play,skip_intro"] if first_to else []
    host = start("host", ["--host", str(port), "--net-spectators", "--net-list", "--auto", "11:0:1", "--net-name", "Host",
                          *set_features], player_frames)
    time.sleep(.5)
    address = "127.0.0.1:" + str(port)
    def lobby(watching):
        for _ in range(30):
            with urlopen(service + f"/v1/lobby?protocol={protocol}", timeout=5) as r:
                matches = json.load(r)["matches"]
            found = [m for m in matches if m["name"] == "Host" and m.get("watching", False) == watching]
            if found:
                return found[0]["code"]
            time.sleep(.5)
        raise AssertionError("host never appeared in the lobby with expected watch status")
    if service != "off":
        address = lobby(False)
    guest_address = "127.0.0.1:" + str(port) if os.environ.get("DIRECT_JOIN") == "1" else address
    guest = start("guest", ["--join", guest_address, "--auto", "12:0:1", "--net-name", "Guest"], player_frames)
    # Joining after the history window catches admission being mistaken for a slow viewer:
    # a new viewer's zero cursor must not be checked before its starting snapshot is assigned.
    deadline = time.monotonic() + (900 if first_to else 180)
    while True:
        assert host.poll() is None and guest.poll() is None, "player stopped before spectator admission"
        path = work / "host.hash"
        lines = path.read_text().splitlines() if path.exists() else []
        complete = [line.split() for line in lines if len(line.split()) == 2]
        if complete and int(complete[-1][0]) >= join_frame:
            break
        assert time.monotonic() < deadline, "host did not reach the late-join test frame"
        time.sleep(.25)
    if service != "off":
        assert lobby(True) == address
    if first_to:
        watcher = start("watcher", ["--watch", address, "--auto", "99:0:1"], player_frames)
        assert host.wait(timeout=1800) == 0 and guest.wait(timeout=60) == 0, "player failed"
        assert watcher.wait(timeout=60) == 0, "spectator failed: inspect watcher.log"
        def events(name):
            return [line.split() for line in (work / (name + ".log")).read_text().splitlines() if line.startswith("online: set FT")]
        w = events("watcher")
        start_at = min(map(int, hashes("watcher")))
        h = [e for e in events("host") if int(e[4]) >= start_at]
        assert w and w == h, f"viewer's set differs: host {h}, viewer {w}"
        assert w[-1][-1] == "complete", "the set did not finish while watched"
        print(f"ok: viewer came in at frame {start_at} and counted the same {len(w)} games to {w[-1][6]}")
        raise SystemExit(0)
    watcher = start("watcher", ["--watch", address, "--auto", "99:0:1"], join_frame + 1400)
    assert watcher.wait(timeout=45) == 0, "spectator failed: inspect watcher.log"
    assert host.poll() is None and guest.poll() is None, "viewer leaving stopped a player"
    assert host.wait(timeout=60) == 0 and guest.wait(timeout=10) == 0, "player failed"
    a, b, w = hashes("host"), hashes("guest"), hashes("watcher")
    shared = a.keys() & b.keys()
    assert len(shared) >= player_frames - 50 and all(a[f] == b[f] for f in shared), "players differ"
    shared = a.keys() & w.keys()
    assert len(shared) >= 200 and all(a[f] == w[f] for f in shared), "viewer differs"
    assert min(map(int, w)) > join_frame, "viewer did not join late"
    print(f"ok: spectator joined at frame {min(map(int, w))} and matched {len(shared)} confirmed frames; viewer controls and exit did not affect players")
finally:
    for p in processes:
        if p.poll() is None:
            p.terminate()
            p.wait(timeout=10)
    for f in files:
        f.close()
