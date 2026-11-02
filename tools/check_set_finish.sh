#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 cps2recomp contributors. Part of cps2recomp.
# A full first-to set: real game results, corrected predictions, and a clean exit on both peers.
# GAME=mvsc FIRST_TO=1 tools/check_set_finish.sh [maximum frames]
set -eu
cd "$(dirname "$0")/.."
GAME=${GAME:?choose a supported game}
ROM=${ROM:-rom/$GAME}
choice=${FIRST_TO:-1}
case "$choice" in 1) target=5 ;; 2) target=10 ;; *) echo 'FIRST_TO must be 1 or 2'; exit 1 ;; esac
frames=${1:-150000}
work=work/$GAME/set-finish-$target
mkdir -p "$work"
port=$((9000 + RANDOM % 1000))
side() {
    local name=$1
    shift
    : > "$work/$name.ini"
    rm -f "$work/$name.eeprom"
    "build/$GAME/$GAME" --rom "$ROM" --headless --unthrottled --frames "$frames" \
        --settings "$work/$name.ini" --eeprom "$work/$name.eeprom" --net-service off --net-fake 5:3:5 \
        --hash-no-picture --hash-no-sound --hash-log "$work/$name.hash" "$@" 2> "$work/$name.log"
}
side host --host "$port" --features "online_set=$choice,free_play,skip_intro" --auto 11:0:1 &
host=$!
trap 'kill "$host" 2>/dev/null || true' EXIT
sleep 0.5
side join --join "127.0.0.1:$port" --features online_set=0 --auto 12:0:1
wait "$host"
trap - EXIT
for name in host join; do
    grep -E "^online: Player [12] wins the FT$target set " "$work/$name.log" > "$work/$name.result"
    [ "$(wc -l < "$work/$name.result")" -eq 1 ] || { echo "FAILED: $name did not finish once"; exit 1; }
    if grep -Eq 'Lost the connection|other player left|desync|disagree' "$work/$name.log"; then
        echo "FAILED: $name did not finish cleanly"; exit 1
    fi
    grep '^online: set FT' "$work/$name.log" > "$work/$name.scores"
done
cmp "$work/host.result" "$work/join.result"
cmp "$work/host.scores" "$work/join.scores"
lines=$(wc -l < "$work/host.hash")
joined=$(wc -l < "$work/join.hash")
if [ "$joined" -lt "$lines" ]; then lines=$joined; fi
[ "$lines" -gt 0 ] && [ "$lines" -lt "$frames" ]
cmp <(head -n "$lines" "$work/host.hash") <(head -n "$lines" "$work/join.hash")
cat "$work/host.result"
echo "ok: both peers ended FT$target successfully; scores and $lines confirmed hashes match"
