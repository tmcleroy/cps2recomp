# Changelog

## Unreleased

- The in-game menu's first page has a "Test menu" row: it presses the board's test switch (as F2
  does), which opens the game's own operator menu, where its settings are (difficulty, rounds and the rest).
- Colour editor: random colours (the Random row: LK, left or right) only at a second press within a second and
  a half, and HK puts back the set from before them until the next change. The Line row goes to the
  animation that best shows the line when the game says which (`Description::animationForLine`).
  "Load colour files again" and "Write the game's colours" are on the editor's page, not Display.
- The game's program started with no arguments (a double click on it, not on the launcher) starts
  the launcher beside it instead: straight into the game (`--play`) once a ROM set has been chosen.
  A start that fails says why in a message box too, unless headless. On Windows the game is a
  windowed program (no console window beside it).
- recomp-common 394d613: the menu leaves room for a page's longest description (it ran over the hint),
  a value too long for its row is drawn small, and the panel is opaque.
- Display options for every game (the port's features, on the Display page of the launcher and the
  in-game menu): Scaling (Sharp, as before; Whole pixels; Smooth), Picture shape (4:3, as before;
  Square pixels; Fill) and Window size (3x, as before; 2x to 6x). A window takes the new shape or
  size at once. `FitPicture` and `Presenter::SetScaling` (present.h); `--scale` still goes before the
  setting, and `--present-size WxH` draws `--present-test`'s picture in a window that size.
- `SetSlowMotion`: a game's sources can ask for the game to run slowly (a frame in every so many
  pictures); the slower of that and the menu's speed is used.
- The check of a rewrite compares video RAM too for a rewrite that names `kChecksVideo` (a routine
  that writes a scroll screen's map or the row scroll table).
- The code for three lines of 0.2.0's list, which was released without it: `kFeatureFirstPage`,
  `Video::wide.rows` and `Video::objectsTaken`; and the board's own picture drawn by a form of the
  renderer that knows its width.
- `fight/`: what a fighting game's training mode has in common (frame data, its meter and a log of
  every move, the input display in numpad notation, the combo readout, a dummy that guards, records
  and replays, boxes on the picture); `docs/FIGHT.md`. `runtime/spare.*`: machines kept as save
  states and run beside the port's, for a game's modes. `SettingsPath()`.
- The dummy at random: what it does while guarding, the escape from a throw and the way up from a
  knock-down; "Answers now and then" (one time in two, with any slot that has a recording).
- Run-ahead for every game (the port's `run_ahead` feature: off, 1 or 2 frames): a press is seen a
  frame or two sooner and the game runs exactly as without it. Not while stopped, stepped or slowed,
  nor when the game says not (`Game::noRunAhead`); `RunningAhead()` for a game's sources. Listed in
  the launcher (`RegisterPortFeatures`). The 68000's wait is kept in a save state (state version 5).
- `Video::wide.fillHoles`, a game's choice: pixels of the added columns that nothing was drawn at
  take the line's own picture, mirrored at the nearest drawn pixel.
- A first-page switch changed in the in-game menu is changed in `launcher.ini` too; the window takes
  the widest picture's shape again when it changes.
- The launcher's Game page says how to open the in-game menu while playing.
- `tools/at-once.sh`: a game's checks run a few at a time, their outputs in order.
- Training aids work only in a training session: `kFeatureTraining` (a game's sources mark theirs with
  `TrainingAids`; the dummy's are marked by `RegisterDummy`) and `Game::inTraining`. Outside a session
  `FeatureValue` gives 0 for them, while the menu shows the player's setting (`FeatureSetting`) and
  `settings.ini` keeps it.
- Custom colours and a colour editor for a fighting game's characters (`fight/colours.h`): the
  `custom_colours` feature (text files a character and a PalMod-changed copy of the ROM file in
  `palettes/` beside settings.ini, written over the ROM image's colours), "Write the game's colours"
  and "Load colour files again", and the editor (Alternative modes → Colour editor) in the game's
  model viewer, with a library of saved sets. A game describes where its colours are and supplies its
  viewer's hooks (`colours::Description`). From Vampire Savior's, with the same files and folders.

## 0.2.0 (2026-10-13)

- A wider picture for a game's sources to ask for (`Video::wide`): up to 64 more columns on each side
  of the board's 384 (512 is the whole raster: 16:9), each scroll screen and each object drawn moved
  sideways as the sources say, and a scroll screen's tiles outside the board's own columns taken
  from the sources where the game only keeps its map up for its own picture. The window, the menu,
  the readout, screenshots and `--present-test` take the picture at the width it has; the window
  starts in the shape `Game::widestPicture` gives. With nothing asked for, the picture and the
  state hashes are what they were.
- A feature's switch can be on the launcher's first page, under "Full screen", instead of on its
  Features page: the feature has `kFeatureFirstPage`, and the game's `game.cfg` names it in
  `display` (name | title | what it says). The launcher says at every start whether it is on.
- `Video::wide.rows`: the scroll screen of 16-pixel tiles moved by its own amount on each row of the
  row scroll table.
- `Video::objectsTaken`: the sources may change the object list the picture is made from, when
  the board has taken it.
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
