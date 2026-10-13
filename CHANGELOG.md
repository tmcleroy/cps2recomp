# Changelog

## Unreleased

## 0.2.0 (2026-10-13)

- A wider picture for a game's sources to ask for (`Video::wide`): up to 64 more columns on each side
  of the board's 384 (512 is the whole raster: 16:9), each scroll screen and each object drawn moved
  sideways as the sources say, and a scroll screen's tiles outside the board's own columns taken
  from the sources where the game only keeps its map up for its own picture. The window, the menu,
  the readout, screenshots and `--present-test` take the picture at the width it has; the window
  starts in the shape `Game::widestPicture` gives. With nothing asked for, the picture and the
  state hashes are what they were.
- `Machine::ObjectsToCome`: the object list the next picture will be made from.
- `CPS2RECOMP_WIDE=n`: n more columns on each side whatever the sources ask, to see what a game has
  beyond its picture's edges.
- recomp-common 0.4.0: the launcher works with a gamepad, or the arrow keys, alone, and has a
  "Muted" switch (the game is started with `--mute`).
- The in-game menu's buttons are the same whatever the game's are set to: the arrows, the d-pad and
  the left stick; Space, Enter and a gamepad's A choose; Backspace and B go back (leave a page,
  close the menu from the first). A "Sound" row switches the sound off and on; `--mute` starts with
  it off (the sound device is opened all the same, so that the row can switch it on).

## 0.1.0

The first public version: the toolkit as its first game, Vampire Savior Recomp, uses it.

- The translators of 68000 and Z80 code to C++ (`tools/recomp.py`, `tools/recompz80.py`), and
  finding a game's code by made-up play (`make explore`).
- The machine: the 68000 with an interpreter for code that has no translation, the bus, interrupts
  and timing, the settings memory, the ROM set and its cipher; the picture; the Z80 and the QSound
  chip; Capcom's sound driver (version 2.01b) as ordinary code, used in place of the Z80 where a
  game has that version.
- The picture on SDL's GPU API (Metal, Vulkan, Direct3D 12), with SDL's renderer where there is no
  device.
- Routines of a game replaced by C++, readable rewrites and their checker; features and settings;
  save states; the in-game menu and the launcher the recomp ports share (recomp-common).
- A game's repository takes a copy with `tools/sync-toolkit.sh`, checked by `make check-toolkit`;
  `make CR=../cps2recomp` builds a game against a clone.
- `make check` here, with no game: the audit, the stand-in game in `example/`, the sync script.
