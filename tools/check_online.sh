#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
#
# Online play (issue #70, port/netplay.h) between copies of the game on this machine: one hosts, the others join it
# over localhost, each with made-up play for its own players, every packet sent late (50 ms, give or take 25) and one
# in twenty lost (--net-fake), so that the guesses go wrong and frames are rolled back. The host has every feature
# that changes play on and the joining side none, so the joining side must take the host's (and all turn off the
# cheats). Each writes a frame's state hash once it has every player's real controls (--hash-log); the logs must be
# the same, and none may stop on a desync. And the host's record of every player's controls (--record), replayed
# without the network, must give the same hashes: each player's controls drove their own player. At the board's pace:
# FRAMES (3000 unless said) take FRAMES / 60 seconds. Each machine gives a name and says a line in the chat (--net-name,
# --net-say): every machine must have every name, and every other machine's line.
#
# JOIN_ROM: the joining side's set, another region's (game.cfg's other_set): it plays the host's program, whose online_words
# the host sends, so the logs and the replay (of the host's set) must still be the same.
#
# ARCADE=1: the host plays alone while it waits (--net-arcade) and the other side joins a few seconds later, so that
# the match begins from the game as the host's play left it, sent whole; the replay from the board's reset is not tried.
#
# A match of four, for a game with tag team (fight/tag.h): the host has its 2 vs 2 on, and the modes marked for matches
# (kFeatureMatch, on or off: games/mvsc's Cross Fever), each of the four with made-up play of their own; the record and
# its replay have all four. FOUR=1: two machines, two people at each (--net-people 2: players 1 and 3 the host's, 2 and
# 4 the other's). MACHINES=4: four machines, one at each (players 1 to 4 in the order they came, each joining machine
# started once the host has the one before; their lines of the chat go through the host). MACHINES=3: three, the host's
# two (players 1 and 3) and one at each of the others (2, then 4).
# PEOPLE lists the people at each machine in arrival order, host first (1,2,1 or 1,1,2 also test a joining pair).
# FIRST_TO=0|1|2 checks the host's Casual/FT5/FT10 choice against different choices on all joining machines.
# WATCH=1 adds a viewer after play starts and compares its confirmed frame hashes with the host's.
#
#   [ARCADE=1] [FOUR=1 | MACHINES=3|4 | PEOPLE=1,2,1] [FIRST_TO=0|1|2] [WATCH=1] [JOIN_ROM=<set>]
#     GAME=<id> check_online.sh [FRAMES]
set -eu
GAME=${GAME:?which game: GAME=<id>}
ROM=${ROM:-rom/$GAME}
JOIN_ROM=${JOIN_ROM:-$ROM}
frames=${1:-3000}
arcade=${ARCADE:-}
four=${FOUR:-}
machines=${MACHINES:-2}
[ -n "$four" ] && machines=2
case "$machines" in 2|3|4) ;; *) echo "MACHINES: 2, 3 or 4"; exit 1 ;; esac
[ "$machines" -gt 2 ] && four=1
layout=${PEOPLE:-}
if [ -z "$layout" ]; then
    case "$machines${four:+f}" in
        2) layout=1,1 ;; 2f) layout=2,2 ;; 3f) layout=2,1,1 ;; 4f) layout=1,1,1,1 ;;
    esac
fi
case "$layout" in
    1,1) four= ;;
    2,2|2,1,1|1,2,1|1,1,2|1,1,1,1) four=1 ;;
    *) echo "PEOPLE: 1,1; 2,2; 2,1,1; 1,2,1; 1,1,2; or 1,1,1,1"; exit 1 ;;
esac
IFS=, read -ra counts <<< "$layout"
machines=${#counts[@]}
hostPeople=${counts[0]}
first_to=${FIRST_TO:-}
case "$first_to" in ""|0|1|2) ;; *) echo "FIRST_TO must be 0, 1 or 2"; exit 1 ;; esac
guest_set=""
if [ -n "$first_to" ]; then
    if ! "build/$GAME/$GAME" --list-features | grep -q '^online_set'; then
        echo "FAILED: this game has not opted in to online sets"
        exit 1
    fi
    guest_set="online_set=$(( (first_to + 1) % 3 ))"
fi
watch=${WATCH:-0}
case "$watch" in 0|1) ;; *) echo "WATCH must be 0 or 1"; exit 1 ;; esac
watch_args=()
[ "$watch" = 1 ] && watch_args=(--net-spectators)
pids=
cleanup() {
    for pid in $pids; do kill "$pid" 2>/dev/null || true; done
    for pid in $pids; do wait "$pid" 2>/dev/null || true; done
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
work=work/$GAME/check
mkdir -p "$work"
# (the joining machines: join, or join1 to join3)
joiners=join
[ "$machines" -gt 2 ] && joiners=$(seq -f 'join%g' 1 $((machines - 1)) | paste -sd' ' -)
for name in host $joiners watch; do
    : > "$work/online-$name.ini"
    rm -f "$work/online-$name.txt" "$work/online-$name.eeprom"
done
# (the features that change play, as tools/check_rollback.sh has them, but for the custom colours: each player's own)
play=$("build/$GAME/$GAME" --list-features 2>&1 | awk -F'\t' '($2=="Game"||$2=="Cheats"||$2=="Display") && NF==5 &&
    $1 !~ /^(display_|window_size|run_ahead|custom_soundtrack|widescreen_walls|custom_colours)/ {print $1}' | paste -sd, -)
play="${play:+$play,}widescreen"
# (the host's stage choice is the match's on games with the top-level Stage page)
if "build/$GAME/$GAME" --list-features 2>&1 | awk -F'\t' '$1=="stage" && $2=="Stage" {found=1} END {exit !found}'; then
    play="$play,stage=2"
fi
if [ -n "$first_to" ]; then play="$play,online_set=$first_to"; fi
# (the people at each machine, and the made-up play's players)
if [ -n "$four" ]; then
    # (the on-or-off features marked for matches: kFeatureMatch, 512 in --list-features's flags)
    modes=$("build/$GAME/$GAME" --list-features 2>&1 | awk -F'\t' 'NF==5 && int($3 / 512) % 2 == 1 {print $1}' | paste -sd, -)
    play="$play,tag_team=2${modes:+,$modes}"
fi
# (the match has them but for the cheats, which are off on all sides: the replay is given the same)
cheats=$("build/$GAME/$GAME" --list-features 2>&1 | awk -F'\t' '$2=="Cheats" {print $1}')
match=$(printf '%s\n' "$play" | tr ',' '\n' | grep -vxF "$cheats" | paste -sd, -)
port=$((7900 + RANDOM % 1000))
side() { # host | join..., then the program's arguments
    name=$1; shift
    rom=$ROM; [ "$name" != host ] && rom=$JOIN_ROM
    exec "build/$GAME/$GAME" --rom "$rom" --headless --frames "$frames" --settings "$work/online-$name.ini" \
        --eeprom "$work/online-$name.eeprom" --hash-log "$work/online-$name.txt" --net-service off \
        --net-fake 50:25:5 "$@" 2> "$work/online-$name.err"
}
named() { # host | join... -> its name
    case "$1" in host) echo "Host P1" ;; join) echo "Joiner" ;; *) echo "Joiner ${1#join}" ;; esac
}
said() { # FILE PATTERN: once a line of FILE has it (10 seconds at most)
    for _ in $(seq 100); do
        grep -q "$2" "$1" 2> /dev/null && return 0
        sleep 0.1
    done
    return 1
}
side host --auto "11:0:$hostPeople" --features "$play" --host "$port" --record "$work/online-host.mov" ${arcade:+--net-arcade} \
    --net-people "$hostPeople" --net-name "Host P1" --net-say "hello from the host" ${watch_args[@]+"${watch_args[@]}"} &
pids=$!
# (the joining side once the host waits: its socket is open by then, however long the game took to start)
said "$work/online-host.err" '^online: Waiting for' || { echo "FAILED: host not waiting after 10 seconds"; exit 1; }
[ -n "$arcade" ] && sleep 4
status=0
n=0
for name in $joiners; do
    n=$((n + 1))
    joinPeople=${counts[$n]}
    side "$name" --auto "$((11 + n)):0:$joinPeople" --join "127.0.0.1:$port" --net-people "$joinPeople" --net-name "$(named "$name")" \
        --features "$guest_set" --net-say "$((90 + 30 * n)):good luck from $(named "$name")!" &
    pids="$pids $!"
    # (each once the one before has come, their players going by it)
    if [ "$machines" -gt 2 ]; then
        said "$work/online-host.err" "^online: $(named "$name") is here" || {
            echo "FAILED: $name not here after 10 seconds"; exit 1;
        }
    fi
done
if [ "$watch" = 1 ]; then
    said "$work/online-host.err" '^online: the match is on:' || { echo "FAILED: match did not start for viewer"; exit 1; }
    side watch --watch "127.0.0.1:$port" --net-name "Viewer" --auto 99:0:4 --net-say "viewer must not chat" &
    pids="$pids $!"
    said "$work/online-watch.err" '^online: spectating from confirmed frame ' || {
        echo "FAILED: viewer did not receive a confirmed starting state"; tail -5 "$work/online-watch.err"; exit 1;
    }
fi
set -- host $joiners
[ "$watch" = 1 ] && set -- "$@" watch
for pid in $pids; do
    wait "$pid" || { echo "FAILED: the $1 stopped"; tail -3 "$work/online-$1.err"; status=1; }
    shift
done
pids=
if [ -n "$first_to" ]; then
    case "$first_to" in 0) format=Casual ;; 1) format=FT5 ;; 2) format=FT10 ;; esac
    for name in host $joiners; do
        if ! grep -qx "online: set format $format" "$work/online-$name.err"; then
            echo "FAILED: $name did not take the host's $format choice"
            status=1
        fi
    done
    [ "$status" = 0 ] && echo "ok: all $machines playing machines took the host's $format choice"
fi
# (each machine's log ends where it last had every other's controls: the shortest is what all have)
lines=$frames
for name in host $joiners; do
    had=$(wc -l < "$work/online-$name.txt" | tr -d ' ')
    [ "$had" -lt "$lines" ] && lines=$had
done
if [ "$lines" -lt $((frames - 30)) ]; then
    echo "FAILED: only $lines frames had every player's controls"
    status=1
else
    same=1
    for name in $joiners; do
        if ! cmp -s <(head -n "$lines" "$work/online-host.txt") <(head -n "$lines" "$work/online-$name.txt"); then
            echo "FAILED: the host and the $name differ from frame $(diff <(head -n "$lines" "$work/online-host.txt") <(head -n "$lines" "$work/online-$name.txt") | grep '^<' | head -1 | cut -d' ' -f2)"
            same=0
            status=1
        fi
    done
    [ "$same" = 1 ] && echo "ok: the $machines machines the same over $lines frames, with $play"
fi
if [ -n "$first_to" ]; then
    scores() { awk -v last="$lines" '/^online: set FT[0-9]+ frame / && $5 <= last' "$1"; }
    for name in $joiners; do
        if ! cmp -s <(scores "$work/online-host.err") <(scores "$work/online-$name.err"); then
            echo "FAILED: host and $name awarded different set scores"
            status=1
        fi
    done
    [ "$status" = 0 ] && echo "ok: set-score events match through the last shared confirmed frame"
fi
if [ "$watch" = 1 ]; then
    # Viewers begin at a confirmed snapshot, not frame zero. Compare by absolute frame number, requiring actual play.
    if awk 'NR==FNR {hash[$1]=$2; next} ($1 in hash) {if (hash[$1]!=$2) bad=1; matched++}
            END {if (bad || matched<100) exit 1; print "ok: viewer matches " matched " confirmed host frames"}' \
            "$work/online-host.txt" "$work/online-watch.txt"; then :; else
        echo "FAILED: spectator playback differs or contains fewer than 100 shared frames"
        status=1
    fi
    for name in host $joiners; do
        if grep -q 'online: chat: Viewer:' "$work/online-$name.err"; then
            echo "FAILED: viewer chat reached $name"
            status=1
        fi
    done
fi
# (every machine knows every player's name, and has every other machine's line of the chat: sent straight to it, or
# through the host, through the lost packets)
case "$layout" in
    1,1) players="player 1 Host P1, player 2 Joiner" ;;
    2,2) players="player 1 Host P1, player 2 Joiner, player 3 Host P1, player 4 Joiner" ;;
    2,1,1) players="player 1 Host P1, player 2 Joiner 1, player 3 Host P1, player 4 Joiner 2" ;;
    1,2,1) players="player 1 Host P1, player 2 Joiner 1, player 3 Joiner 2, player 4 Joiner 1" ;;
    1,1,2) players="player 1 Host P1, player 2 Joiner 1, player 3 Joiner 2, player 4 Joiner 2" ;;
    1,1,1,1) players="player 1 Host P1, player 2 Joiner 1, player 3 Joiner 2, player 4 Joiner 3" ;;
esac
for name in host $joiners; do
    if ! grep -qxF "online: $players" "$work/online-$name.err"; then
        echo "FAILED: the $name doesn't have every player's name ($players)"
        status=1
    fi
done
if [ -n "$four" ]; then
    # (the four at their machines: each machine's players as said)
    case "$layout" in
        2,2) where="host=players 1 and 3,join=players 2 and 4" ;;
        2,1,1) where="host=players 1 and 3,join1=player 2,join2=player 4" ;;
        1,2,1) where="host=player 1,join1=players 2 and 4,join2=player 3" ;;
        1,1,2) where="host=player 1,join1=player 2,join2=players 3 and 4" ;;
        1,1,1,1) where="host=player 1,join1=player 2,join2=player 3,join3=player 4" ;;
    esac
    placed=1
    IFS=, read -ra items <<< "$where"
    for item in "${items[@]}"; do
        name=${item%%=*} who=${item#*=}
        grep -q "^online: the match is on: you are $who " "$work/online-$name.err" || { echo "FAILED: the $name is not $who"; placed=0; status=1; }
    done
    [ "$placed" = 1 ] && echo "ok: a match of four at $machines machines ($(echo "$where" | sed 's/=/: /g; s/,/, /g'))"
fi
heard=1
for name in host $joiners; do
    for other in host $joiners; do
        [ "$other" = "$name" ] && continue
        line="good luck from $(named "$other")!"; [ "$other" = host ] && line="hello from the host"
        grep -qxF "online: chat: $(named "$other"): $line" "$work/online-$name.err" || { echo "FAILED: the $name didn't hear the $other"; heard=0; status=1; }
    done
done
[ "$heard" = 1 ] && echo "ok: the names and the chat came across"
if [ -n "$arcade" ]; then
    # (the host played before the other side came: the match began from there)
    played=$(sed -n 's/^online: the match begins after \([0-9]*\) frames of the host.s play.*/\1/p' "$work/online-host.err")
    if [ "${played:-0}" -lt 120 ]; then
        echo "FAILED: the host had not played while it waited (${played:-no} frames)"
        status=1
    else
        echo "ok: the match began after $played frames of the host's own play"
    fi
    for name in host $joiners; do
        echo "$name: $(grep '^online: frame' "$work/online-$name.err" | tail -1 | sed 's/^online: //')"
    done
    exit $status
fi
# And the match is what the game does with those controls: every player's (--record), replayed without the network
: > "$work/online-replay.ini"
"build/$GAME/$GAME" --rom "$ROM" --headless --frames "$lines" --settings "$work/online-replay.ini" --features "$match" \
    --movie "$work/online-host.mov" --hash-log "$work/online-replay.txt" --hash-no-picture --hash-no-sound 2> /dev/null
if cmp -s <(head -n "$lines" "$work/online-host.txt") <(head -n "$lines" "$work/online-replay.txt"); then
    echo "ok: the match the same as every player's controls replayed without the network"
else
    echo "FAILED: the match and its replay differ from frame $(diff <(head -n "$lines" "$work/online-host.txt") <(head -n "$lines" "$work/online-replay.txt") | grep '^<' | head -1 | cut -d' ' -f2)"
    status=1
fi
for name in host $joiners; do
    echo "$name: $(grep '^online: frame' "$work/online-$name.err" | tail -1 | sed 's/^online: //')"
done
exit $status
