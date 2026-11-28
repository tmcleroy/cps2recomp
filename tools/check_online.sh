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
# With three people use PEOPLE=1,1,1, 2,1 or 1,2 and TAG_TEAM=3 (2 vs 1) or 4 (1 vs 2).
# NET_PROFILES lists one outgoing MS:JITTER:LOSS profile per machine, host first. NET_DELAY chooses the host's delay;
# each joining machine takes its own, the next one up (each machine's delay is its own: port/netplay.cpp, Delays).
# MVC_TEAM_ORDER=0|1 tests Dreamcast or adjacent teammates with four MVC players. Guests start with the opposite preference.
# MVC_GAUGE=0|1 tests fighting-only or passive meter against opposite guest settings.
# SLOWDOWN=0|1 checks the host's board timing choice against the opposite guest setting.
# MVC_CUSTOM=1 exercises the alternative Cross Fever rules on the host and in the offline replay.
# FIRST_TO=0|3|5|10 checks the host's Casual/FT3/FT5/FT10 choice against different choices on all joining machines.
# WATCH=1 adds a viewer after play starts and compares its confirmed frame hashes with the host's.
# TRAINING=FRAME:RESTART (a match of two, a fighting game): the host starts the training session for both at FRAME
# (--net-command FRAME:mode=3), with its refills and fixed damage on, and the joining side asks to start over at
# RESTART (restart=1, the left corner); each side has its own training display (the host hit boxes, the other the
# frame meter and the input display). Every machine must start the session and start over on the same frames, and
# the logs and the replay still be the same.
# BALL=FRAME[:SEED] (a match of two, a game with the Ball battle, fight/court.h): the host starts the Ball battle for both
# at FRAME (--net-command FRAME:mode=1:SEED, as its match menu's Start does; the joining side says it is ready a second
# before), each side's made-up play its fighter's on the court. Every machine must play the court (CPS2RECOMP_COURT_LOG's
# lines), and the logs still be the same; the replay of the record is not tried (the mode began by an online event).
# SEATS=FRAME:LAYOUT (a match of four, or of two): the host changes the seats at FRAME (issue #448: --net-command
# FRAME:seats=LAYOUT, e.g. 1200:1.0.3.2 swaps the sides at four machines, 1200:1.0 at two); every machine must make it, the same frame, and the logs and the
# replay of the host's record still be the same.
#
#   [ARCADE=1] [FOUR=1 | MACHINES=3|4 | PEOPLE=1,2,1] [FIRST_TO=0|3|5|10] [WATCH=1] [SEATS=FRAME:LAYOUT] [BALL=FRAME[:SEED]] [JOIN_ROM=<set>]
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
tag_mode=
case "$layout" in
    1,1,1|1,2|2,1)
        case "${TAG_TEAM:-3}" in 3|4) tag_mode=${TAG_TEAM:-3} ;; *) echo "Three humans require TAG_TEAM=3 or 4"; exit 1 ;; esac
        four= ;;
    1,1) four= ;;
    2,2|2,1,1|1,2,1|1,1,2|1,1,1,1) four=1; tag_mode=2 ;;
    *) echo "PEOPLE: 1,1; 1,1,1; 1,2; 2,1; 2,2; 2,1,1; 1,2,1; 1,1,2; or 1,1,1,1"; exit 1 ;;
esac
IFS=, read -ra counts <<< "$layout"
machines=${#counts[@]}
hostPeople=${counts[0]}
profiles=()
if [ -n "${NET_PROFILES:-}" ]; then
    IFS=, read -ra profiles <<< "$NET_PROFILES"
    if [ "${#profiles[@]}" -ne "$machines" ] || [[ "$NET_PROFILES" = *, ]]; then
        echo "NET_PROFILES requires one comma-separated profile per machine, host first"; exit 1
    fi
    for profile in "${profiles[@]}"; do
        if ! [[ "$profile" =~ ^[0-9]{1,4}:[0-9]{1,4}:([0-9]{1,2}|100)$ ]]; then
            echo "NET_PROFILES expects MS:JITTER:LOSS, for example 40:10:2"; exit 1
        fi
    done
fi
host_delay=${NET_DELAY:-2}
case "$host_delay" in [0-8]) ;; *) echo "NET_DELAY must be 0 through 8"; exit 1 ;; esac
first_to=${FIRST_TO:-}
case "$first_to" in ""|0|3|5|10) ;; *) echo "FIRST_TO must be 0 (Casual), 3, 5 or 10"; exit 1 ;; esac
guest_set=""
if [ -n "$first_to" ]; then
    if ! "build/$GAME/$GAME" --list-features | grep -q '^online_first_to'; then
        echo "FAILED: this game has not opted in to online sets"
        exit 1
    fi
    # (online_first_to's choices: 0 Casual, 1 FT3, 2 FT5, 3 FT10; the guest asks for another)
    case "$first_to" in 0) first_to=0 ;; 3) first_to=1 ;; 5) first_to=2 ;; 10) first_to=3 ;; esac
    guest_set="online_first_to=$(( (first_to + 1) % 4 ))"
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
if [ -n "${SLOWDOWN:-}" ]; then
    case "$SLOWDOWN" in 0|1) ;; *) echo "SLOWDOWN must be 0 (Arcade perfect) or 1 (None)"; exit 1 ;; esac
    if ! "build/$GAME/$GAME" --list-features | awk -F'\t' '$1=="slowdown" && $2=="Game" && $6=="Arcade perfect|None" {found=1} END {exit !found}'; then
        echo "SLOWDOWN requires the Arcade perfect / None board timing option"; exit 1
    fi
    play="$play,slowdown=$SLOWDOWN"
    guest_set="${guest_set:+$guest_set,}slowdown=$((1 - SLOWDOWN))"
fi
# (the host's stage choice is the match's on games with the top-level Stage page)
if "build/$GAME/$GAME" --list-features 2>&1 | awk -F'\t' '$1=="stage" && $2=="Stage" {found=1} END {exit !found}'; then
    play="$play,stage=2"
fi
if [ -n "$first_to" ]; then play="$play,online_first_to=$first_to"; fi
training=${TRAINING:-}
ball=${BALL:-}
if [ -n "$ball" ]; then
    if [ "$machines" != 2 ] || [ -n "$four" ]; then echo "BALL: a match of two"; exit 1; fi
    if ! "build/$GAME/$GAME" --list-features 2>&1 | awk -F'\t' '$1=="court_points" {found=1} END {exit !found}'; then
        echo "BALL: $GAME has no Ball battle"; exit 1
    fi
    ball_at=${ball%%:*}
    ball_seed=${ball#*:}; [ "$ball_seed" = "$ball" ] && ball_seed=1
    [ "$ball_at" -gt 60 ] || { echo "BALL: a frame after 60"; exit 1; }
    export CPS2RECOMP_COURT_LOG=1
fi
if [ -n "$training" ]; then
    aids=$("build/$GAME/$GAME" --list-features 2>&1 | awk -F'\t' '$1 ~ /^(health_refill|gauge_refill|fixed_damage)$/ {print $1}' | paste -sd, -)
    play="$play,${aids:+$aids,}hit_boxes"
    guest_set="${guest_set:+$guest_set,}frame_meter,input_display"
fi
# (the people at each machine, and the made-up play's players)
if [ -n "$tag_mode" ]; then
    # (the on-or-off features marked for matches: kFeatureMatch, 512 in --list-features's flags)
    modes=$("build/$GAME/$GAME" --list-features 2>&1 | awk -F'\t' 'NF==5 && int($3 / 512) % 2 == 1 {print $1}' | paste -sd, -)
    play="$play,tag_team=$tag_mode${modes:+,$modes}"
fi
if [ "${MVC_CUSTOM:-0}" = 1 ]; then
    if [ "$GAME" != mvsc ] || [ -z "$tag_mode" ]; then
        echo "MVC_CUSTOM=1 needs an MVC versus layout with three or four people"; exit 1
    fi
    play="$play,cross_fever_arcade_time=1,cross_fever_partner_tag=1,cross_fever_taunt=2,cross_fever_reselect=1"
fi
gauge=${MVC_GAUGE:-}
if [ "${MVC_CUSTOM:-0}" = 1 ]; then gauge=${gauge:-0}; fi
if [ -n "$gauge" ]; then
    case "$gauge" in 0|1) ;; *) echo "MVC_GAUGE must be 0 (fighting only) or 1 (passive)"; exit 1 ;; esac
    if [ "$GAME" != mvsc ] || [ -z "$tag_mode" ]; then
        echo "MVC_GAUGE requires an MVC versus layout with three or four people"; exit 1
    fi
    play="$play,cross_fever_auto_gauge=$gauge"
    guest_set="${guest_set:+$guest_set,}cross_fever_auto_gauge=$((1 - gauge))"
fi
team_order=${MVC_TEAM_ORDER:-}
if [ -n "$team_order" ]; then
    case "$team_order" in 0|1) ;; *) echo "MVC_TEAM_ORDER must be 0 or 1"; exit 1 ;; esac
    if [ "$GAME" != mvsc ] || [ "$tag_mode" != 2 ]; then
        echo "MVC_TEAM_ORDER requires four people in MVC"; exit 1
    fi
    play="$play,tag_team_order=$team_order"
    guest_set="${guest_set:+$guest_set,}tag_team_order=$((1 - team_order))"
fi
# (the match has them but for the cheats, which are off on all sides: the replay is given the same)
cheats=$("build/$GAME/$GAME" --list-features 2>&1 | awk -F'\t' '$2=="Cheats" {print $1}')
match=$(printf '%s\n' "$play" | tr ',' '\n' | grep -vxF "$cheats" | paste -sd, -)
port=$((7900 + RANDOM % 1000))
side() { # host | join..., then the program's arguments
    name=$1; shift
    rom=$ROM; [ "$name" != host ] && rom=$JOIN_ROM
    case "$name" in host) index=0 ;; join) index=1 ;; join*) index=${name#join} ;; watch) index=$machines ;; esac
    profile=${profiles[$index]:-50:25:5}
    echo "$name sending with $profile ms:jitter:loss" >&2
    exec "build/$GAME/$GAME" --rom "$rom" --headless --frames "$frames" --settings "$work/online-$name.ini" \
        --eeprom "$work/online-$name.eeprom" --hash-log "$work/online-$name.txt" --net-service off \
        --net-fake "$profile" "$@" 2> "$work/online-$name.err"
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
side host --net-delay "$host_delay" --auto "11:0:$hostPeople" --features "$play" --host "$port" --record "$work/online-host.mov" ${arcade:+--net-arcade} \
    --net-people "$hostPeople" --net-name "Host P1" --net-say "hello from the host" ${watch_args[@]+"${watch_args[@]}"} \
    ${SEATS:+--net-command "${SEATS%%:*}:seats=${SEATS#*:}"} ${training:+--net-command "${training%%:*}:mode=3:1"} ${ball:+--net-command "$ball_at:mode=1:$ball_seed"} &
pids=$!
# (the joining side once the host waits: its socket is open by then, however long the game took to start)
said "$work/online-host.err" '^online: Waiting for' || { echo "FAILED: host not waiting after 10 seconds"; exit 1; }
[ -n "$arcade" ] && sleep 4
status=0
n=0
for name in $joiners; do
    n=$((n + 1))
    joinPeople=${counts[$n]}
    side "$name" --net-delay "$(( (host_delay + n) % 9 ))" --auto "$((11 + n)):0:$joinPeople" --join "127.0.0.1:$port" --net-people "$joinPeople" --net-name "$(named "$name")" \
        --features "$guest_set" --net-say "$((90 + 30 * n)):good luck from $(named "$name")!" \
        ${training:+--net-command "${training#*:}:restart=1"} ${ball:+--net-command "$((ball_at - 60)):ready=1"} &
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
    case "$first_to" in 0) format=Casual ;; 1) format=FT3 ;; 2) format=FT5 ;; 3) format=FT10 ;; esac
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
    1,1,1) players="player 1 Host P1, player 2 Joiner 1, player 3 Joiner 2" ;;
    1,2|2,1)
        case "$layout/$tag_mode" in
            2,1/3|1,2/4) players="player 1 Host P1, player 2 Joiner" ;;
            *) players="player 1 Joiner, player 2 Host P1" ;;
        esac
        [ "$layout" = 2,1 ] && players="$players, player 3 Host P1" || players="$players, player 3 Joiner" ;;
    1,1) players="player 1 Host P1, player 2 Joiner" ;;
    *)
        # Controller numbers for each machine, in join order. Check names and local assignments from the same fixture.
        case "$layout/${team_order:-0}" in
            2,2/0) seats=(13 24) ;;       2,2/1) seats=(12 34) ;;
            2,1,1/0) seats=(13 2 4) ;;    2,1,1/1) seats=(12 3 4) ;;
            1,2,1/0) seats=(1 24 3) ;;    1,2,1/1) seats=(1 34 2) ;;
            1,1,2/*) seats=(1 2 34) ;;
            1,1,1,1/*) seats=(1 2 3 4) ;;
        esac
        machine=0
        where=""
        for name in host $joiners; do
            which=${seats[$machine]}
            first=${which:0:1}
            seat_names[$first]="$(named "$name")"
            who="player $first"
            if [ "${#which}" = 2 ]; then
                second=${which:1:1}
                seat_names[$second]="$(named "$name")"
                who="players $first and $second"
            fi
            where="${where:+$where,}$name=$who"
            machine=$((machine + 1))
        done
        players="player 1 ${seat_names[1]}, player 2 ${seat_names[2]}, player 3 ${seat_names[3]}, player 4 ${seat_names[4]}" ;;
esac
n=0
for name in host $joiners; do
    own=$(( (host_delay + n) % 9 ))
    n=$((n + 1))
    if ! grep -Eq "^online: the match is on: .* \($own frames? of delay; the others' [0-9, ]+\)$" "$work/online-$name.err"; then
        echo "FAILED: $name did not keep its own delay $own"; status=1
    fi
    if [ "$name" != host ] && ! grep -Eq "^online: the match is on: .*the others' ([0-9]+, )*$host_delay(, [0-9]+)*\)$" "$work/online-$name.err"; then
        echo "FAILED: $name was not told the host's delay $host_delay"; status=1
    fi
    if [ "${MVC_CUSTOM:-0}" = 1 ]; then
        rules=$(sed -n 's/^online: settled match rules //p' "$work/online-$name.err" | head -1)
        for rule in "tag_team=$tag_mode" cross_fever=1 cross_fever_arcade_time=1 \
            cross_fever_partner_tag=1 cross_fever_taunt=2 cross_fever_reselect=1; do
            case ",$rules," in *",$rule,"*) ;; *) echo "FAILED: $name did not take $rule"; status=1 ;; esac
        done
    fi
    if [ -n "$gauge" ]; then
        rules=$(sed -n 's/^online: settled match rules //p' "$work/online-$name.err" | head -1)
        case ",$rules," in *",cross_fever_auto_gauge=$gauge,"*) ;; *) echo "FAILED: $name did not take host gauge $gauge"; status=1 ;; esac
    fi
    if [ -n "$team_order" ]; then
        rules=$(sed -n 's/^online: settled match rules //p' "$work/online-$name.err" | head -1)
        case ",$rules," in *",tag_team_order=$team_order,"*) ;; *) echo "FAILED: $name did not take team order $team_order"; status=1 ;; esac
    fi
    if ! grep -qxF "online: $players" "$work/online-$name.err"; then
        echo "FAILED: the $name doesn't have every player's name ($players)"
        status=1
    fi
done
if [ -n "$four" ]; then
    placed=1
    IFS=, read -ra items <<< "$where"
    for item in "${items[@]}"; do
        name=${item%%=*} who=${item#*=}
        grep -q "^online: the match is on: you are $who " "$work/online-$name.err" || { echo "FAILED: the $name is not $who"; placed=0; status=1; }
    done
    [ "$placed" = 1 ] && echo "ok: a match of four at $machines machines ($(echo "$where" | sed 's/=/: /g; s/,/, /g'))"
fi
if [ -n "${SEATS:-}" ]; then
    seated=1
    want="^online: seats changed at frame $(( ${SEATS%%:*} + host_delay )):"
    for name in host $joiners; do
        grep -q "$want" "$work/online-$name.err" || { echo "FAILED: the $name didn't change the seats at frame $(( ${SEATS%%:*} + host_delay ))"; seated=0; status=1; }
    done
    [ "$seated" = 1 ] && echo "ok: every machine changed the seats ($SEATS) on the same frame"
fi
if [ -n "$training" ]; then
    trained=1
    started=$(sed -n 's/^online: training session started at frame \([0-9]*\)$/\1/p' "$work/online-host.err" | head -1)
    over=$(sed -n 's/^online: training started over at frame \([0-9]*\) .*/\1/p' "$work/online-host.err" | head -1)
    [ -n "$started" ] || { echo "FAILED: the host didn't start the training session"; trained=0; status=1; }
    [ -n "$over" ] || { echo "FAILED: the host didn't start the round over (no round fought by then?)"; trained=0; status=1; }
    for name in $joiners; do
        grep -qx "online: training session started at frame $started" "$work/online-$name.err" || {
            echo "FAILED: the $name didn't start the training session at frame $started"; trained=0; status=1; }
        grep -q "^online: training started over at frame $over " "$work/online-$name.err" || {
            echo "FAILED: the $name didn't start over at frame $over"; trained=0; status=1; }
    done
    [ "$trained" = 1 ] && echo "ok: every machine started training at frame $started and started over at frame $over"
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
if [ -n "$ball" ]; then
    played=1
    for name in host $joiners; do
        courts=$(grep -c '^court t' "$work/online-$name.err" || true)
        if [ "${courts:-0}" -lt 20 ]; then
            echo "FAILED: the $name did not play the Ball battle (${courts:-0} court lines)"; played=0; status=1
        fi
    done
    if [ "$played" = 1 ]; then
        echo "ok: every machine played the Ball battle (from frame $ball_at, seed $ball_seed; host: $(grep '^court t' "$work/online-host.err" | tail -1 | grep -o 'score [0-9-]*'))"
    fi
    echo "(the replay of the record is not tried: the Ball battle began by an online event)"
    for name in host $joiners; do
        echo "$name: $(grep '^online: frame' "$work/online-$name.err" | tail -1 | sed 's/^online: //')"
    done
    exit $status
fi
if [ -n "$training" ]; then
    # (a record has the controls only, not the online events that start a mode, so it replays as Versus)
    echo "(the replay of the record is not tried: the training session began by an online event)"
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
