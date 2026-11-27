# Street Fighter II: Hyper Fighting CSL Remix Edition Recomp

> Part of **[CPS-2 Recomp](../../README.md)**: one download and one launcher for every Capcom arcade game this
> project has ported. This page is about Street Fighter II: Hyper Fighting — Corner Store Legends (CSL) Remix Edition;
> the [main page](../../README.md) says how to download and play.

An unofficial native PC port of *Street Fighter II′: Hyper Fighting — CSL Remix Edition* (a community balance mod of
*Street Fighter II′: Hyper Fighting* World 921209, CPS-1 board) for macOS, Linux (Steam Deck included) and Windows,
made by static recompilation. The Remix is the first CSL mod: the [CSL Edition](../sf2hf_csl/README.md)'s extras came
from it. In the launcher it is under Hyper Fighting's row, with the other mods of that game.

It plays like Hyper Fighting with the characters made more even, and has the extras its guide names (colour choice,
2-player stage select, built-in free play and the rest) and the CSL Edition's recoloured stages (the two mods'
graphics ROMs are the same; its title says CSL REMIX EDITION). Its program and memory stay Hyper Fighting's, so this port builds the same
[Super Street Fighter II Turbo](../ssf2t/README.md) sources with `sf2hf`'s `family.h` mapping and its own translated
68000 code.

On top of that, the port adds a full **training mode** (hit boxes, frame data and a frame meter, input display, combo
and stun readouts, a dummy that records, replays and guards), the **alternative modes** (attract mode, the stage and
model viewers, survival, endless battle, ring out, a tag team battle, the [Ball battle](../../docs/COURT.md)), **widescreen**, **online play** with rollback,
**custom colours** with a colour editor, save states, run-ahead, arcade timing, and a custom soundtrack.

> **Beta.** The game plays from start to ending and its code is checked instruction by instruction, but nobody has
> played it by hand yet. Expect rough edges, and please [report what you find](#feedback-and-contributing).

**No game files are included.** You need your own copy of the `sf2hf_cslr` (CSL Remix Edition) ROM set. This project
cannot help you get one.

This project is not affiliated with, endorsed by, or sponsored by Capcom. *Street Fighter* is a trademark of Capcom
Co., Ltd.

![The title screen](../../docs/screenshots/sf2hf_cslr/title.png)

![Ryu against Ken in Ryu's stage](../../docs/screenshots/sf2hf_cslr/fight.png)

## What the Remix changes

The mod's own guide, in short (original Hyper Fighting → the Remix):

- **Dizzy:** 30 points the first time in a round, then 34, then 35 (31 every time before). Light attacks add 2 (4),
  heavy ones 13 (14), many jumping heavy attacks and some heavy punches 6 (14), knockdowns 12 (16); the meter empties
  sooner after a knockdown (100 frames, 120). Many special moves take Super Turbo's dizzy values. The training mode's
  stun readout follows the Remix's limits.
- **Throws** do 28 (32), but E. Honda's and Zangief's keep 32. **Damage's** random spread is Super Turbo's (the same on
  average).
- **E. Honda:** faster standing jab (4 frames, 6), a jab chain into the Hundred Hand Slap, Butt Slam from Up-Back or
  Up-Forward too. **Blanka:** Vertical Ball from Up-Back or Up-Forward too; a landing bug fixed; Champion Edition's
  hurt boxes on his jumping medium and heavy kicks. **Guile:** shorter Sonic Boom recovery (23, 26) and Flash Kick
  charge (60, 65).
- **Chun-Li** and **Dhalsim:** most normal attacks do more damage; Chun-Li's straight-up jumping heavy kick knocks
  down and her Spinning Bird Kick hits wider; Dhalsim's drills do 16 (12).
- **Zangief:** longer and lower far medium kick, a higher-hitting far heavy kick. **M. Bison:** the second Scissor Kick
  knocks down, Psycho Crusher a frame faster, longer normals, blocked Head Stomps take a little health. **Sagat:**
  shorter Low Tiger Shot recovery. **Balrog:** light Dash Punch knocks down, close heavy punch does 26 (4), longer throw
  and headbutt grab. **Vega:** much harder normals, a claw that never falls off, lower standing light and medium
  attacks, faster wall-dive landing.
- Ryu and Ken: their heavy normals add fewer dizzy points; nothing else.

## What the port adds

The game's 68000 program is translated ahead of time into C++ and compiled into the port, and so is its sound CPU's.
The board is the toolkit's classic CPS-1 (no QSound): the Z80 drives a YM2151 ([ymfm](https://github.com/aaronsgiles/ymfm))
and an OKI MSM6295 (`sound/okim6295.cpp`). Everything the port adds is Super Turbo's sources, mapped through `family.h`
([how it was made](PROGRESS.md)).

Training mode, the alternative modes, widescreen and its invisible walls, the colour editor, the zoom, the in-game menu
and the rest work as on the [Hyper Fighting page](../sf2hf/README.md):

- No super combo gauge, no hidden characters (all twelve are on the select screen), no game speed.
- **The Board page** exposes the board's DIP switches: difficulty, continue, demo sound (free play is built into the
  Remix itself).
- **Custom colours**: the port's colour editor and `palettes/` overrides apply to `COLOR 01` (Normal) and `COLOR 02`
  (Other); PalMod's file is `s2te_22.7f`.
- **Other games' stages**: the training mode, the viewers and the battles can be fought on the other fighting games'
  stages, Super Turbo's redrawn ones among them (their sets needed beside this one's).
- **A custom soundtrack**: on the classic board too. [The list of its songs](../../docs/SONGS.md#street-fighter-ii-hyper-fighting)
  names the music (the sound ROM and samples are Hyper Fighting's). [How →](../../docs/SOUNDTRACK.md)

## What you need

- The CSL Remix Edition ROM set (`sf2hf_cslr.zip` or a folder with its files). The launcher checks every file
  by checksum.
- One of: macOS 13 or later; Linux x86_64 or SteamOS; Windows 10 or 11, 64-bit.
- A gamepad or stick with six buttons is recommended.

## Building from source

With your own ROM set in `rom/sf2hf_cslr/` (ignored by Git), CMake, Ninja, SDL 3 and Python 3, from the repository's top:

```sh
make GAME=sf2hf_cslr            # twice the first time
make GAME=sf2hf_cslr explore    # once: finds the game's code by made-up play and translates it
make GAME=sf2hf_cslr run
make GAME=sf2hf_cslr check FRAMES=44000 MOVIE=games/sf2hf_cslr/movies/ladder.txt ARGS="--features free_play,no_damage,one_hit"
```

[docs/BUILDING.md](../../docs/BUILDING.md) has every platform, the checks and the release builds, and
[PROGRESS.md](PROGRESS.md) what was found about the game and what is next.

## Feedback and contributing

Bug reports and pull requests are welcome: see [CONTRIBUTING.md](../../CONTRIBUTING.md) for the rules (above all:
nothing from the game, ever).

## License and credits

GPL-3.0-only (see [LICENSE](../../LICENSE)); [THIRD_PARTY_NOTICES.md](../../THIRD_PARTY_NOTICES.md) lists what comes
from other projects. The repository holds no game data and nothing derived from it.
