#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
#
# A version's section of CHANGELOG.md, made from changes/ (a file of changelog lines for each pull request, so
# that two pull requests open at once never change the same lines) and any lines still under "Unreleased": the
# newest first, each file by when it came into main. The files are removed. Run in the release's pull request
# (docs/RELEASING.md, step 1).
#   tools/release/changelog.sh 0.5.0
set -eu
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
VERSION=${1:?usage: changelog.sh VERSION}
cd "$ROOT"
if grep -q "^## $VERSION\$" CHANGELOG.md; then
    echo "error: CHANGELOG.md already has a section for $VERSION" >&2
    exit 1
fi
lines=$(mktemp)
trap 'rm -f "$lines" "$lines.new"' EXIT
# (a file not committed yet counts as the newest)
for file in changes/*.md; do
    [ -f "$file" ] && [ "$file" != changes/README.md ] || continue
    when=$(git log -1 --diff-filter=A --format=%ct -- "$file")
    echo "${when:-9999999999} $file"
done | sort -rn | while read -r _ file; do cat "$file"; done > "$lines"
# The new heading goes under "Unreleased", so the lines still there become the version's too, after the files'
awk -v version="$VERSION" -v lines="$lines" '
    !done && $0 == "## Unreleased" {
        print; print ""; print "## " version; print ""
        while ((getline line < lines) > 0) print line
        done = 1; heading = 1; next
    }
    heading && $0 == "" { heading = 0; next }
    { heading = 0; print }
' CHANGELOG.md > "$lines.new"
section=$(awk -v v="## $VERSION" '$0 ~ "^## " { on = ($0 == v); next } on && NF' "$lines.new")
if [ -z "$section" ]; then
    echo "error: nothing in changes/ or under \"Unreleased\" for $VERSION" >&2
    exit 1
fi
mv "$lines.new" CHANGELOG.md
for file in changes/*.md; do
    [ -f "$file" ] && [ "$file" != changes/README.md ] || continue
    git rm -q --cached "$file" 2>/dev/null || true
    rm -f "$file"
done
echo "CHANGELOG.md: $VERSION's section written; changes/ emptied"
