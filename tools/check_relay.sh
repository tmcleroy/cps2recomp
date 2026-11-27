#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
#
# Online play through the relay (port/turn.h, port/netplay.h's RelaySocket), on this machine: a TURN server (coturn's
# turnserver, on this machine's LAN address so it can send to the internet's addresses too), the online service's
# loopback copy (online/dev-service.mjs) and two copies of the game joining by a code. Each side is `on` (the relay
# beside the direct addresses) or `only` (--net-relay only: nothing directly once the relay is there, as on a network
# that lets nothing else through). Each case must make the match, play it with every packet late (50 ms, give or take
# 25) and one in twenty lost, and the two hash logs agree; a side `only` must have sent through the TURN server.
#
#   GAME=<id> [CASES="on:only only:on only:only on:on"] tools/check_relay.sh [FRAMES]
#
# Needs coturn (brew install coturn, apt install coturn), Node.js 22 and the game built (make GAME=<id>) with its set
# in rom/<id>. The STUN servers on the internet are asked as in a real match.
set -u
GAME=${GAME:?which game: GAME=<id>}
FRAMES=${1:-2000}
CASES=${CASES:-"on:only only:on only:only on:on"}
ROM=${ROM:-rom/$GAME}
cd "$(dirname "$0")/.."
turnserver=$(command -v turnserver || ls /opt/homebrew/opt/coturn/bin/turnserver 2>/dev/null) || { echo "needs coturn's turnserver"; exit 2; }
lan=${RELAY_IP:-$( (ipconfig getifaddr en0 || ipconfig getifaddr en1 || hostname -I | awk '{print $1}') 2>/dev/null | head -1)}
[ -n "$lan" ] || { echo "no LAN address for the TURN server (RELAY_IP=...)"; exit 2; }
work=$(mktemp -d "${TMPDIR:-/tmp}/cps2recomp-relay.XXXXXX")
trap 'kill $(jobs -p) 2>/dev/null; rm -rf "$work"' EXIT
turnPort=$((34000 + RANDOM % 1000)) servicePort=$((35000 + RANDOM % 1000))
status=0
for case in $CASES; do
    hostMode=${case%%:*} joinMode=${case#*:}
    dir="$work/$hostMode-$joinMode"
    mkdir -p "$dir"
    "$turnserver" -n --listening-ip="$lan" --relay-ip="$lan" --listening-port="$turnPort" --lt-cred-mech --user=test:secret \
        --realm=relay.test --allow-loopback-peers --no-tls --min-port=50000 --max-port=50200 --log-file=stdout --verbose \
        > "$dir/turn.log" 2>&1 &
    turn=$!
    PORT=$servicePort node online/dev-service.mjs > "$dir/service.log" 2>&1 &
    service=$!
    sleep 1.5
    export CPS2RECOMP_NET_SERVICE="http://127.0.0.1:$servicePort" CPS2RECOMP_NET_RELAY="$lan:$turnPort test secret"
    side() { # NAME MODE SEED, then more arguments
        local name=$1 mode=$2 seed=$3; shift 3
        timeout 120 "build/$GAME/$GAME" --rom "$ROM" --headless --frames "$FRAMES" --settings "$dir/$name.ini" \
            --eeprom "$dir/$name.eeprom" --hash-log "$dir/$name.txt" --auto "$seed:0:1" --net-relay "$mode" \
            --net-fake 50:25:5 --net-name "$name" "$@" 2> "$dir/$name.err"
    }
    side host "$hostMode" 11 --host $((36000 + RANDOM % 1000)) &
    host=$!
    code=
    for _ in $(seq 150); do
        code=$(grep -o "the match's code is [A-Z0-9-]*" "$dir/host.err" 2>/dev/null | awk '{print $5}')
        [ -n "$code" ] && break
        sleep 0.1
    done
    if [ -z "$code" ]; then
        echo "FAILED ($case): the host had no code"; status=1
    else
        side join "$joinMode" 12 --join "$code" &
        join=$!
        wait "$host"; hostExit=$?
        wait "$join"; joinExit=$?
        shared=$(( $(wc -l < "$dir/host.txt") < $(wc -l < "$dir/join.txt") ? $(wc -l < "$dir/host.txt") : $(wc -l < "$dir/join.txt") ))
        relayed=$(grep "peer usage" "$dir/turn.log" | sed 's/.*rp=\([0-9]*\).*sp=\([0-9]*\).*/\1 \2/' | awk '{s += $1 + $2} END {print s + 0}')
        if [ "$hostExit" != 0 ] || [ "$joinExit" != 0 ]; then
            echo "FAILED ($case): the host ended $hostExit, the joiner $joinExit: $(grep -hv '^online\|^relay' "$dir/join.err" | head -1)"; status=1
        elif [ "$shared" -lt $((FRAMES / 2)) ]; then
            echo "FAILED ($case): only $shared frames hashed on both sides"; status=1
        elif ! diff <(head -"$shared" "$dir/host.txt") <(head -"$shared" "$dir/join.txt") > /dev/null; then
            echo "FAILED ($case): the hash logs differ"; status=1
        elif [ "$case" != on:on ] && [ "$relayed" -lt $((FRAMES / 4)) ]; then
            echo "FAILED ($case): only $relayed packets went through the relay"; status=1
        else
            echo "ok: host $hostMode, joiner $joinMode: $shared frames the same, $relayed packets through the relay"
        fi
    fi
    kill "$turn" "$service" 2>/dev/null
    wait "$turn" "$service" 2>/dev/null
done
exit $status
