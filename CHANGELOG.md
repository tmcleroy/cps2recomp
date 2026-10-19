# Changelog

## Unreleased

- **Vampire Savior games: tester feedback** (from vampire-savior-recomp#49). Endless battle (Shuffle, Scroll) and
  ring out: what the stage has in front of the fighters (Abaraya's kettle, Green Scream's leaves) is in front of them
  again, and a move's own picture (Jedah's Finale Rosso turning everything red) replaces the stage behind while it is
  up (`fight/battle.cpp`, for every game: a stage machine's pieces listed before its own players are drawn over the
  fighters; `Description::planesHeld`). The fighters' unseen match is on Feast of the Damned
  (`Description::fightersStage`): on Fetus of God its camera took the stage behind past its ends. **Training: Gauge
  refills** (`gauge_refill`). **Widescreen**: Felicia's wall cling, Q-Bee's flight to the wall and carrying throw, and
  a catch of Bishamon's go to the wide picture's edge (`games/vampire_savior/src/moves.cpp`, readable rewrites, with
  their places in Vampire Savior 2 and Vampire Hunter 2 in `family.h`).

- **A release has only its three downloads**, their checksums and three small update lists (it had a file for
  every file of every download too, 46 more). The launcher's **Update** now downloads the system's download
  itself, checks it, and puts in place what changed (recomp-common 91b4e51; `update-<system>.txt` names the
  archive, `tools/release/lib.sh`). **A launcher of 0.4.0-beta.2 can't update itself to this version** (it reads
  only the old lists and says the list of files could not be read): download it by hand once, and later versions
  come through Update again.
- **Each release's page says how to install it** on macOS, Windows, Linux and the Steam Deck and Steam Machine,
  above its changes (`tools/release/notes.sh`, used by the release workflow).

## 0.4.0-beta.2

- **Vampire Savior Definitive** no longer sometimes draws a screen of wrong tiles: the scroll screens looked up
  whether a tile was the extra set's at the graphics' offset instead of the map's place, reading past the end of
  that list (`video/video.cpp`, DrawScroll; the CPS-1 games' tile offset had taken the map base's name).

- **Ring out, in Street Fighter Alpha 2 and the Vampire Savior games**: a fighter knocked over the end of the stage
  is thrown back with the game's own knock-down animation (and in Street Fighter Alpha 2 its cry), and falls
  through the floor and out of the picture (before, its picture slid down); it is out as soon as it is past where
  the board would have stopped it, while still in sight. The winner jumps off, and at the top of its jump goes on
  in the next match as it was (place, picture, speeds): the camera follows it down past the old stage to the next
  one's floor, the winner in front of both, and it lands as from any jump. (Vampire Savior's way down faded
  through black; it is the same as Street Fighter Alpha 2's now.) The winner no longer stands about waiting for the
  next match: each match and stage after the first is begun from a machine kept at the character choice (about 500
  frames to its round, not 2,000 from switching on), and made ready twice as fast while the one out falls. In Street
  Fighter Alpha 2 the player's colours stay the ones chosen from match to match (they went back to the first set). Each
  match begun from the character choice is given a random number of its own, so the computer's choices do not repeat
  from one match to the next.
- **The fighting modes in the toolkit** (`fight/battle.h`): survival, the endless battle and the ring out, one
  copy for every fighting game, which describes its memory and makes the ring out's two movements at its players'
  step (`games/sfa2/src/player.cpp`, PlayerFighting; `games/vampire_savior/src/battle.cpp`, RingOutStep from the
  player step hook). The camera may be held beyond a stage's ends (`CameraWanted::beyondEnds`).

- **Updates in the launcher**: it checks this repository's releases when it opens and offers a newer version at the
  top of the Games page; **Update** downloads only the files that changed (a new game: its program and the
  launcher), checks each against the release's SHA-256, puts them all in place or none, and starts the new launcher
  (recomp-common 1ea2766, `launcher/src/update.h`). The release scripts write what it reads: each system's
  `update-<system>.txt` and every file gzipped under its SHA-256 (`tools/release/lib.sh`, `update_files`;
  docs/RELEASING.md). The first release with them is the first a launcher can update to: a copy older than this
  feature is updated by hand once.

- **Vampire Savior Definitive** (beta; `games/vsav_definitive`; ROM sets `vsavj` and `vsav2`): Vampire Savior with
  Donovan, Phobos and Pyron taken from Vampire Savior 2 (its code, pictures and sounds), all eighteen characters
  by Vampire Savior's rules. At the character choice the three have places of their own, a row under the grid
  with their faces (and the Definitive page of the in-game menu);
  the training mode, the modes and the model viewer have them; the arcade goes to its end with them. The launcher
  gives it the `vsav2` set chosen on Vampire Savior 2's page (recomp-common 568e39f, `Launch::file_of`; `--rom2`).
  How: an extra ROM set mapped from A00000 (`game.cfg`'s `extra_set`), objects and scroll tiles drawn from its
  graphics, a second sound side with its sound program and samples (`machine/sound_board.h`), and `tools/graft.py`
  with the game's `graft.cfg` (tables by character, `extend`, `mask`, `divert`, `ram`, `patch`). Not yet: Donovan's
  stage and Vampire Savior 2's music.
- Hooks are numbered in 16 bits (from the 256th on they went to the wrong hooks).
- **Two beat-'em-ups for three players: Alien vs. Predator and Cadillacs and Dinosaurs** (`games/avsp`, ROM set
  `avspu`, USA 940520, a CPS-2 game; `games/dino`, ROM set `dino`, World 930201, a CPS-1 game with QSound). Each has
  three players at once, free play, a round or stage to start at, the board's settings on the Board page (Alien vs.
  Predator's Players setting lets the third join: on in Enhanced), infinite health, lives and time, straight to the
  title, and everything the port gives every game. Both translated whole and checked (`make check`).
- **A CPS-1 board with QSound** ("CPS-1.5") in the toolkit: `board = cps1` and `cps_b` in a game.cfg; its memory map,
  inputs for three or four players, 12 MHz, the objects in video RAM placed among the scroll screens by the layer
  control (with the layer below them keeping its high pens in front), the settings memory as 128 bytes, no program
  cipher, and the sound CPU's Kabuki cipher deciphered with a key worked out from the player's own ROM in about a
  second (`machine/kabuki.*`; no key is in the repository). The Z80 reads opcodes through `Z80Bus::ReadOpcode`.
- **Up to four players**: game.cfg's `players`, the CPS-2's third and fourth players' wiring, movies with a column
  for each player who presses something, run-ahead and the spare machines keeping every player. recomp-common's
  bindings take up to four players; the Controllers page has a button for each.
- **A game's own names for its buttons** (game.cfg's `buttons`, "Attack | Jump | Shoot"): its launcher shows them and
  only them, laid out on a gamepad as a beat-'em-up's are; in the collection every game shares one table, a fighting
  game's six with the other games' names beside them.
- **Rapid fire** (`rapid_fire`, Game page, every game): a held button pressed for two frames and let go for two, for
  the first button or all of them; the first press is not held back.
- `--dump-ram` may be given more than once. The launcher's words fit a game with no key file or training mode.

- **Arcade timing** (the `arcade_timing` feature, Game page, off unless chosen): the 68000 runs as on the board,
  its acknowledge a clock late for every access outside 500000-7FFFFF and its bus taken by the CPS-A's DMA (the row
  scroll, the scroll layers' tile rows, the palette), so a game slows down where and as much as on the board
  (`docs/TIMING.md`). Measured with jtcps2 simulated as a reference: without it the port's 68000 did about 1.35 times
  the board's work in a frame; with it, Street Fighter Alpha 2's work RAM is the reference's at 5,639 of 5,671 frames
  through a whole fight (the rest are registers kept aside, caught at a different point at the frame's end). With
  it on, the game's own code runs in place of the readable rewrites, but where training mode, the sources' own
  modes, widescreen or a cheat need them (`Game::rewritesNeeded`). With it off nothing changes. Save states are
  version 7 (5 and 6 still load). `CPS2RECOMP_BUS_LOG` logs each frame's waits and DMA.

- **Two more games: Vampire Savior 2 and Vampire Hunter 2** (`games/vsav2`, `games/vhunt2`; ROM sets `vsav2`, Japan
  970913, and `vhunt2`, Japan 970929). Capcom made both from Vampire Savior's program: Vampire Savior 2 brings back
  Donovan, Phobos (Huitzil) and Pyron, Vampire Hunter 2 is Night Warriors' cast on Vampire Savior's engine, and
  both have a thirteenth stage (Donovan's). They build Vampire Savior's sources, so each has everything its port
  has: the training mode and the dummy, widescreen and invisible walls, survival, the endless battle, ring out, the
  stage and model viewers, attract mode, the hidden characters (each game's own: Oboro Bishamon as the board gives
  him, Shadow, Dark Gallon in Vampire Hunter 2), custom colours and the colour editor (PalMod's file in Vampire Savior
  2 only), a custom soundtrack with each game's music named, the board's settings, save states and run-ahead. Every
  rewrite is checked against each game's own code (`make GAME=<id> check-rewrites`), and widescreen as Vampire
  Savior's is (`check-widescreen`).
- **A game made from another's program builds that game's sources** ([docs/TOOLKIT.md](docs/TOOLKIT.md)): a
  `game.cfg`'s `sources`, places named `At()` and `AtA5()` and listed in the game's `family.h`, `tools/portmap.py`
  (where one program's code and variables are in another's) and `tools/places.py` (that list); a hook or rewrite at
  place 0 (`kNoPlace`) is not made. Vampire Savior is as it was, frame for frame.

- **A custom soundtrack's folder can be any folder of music**: with no file in it named for a song, its files play
  as a playlist in place of all the game's music, one after another in their names' order ("2 ..." before
  "10 ..."), from the first again after the last; the feature's new choice "On, shuffled" plays them in a random
  order, a new one each time round. Files named for songs work as before.

- **A custom soundtrack**, in every game: your own music files (Ogg Vorbis, FLAC, MP3 or WAV; an arranged soundtrack,
  say) in place of the board's music, song by song, with the board's sound effects and voices. The "Custom
  soundtrack" feature (Sound; on in Enhanced) and a folder on each game's page (recomp-common 6c526f4), or
  `--soundtrack DIR`. Files are named after the songs, which each game names in the `songs.txt` it writes there
  (Street Fighter Alpha 2's 59, Vampire Savior's 80, Super Puzzle Fighter II Turbo's 24); `soundtrack.ini` names
  other files, their loops and loudness. The music the game asks for is caught where it leaves its requests for the
  sound CPU (`machine/soundtrack.h`), which is then told to stop its music, and played in the game's time: save
  states (now version 6; version 5's still load), run-ahead and spare machines keep it where the game is. With the
  feature off, or no files, nothing changes. Decoders: stb_vorbis, dr_flac, dr_mp3, dr_wav (`third_party/`,
  public domain).
- `CPS2RECOMP_SOUNDTRACK_LOG` lists each piece of music the game asks for, with the frame; `--music-test FRAMES`
  plays each on the board in turn and says how long it lasts; `--soundtrack-dump FILE` writes what the soundtrack
  plays (headless).

- The games can run at once, each in its own window (one of each). A shared feature one game's menu changes while
  another runs is set in that one when it ends. (recomp-common 5e82073)

- `make collection-app` makes the collection's app bundle for your own machine (`build/collection/CPS-2 Recomp.app`:
  the launcher and every built game, with Homebrew's SDL 3 inside), as `make GAME=<id> app` does for one game.

## 0.4.0-beta.1

CPS-2 Recomp: the toolkit and its games in one repository, and one launcher for all of them.

- **The games are here**: Street Fighter Alpha 2 (`games/sfa2`), Super Puzzle Fighter II Turbo
  (`games/puzzle_fighter`) and Vampire Savior (`games/vampire_savior`), each with its README, docs, movies and
  checks. Each was its own repository with a copy of the toolkit; their histories until now are in their own
  changelogs (`games/<id>/CHANGELOG.md`). A toolkit change now reaches every game at once, and is checked with
  every game. `tools/sync-toolkit.sh` and the toolkit copies are gone.
- **One launcher for every game** (recomp-common's collection launcher; `make collection`): a Games page that
  finds each game's ROM set in a folder you choose (or one dropped on the window), a Features page with what the
  games share, set for all of them at once (a feature changed in one game's own menu is set for the others when
  it ends), the Controllers page for all of them, and a page for each game whose ROM set you have, with its
  Display switches, Play, and the features only it has. Each game keeps its own settings folder, as its own
  launcher did, and the ROM set chosen there is taken over.
- From a terminal: `cps2recomp-launcher --play ID` (straight into a game), `--play ID -- ARGS` (more arguments
  for the game, after the launcher's own), `--page ID`. A game's program still runs by itself, with all its
  arguments; `--features` may now be given more than once (later ones win). A game's program opened by itself
  starts the collection's launcher at its page when there is no launcher of its own beside it.
- The release is the collection: `CPS2Recomp-macOS.zip`, `CPS2Recomp-linux-x86_64.tar.gz`,
  `CPS2Recomp-windows-x64.zip`, each with the launcher and every game (`tools/release/`). CI builds and starts all
  of it, with no translated code, on all three systems.
- Build from the top: `make GAME=<id>` and its targets as before, `make games`, `make collection`,
  `make collection-app`; ROM sets in `rom/<id>/`.
- Settings: `kFeatureOwn` (`OwnFeatures`), a game's feature that the collection never sets for another game (the
  survival and ring out records, the animations' numbers).

## 0.3.0 (2026-10-15)

- recomp-common 0.5.0: in the in-game menu, Right on a page's row goes into that page; the
  launcher's Features page has a search box (Ctrl+F or Cmd+F; Esc empties it, a second Esc leaves it;
  a gamepad's A goes in and B leaves with what was typed kept).
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
