// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Input Lab contributors
#pragma once
#include "launcher.h"
void DrawKeyboardControls(launcher::PageContext &);
bool CapturingKeyboardControls();
bool ConsumeKeyboardControlsEvent(const SDL_Event &);
SDL_Scancode PollKeyboardBinding(int);
bool PhysicalKeyboardCapture();
int PhysicalKeyboardLayout(int);
