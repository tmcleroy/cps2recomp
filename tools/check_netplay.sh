#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
# Run the production netplay protocol regressions without ROMs or external network access.
# Needs a C++20 compiler, SDL3 development files, zlib, pkg-config, Python 3 and curl.
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
work=$(mktemp -d "${TMPDIR:-/tmp}/cps2recomp-netplay.XXXXXX")
trap 'rm -rf "$work"' EXIT
cd "$root"
(cd example && python3 ../tools/gamecfg.py --header "$work/game_info.h" example)
pkg_flags=()
if [ "${SDL_STATIC:-0}" = 1 ]; then pkg_flags=(--static); fi
read -ra sdl_flags <<< "$(pkg-config ${pkg_flags[@]+"${pkg_flags[@]}"} --cflags --libs sdl3)"
read -ra cpp_flags <<< "${CPPFLAGS:-}"
read -ra cxx_flags <<< "${CXXFLAGS:-}"
read -ra ld_flags <<< "${LDFLAGS:-}"
case "$(uname -s)" in
    Darwin) strip_flags=(-Wl,-dead_strip) ;;
    *) strip_flags=(-Wl,--gc-sections) ;;
esac
"${CXX:-c++}" -std=c++20 -O1 -ffunction-sections -fdata-sections \
    -DRECOMP_VERSION='"netplay-test"' -I"$work" -Imachine -Iruntime \
    ${cpp_flags[@]+"${cpp_flags[@]}"} ${cxx_flags[@]+"${cxx_flags[@]}"} tools/check_netplay.cpp \
    "${strip_flags[@]}" ${ld_flags[@]+"${ld_flags[@]}"} "${sdl_flags[@]}" -lz -pthread -o "$work/check_netplay"
# The failed-service test only contacts loopback, even on machines with a system HTTP proxy configured.
NO_PROXY=127.0.0.1 no_proxy=127.0.0.1 "$work/check_netplay"
