# Features, hooks and finding things out

A game's own sources live in `games/<id>/src` and are compiled into the port. What they can do is in
`runtime/game.h` and `runtime/lift.h`.

## Features

A feature is a switch the player sets: in the launcher's Features page, in the in-game menu
(`port/menu.cpp` over the shared `recomp-common/menu.cpp`: Esc, F1, a gamepad stick's click or its Start and Select together), or for one run with `--features a,b,-c`. The
choices are kept in `settings.ini` (`--settings`). With every feature off the game runs exactly as
the board does.

```cpp
TheGame().features.push_back({"free_play", "Game", "Free play", "Start works without a coin.", kFeatureEnhanced});
...
if (FeatureOn("free_play")) { ... }
```

`kFeatureEnhanced` puts a feature in the launcher's Enhanced preset; `kFeatureExclusive` allows one
of its category at a time. `--list-features` prints them all (the launcher reads that).

The collection's launcher shows a feature that more than one game has, under the same name and with the
same choices, once on its Features page and sets it for every game; the rest are on each game's page. So a
feature every game can have keeps one name and one list of choices everywhere. A game marks a feature of
its own that only shares a name with another game's (a record, an animation's number) with `OwnFeatures`
(`kFeatureOwn`): it stays on that game's page.

A feature can be a choice of several things instead of on or off (`choices` in its entry; the first
means off; `FeatureValue` gives the number chosen; `--features name=2`), can last for one run only
(`kFeatureSession`: not kept in settings.ini, off again at Reset game), and the menu shows each category
as a page of its own.

Every game also has the port's own features (`RegisterPortFeatures`, `port/main.cpp`): run-ahead, rewind and the
practice point on the Game page, and on the Display page how the picture is fitted to the window (`display_scaling`,
`display_shape`: `port/present.h`) and the size it opens at (`window_size`). The first choice of each
is how the port always drew it, so they are the same in every preset.

## Players and buttons

A game says in its `game.cfg` how many play at once (`players = 3`; two unless it says, four at most) and, if it is not
a fighting game, what it calls its buttons (`buttons = Attack | Jump | Shoot`). The machine reads every player the board
has; the window reads a gamepad and keys for each of the game's players; the launcher's Controllers page has a button
for each player and, in a game's own launcher, only the game's buttons, under its names, laid out on a gamepad as a
beat-'em-up's are (`port/bindings.h`). In the collection every game shares one table: a fighting game's six buttons,
each with the other games' names for it, for as many players as the game with the most. A game can be given a layout
of its own there (the Controllers page's Layout for and Its own layout; kept in the collection's `launcher.ini`), laid
out with its own launcher's table; the shared buttons its table has not got are `none` in its `settings.ini`, and the
game, which reads the shared table, has only its own. A fighting game's table (and the shared one) has two more:
`ppp` and `kkk`, all three punches and all three kicks at once, with no key or button until they are given one.

For JoyToKey or another controller-to-keyboard mapper, turn on **Game → Keyboard only**. The game reads the mapped
keys and ignores native gamepad buttons, sticks and shortcuts, including right-stick chat. The same switch is in
the online match menu and applies to your machine. Turn it off to use your gamepad bindings again.

A controller SDL doesn't know as a gamepad (an arcade panel that isn't an XInput one, issue #213) is given its
mapping by a `gamecontrollerdb.txt` in [SDL_GameControllerDB](https://github.com/mdqinc/SDL_GameControllerDB)'s form:
beside the programs, in the launcher's folder, or (the game only) in the game's own. The launcher and the game read it
when they start (`recomp-common/gamepads.h`); a line there for the same controller wins over SDL's own.

## Screenshots

F12 saves the picture as the window shows it, at the board's size (384 by 224, wider in widescreen), with the
readouts on it but not the camera's zoom, to `screenshots/` in the game's folder, as `<game> <date> <time>.png`
(`Port::Screenshot`, issue #31). Where it went is said at the bottom for a few seconds.

**SVG pictures too** (the Game page's `export_svg`, issue #368) writes an SVG beside the PNGs of screenshots, Save scene,
Save whole stage and the model viewer's picture and animation (each frame and the sheet; not a scene clip's frames).
Each of the board's pixels is a square of the SVG's grid, so it scales to any size exactly: what a row has of a colour
side by side is one rectangle, made taller where the rows below have the same, and each colour's rectangles one path
(an opaque picture's commonest colour one rectangle behind it all); see-through pixels are left out, a hit box's
partly see-through fill keeps how much (`image_files::WriteSvg`). It is shown three times the board's lines, its pixels
in the Display page's shape (`port::SvgSize`), and drawn with `shape-rendering="crispEdges"`. A scene's is
`scene.svg`, the backdrop and then each board layer back to front and what is drawn over them, each a group (an
Inkscape layer: drawing programs show them as layers); in a fighting game's round `parts.svg`, the same of its parts;
a whole stage's `stage.svg`, each layer at its place. Drawn, they are the PNG pixel for pixel. A scene's SVG is a few
hundred kilobytes.

## Video recording

The in-game menu's **Game > Video** page, every game (`port/video_record.h`, issue #376). **Record video** (Alt+F12)
records what is played and heard as an MP4 in `videos/` in the game's folder, as `<game> <date> <time>.mp4`, until the
row (now **Stop recording video**) or Alt+F12 again; quitting the game finishes it too. The picture is as screenshots
take it, readouts and all, without the port's lines at the bottom (the speed, the zoom's label, what was saved: the
window says **RECORDING VIDEO** and the time all the while, the video doesn't). The sound is the game's as heard (the
Sound filter's too) and the custom soundtrack's, mixed at 48 kHz.

The picture is made larger with each of the board's pixels a sharp block (only the edges between blocks blended, as
the Display page's Sharp scaling does), in the shape the Display page shows it in (4:3, or square pixels); **Video
size** chooses 3 times the board's lines (672, the default), 2, 4 or 1080 lines. The size is the game's widest picture's
(its widescreen), fixed for the file: a narrower picture has black beside it. A vertical game turned upright is
recorded upright. The video's time is the game's: a frame for each of the board's frames run, so stopped or in the menu
nothing is recorded and slowed or sped up it plays at the board's speed.

It is H.264 (Constrained Baseline, BT.709 said in the stream, a key frame every two seconds, the same quantizer for
every picture) and AAC-LC at 48 kHz, which every player, browser and phone plays: minih264 and minimp4 in
`third_party/`, FAAC downloaded at build time. The encoding is done on a thread of its own: should it fall behind
(a slow machine at a large size), a picture is left out and the one before shown longer, the sound all kept, rather
than the game held up. `--record-video FILE.mp4` records a whole run, from its first frame to its last, headless or not;
headless (or `--unthrottled`) nothing is left out, the game waiting for the encoder instead, so `--movie M.txt
--headless --record-video M.mp4` makes a movie's play into a video faster than it plays.

**Save replay** (Alt+Shift+F12, the same page) saves what was just played as a video, with no recording started: the
last 30 seconds, or 60 or 10 (**Replay length**; Off keeps nothing). All the while the port keeps the game's state (a
save state's, about 350 KB) once a second and every frame's controls (`Port::ReplayKeep`, before each frame runs), so 30
seconds is about 11 MB. Asked for, the frames are run again from the oldest state with those controls, a few in each
tick in place of the port's own machine and game, which are put back as they were (as run-ahead does; the picture, the
controls and the video's switches kept by hand, as for a spare), while the encoder keeps up (`Port::ReplayMake`, no more
than 4 ms a tick: the game goes on at its speed). At each state kept the replay is put back to it, so that anything a
frame run again does differently (a save state loaded, the board reset) lasts a second at most. Checked against a
recording of the same run made as it played (sfa3, 30 seconds): the same pictures, and the same sound to the sample.
The custom soundtrack is not in it (its player is the port's, not the game's), and online play keeps nothing.

## Scene export

The picture taken apart (`port/scene.h`, issue #366; the in-game menu's **Game > Scene export** page, every game).
**Save scene** (Shift+F12) writes a folder in `exports/scenes/` beside the saves: `picture.png`, the picture as the
board drew it, and each of the board's layers by itself, with nothing where the layer has nothing: its three scroll
screens (`scroll1`, 8-pixel tiles; `scroll2`, 16; `scroll3`, 32) and its objects (which hold the fighters, the panel
and parts of stages). A CPS-2 puts objects among the scroll screens by their level, so its objects can be split in
more than one layer (`objects-0` to `objects-3`: how many scroll screens are behind them). `scene.json` lists the
layers back to front; laid over each other on the backdrop colour in that order they make `picture.png` again. A
fight on another game's stage has that game's layers too (`stage-...`), behind the game's.

In a round of a fighting game, the scene is also saved by what is in it (issue #367): `part-stage-behind`, `part-player1`,
`part-player2`, `part-stage-front` and `part-panel` (and `part-other`: shadows, and anything the game does not name),
listed under `parts` in `scene.json` back to front. Each part is the frame drawn again with only its objects and scroll
screens, so a fighter is whole where the other stands in front of it. The game's own description says what each object
is (`battle::PartOf`: the panel's, a player's by their colour sets, the stage's); the stage's pieces are behind or in
front of the fighters by where they are among the layers, and on another game's stage by that game's marks of what is in
front. Clips have the parts too, a folder each. Not yet right in Street Fighter II, Champion Edition and Hyper
Fighting, whose fighters' colour sets go by character (issue #394).

Each object entry of the board's list is saved by itself too (issue #397): `entries.png` holds every entry drawn alone
and whole (the frame drawn again with only its tiles), side by side, and `entry-map.png` says which entry each object
pixel of the picture is of (1 + its place in the list, red the low byte, green the high). `scene.json`'s `entries`
lists them front to back: the board (`game`, or `stage` for another game's), the entry's number, its first tile, colour
set, level and attributes, the box of the picture it covers, where it is in `entries.png`, its objects layer, its part
and what the game says it is (`player1`, `stage`, `panel`...). A clip's frames have the map and the list only
(`entry-map/0001.png`, `entries/0001.json`, each box what of the entry is in front), from one more drawing of the
objects (`PictureOptions::objectEntries`).

What the port draws over the picture is saved too, as layers of its own (issue #398), whenever there is any: training
mode's aids, a shooting game's hit boxes. `overlay-boxes` has the hit boxes, `overlay-hud` the rest (the text, the frame
meter), each see-through where it covers nothing and as solid as it is drawn where it does (listed under `overlays` in
`scene.json`, a folder each in a clip). `scene.json` lists the hit boxes as well (`boxes`, a clip's in each frame of
`frameList`): each box's player (1 or 2), its kind (`hurt`, `attack`, `push`, `throw`, `throw-range`; a shooting
game's `body`, `shot`, `ship`), `thrown` for something thrown, its rectangle in the picture, its colour and how solid its
inside is (`fill`, of 255). A player's own boxes are theirs by the game's records; a thrown thing's go with the thrower
where the game says (the Marvel games), else with the player whose picture they cover more of, else `null`. They are the
boxes shown with that picture, as on the screen (run-ahead's too).

Each layer is the frame drawn again from its records (`video/native.h`, `LastFrame`) with only that layer on, so what
the game changed part way down the screen is in it as it was. **Record clip** (Ctrl+F12) does the same for every frame
of 3, 5 or 10 seconds (**Clip length**), each layer's frames in a folder of their own (`objects/0001.png`...), with
each frame's layers in `scene.json`; the files are written by threads beside the game.

The stage viewer's **Save whole stage** puts a stage together from one end to the other ([more](MODES.md#stage-viewer)).

**The scene diorama** (`tools/diorama/diorama.html`, issue #380) shows what these save as cut-outs at their own depths:
the launcher's **Open diorama** (Games page, Tools) opens it in the browser, as does opening the file itself, then open a saved folder (or drop it on the page). Each layer is put further back and made
larger by as much, so the front view (Game view) is the game's picture at any spread; turn it, pull the layers apart,
hide or solo them, add shadows between them, haze or scanlines, take a photo at up to 4x, or record a video (MP4, or
WebM where the browser makes no MP4: a clip once through, a scene or stage turning). The board's pixels are shown as on
its 4:3 screen (or square, when chosen); a scene that says its pixels' shape (`pixelAspect` in `scene.json`, a pixel's width over its height: spelunkomp's boards save it) is shown in that shape. Every scene opened is kept in the browser's list until its data for the page is
cleared. In a saved scene, click an object to pick it out of its layer (a frame is drawn round it): hide it, show it
alone, or bring it forward or back a step at a time (Delete, [ and ]); its layer is drawn again without it, the objects
it covered there in its place. A clip steps frame by frame or plays at 1x to 1/16x. A scene saved with hit boxes shows
each player's as a layer of their own just in front of that player (with the board's layers, in front of the frontmost
objects), drawn from `scene.json`'s list, and the readout in front of everything; each can be switched off in the list
like any layer. A whole stage's layers go back by their parallax pace, so at a spread of 1 moving the camera across it
gives the game's own parallax. **Save 3D model (glTF)** writes what is shown as one `.glb` file for Blender and other
3D programs: the backdrop, each layer that is on, each object picked out and each overlay as a flat cut-out where it is
(at the spread chosen), each an object of its own named as in the list, and the camera as it looks; a pixel a
centimetre, as wide as the page shows it, the pictures unlit and sharp (nearest filtering), see-through pixels cut out
(a clip's: the frame shown). From its camera, Blender draws the game's picture. Nothing leaves the browser.

**Rapid fire** is the port's own (`rapid_fire`, Game page, every game): a held button is the game's pressed for two
frames and let go for two, for the first button or for all of them, from the frame it goes down. Movies keep what the
player held.

**SOCD cleaner** is the port's own too (`socd`, Game page, every game; `port/socd.h`), Fightcade's four choices for two
opposite directions held at once: Off (both reach the game, as on the board), Neutral (up + down and left + right are
neither), Hit box (up + down is up, left + right neither) and 4-way (Neutral, and of a vertical and a horizontal held
together only the one pressed last). It cleans the controls where the window reads them, before online play and
movies, so each player's choice is their own.

## A custom soundtrack

Every game whose sources say where its sound program keeps its songs gets the port's `custom_soundtrack` feature:
the player's own music files in place of the board's music (`port/soundtrack.h` has the folder's rules).

```cpp
TheGame().songTable = 0x9006;  // song 0's entry in the sound program's table (four bytes a song)
TheGame().songCount = 0x400;
TheGame().songs = {{0x01, "ryu", "Ryu's theme"}, {0x15, "versus", "Versus screen", true}, ...};
```

A song is music when its data starts with a 0 (`machine/soundtrack.h` finds them all from the table); `songs` gives
the ones that are known a name for the player's files and says which the board plays once. (A folder with no file
named for a song is a playlist: every piece of music is replaced, and `port/soundtrack.cpp` plays the files on.) Find the table from
the numbers a game asks for (`CPS2RECOMP_SOUNDTRACK_LOG`, or the shared memory's first sixteen bytes), find what
each piece is by playing (the arcade ladder with each character, attract mode, a lost game), and which end of
themselves with `--music-test 5400` (with `--test-switch 300:99999999`, so that the game asks for nothing).
`games/<id>/src/music.cpp` is each game's. In every sound program seen so far (the CPS-2's drivers and the CPS-1
QSound board's) the table follows a word of how many songs there are, near the start of the second 32 KB: a run of
four-byte entries whose fourth byte is 0 and whose address is inside the program finds it.

## The board's timing

The port's `slowdown` feature (Game page; its first choice, Arcade perfect, unless another is chosen) runs the 68000
with the board's waits and the video chip's DMA ([TIMING.md](TIMING.md)); `slowdown=1` (None) gives it every clock.
With the board's timing, the game's own code runs in place of the rewrites, whose time is only their
originals' without the waits; a game whose sources add something inside a rewrite (a cheat that changes the damage,
widescreen's camera, a mode of its own) says when the rewrites are needed with `TheGame().rewritesNeeded`, and they
run then, with time close to the board's but not exact. A new feature that works inside a rewrite belongs in that
list.

## Commands

`TheGame().commands` are things the game's sources do when asked: a row of the menu (on the first
page, or a category's), and a key while playing if it names one (`kKeyRestart`: Backspace or a
gamepad's left shoulder). A command gets the machine; it may reset it, load something it kept
(`machine.Serialize`), and ask for the game to run unseen until it says otherwise
(`SetFastForward`) while it works the controls from `beforeFrame`. Vampire Savior's "Training
mode" and "Start over" are commands (`src/training.cpp`).

## Stopping and slowing

The port can stop the game (`P`), go on a frame at a time (`O`) and run it at a half, a quarter
or an eighth of its speed (the menu's Speed row, `-` and `=`). The game's state is the same
whatever was done (`--keys TICK:KEY,...` presses these keys in a headless run for tests, sets
features with `fNAME=N` and runs commands with `cNAME`).

## Rewind and the practice point

Two switches on the Game page, off unless chosen, in every game and never online (`Port::RewindTick`,
`Port::PracticeSet`; what a player sees of them is in [PRACTICE.md](PRACTICE.md#rewind-and-a-practice-point)).
**Rewind** keeps the game's state (a save state's, as `Port::KeepState` makes it) every 3 frames, before the frame
runs, with that frame's controls, in a ring of 600 (`port/rewind.h`, `StateRing`): the newest whole, each older one
as how it differs from the one after it, XORed and its runs of equal bytes packed, so half a minute of play is 15 to
70 MB (Giga Wing; 256 MB at most). Keeping one takes about 0.1 to 0.2 ms. Held (R, or a gamepad's left trigger), each
tick puts the newest back and runs its frame again with its controls, drawn and unheard, so the game goes back three
frames a tick (the custom soundtrack, the port's own player, plays on); the frame count goes back with it, and the instant replay's states start again from there. **The
practice point** is one state kept by a key (`]`), put back by Backspace, and the controls of every frame since,
which Backslash plays again from the point until a button is pressed that was not held when it began. Until a point
is set, Backspace and Backslash are the game's (`kKeyRestart`, `kKeyExtra`). `--keys TICK:]`, `TICK:k1` and
`TICK:k2` press them in a headless run, and `TICK:w` holds rewind down (again: lets it go); a movie (`--movie`)
follows the frame count back. `make GAME=<id> check-rewind` (`tools/check_rewind.sh`) checks that the game comes out
as it would without them. `CPS2RECOMP_REWIND_LOG=1` says every 10 seconds how many states are kept, their memory and
what keeping one took.

## The camera's zoom

The window can show part of the picture larger, about one player's fighter (`port/zoom.h`): a gamepad's left
stick's click held and a shoulder pressed, left for player 1 and right for player 2, at 1.25x to 2x (keyboard:
Left Shift held, Q and E). The fighter stays in the middle; the d-pad (arrows) with the click (Shift) held moves
the camera off it. The right stick's click (W) keeps both fighters in the picture instead, as close as shows both
whole; how much room a fighter takes is `Game::focusTall` (from its feet up) and `focusLift`. It is the
window's alone (the game, save states and screenshots never see it). A game has it once its sources say where to
look: `TheGame().focus = [](int player, int &x, int &y) { ...; return true; }`, the place in the picture's pixels
(widescreen's columns counted) to keep in the middle, false when there is no fighter to look at. A fighting game gives
it through `modes::Focus(place, lift, tall)` (`fight/alternative.h`): `place` the fighter's feet in the picture, `lift`
how far above them to look, `tall` how tall a fighter stands; it is on only while a round is fought and not in the
model viewer. Vampire Savior's
is in `src/camera.cpp`: where the fighter stands, 44 lines up from its feet. `--zoom SIZE:PLAYER` starts zoomed.

The menu's **By distance** (`--zoom 1:4`, issue #272) sizes the camera by how far apart the fighters are (1.11x as a
round starts, 1.39x together, the whole picture from four fifths of the way to the ends) and leaves the game's panel
unzoomed. For that the frame is drawn twice, a line at a time (`Machine::panel`): once as it is, with the panel's pixels
marked, and once without the panel; the window zooms the second and lays the panel's pixels over it at their size. The
panel is `Game::panel` (its objects, by tile and attributes, as `Video::hideObject` has them) and `Game::panelLayers`
(its scroll screens), which `battle::Register` fills from the description's `thePanel` and `panelLayers`; a game
without them has its panel zoomed with the rest.

## Where a feature does its work

- **After every frame** (`TheGame().frame`), on the game's memory: `Ram8/16/32`, `SetRam8/16/32`
  take the 68000's addresses. Good for things that are a value to hold (credits, a gauge).
- **Before a frame** (`TheGame().beforeFrame`), with the players' input set: to change what the
  game is told is pressed.
- **In the game's own code, with a hook** (`lift.h`):

  ```cpp
  AddHook(0x018AB0, "TakeDamage", [](M68K &c) { ...; return false; });
  ```

  The function runs whenever the 68000 arrives at that address (the start of a routine or block),
  with the registers as the game has them. Returning false lets the game's code go on; returning
  true says the hook did the routine's work and left the CPU where the routine would (`Return(c)`
  is its `rts`). The translator reads hook addresses out of the sources, so after adding one,
  `make` again. Hooks run the same in every `--cpu` mode, so `make check` covers them.
- **By rewriting the routine** (`AddRewrite`): the game's rule as readable C++, with the feature a
  line inside it. See [READABLE.md](READABLE.md).
- **A readout over the picture**: `SetHud(lines)` shows short lines of text as small panels (a
  training display), `SetBoxes(boxes)` draws rectangles (hit boxes). `TheGame().state` saves the sources' own variables with save states.

Vampire Savior's sources: `vsav.h` (names for what is known of the game), `cheats.cpp` (free play;
damage and the clock through hooks), `training.cpp` (damage readout, input display, health that
refills, and the dummy: `beforeFrame` changes what the game is told player 2 presses).

- **A wider picture** (`machine.video.wide`, video/video.h; set before every frame): `side` more
  columns on each side of the board's 384, up to 64 (512 columns, the board's whole raster, is 16:9
  where 384 is 4:3). Nothing about the game changes, only what is drawn of it, so the sources say
  how: `scroll[n]` draws a scroll screen moved to the right, `object` says how far each object is
  moved (the stage's go with the view, a panel's stay), and `tile` gives the tiles beyond the
  board's own columns for a scroll screen whose map the game only keeps up as far as its own
  picture shows. `TheGame().widestPicture` tells the window what shape to start in. Vampire Savior's
  `widescreen.cpp` is the example.

- **A switch on the launcher's first page.** A feature with `kFeatureFirstPage` is left out of
  the list the launcher's Features page is made from; the game's `game.cfg` puts its switch under
  "Full screen" with `display = name | Title | What it says` (more of them with `||` between). The
  launcher keeps the switch in its own `launcher.ini` and starts the game with `--features name` or
  `--features -name`, which goes before what `settings.ini` has. The in-game menu still has it.

## Finding things out

- `--dump-ram F:FILE` writes work RAM after frame F (and the video memory and registers beside
  it); `tools/ramfind.py DUMP=VALUE ...` lists the addresses that held known values in several
  dumps. That is how the credit counter, the clock and the health words were found.
- `--poke ADDR=BYTE,...` holds bytes at values, to try a guess.
- `--trace LOW-HIGH:FILE` writes a stretch of work RAM after every frame; with a movie that does
  known things at known frames (walk, punch, crouch, jump) the bytes that change with each show
  what they are. That is how the players' positions and the attack flag were found.
- `--watch LOW[-HIGH]` reports every write the 68000 makes to an address range with the address of
  the instruction that made it; `work/<id>/program.lst` is the disassembly to read it against.
  That is how the damage routine and the clock's tick were found.
- `--shot-every N:PFX` and `tools/sheet.sh` make a contact sheet of a run; `CPS2RECOMP_LAYERS=mask`
  draws only some layers (bit 0 objects, bits 1-3 the scroll screens); `CPS2RECOMP_WIDE=n` draws n
  more columns on each side of the picture (64 at most), to see what a game has beyond its edges.
- `--state-save F:FILE` / `--state-load F:FILE` and movies (`--record`, `--movie`) get back to a
  moment exactly.

## The menu is shared

The menu itself (its look, its rules, the pages made from the features and commands) is the one the
recomp ports share: `recomp-common/menu.{h,cpp}` and `recomp-common/text.h` are a copy of the `recomp-common`
repository's `src/`, made by `tools/sync-common.sh` (which records the commit and the files' hashes in
`recomp-common/COMMIT`) and never changed here. `make check-common` fails if the copy is not what was
recorded; `make check` and `make check-rewrites` run it first. To change the menu, change
recomp-common, run its tests, commit there, then run `tools/sync-common.sh` here and commit the copy.

`port/menu.{h,cpp}` is what is this toolkit's about it: the first page's rows in their three groups
(save states; the pages, Speed, Insert coin, Player 2 joins; Reset all options, Reset game, Quit game,
Resume), CPS-2's
buttons turned into the menu's six, the speeds, and the games' features and commands handed over.
A game needs nothing more: its features and commands show up from `TheGame()`. A game with pages
that are under another page, or a page that belongs at the head of the menu before the save states
(a page of ways to run the game, say), names them in `TheGame().pages` (`runtime/game.h`, `Page`).
