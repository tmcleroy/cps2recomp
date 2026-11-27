# Street Fighter II: Hyper Fighting Recomp

> Part of **[CPS-2 Recomp](../../README.md)**: one download and one launcher for every Capcom arcade game this
> project has ported. This page is about Street Fighter II: Hyper Fighting; the [main page](../../README.md) says how
> to download and play.

An unofficial native PC port of the arcade game *Street Fighter II′: Hyper Fighting* (Capcom, 1992, CPS-1 board; *Street Fighter II′ Turbo* in Japan)
for macOS, Linux (Steam Deck included) and Windows, made by static recompilation.

Hyper Fighting is Champion Edition made faster, with new special moves and new colours. Capcom made it from Champion
Edition's program, and Super Street Fighter II Turbo's grew out of the same line, so this port builds the same sources as the
[Super Street Fighter II Turbo port](../ssf2t/README.md), with this game's own places and board, so it has what that
one has but what Super Turbo added to the game (super combos, the hidden characters, the speeds).

It has a full **training mode** (hit boxes and throw range, frame data and a frame meter, input display, combo and
stun readouts, fixed damage, a dummy that records, replays and guards high or low as each attack needs, mashes out
of stun or is played by the game's own computer player), the **alternative modes** (attract mode, the stage and
model viewers, survival, endless battle, ring out, a tag team battle), **widescreen**, **custom colours** with a
colour editor, and the features every game shares: free play, straight to the title, the board's DIP switches,
cheats, save states, run-ahead, the board's speed, a custom soundtrack.

> **Beta.** The game plays from start to ending and its code is checked instruction by instruction, but nobody has
> played it by hand yet. Expect rough edges, and please [report what you find](#feedback-and-contributing).

**No game files are included.** You need your own copy of the game: the arcade ROM set `sf2hf` (World, 921209), from
a board you own. This project cannot help you get one.

This project is not affiliated with, endorsed by, or sponsored by Capcom. *Street Fighter* is a trademark of Capcom
Co., Ltd.

![The title screen](../../docs/screenshots/sf2hf/title.png)

![Ryu against Ken (in Hyper Fighting's colours) in E. Honda's bathhouse](../../docs/screenshots/sf2hf/fight.png)

## What it is

The game's 68000 program is translated ahead of time into C++ and compiled into the port, and so is its sound CPU's.
The board is the toolkit's classic CPS-1 (no QSound): the Z80 drives a YM2151 ([ymfm](https://github.com/aaronsgiles/ymfm))
and an OKI MSM6295 (`sound/okim6295.cpp`). Everything the port adds is Super Turbo's sources, which name the places they
use in Super Turbo's program; this game's `family.h` says where each is here ([how it was made](PROGRESS.md)).

Training mode, the alternative modes, widescreen and its invisible walls, the colour editor, the zoom, the in-game menu
and the rest work as on the [Super Turbo page](../ssf2t/README.md), with these differences:

- No super combo gauge, no hidden characters (all twelve are on the select screen), no game speed.
- **Free play** is the board's own DIP switch, set by the feature.
- **The Board page** is the board's DIP switches: difficulty, continue, demo sound, two coins to start.
- **Custom colours**: each character has two sets, its own (Normal) and the one it has when both players choose it
  (Other), of one palette. PalMod's file for it is `s2te_22.7f`.
- **Other games' stages**: the training mode, the viewers and the battles can be fought on the other fighting games'
  stages, Super Turbo's redrawn ones among them (their sets needed beside this one's).
- **A custom soundtrack**: on the classic board too. [The list of its songs](../../docs/SONGS.md#street-fighter-ii-hyper-fighting)
  names the music. A replaced tune stops the board's sound effects for a moment as it starts (the classic sound program
  has no command that stops only its music). [How →](../../docs/SOUNDTRACK.md)

## What you need

- The ROM set `sf2hf` (*Street Fighter II′: Hyper Fighting*, World 921209): `sf2hf.zip` or a folder with its files.
  A merged zip with its clones in it works too: the launcher finds each file by checksum. Other regions and revisions
  are refused.
- One of: macOS 13 or later; Linux x86_64 or SteamOS; Windows 10 or 11, 64-bit.
- A gamepad or stick with six buttons is recommended.

## Building from source

With your own ROM sets in `rom/sf2hf/` and `rom/ssf2t/` (ignored by Git; Super Turbo's is needed only to make
`family.h` again), CMake, Ninja, SDL 3 and Python 3, from the repository's top:

```sh
make GAME=sf2hf            # twice the first time
make GAME=sf2hf explore    # once: finds the game's code by made-up play and translates it
make GAME=sf2hf run
make GAME=sf2hf check FRAMES=44000 MOVIE=games/sf2hf/movies/ladder.txt ARGS="--features free_play,no_damage,one_hit"
```

[docs/BUILDING.md](../../docs/BUILDING.md) has every platform, the checks and the release builds, and
[PROGRESS.md](PROGRESS.md) what was found about the game and what is next.

## Feedback and contributing

Bug reports and pull requests are welcome: see [CONTRIBUTING.md](../../CONTRIBUTING.md) for the rules (above all:
nothing from the game, ever).

## License and credits

GPL-3.0-only (see [LICENSE](../../LICENSE)); [THIRD_PARTY_NOTICES.md](../../THIRD_PARTY_NOTICES.md) lists what comes
from other projects. The repository holds no game data and nothing derived from it.
