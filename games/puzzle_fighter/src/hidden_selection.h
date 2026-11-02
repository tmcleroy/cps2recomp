// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Input Lab contributors
#pragma once
namespace inputlab {
inline int CyclePuzzle(int normal, int character) {
    return character == 8 ? 9 : character == 9 ? 10 : character == 10 ? normal : 8;
}}
