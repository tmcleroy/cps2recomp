# Street Fighter II: Hyper Fighting CSL Edition Recomp

> Part of **[CPS-2 Recomp](../../README.md)**: one download and one launcher for every Capcom arcade game this
> project has ported. This page is about Street Fighter II: Hyper Fighting — Corner Store Legends (CSL) Edition; the
> [main page](../../README.md) says how to download and play.

An unofficial native PC port of *Street Fighter II′: Hyper Fighting — Corner Store Legends (CSL) Edition* (a community
mod based on *Street Fighter II′: Hyper Fighting* World 921209, CPS-1 board) for macOS, Linux (Steam Deck included)
and Windows, made by static recompilation.

The fighting itself plays the same as *Hyper Fighting* (World 921209), with quality-of-life additions built into the
ROM: about twenty colours per character, 2-player stage select, on-screen stun and reversal callouts, no bonus stages,
and cleaned-up, recoloured stages. Because its program and RAM layout stay aligned with Hyper Fighting, this port
builds the same [Super Street Fighter II Turbo](../ssf2t/README.md) sources with `sf2hf`'s `family.h` mapping and its
own translated 68000 code.

On top of CSL Edition's own features, the port adds a full **training mode** (hit boxes and throw range, frame data
and a frame meter, input display, combo and stun readouts, fixed damage, a dummy that records, replays and guards high
or low as each attack needs, mashes out of stun or is played by the game's own computer player), the **alternative
modes** (attract mode, the stage and model viewers, survival, endless battle, ring out, a tag team battle),
**widescreen**, **online play** with rollback, **custom colours** with a colour editor, save states, run-ahead, arcade
timing, and a custom soundtrack.

> **Beta.** The game plays from start to ending and its code is checked instruction by instruction, but nobody has
> played it by hand yet. Expect rough edges, and please [report what you find](#feedback-and-contributing).

**No game files are included.** You need your own copy of the `sf2hf_csl` (Corner Store Legends Edition) ROM set. This
project cannot help you get one.

This project is not affiliated with, endorsed by, or sponsored by Capcom. *Street Fighter* is a trademark of Capcom
Co., Ltd.

![The title screen](../../docs/screenshots/sf2hf_csl/title.png)

![Ryu against Chun-Li in Chun-Li's market (CSL colours)](../../docs/screenshots/sf2hf_csl/fight.png)

## What CSL Edition adds

- **Character colours (~20 per fighter, plus Random):** after picking a fighter with any punch or kick button, a
  `COLOR 01` label appears under the grid. Press Left or Right to preview colours on the portrait (`COLOR 01` is the
  original, `COLOR 02` the original alternate, `RANDOM` wraps to the left of `COLOR 01`), and press any punch or kick
  again to lock it in (about 12 seconds in 2-player; 1.5 seconds on the ROM in 1-player, extended to ~12 seconds by
  default in Enhanced via **Game → 1P colour select time**).
- **Stage select (2-player matches):** before picking your fighter, move the cursor over the character whose home stage
  you want and press Start (confirmed by a sound), then pick your fighter as usual. Player 1 chooses on a fresh
  2-player game; when a challenger joins or a player continues after a loss, the player choosing sets the stage.
- **Longer character select timer:** 40 seconds instead of 20.
- **Free play built in:** no coins needed; press Start to begin, join mid-fight, or continue.
- **No bonus stages:** the car, barrel and drum stages are skipped in 1-player arcade.
- **STUN and REVERSAL callouts:** `STUN` appears under a fighter's name when they are close to dizzy; `REVERSAL`
  appears when a special move or throw is performed on the first possible frame after hitstun, blockstun, knockdown or
  a throw.
- **M. Bison's cape entrance:** human players now get M. Bison's round-one cape entrance too.
- **Vega's stage — no cage climbing:** Vega's wall-jump moves bounce off the screen edge as on every other stage
  instead of climbing the background fence.
- **Cleaner, recoloured stages:** Zangief's foreground fence and exit signs are removed; Dhalsim's elephants are
  silenced; Sagat's stage uses night-time colours; M. Bison's fence colours by the towers are matched; and every stage
  except Balrog's has refreshed colours.
- **New title screen:** *CORNER STORE LEGENDS* under the Street Fighter II′ logo on a black background.

## What the port adds

The game's 68000 program is translated ahead of time into C++ and compiled into the port, and so is its sound CPU's.
The board is the toolkit's classic CPS-1 (no QSound): the Z80 drives a YM2151 ([ymfm](https://github.com/aaronsgiles/ymfm))
and an OKI MSM6295 (`sound/okim6295.cpp`). Everything the port adds is Super Turbo's sources, mapped through `family.h`
([how it was made](PROGRESS.md)).

Training mode, the alternative modes, widescreen and its invisible walls, the colour editor, the zoom, the in-game menu
and the rest work as on the [Hyper Fighting page](../sf2hf/README.md):

- No super combo gauge, no hidden characters (all twelve are on the select screen), no game speed.
- **The Board page** exposes the board's DIP switches: difficulty, continue, demo sound (free play is built into CSL
  Edition itself).
- **Custom colours**: the port's colour editor and `palettes/` overrides apply to `COLOR 01` (Normal) and `COLOR 02`
  (Other); PalMod's file is `s2te_22.7f`.
- **Other games' stages**: the training mode, the viewers and the battles can be fought on the other fighting games'
  stages, Super Turbo's redrawn ones among them (their sets needed beside this one's).
- **A custom soundtrack**: on the classic board too. [The list of its songs](../../docs/SONGS.md#street-fighter-ii-hyper-fighting)
  names the music (the sound ROM and samples are Hyper Fighting's). [How →](../../docs/SOUNDTRACK.md)

## What you need

- The Corner Store Legends Edition ROM set (`sf2hf_csl.zip` or a folder with its files). The launcher checks every file
  by checksum.
- One of: macOS 13 or later; Linux x86_64 or SteamOS; Windows 10 or 11, 64-bit.
- A gamepad or stick with six buttons is recommended.

## Building from source

With your own ROM set in `rom/sf2hf_csl/` (ignored by Git), CMake, Ninja, SDL 3 and Python 3, from the repository's top:

```sh
make GAME=sf2hf_csl            # twice the first time
make GAME=sf2hf_csl explore    # once: finds the game's code by made-up play and translates it
make GAME=sf2hf_csl run
make GAME=sf2hf_csl check FRAMES=44000 MOVIE=games/sf2hf_csl/movies/ladder.txt ARGS="--features free_play,no_damage,one_hit"
```

[docs/BUILDING.md](../../docs/BUILDING.md) has every platform, the checks and the release builds, and
[PROGRESS.md](PROGRESS.md) what was found about the game and what is next.

## Feedback and contributing

Bug reports and pull requests are welcome: see [CONTRIBUTING.md](../../CONTRIBUTING.md) for the rules (above all:
nothing from the game, ever).

## License and credits

GPL-3.0-only (see [LICENSE](../../LICENSE)); [THIRD_PARTY_NOTICES.md](../../THIRD_PARTY_NOTICES.md) lists what comes
from other projects. The repository holds no game data and nothing derived from it.
