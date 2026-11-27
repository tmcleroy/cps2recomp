#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
#
# All release builds on a Mac with Docker: macOS and the Android APK here; Linux x86_64, Linux ARM64 and Windows
# natively in ARM64 containers on Apple silicon (tools/release/x86_64/Dockerfile and tools/release/Dockerfile; on an
# x86_64 host, Linux x86_64 and Windows in tools/release/Dockerfile). Writes build/release/ with the five downloads,
# SHA256SUMS.txt, and each system's update-<system>.txt (what the launcher updates itself from).
# Needs the translated code of a build of each game with its ROM set (make GAME=<id>, make GAME=<id> explore).
#
#   TARGETS="macos android" tools/release/all.sh   # subset of targets (split across two Macs)
#   TARGETS="sums" tools/release/all.sh            # SHA256SUMS.txt and the manifests' check, once the other Mac's
#                                                  # archives and update-*.txt are copied into build/release/
set -eu
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

TARGETS="${TARGETS:-macos linux-x86_64 linux-arm64 windows android}"
HOST_ARCH="$(uname -m)"
ALL_START="$(date +%s)"

fmt_dur() {
    s=$1
    if [ "$s" -ge 60 ]; then
        printf '%dm%02ds' $((s / 60)) $((s % 60))
    else
        printf '%ds' "$s"
    fi
}
has_target() {
    case " $TARGETS " in *" $1 "*) return 0 ;; *) return 1 ;; esac
}
step() {
    label=$1; shift
    t0=$(date +%s)
    printf '[%s] %s...\n' "$(date '+%H:%M:%S')" "$label"
    "$@"
    t1=$(date +%s)
    printf '[%s] %s done (%s)\n' "$(date '+%H:%M:%S')" "$label" "$(fmt_dur $((t1 - t0)))"
}

[ "${NO_AUDIT:-}" = 1 ] || step "audit" tools/audit.sh

# (CCACHE_BASE, if set: the folder each container keeps its per-target caches in, mounted at /ccache)
if [ -n "${CCACHE_BASE:-}" ]; then
    mkdir -p "$CCACHE_BASE"
    CCACHE_BASE="$(cd "$CCACHE_BASE" && pwd)"
fi
# (the commit, for the games' logs: the image has no git of this tree. CMakeLists.txt's RECOMP_COMMIT)
RECOMP_COMMIT="${RECOMP_COMMIT:-$(git -C "$(dirname "$0")" describe --always --dirty --abbrev=8 --match=NONE 2>/dev/null || true)}"
export RECOMP_COMMIT
# a release script in a build image: $1 the image, $2 the script, the rest more arguments for docker run
in_image() {
    image=$1 script=$2; shift 2
    set -- docker run --rm "$@" -e GAMES="${GAMES:-}" -e NO_GAME_CODE="${NO_GAME_CODE:-}" -e NO_CCACHE="${NO_CCACHE:-}" \
        -e TRANSLATED="${TRANSLATED:-}" -e RECOMP_COMMIT
    [ -z "${CMAKE_BUILD_PARALLEL_LEVEL:-}" ] || set -- "$@" -e CMAKE_BUILD_PARALLEL_LEVEL="$CMAKE_BUILD_PARALLEL_LEVEL"
    [ -z "${CCACHE_BASE:-}" ] || set -- "$@" -v "$CCACHE_BASE":/ccache -e CCACHE_BASE=/ccache
    # (Windows' signing, docs/RELEASING.md: the certificate's folder read-only, its password passed on from this
    # environment and never written on the command line)
    if [ -n "${WINDOWS_SIGN_PFX:-}" ]; then
        set -- "$@" -v "$(cd "$(dirname "$WINDOWS_SIGN_PFX")" && pwd)":/sign:ro -e WINDOWS_SIGN_PFX="/sign/$(basename "$WINDOWS_SIGN_PFX")" \
            -e WINDOWS_SIGN_PASSWORD -e WINDOWS_SIGN_TIMESTAMP
    fi
    [ -z "${WINDOWS_SIGN:-}" ] || set -- "$@" -e WINDOWS_SIGN
    "$@" -v "$ROOT":/src -w /src "$image" "$script"
}

# Build only the Docker images needed for the selected targets.
need_x86=0 need_arm=0 need_amd64=0
if [ "$HOST_ARCH" = "arm64" ] || [ "$HOST_ARCH" = "aarch64" ]; then
    has_target linux-x86_64 && need_x86=1
    { has_target linux-arm64 || has_target windows; } && need_arm=1
else
    { has_target linux-x86_64 || has_target windows; } && need_amd64=1
    has_target linux-arm64 && need_arm=1
fi
[ "$need_x86" = 1 ] && step "docker build (linux-x86_64 cross)" docker build -q -t cps2recomp-build-x86_64 tools/release/x86_64
[ "$need_arm" = 1 ] && step "docker build (arm64)" docker build --platform linux/arm64 -q -t cps2recomp-build-arm64 tools/release
[ "$need_amd64" = 1 ] && step "docker build (amd64)" docker build --platform linux/amd64 -q -t cps2recomp-build tools/release

# (every game's code translated again from its sources, once for all the systems: lib.sh, #363)
if [ "${NO_GAME_CODE:-}" != 1 ]; then
    step "translate" sh -c '. "$(dirname "$0")/lib.sh" && translate_games' "$ROOT/tools/release/all.sh"
    TRANSLATED=1
    export TRANSLATED
fi

rm -rf build/release/update-files

if has_target macos; then
    rm -f build/release/update-macos.txt
    step "macos" tools/release/macos.sh
fi
if has_target linux-x86_64; then
    rm -f build/release/update-linux-x86_64.txt
    if [ "$HOST_ARCH" = "arm64" ] || [ "$HOST_ARCH" = "aarch64" ]; then
        step "linux-x86_64 (native cross)" in_image cps2recomp-build-x86_64 tools/release/linux.sh
    else
        step "linux-x86_64" in_image cps2recomp-build tools/release/linux.sh --platform linux/amd64
    fi
fi
if has_target linux-arm64; then
    rm -f build/release/update-linux-arm64.txt
    step "linux-arm64" in_image cps2recomp-build-arm64 tools/release/linux.sh --platform linux/arm64
fi
if has_target windows; then
    rm -f build/release/update-windows-x64.txt
    if [ "$HOST_ARCH" = "arm64" ] || [ "$HOST_ARCH" = "aarch64" ]; then
        step "windows-x64 (native cross)" in_image cps2recomp-build-arm64 tools/release/windows.sh --platform linux/arm64
    else
        step "windows-x64" in_image cps2recomp-build tools/release/windows.sh --platform linux/amd64
    fi
fi
if has_target android; then
    # the Android app, signed with the release key (~/.config/cps2recomp/android-signing.env; the script refuses without it)
    step "android" env RELEASE=1 tools/release/android.sh
fi

# SHA256SUMS.txt: on a run of all five, or TARGETS=sums once the other Mac's archives and update-*.txt are copied in.
# (A run of some of them leaves it alone: build/release may still hold another version's files.)
ARCHIVES="CPS2Recomp-macOS.zip CPS2Recomp-linux-x86_64.tar.gz CPS2Recomp-linux-arm64.tar.gz CPS2Recomp-windows-x64.zip CPS2Recomp-android.apk"
MANIFESTS="update-macos.txt update-linux-x86_64.txt update-linux-arm64.txt update-windows-x64.txt"
if has_target sums || { has_target macos && has_target linux-x86_64 && has_target linux-arm64 && has_target windows && has_target android; }; then
    cd build/release
    VERSION="$(cat "$ROOT/VERSION")"
    for f in $ARCHIVES $MANIFESTS; do
        [ -f "$f" ] || { echo "error: no build/release/$f" >&2; exit 1; }
    done
    shasum -a 256 $ARCHIVES > SHA256SUMS.txt
    cat SHA256SUMS.txt
    # (each manifest is this version's and names its system's archive as it is)
    for manifest in $MANIFESTS; do
        grep -qx "version $VERSION" "$manifest" || { echo "error: $manifest is not version $VERSION's" >&2; exit 1; }
        set -- $(sed -n 's/^archive //p' "$manifest")
        if [ "$(shasum -a 256 "$3" | cut -d' ' -f1)" != "$1" ]; then
            echo "error: $manifest does not name $3 as it is" >&2
            exit 1
        fi
    done
    echo "update manifests: $MANIFESTS"
    cd "$ROOT"
fi

ALL_END="$(date +%s)"
printf '[%s] version %s: release files in %s/build/release (total %s)\n' \
    "$(date '+%H:%M:%S')" "$(cat "$ROOT/VERSION")" "$ROOT" "$(fmt_dur $((ALL_END - ALL_START)))"
