# New Cheats: complete guide

English | [Español](CHEATS.es.md)

This guide covers **every cheat added or changed by Input Lab**, not every pre-existing upstream cheat.
The official training, invulnerability, gauge and other options remain available and retain their own
in-menu descriptions. These additions appear under **Game → Cheats** in-game and on the corresponding
game's launcher page. They are stored **per game**; P1/P2 selectors are also independent per player.
New selectors default to Off/Vanilla. Online retains the official restrictions on cheats.

| Cheat | Validated base game/set | Scope |
|---|---|---|
| Damage hits | SFA `sfa` Europe 950727; SFA2 `sfa2u` USA 960430; SFZ2AL `sfz2al` Asia 960826; Super Turbo `ssf2xjr1` Japan 940223 | Live human/CPU damage-profile equalization |
| P1 / P2 Select Character | Super Turbo `ssf2xjr1` Japan 940223 | Force the next initialized character, including selectable Akuma or CPU-boss Shin Akuma |
| Select Speed | Super Turbo `ssf2xjr1` Japan 940223 | Original internal turbo/frameskip rule |
| P1 / P2 Select Colour | Super Turbo `ssf2xjr1` Japan 940223 | Ten selector choices; correct two-palette mapping for Akuma/Shin Akuma |
| Hidden characters | SFA2 `sfa2u` USA 960430 | Toggle hidden/classic forms with START |
| Hidden characters | Vampire Savior `vsavj` Japan 970519 | Toggle Dark Gallon/Oboro Bishamon with START; retain Shadow |
| Hidden characters | Puzzle Fighter `spf2t` Europe 960529 | Cycle hidden characters with START |

Addresses and instruction behavior were checked for these base revisions. A matching title or revision date
alone does not establish compatibility with another regional set. MAMEPPK XML cheats are a separate project;
this fork implements native hooks and does not distribute that XML.

## Damage hits

**Purpose:** choose which original damage profile is used when a human fights the CPU, without converting
the human into a CPU-controlled fighter or replacing the game's damage formula with a fixed multiplier.

| Choice | Effect |
|---|---|
| **Vanilla** | Uses the game's original damage calculations and human/CPU distinction. |
| **Equal to human** | Both sides use the current human-attacking-CPU profile. The CPU's corresponding calculations use that reference as well. |
| **Equal to CPU** | Both sides use the current CPU-attacking-human profile. The human's corresponding calculations use that reference as well. |

“Current” matters: difficulty and the game's adaptive ranks remain live. Equal to CPU **does not force
maximum damage**, and its profile is not guaranteed to be stronger at every difficulty or moment. These
options equalize the relevant human/CPU profile, not the damage of every move or every character.
Character, move, blocking, health, combo and random modifiers remain in the original calculation.
The cheat does not change AI ownership or overwrite stored ranks.

**Example:** use Equal to human to compare a CPU opponent against the human-side profile; use Equal to CPU
to try the reverse. The same attack can still produce different final damage when its circumstances differ.
Selecting Vanilla returns to the original policy; no fighter recreation is needed for the next calculation.

**Covered routes:**

- **SFA:** normal damage, grabs/throws and original third-fighter ownership handling.
- **SFA2:** normal/alternative damage paths, throws and holds; retains the earlier Input Lab implementation.
- **SFZ2AL:** Dramatic Battle's third fighter and projectile/helper ownership. With the policy enabled,
  the original routine runs while only the relevant rank/class reads are substituted.
- **Super Turbo:** attack/defence rank reads for hits, projectiles and throws.

Ordinary human-versus-human and CPU-versus-CPU retain Vanilla. Dramatic Battle support is game-specific;
it does not mean this policy was added to every alternative mode in every other title. No player input
“mashing damage bonus” is introduced or claimed by this cheat.

Implementation: [shared reference selection](../runtime/damage_policy.h),
[SFA](../games/sfa/src/inputlab_damage.cpp), [SFA2](../games/sfa2/src/damage.cpp),
[SFZ2AL](../games/sfz2al/src/inputlab_damage.cpp), [Super Turbo](../games/ssf2t/src/inputlab_cheats.cpp).

## Super Turbo: P1 / P2 Select Character

**Purpose:** force a chosen character when that player's character is next initialized. Each player has
Off, the sixteen regular characters, **Akuma / Gouki**, and **Shin Akuma / Gouki (CPU boss version)**.

Choose the option **before confirming a character**. Changing it in an active fight takes effect at the
next character initialization. Off stops forcing future selections; it does not transform an already
active fighter back in the middle of an animation.

Akuma is the normal selectable version. Shin Akuma uses a bridge from human motion commands to the
original CPU-boss resources: attack and animation resources, Tatsumaki turns, teleport animation,
double aerial projectile and its trajectory, plus the original attacker-hitstop handoff when an accepted
normal-to-special cancel is committed. Ground projectile behavior also uses the boss path.

**Limits:** this does not turn on CPU control, force Ken as the opponent, equalize damage, enlarge hitboxes,
remove pushback or change juggle rules. Use Damage hits separately if desired. Opponent size, distance,
timing and native hit rules still matter. The bridge was compared against original CPU routines, but
complete 1:1 equivalence for every possible combat sequence is **not claimed**. In particular, aerial
projectiles → close HP → HP Shoryuken is not guaranteed to connect fully against every character.

Implementation: [native cheat hooks](../games/ssf2t/src/inputlab_cheats.cpp) and
[character initialization](../games/ssf2t/src/choosing.cpp).

## Super Turbo: Select Speed

**Purpose:** override the board's **internal turbo frameskip comparison**, independently of CPU overclock,
video refresh, VSync or overall emulation speed.

| Choice | Effect |
|---|---|
| **Off** | Immediately resumes the original board/player speed choice. |
| **No frameskip (like SSF2)** | Uses the no-skip comparison; every game step is retained. |
| **Turbo 1–4 (Japan)** | Chooses one of the four original Japanese turbo comparison settings. |
| **Maximum frameskip (double speed)** | Uses the most aggressive internal skip setting. |

The implementation replaces only the operand in the original comparison. It does not overwrite the
board/player's saved speed choice and does not synthesize or interpolate new animation frames.
“Double speed” is the selector's name for the internal rule, not a promise of a measured twofold
frame rate in every scene. Lowering frameskip may make a sequence easier to follow without changing hit rules.

Implementation: SelectedInternalSpeed in [inputlab_cheats.cpp](../games/ssf2t/src/inputlab_cheats.cpp).

## Super Turbo: P1 / P2 Select Colour

**Purpose:** force the native palette associated with one of the ten normal selection choices. It is a
selector, not a reassignment of physical confirmation buttons. For example, choosing **LK button** in
this cheat forces that palette even if the character is confirmed using HP on the controller.

| Choice | Regular characters | Akuma and Shin Akuma |
|---|---|---|
| **Off** | Original game choice | Original game choice |
| **LP / MP / HP button** | Corresponding individual native palette | Black, native set 0A |
| **LK / MK / HK button** | Corresponding individual native palette | Dark grey, native set 0B |
| **Start button** | Native Start palette | Dark grey, native set 0B |
| **Hold any P/K button** | Native held-button palette | Dark grey, native set 0B |
| **SSF2 primary** | Original-character primary palette | Black, native set 0A |
| **SSF2 alternate** | Original-character alternate palette | Dark grey, native set 0B |

The sixteen regular characters retain their ten native choices. Akuma/Shin Akuma have two native sets;
the ten choices map to those sets instead of indexing a nonexistent ten-palette Akuma table. No new
palette is invented and no graphics, character stats or combat rules are altered.

Choose **before the next round loads its colours**. The original palette loader copies the chosen set
into video RAM. Akuma's character-selection portrait remains black, so inspect the fighter after the
match starts. Off stops forcing the colour; it does not instantly reconstruct the current round's palette.

Implementation: ApplyInputLabColour in [inputlab_cheats.cpp](../games/ssf2t/src/inputlab_cheats.cpp).

## Hidden characters: START-based selection

These modify features that already existed upstream: the option is named **Hidden characters**, lives
under **Cheats**, and offers **Off / Player 1 / Player 2 / Both**. Only enabled sides get the shortcut.
They change selection, not the encounter conditions for secret CPU challenges.

### Street Fighter Alpha 2

Highlight the regular character and press/release **START** to toggle its hidden form; press START again
to return, then confirm normally:

| Highlight | Alternate form |
|---|---|
| Ryu | Evil Ryu |
| Akuma | Shin Akuma |
| Chun-Li | Classic Chun-Li |
| Dhalsim | Classic Dhalsim |
| Zangief | Classic Zangief |

This replaces the earlier automatic substitution: simply highlighting Ryu no longer forces Evil Ryu.
Confirmation and available native colours follow the game; this shortcut does not add extra palettes
to forms that lack them. It does not force Shin Akuma to appear as an arcade challenger.

### Vampire Savior

Highlight **Gallon** and press START to toggle **Dark Gallon**. Highlight **Bishamon** and press START to
toggle **Oboro Bishamon**, the CPU last-boss variant not selectable through a normal board code.
Press START again to return to the regular form. Confirm with a punch for the first colours or a kick
for the second colours. The **?** box retains **Shadow**, who becomes each defeated opponent.
The enabled-player scope also applies to fighters in the supported alternative modes.

### Super Puzzle Fighter II Turbo

On character selection, press START to cycle **normal → Devilot → Akuma → Dan → normal**. Move the cursor
to return to the normal highlighted character. Confirm with either of the game's two action buttons
to select its corresponding native colour. This replaces the less discoverable direct kick shortcuts;
the original board codes still work when the cheat is Off. The native hidden-character attack behavior,
including their reduced attack output, is retained; this is not an attack-power equalization cheat.

Other game families retain their official hidden-character implementations. Shared source files are
guarded so SFA2-specific additions are not accidentally imposed on Alpha 3 or SFZ2AL.

## How the checks should be read

The full 017/018 build was tested with original routines and native game execution: damage policies,
instruction flags/cycles/RAM, CPU-boss versus human initialization, cancel paths, original palette loads
and menu persistence. With the new colour selector Off, 2,100 rendered frames matched 017.
See [the integrated fork validation record](https://github.com/Sol1vaN/cps2recomp/blob/input-lab/docs/validation/inputlab-018.json). These results do not certify every regional
revision, every imaginable combo, or an isolated upstream PR that has not run the same integration tests.


## Reproducing the native policy checks

Configure and build `tests/inputlab` with the same SDL3/zlib/compiler settings as the game. This builds
the original game-independent core from this branch, not a library from the complete Input Lab fork.
Run `sfa_policy_tests`, `sfa2_damage_tests`, `sfz2al_policy_tests`, `ssf2t_policy_tests` and
`ssf2t_colour_tests` with two arguments: your locally dumped `program.bin.data` and `program.bin`
for the corresponding validated revision. No ROM input or generated translation is checked into this PR.
The tests compare actual original instructions/routines with the hooks, including flags/cycles and VRAM.
