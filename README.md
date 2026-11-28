# CPS-2 Recomp

**[Join me on Discord](https://discord.gg/FSC9GZ6nm)**

[![CI](https://github.com/tmcleroy/cps2recomp/actions/workflows/ci.yml/badge.svg)](https://github.com/tmcleroy/cps2recomp/actions/workflows/ci.yml)
[![Release](https://img.shields.io/github/v/release/tmcleroy/cps2recomp?include_prereleases)](https://github.com/tmcleroy/cps2recomp/releases)
[![License: GPL-3.0](https://img.shields.io/badge/license-GPL--3.0-blue)](LICENSE)

**Unofficial native PC ports of Capcom's CPS-2 arcade games (and a CPS-1 one), for macOS, Linux (Steam Deck and
ARM64 included), Windows and Android, in one download with one launcher.** Each game's program is translated ahead of time
into C++ and runs on a machine written for this project, not an emulator. The fighting games add a full training
mode, online play with rollback, widescreen and new modes.

**[Get started →](#getting-started)**

**No game files are included.** You need your own copy of each game: its arcade ROM set (a CPS-2 game's with its key
file). Only the versions named below work for each game, and this project cannot help you get one.

![The model viewer: Psylocke at 1.5x, going through her animations](docs/screenshots/msh/model-viewer-psylocke.gif)

![The colour editor: Rogue's green suit turned purple and her yellows pink, colour by colour](docs/screenshots/xmvsf/colour-editor-rogue.gif)

![Training mode in Street Fighter Alpha 2, in widescreen: hit boxes, input displays, the damage readout and the frame meter](docs/screenshots/sfa2/training.gif)

![Custom colours: Morrigan in nine colourings made in the colour editor](docs/screenshots/vampire_savior/morrigan-colours-1.png)

## The games

| Game | ROM set | Notes |
| --- | --- | --- |
| [**Super Street Fighter II**](games/ssf2/README.md) (1993) | `ssf2` (World 931005) | The New Challengers: the first on the CPS-2, with bonus stages |
| [**Super Street Fighter II Turbo**](games/ssf2t/README.md) (1994) | `ssf2xjr1` (Japan 940223) | The tournament revision; Akuma and the old characters without codes |
| [**Hyper Street Fighter II**](games/hsf2/README.md) (2004) | `hsf2` (USA 040202) | Every character in each game of the series' version, against each other; Akuma without a code |
| [**Street Fighter Alpha**](games/sfa/README.md) (1995) | `sfa` (Europe 950727) | |
| [**Street Fighter Alpha 2**](games/sfa2/README.md) (1996) | `sfa2u` (USA 960430) | Team battle and Marvel-style tag (2 vs 2 for four players) |
| [**Street Fighter Zero 2 Alpha**](games/sfz2al/README.md) (1996) | `sfz2al` (Asia 960826) | Alpha 2's later revision, with the EX characters |
| [**Street Fighter Alpha 3**](games/sfa3/README.md) (1998) | `sfa3u` (USA 980904) | Team battle and tag; Balrog, Juni and Juli without codes |
| [**Super Gem Fighter Mini Mix**](games/pocket_fighter/README.md) (1997) | `sgemf` (USA 970904) | |
| [**X-Men: Children of the Atom**](games/xmcota/README.md) (1994) | `xmcota` (Euro 950331) | Juggernaut, Magneto and Akuma without codes |
| [**Marvel Super Heroes**](games/msh/README.md) (1995) | `msh` (Europe 951024) | Dr. Doom and Thanos without codes |
| [**X-Men vs. Street Fighter**](games/xmvsf/README.md) (1996) | `xmvsf` (Europe 961004) | Tag-team co-op, and 2 vs 2 for four players |
| [**Marvel Super Heroes vs. Street Fighter**](games/mshvsf/README.md) (1997) | `mshvsf` (Europe 970625) | Tag-team co-op, and 2 vs 2 for four players |
| [**Marvel vs. Capcom**](games/mvsc/README.md) (1998) | `mvsc` (Europe 980123) | Tag-team co-op, and 2 vs 2 for four players |
| [**Super Puzzle Fighter II Turbo**](games/puzzle_fighter/README.md) (1996) | `spf2t` (Europe 960529) | A puzzle training mode and CPU battle |
| [**Darkstalkers: The Night Warriors**](games/dstlk/README.md) (1994) | `dstlk` (Europe 940705) | Huitzil and Pyron at a switch |
| [**Night Warriors: Darkstalkers' Revenge**](games/nwarr/README.md) (1995) | `nwarr` (Europe 950316) | Huitzil and Pyron at a switch |
| [**Cyberbots: Fullmetal Madness**](games/cybots/README.md) (1995) | `cybots` (Europe 950424) | Giant robots on four buttons; Super-8, Gaits, Warlock and Killer Bee at a switch |
| [**Vampire Savior**](games/vampire_savior/README.md) (1997) | `vsavj` (Japan 970519) | Tag team battle |
| [**Vampire Savior 2**](games/vsav2/README.md) (1997) | `vsav2` (Japan 970913) | Donovan, Phobos and Pyron back |
| [**Vampire Hunter 2**](games/vhunt2/README.md) (1997) | `vhunt2` (Japan 970929) | Night Warriors' cast on Vampire Savior's engine |
| [**Vampire Savior Definitive**](games/vsav_definitive/README.md) (beta) | `vsavj` and `vsav2` (and `dstlk`, `nwarr`, `sfa`, `sfa2u`, `sfa3u`, `ssf2xjr1`, `msh`, `xmvsf`, `mshvsf`, `mvsc`, `xmcota`, `sgemf`, `cybots` for their stages) | Vampire Savior with all eighteen characters, and other games' stages |
| [**Alien vs. Predator**](games/avsp/README.md) (1994) | `avspu` (USA 940520) | Beat-'em-up: three players, a stage to start at, score attack |
| [**Street Fighter II: The World Warrior**](games/sf2/README.md) (1991, CPS-1) | `sf2` (World 910522) | The first, eight characters to choose; on the classic CPS-1 |
| [**Street Fighter II: Champion Edition**](games/sf2ce/README.md) (1992, CPS-1) | `sf2ce` (World 920513) | The bosses playable, mirror matches; on the classic CPS-1 |
| [**Street Fighter II: Hyper Fighting**](games/sf2hf/README.md) (1992, CPS-1) | `sf2hf` (World 921209) | Faster, new moves and colours; on the classic CPS-1 |
| [**Street Fighter II: Hyper Fighting CSL Edition**](games/sf2hf_csl/README.md) (CPS-1 mod) | `sf2hf_csl` (Corner Store Legends Edition) | Hyper Fighting mod: ~20 colours per fighter, 2P stage select, stun/reversal callouts, cleaned-up stages |
| [**Street Fighter II: Hyper Fighting CSL Remix Edition**](games/sf2hf_cslr/README.md) (CPS-1 mod) | `sf2hf_cslr` (CSL Remix Edition) | Hyper Fighting balance mod, the first CSL mod: more even characters, Super Turbo dizzy, the CSL extras |
| [**Ghouls'n Ghosts**](games/ghouls/README.md) (1988, CPS-1) | `ghouls` (World) | Platform game; classic CPS-1 with YM2151 and OKI sound |
| [**Cadillacs and Dinosaurs**](games/dino/README.md) (1993, CPS-1) | `dino` (World 930201) | Beat-'em-up: three players, a stage to start at, score attack |
| [**The Punisher**](games/punisher/README.md) (1993, CPS-1) | `punisheru` (USA 930422) | Beat-'em-up: two players, a stage to start at, score attack |
| [**Final Fight**](games/ffight/README.md) (1989, CPS-1) | `ffight` (World, set 1) | Beat-'em-up: two players, a stage to start at, score attack, stage viewer |
| [**Giga Wing**](games/gigawing/README.md) (1999) | `gigawing` (USA 990222) | Shoot-'em-up: training mode, true hit boxes, score attack, caravan, boss rush |
| [**Progear**](games/progear/README.md) (2001) | `progearu` (USA 010117) | Cave's horizontal shoot-'em-up: training mode, true hit boxes, score attack, caravan, boss rush |
| [**Dimahoo**](games/dimahoo/README.md) (2000) | `dimahoou` (USA 000121) | Raizing's vertical shoot-'em-up: training mode, true hit boxes, score attack, caravan, boss rush |

Each game's page has all it does, with pictures.

| | | |
| --- | --- | --- |
| [![Street Fighter Alpha 2: training mode](docs/screenshots/sfa2/training.png)](games/sfa2/README.md) | [![Vampire Savior: training mode](docs/screenshots/vampire_savior/training.png)](games/vampire_savior/README.md) | [![Cadillacs and Dinosaurs: three players](docs/screenshots/dino/three-players.png)](games/dino/README.md) |

## Training mode

Every fighting game has a full training mode, from the in-game menu (not yet the two classic Street Fighter II games):

- **Hit boxes** (and throw boxes in Alpha 2 and 3), **frame data** and a **frame meter**, an **input display**, and a
  combo and damage readout (and stun, in the Street Fighter games and Pocket Fighter).
- **A dummy** that stands, crouches or jumps, guards (high, low or after the first hit), techs, gets up quickly,
  reverses, and **records and replays** what you show it (each game's page says what its dummy can do).
- Health and meter refilled, a stage of your choice, and **start over** from the round's beginning with one button.

**[More about training mode →](docs/TRAINING.md)**

## Online play

Two players on two machines, or four on up to four in the tag-team games, with **rollback** netcode. Any mix of macOS,
Linux, the Steam Deck and Windows can play each other.

- **Host** a match from the launcher, or from the game you're playing (its menu's **Online** tab: you play on until
  someone joins), and give the other player its **code**: they type it on the launcher's **Online** page. If you've
  set your name, the match is also listed in the **lobby**, where anyone can pick it from **Online matches**. Nothing
  needs setting up on either router.
- The **waiting room** lists the players whose Online page is open: **Invite** one to a match in any game, and they
  **Accept** from their launcher.
- Both players need the same version of CPS-2 Recomp (the launcher shows yours at the top); a match on another
  version is greyed in the list. Your ROM sets may be different regions of the game: the match is the host's region
  (best effort: the same set is surest).
- Each player presses Start to come in, as at the arcade. The match uses the host's board settings (difficulty,
  timer, rounds); cheats are off.
- **Four players**: in the tag-team games, with Tag team at 2 vs 2 on the host, a match is four people, each on their
  own machine or two at a machine (**People at this machine** on the Online page), all joining with the one code.
- **Ball battle and tag team online**: in two-person matches, the host starts either from the in-game menu (Esc), which
  pauses the match for both players while each picks their own fighters; the host starts once the other says Ready.
- **Training together online**: in every fighting game the host can start a **training session** for both players
  (Esc, **Start Training**): no clock, no knock-outs, the host's refills, and each player's own hit boxes, frame data and
  input display. Either player can start the round over for both.
- **Your own input delay and run-ahead**: each player chooses their own, as on Fightcade (the launcher's Online page,
  or the game's menu); the match is the same for everyone. The **input lag readout** (Display tab) shows what a
  press takes to show, online too.
- **On screen:** the players' **names** and the **score** at the top, the **win streak** of a player who keeps winning,
  and, if you like, **network stats** at the top right as on Fightcade: ping, jitter, each player's delay and
  run-ahead, and rollbacks.
- **FT3 / FT5 / FT10:** in every fighting game, the host chooses **Set format** under Host on the **Online** page.
  Everyone sees the score, and the first side to reach the target wins the set; after the machines agree on the
  result it is shown for two seconds and they return to the launcher. Casual keeps the score with no end.
- **Watch** a match: a host who allows spectators has **Watch** on its lobby entry once the match is under way. Up to
  eight viewers follow it, a moment behind, without touching it.
- **Chat** with the other players during a match: press T (or a gamepad's right stick, or the touch screen's Chat) and
  type, on a keyboard or your system's on-screen keyboard. It goes between the playing machines.
- A dropped connection waits for the other player to come back with the same code.

**[More about online play →](docs/ONLINE.md)**: the lobby, the waiting room, joining by address, and how rollback works.

## Everything else

- **Widescreen** in the fighting games: 16:9, with more of the stage on each side ([more](docs/PICTURE.md#widescreen)).
- **Other games' stages**: Vampire Savior Definitive, the other Vampire Savior games and the Alpha games can fight on
  every other fighting game's stages in the collection, as those games show them, their music too. Each needs that game's ROM set.
  <img width="6144" height="4064" alt="vsav-all-stages" src="https://github.com/user-attachments/assets/71327539-0b51-499f-af01-8c0c55448471" />

- **Stage select**: the in-game menu's **Stage** tab puts the fights on the stage you choose, the game's own or
  another game's, whatever is played: arcade, versus, training, survival, the team and endless battles, and online
  (the host's choice) ([more](docs/MODES.md#the-stage)).
- **Alternative modes** in the fighting games: survival, ring out, an endless battle the game plays itself, and in
  most a tag team battle; plus a stage viewer and a model viewer that saves a character's animations as PNG frames, sheets and looping GIFs ([more](docs/MODES.md#model-viewer)).
- **Scene and stage export**: any moment of any game saved as its layers, each a see-through PNG (the scroll screens,
  the objects, and each sprite by itself), for a frame or a few seconds of them; and in the fighting games' stage viewer, a whole stage from end to
  end, each layer in full with its parallax pace ([more](docs/FEATURES.md#scene-export)).
- **Video recording**: what you play and hear saved as an MP4 that plays anywhere, the pixels sharp at 3 times the
  board's size or more, the custom soundtrack too (Alt+F12, or the menu's **Capture** tab); and **instant replay**, the
  last 30 seconds saved after they happen (Alt+Shift+F12) ([more](docs/FEATURES.md#video-recording)).
- **Ball battle**, a ball game in the Vampire Savior games, the Street Fighter Alphas, the Street Fighter IIs and Marvel vs. Capcom: each fighter on a half of the stage, hitting a big ball past the other with
  their attacks, first to 3, 5 or 10 ([more](docs/COURT.md)).
- **Shoot-'em-up training mode** in Giga Wing, Progear and Dimahoo: any stage or straight at its boss, back to the start with one
  button, or to a practice point you set anywhere, and rewind through the last half a minute ([more](docs/PRACTICE.md)); the ships' true hit boxes, the enemies' and their shots' in any mode; a score attack,
  a caravan and a boss rush; and the board's slowdown, or none ([more](docs/MODES.md#the-shoot-em-ups)).
- **The picture your way**: sharp or smooth scaling, 4:3 or square pixels (a vertical game stood upright), and RetroArch shaders (CRT, scanlines,
  xBRZ and more, or your own preset). A camera zoom follows either fighter, keeps both in view, or closes in
  and out with the fighters' distance while the health bars stay put ([more](docs/PICTURE.md)).
- **Arcade monitors**: a CRT (CRT Emudriver or a custom resolution) gets the board's own 384 x 224 in full screen, or a
  super resolution such as 2560 x 224, on the screen you choose, synced to the screen with a frame delay, and on Windows
  made at the board's exact 59.637 Hz with Switchres, as GroovyMAME does: set up on the launcher's Settings page.
- **Controls**: keyboard, gamepad or arcade stick, every button changeable per player and per game.
- **An in-game menu**: save states, reset and quit always in its top row, every option under the same tabs in every
  game (Game, Display, Sound, Online, Training, Modes, Stage, Cheats, Capture), rewind and a practice point, speed and
  pause, frame advance; for a keyboard, a gamepad (LB/RB change tab, X puts an option back) or the mouse. On an arcade
  monitor it is drawn at the board's own resolution, sharp on a 15 kHz CRT.
- **English, Brazilian Portuguese or Spanish** in the launcher and every game's menu: chosen once in the launcher (or
  the system's language). The games' own words are left as they are.
- **The board's speed**: the processor runs as on the real board, so every game plays at the board's speed and
  slows down where and as much as the board does, checked frame by frame against a cycle-exact model of it
  ([docs/TIMING.md](docs/TIMING.md)), measured frame by frame against the boards' FPGA cores
  ([the proof](docs/ACCURACY.md)). **Slowdown → None** gives it every clock instead.
- **Run-ahead**, the board's operator settings, free play, rapid fire, and straight to the title at start-up.
- **A custom soundtrack**: your own music files in place of the board's music ([how](docs/SOUNDTRACK.md)).
- **Multi game**: play several games one after another, such as an endless battle through every stage of each
  ([more](docs/MODES.md#multi-game)).
- **A launcher in the in-game menu's look**: your games as a grid of their last screens, Play and the few things set
  before a game starts; LB/RB through its tabs. **Updates** in the launcher, and a **portable mode** that keeps
  everything on a drive you carry ([the launcher](docs/LAUNCHER.md)).


## Getting started

1. Download the build for your system from the [releases page](https://github.com/tmcleroy/cps2recomp/releases) and
   unpack it.
2. Open the launcher (`CPS-2 Recomp.app`, `cps2recomp-launcher`). On its Settings tab, **Choose folder…** and pick the
   folder you keep your ROM sets in: it finds each game's set by name (`sfa2u.zip` or a folder of the set's files).
   Your ROM sets are only ever read.
3. Press **Play** beside a game.

While playing, Esc or `F1` opens the in-game menu (on a gamepad: the left stick's click, or Start and Select
together).

- **macOS:** the app isn't notarized: the first time, right-click it and choose Open. Move it to Applications first,
  so that it can update itself.
- **Windows:** run `cps2recomp-launcher.exe`.
- **Linux:** run `cps2recomp-launcher`.

### Android

1. Download `CPS2Recomp-android.apk` from the releases page on your phone or tablet and open it to install it
   (Android 9 or later, 64-bit ARM; allow installs from your browser or file manager when asked).
2. Open CPS-2 Recomp and press **Add ROM sets…** on its Games page: choose your sets' zips, as many at once as you
   like. They are copied into the app's folder.
3. Press **Play** beside a game.

The games show a stick and buttons on the screen; a gamepad connected over Bluetooth or USB works too, and takes their
place. Back, or the screen's **Menu**, opens the in-game menu.

### Steam Deck

1. In Desktop Mode, download `CPS2Recomp-linux-x86_64.tar.gz` from the releases page, extract it, and move the
   `CPS2Recomp` folder somewhere it can stay (a new `Applications` folder in your home folder, say).
2. Copy your ROM sets into one folder on the Deck.
3. Open `cps2recomp-launcher`, **Choose folder…** on the Games page, and pick that folder.
4. On each game's page, press **Add to Steam**. The games then appear in Gaming Mode under **Non-Steam**.

## What you need

- The ROM set of each game you want, as named in the table above: a zip (with its parent set's zip beside it if your
  collection keeps the shared files there) or a folder of its files. The launcher checks every file and says which
  is missing or different. **Other regions** of the same release work too, played as the board plays in that region:
  the Marvel games', Super Gem Fighter's (*Pocket Fighter* in Japan), Puzzle Fighter's, Alpha's, Alpha 3's, Super
  Turbo's, Alien vs. Predator's, Giga Wing's, Progear's and Dimahoo's; each game's page names them. Other revisions don't work: each port is made from
  one version's program. Vampire Savior Definitive needs both `vsavj` and `vsav2`, and shows the first
  Darkstalkers', Night Warriors', Alpha's, Alpha 2's, Alpha 3's, Super Turbo's, Super Gem Fighter's, Cyberbots' and five Marvel
  games' stages with `dstlk`, `nwarr`, `sfa`, `sfa2u`, `sfa3u`, `ssf2xjr1`, `msh`, `xmvsf`, `mshvsf`, `mvsc`, `xmcota`,
  `sgemf` and `cybots` too.
- macOS 13 or later (Apple silicon or Intel), Linux x86_64 or ARM64 (SteamOS included), Windows 10 or 11, 64-bit, or
  Android 9 or later on 64-bit ARM, with Vulkan.
- A gamepad or stick is recommended (six buttons for the fighting games, four for Cyberbots).

## How it works

Each game's 68000 program is translated ahead of time into C++ and compiled into that game's program, and the parts
its features touch are rewritten by hand as readable C++, each checked call by call against the game's own code. The
board (video, the QSound chip, the sound CPU, the settings memory) is code written for this project, not an emulator.
The [toolkit](docs/TOOLKIT.md) is shared by every game, so a fix or a feature there reaches all of them.

## Building from source

With your own ROM sets in `rom/<id>/` (ignored by Git), CMake, Ninja, SDL 3 and Python 3:

```sh
make GAME=sfa2              # twice the first time
make GAME=sfa2 explore      # optional, about ten minutes: finds more of the game's code (games/<id>/entries.txt)
make collection             # every game whose ROM set is here, and the launcher
```

[docs/BUILDING.md](docs/BUILDING.md) has every platform and the checks; [docs/TOOLKIT.md](docs/TOOLKIT.md) has how the
toolkit works and how a game is added.

## Feedback and contributing

Bug reports and pull requests are welcome: see [CONTRIBUTING.md](CONTRIBUTING.md) for the rules (above all: nothing
from any game, ever, and never a key). Security reports go by [SECURITY.md](SECURITY.md).

## Related projects

| Project | What |
| --- | --- |
| [satrecomp](https://github.com/tmcleroy/satrecomp) | The same kind of toolkit for Sega Saturn games |
| [recomp-common](https://github.com/tmcleroy/recomp-common) | The in-game menu, settings and launcher this project and satrecomp share |

## License and credits

[GPL-3.0-only](LICENSE). [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) has the notices in full.

- The program cipher (`machine/cps2crypt.*`) is MAME's (BSD-3-Clause), worked out by Andreas Naive and Nicola
  Salmoria. The QSound model follows MAME's (superctr, Valley Bell), and the hardware facts follow MAME's CPS-2 and
  CPS-1 drivers. The CPS-1 sound CPU's Kabuki cipher (`machine/kabuki.*`) is written from the facts MAME's
  `kabuki.cpp` publishes (Nicola Salmoria); its keys are not here: the port works each out from the player's own ROM.
- The launcher's look comes from the launcher of [wiiparty-recomp](https://github.com/arelkair/wiiparty-recomp) by
  arelkair (GPL-3.0-or-later), in `recomp-common/launcher/`.
- The programs use SDL 3 (zlib), Dear ImGui (MIT) and the Inter font (SIL OFL 1.1); the release archives carry their
  licenses.
- The Windows programs make arcade-monitor modes with [Switchres](https://github.com/antonioginer/switchres) (Chris
  Kennedy, Antonio Giner, Alexandre Wodarczyk, Gil Delescluse; GPL-2.0-or-later), downloaded at build time.

The repository holds no game data and nothing derived from it.

This project is not affiliated with, endorsed by, or sponsored by Capcom or Marvel. *Street Fighter*, *Super Puzzle
Fighter*, *Super Gem Fighter*, *Pocket Fighter*, *Vampire Savior*, *Vampire Hunter*, *Night Warriors*, *Darkstalkers*, *Mega Man*,
*Ghouls'n Ghosts*, *Strider*, *Captain Commando*, *Cyberbots*, *Giga Wing*, *Progear* and *Dimahoo* are trademarks of Capcom Co., Ltd.; *X-Men*, *Marvel Super Heroes*,
*The Punisher* and their characters are trademarks of Marvel Characters, Inc.; *Alien vs. Predator* and *Cadillacs and Dinosaurs* are
trademarks of their owners; "CPS-1", "CPS-2" and "QSound" are names and trademarks of their owners, used here only to
say what the programs work with.
