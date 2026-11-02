#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 cps2recomp contributors. Part of cps2recomp.
set -eu
cd "$(dirname "$0")/.."
mkdir -p build/check-first-to
"${CXX:-c++}" -std=c++20 -Wall -Wextra -Werror tools/check_first_to.cpp -o build/check-first-to/check
build/check-first-to/check
