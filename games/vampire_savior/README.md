# Vampire Savior Recomp

> Part of **[CPS-2 Recomp](../../README.md)**: one download and one launcher for every CPS-2 game this
> project has ported. This page is about Vampire Savior; the [main page](../../README.md) says how to
> download and play.

An unofficial native PC port of the arcade game *Vampire Savior: The Lord of Vampire* (Capcom,
1997, CPS-2 board) for macOS, Linux (Steam Deck included) and Windows, made by static
recompilation, with a full training mode.

**[Get started: download it and play →](#playing)**

> **Beta.** The game plays from start to ending and its code is checked instruction by
> instruction, but it has been played on few machines and controllers yet. Expect rough edges, and
> please [report what you find](#feedback-and-contributing).

**No game files are included.** You need your own copy of the game: the arcade ROM set `vsavj`
(Japan, 970519) with its key file, from a board you own. **Only that version works**: the sets of
other regions and revisions (`vsav`, `vsavu`, `vsava`, `vsavh` and the rest) do not. This project
cannot help you get one.

This project is not affiliated with, endorsed by, or sponsored by Capcom. *Vampire Savior* and
*Darkstalkers* are trademarks of Capcom Co., Ltd.

![The title screen](../../docs/screenshots/vampire_savior/title.png)

### New: dress the characters in your own colours

A colour editor in the game, with the character in front of you: any of their ten colour sets, 16
levels each of red, green and blue, random colours at the press of a button, and PalMod's colours
too. Then choose the character with that button and fight in them. [More, with a video
→](#custom-colours)

![The colour editor's Random colours, six times on Lilith](../../docs/screenshots/vampire_savior/random-colours.gif)

## What it is

The game's 68000 program is translated ahead of time into C++ and compiled into the port, and the
parts that features touch (the camera, the stages, hit tests, the fight's rules: 123 routines so
far) are rewritten by hand as readable C++. What the program expects of the board (its video, the
QSound chip, the sound CPU and its registers) is code written for this project, not an emulator
([how it works](docs/PORT.md)).

- Plays with a keyboard, a gamepad or an arcade stick; every key and button can be changed, for each player, in the
  launcher. Two more can be given a key or a button: all three punches at once, and all three kicks. In the collection
  it can have a layout of its own, apart from the other games'.
- An in-game menu (Esc or `F1`; on a gamepad, the left stick's click or Start and Select together)
  with the training mode and the other modes, save states, speed (1/8 to 8x), the cheats,
  every feature, and a Reset all options row.
- The picture your way (Display page): sharp, whole pixels or smooth; 4:3, square pixels or the
  whole screen; and the window's size.
- **Zoom**: hold the left stick's click and press the left shoulder to zoom in on player 1's fighter, the right
  shoulder for player 2's (keyboard: hold Left Shift and press Q or E). Each press for the same player goes a size
  up, 1.25x, 1.5x, 1.75x and 2x (the model viewer's sizes: at 2x the top of a tall fighter is off the screen), then
  back to the whole picture; a press for the other player looks at that one instead. The fighter is kept in the
  middle all the while (past the stage's ends is black); with the click (or Left Shift) still held, the d-pad or
  the stick (the arrows) moves the camera to frame it as you like, until the zoom is off or looks at the other fighter. The size eases from one
  to the next, it is said at the bottom for a few seconds, and outside a fight the picture is whole again. The
  game's own health bars are zoomed with it; the port's own text (a mode's score, the training display) is not.
  With the left stick's click held, the right stick's click (keyboard: Left Shift and W) keeps **both fighters** in
  the picture instead: the camera as close as shows both whole, tight when they are close and wide when they are
  apart (2x at most, the whole picture at least), kept inside the stage; again for the whole picture. The left
  stick's click alone still opens the menu, when it is let go.
- **Rapid fire** (Game page): a button held down is pressed again and again, fifteen times a second, for the first
  button or for all of them; the first press is seen at once.
- **Pure** plays exactly as the board does. **Enhanced** (the default) adds free play and your own
  [character colours](#custom-colours). Every other feature is a switch of its own.
- The board's operator settings (difficulty, attack power, timer speed, game speed and the rest) on
  the Board page (launcher's Features, in-game menu), each as the board is set unless you choose; or
  the game's own test menu from the in-game menu.
- A search box on the launcher's Features page (Ctrl+F or Cmd+F) finds any feature by a word in its
  name, description or choices.
- **The board's speed**: the processor runs as on the arcade board, so the game plays at the board's speed and
  slows down where and as much as the board does; **Slowdown → None** (Game page) gives it every clock instead.
  [More →](../../docs/TIMING.md)
- **A custom soundtrack**: your own music in place of the board's music, with the board's sound effects and voices:
  a folder of music plays as a playlist (in order or shuffled), or files named for the game's songs replace those
  songs. The `songs.txt` the game writes in the soundtrack folder names all 80 pieces
  of its music (each stage's, each character's win and ending, the opening and the screens between fights). [How →](../../docs/SOUNDTRACK.md)
- Your ROM set is only ever read. Settings, save states and the board's settings memory go in your
  own user folder, or beside the launcher in [portable mode](../../docs/LAUNCHER.md#portable-mode).

| | |
| --- | --- |
| ![The launcher: choose your ROM set and press Play](../../docs/screenshots/vampire_savior/launcher.png) | ![The in-game menu, over a fight](../../docs/screenshots/vampire_savior/menu.png) |
| ![Endless battle in widescreen: Felicia and Morrigan in Vanity Paradise](../../docs/screenshots/vampire_savior/endless-widescreen.png) | ![The model viewer: Lilith's animations A60 to A90 on white](../../docs/screenshots/vampire_savior/model-viewer.gif) |

## Training mode

"Alternative modes" at the top of the in-game menu has a Training mode page: Start goes straight
to the character choice with two players and into a fight with no clock that nobody loses, and
the page holds the aids below.

| Feature | What it does |
| --- | --- |
| Hit boxes | The boxes the game itself tests: where a player can be hit (blue), attacks (red; orange for things thrown), and the boxes that keep the players apart (green) |
| Frame data and frame meter | Startup, active and recovery of each move and the advantage after a hit or a guarded hit, counted in the game's own steps; a row of coloured cells for each player, one a step |
| Input display | What each player presses and for how long, the stick as a keypad number |
| Combo readout | Hits, damage, how much is lost for good and how much comes back, the round's best |
| Health refills | A player's health comes back when they have not been hit for a moment |
| Gauge refills | Both players' special gauges kept full: ES and EX moves and Dark Force whenever you like |
| The dummy | Stands, crouches, jumps; guards (always, after a hit, at random, the first hit only, standing only or crouching only; high or low as each attack needs); pushes away or guard cancels; softens throws; gets up where it fell or rolls towards or away; records and replays in three slots (one, or any at random), and can answer with a recording after guarding or being hit |
| Start over | The round as it began, in the middle or a corner, sides swapped or not (Backspace, or a gamepad's left shoulder) |
| Stage | Any of the twelve stages, or the game's own choice |
| Stop, step, slow | Stop the game, go on one frame at a time, or play at a half, a quarter or an eighth of the speed (anywhere, not only in training) |

Also: save states in nine slots, free play, and a few cheats (no damage, one-hit knockouts, full
gauge, infinite time). [docs/FEATURES.md](docs/FEATURES.md) has the whole list.

![Training mode: hit boxes, input display, frame data and the frame meter](../../docs/screenshots/vampire_savior/training.png)

## Alternative modes

The in-game menu's first row, "Alternative modes", has ways of playing and watching the game that
the board does not have. Each has a page with a Start row and its own choices, and runs until
another is started or the menu's Reset game.

| Mode | What it does |
| --- | --- |
| Training mode | A fight with no clock that nobody loses, and the aids above |
| Attract mode | The game's own show on a loop, as a cabinet left alone plays it, with no coin to keep out: everything, the introduction only, or its eight demonstration fights one after another |
| Stage viewer | Any of the twelve stages with nobody on it and no panel, its own pieces moving and its music playing (or another stage's). Scroll it with the stick, or let it drift from end to end. "Infinite" is all twelve in turn, each scrolling into the next as its tune fades into the next one's ([how it works](docs/INFINITE_STAGE.md): two games at once on one machine) |
| Model viewer | One character by itself, on a stage or on plain white or black, from a quarter of its size to 25 times. It stands and moves with your controls, or goes through any of its animations (named A0, B12, C3... as the game's own lists number them; left and right step through them), at full speed or in slow motion. At its own size, what the stage has in front of the fighters stays in front (or, with "Stage in front" off, behind it) |
| Survival | One opponent after another on the health you have left. How many can you beat? |
| Endless battle | Two fighters the game plays, for as long as you like, with no bars and no clock, on one stage, on stages that fade one into another, or on a floor that scrolls through them all. Touch the controls to take a fighter over |
| Ring out | No health. Knock your opponent over the end of the stage (it is thrown back, as by a knock-down, and falls through the floor), then jump down after it and land on the next stage. The CPU plays it as a ring out, not for damage, and guards as it always does: it walks you towards an end, throws or sweeps you when your back is to it, and keeps off its own end, getting out of a corner with a throw that swaps sides or a jump over you |
| Ball battle | A ball game. A line splits the stage and each fighter keeps to a half; hit the big ball with your attacks (harder attacks send it faster, and where you strike it decides the angle: under it for a lob, over it for a spike) past the other fighter and off their end for a point. Three hits in a row make a power shot; the ball makes whoever it hits reel or fall unless they block, and the fighters can hit each other too (nobody loses health). First to 3, 5 or 10, against the CPU (easy, medium or hard) or player 2; the whole court in a 16:9 frame, or a camera that follows the play ([more](../../docs/COURT.md)) |
| Tag team battle | Two teams of two or three, chosen on its page: one fighter of each team at a time, all on one stage, each on one health bar (a team of two has two bars between them, as one fighter has in a usual match), or as many as the board gives. The one knocked down is out and the next of that team comes on with a full bar; the winner stays on with the health it has left. Team 2 is the CPU (its difficulty chosen on the page), or player 2. Fighters begin ready, with no entrances, and the same character twice in a match (on either team) is never in the same colours. With "Tag team" on the same page, each fighter is a person's own: co-op (players 1 and 2 against the computer's team) or 2 vs 2 for four players, a label under the bars saying who is on. **Tag style**, the default ("Style: Knock-out" for the one-at-a-time style above): Marvel-style, each fighter keeps a bar of its own and you tag the next one in with a gamepad's left trigger (or Start) whenever yours can act: yours calls the next one in and flies off the screen, and the next one jumps in from that side with a jump attack that can hit. When a bar empties, that fighter is knocked down (the announcer's call with it) and lies there for two seconds, not to be hit (time for a setup), and the next one jumps in, resting fighters get back the health that comes back by itself, the team's super gauges carry over, the others' health is listed under the bars, and a time over goes to the team with more health in all. The CPU's team tags too, as a person would: when the one on is low and a partner has more (or has rested), while you are down or far off, or after backing away from you first, never in the middle of your combo, and more or less often by its difficulty. In co-op and 2 vs 2 the partner tags in with their own left trigger |
| Colour editor | Change a character's colours while you watch: [custom colours](#custom-colours) |

![The model viewer: Lilith's animations A60 to A90, one after another, on white](../../docs/screenshots/vampire_savior/model-viewer.gif)

## Widescreen

![Widescreen: Morrigan and Lilith in Feast of the Damned, the whole hall in view](../../docs/screenshots/vampire_savior/widescreen.png)

A switch on the launcher's first page, under "Full screen" (and on the in-game menu's Display
page). Fights are shown and played in 16:9: the picture is 512 columns wide where the board's is
384, at the same height and with everything the same size, so there is a third more of the stage
in view.

It is **off unless you switch it on**, in every preset, because it changes how the game plays:

- The players can go to the wide picture's edges, so they can stand further apart than on the
  board (464 at most, where the board allows 336). The stage's own ends are where they were.
- The camera stops 64 short of where it did, so that the picture ends where the stage does.
- A stage's own pieces and things thrown are kept while they are in the wide picture, not only
  the board's. (Some pieces still come in a little late at the wide picture's edge, and at the
  very ends of a stage a few columns of the planes behind are not painted: see the roadmap.)
- The bars, the clock and the gauges stay in the middle. Survival, the endless battle, the ring
  out and the stage viewer are wide too. Everything that is not a fight keeps the board's 4:3.

With it off the game is exactly the board's. Nothing here stretches the picture: the extra
columns are the game's own stage, drawn by the port from what the game already has.

**Invisible walls**, on the Display page (launcher's Features, in-game menu), keeps the 16:9 picture and plays the game
exactly as the board does: the players are stopped where the board's picture ends, by walls that
are not seen, the camera goes as far as the board's, and the stage's pieces and things thrown are
kept as on the board. Near a stage's end the view stays on the stage, so the play area is then to
one side of the wide picture. The game's memory is the same as with widescreen off, frame for
frame (`make check-widescreen` compares it along the ladder). Pieces of a stage that the board
draws only once they are in its own picture can come in at the play area's edge rather than the
picture's.

## Custom colours

Give any character colours of your own: change them in the game's colour editor, or bring the ones
you have made with PalMod.

https://github.com/user-attachments/assets/bb498cab-aac9-4e74-94a5-6c4f0d958ff6

| | |
| --- | --- |
| ![The colour editor: Lilith with cyan hair and a blue outfit, the panel at the right](../../docs/screenshots/vampire_savior/colour-editor.png) | ![Lilith in a fight in colours from the editor's Random colours](../../docs/screenshots/vampire_savior/custom-colours-fight.png) |

Morrigan in eighteen sets of colours made with the editor:

| | |
| --- | --- |
| ![Morrigan in nine colour schemes made with the colour editor](../../docs/screenshots/vampire_savior/morrigan-colours-1.png) | ![Morrigan in nine more colour schemes made with the colour editor](../../docs/screenshots/vampire_savior/morrigan-colours-2.png) |

**The colour editor** is on the in-game menu's "Alternative modes" page. It shows the character
with a panel at the right, worked with player 1's controls: up and down choose a row, left and right
change it.

- **Character, background, size, animation**: who you are dressing, on what, how large, standing
  or going through one of their animations.
- **Set**: which of the character's ten colour sets, by how it is chosen at the character choice:
  LP, MP, HP, LK, MK, HK, two punches together (PP), two kicks together (KK), and the two used with
  auto guard.
- **Line**: the character's own colours, or one of three lines for their effects. Choosing a line
  shows the animation that uses it most, for the characters that keep all four lines on show
  (Bulleta, Victor, Morrigan, Anakaris and Q-Bee; it is found in the first few seconds the
  editor shows a character). The others' extra lines are seen only when an effect needs them.
- **Colour, Red, Green, Blue**: one of the line's 15 colours (it blinks on the character when you
  choose it), and its red, green and blue in the board's 16 levels each.
- **Random colours**: every colour of the set at random, at a second press on its row (LK, left
  or right, twice), so that one press by mistake loses nothing. HK puts back the colours from
  before the random ones, until the next change.
- **Save set** and **Load**: keep a set you like in the character's library and put it back later,
  into this set or any other (a design made on LP can go on KK). Left and right on Load try each
  saved one on the character; LP loads it, MK twice deletes it, and moving off the row without
  loading puts your colours back.
- LP copies a colour, MP pastes it, HP gives back the game's; the menu page puts back the game's
  colours for the whole set or the whole character.

Changes show at once and are saved as you go. **To play in them, choose the character with the
button of the set you changed** (Lilith with LP, say), anywhere: arcade, versus, training.

**Files.** What you change goes in a `palettes` folder beside `settings.ini`, a text file a
character (`Lilith.txt`) holding only the sets that are not the game's. The folder is:

| System | Folder |
| --- | --- |
| Windows | `%APPDATA%\Vampire Savior\palettes` (paste it into Explorer's address bar; it is the `AppData\Roaming` folder in your user folder) |
| macOS | `~/Library/Application Support/Vampire Savior/palettes` |
| Linux, SteamOS | `~/.local/share/Vampire Savior/palettes` |
| [Portable mode](../../docs/LAUNCHER.md#portable-mode) | `user/Vampire Savior/palettes` beside the launcher |

You can write or share them by hand: "Write the game's colours" (on the colour editor's page of
the menu) puts every character's own colours in `palettes/original` to
start from, and [docs/FEATURES.md](docs/FEATURES.md) has the format. Saved sets are files too, one a
set, in `palettes/library/<Character>/` (`Lilith 1.txt`, `Lilith 2.txt`...): to share a design, share
its file, and to use one someone gave you, put it in that folder.

**PalMod.** Colours made with [PalMod](https://github.com/Preppy/PalMod), the community's palette
editor, work too: edit a copy of your set's `vm3j.10b` with it (never the one in your ROM set) and
put the copy in `palettes`. The characters' colours it changed are used, their effects' too
(poisoned, burned and the like); anything else in that file is left as the game's, so it cannot
change how the game plays. Text files go over it.

"Load colour files again", on the same page, reads files you changed while the game runs.
"Custom colours" on the Display page switches all of it: on in Enhanced (it changes nothing until
there are files), off in Pure.

## What you need

- The ROM set `vsavj` (*Vampire Savior: The Lord of Vampire*, Japan 970519): `vsavj.zip` (with
  `vsav.zip` beside it if your collection keeps the shared files there) or a folder with the set's
  files, including `vsavj.key`. The launcher checks every file by checksum and tells you which is
  missing or different. Sets of other regions and revisions are refused: the port is made from this
  one version's program, and its training features use addresses found in it.
- One of: macOS 13 or later (Apple silicon or Intel); Linux x86_64 or SteamOS; Windows 10 or 11,
  64-bit.
- A gamepad or stick with six buttons is recommended.

## Playing

Vampire Savior comes with [CPS-2 Recomp](../../README.md#getting-started): download it for your system from the
[releases page](https://github.com/tmcleroy/cps2recomp/releases), open its launcher, and show it your ROM
set (drop `vsavj.zip` or its folder on the window, or choose the folder you keep your sets in: it finds each
game's by name). Then press **Play** beside Vampire Savior, or on its own page, which has its Display switches and
the features only it has. The [Steam Deck](../../README.md#steam-deck) steps are on the main page too.

From a terminal, `cps2recomp-launcher --play vampire_savior` starts it straight away with what the launcher has set,
and anything after `--` goes to the game: `cps2recomp-launcher --play vampire_savior -- --features widescreen`. The
game's own program (`vampire_savior`, beside the launcher) runs by itself too: `vampire_savior --help` lists what it takes.

While playing, Esc or `F1` (on a gamepad, the left stick's click, or Start and Select together) opens the
in-game menu. Only the menu's "Quit game" row (or closing the window) ends the game.

## Building from source

With your own ROM set in `rom/vampire_savior/` (ignored by Git), CMake, Ninja, SDL 3 and Python 3, from the
repository's top:

```sh
make GAME=vampire_savior            # twice the first time
make GAME=vampire_savior explore    # once, about ten minutes: finds the game's code by made-up play and translates it
make GAME=vampire_savior run
```

[docs/BUILDING.md](../../docs/BUILDING.md) has every platform, the checks and the release builds;
[docs/PORT.md](docs/PORT.md) says how this port works, and [PROGRESS.md](PROGRESS.md) what was found about
the game and what is left.

## Feedback and contributing

Bug reports and pull requests are welcome: see [CONTRIBUTING.md](../../CONTRIBUTING.md) for the rules (above
all: nothing from the game, ever) and [docs/ROADMAP.md](docs/ROADMAP.md) for what is next for this game.

## License and credits

GPL-3.0-only (see [LICENSE](../../LICENSE)). [THIRD_PARTY_NOTICES.md](../../THIRD_PARTY_NOTICES.md) lists what
comes from other projects: the CPS-2 program cipher is MAME's (BSD-3-Clause; worked out by Andreas Naive and
Nicola Salmoria), the QSound model follows MAME's high-level one (superctr, Valley Bell), the hardware facts
follow MAME's CPS-2 driver, and the launcher's look comes from arelkair's wiiparty-recomp. SDL 3, Dear ImGui and
the Inter font are used by the programs.

The repository holds no game data and nothing derived from it.
