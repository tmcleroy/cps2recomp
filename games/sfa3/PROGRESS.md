# Street Fighter Alpha 3: progress

The ROM set is `sfa3u`, *Street Fighter Alpha 3 (USA 980904)*, a clone of `sfa3`: the 980904 revision is the last and
the one Alpha 3 is played on in tournaments and online (issue #89: "whichever one the competitive community prefers";
FBNeo's and Fightcade's are this revision, Europe's or Japan's, and the USA set was taken by the project's rule). Every
file's CRC32 was checked against MAME's `cps2.cpp`, the key included. The sound driver is Capcom's 1.71 (`sz3.01`),
run translated.

## Where it stands

- **Native**: the 68000 program translated by `tools/recomp.py` (about 123,600 instructions, 4,654 entries from made-up
  play and the movies, 1,172 guessed), to `code_end` 100000 (nothing past it ran in any run). The Z80 program
  translated. No emulator is linked.
- **Rewritten as readable C++** (`src/widescreen.cpp`, checked by `check_rewrites.sh`): the camera's place across
  (017E7E) and up (017EFC), keeping a player inside the picture (02ADA2) and both together in a throw (02AE4C).
- **Features**: free play (the game's own), straight to the title, the Board page, cheats (no damage, full gauge and no
  stun for each player, one-hit knockouts, infinite time), the custom soundtrack's names, the training session and its
  aids (with Alpha 2's own: throw range, custom combo time, stun readout, fixed damage, the dummy's Alpha counter, tech,
  quick rise, stun mashing and the game's own computer player), the Alternative modes page (attract mode, the stage and
  model viewers, survival, endless battle, ring out, the tag team battle in both styles), custom colours and the colour
  editor, widescreen and invisible walls, the hidden characters (Balrog, Juni, Juli) with no code.
- A small toolkit change for this driver: `Game::musicBelow` (`machine/soundtrack.h`), for a sound program whose songs'
  data do not tell music from effects; and one in `video/video.cpp`: the wide picture's holes are filled before the
  objects are drawn, so that a fighter or the panel beside one is not mirrored into it.

## What was found, and how

All in `src/sfa3.h` (and the file using each) with the routine or the experiment it came from. A5 is FF8000. The game
is built as Alpha 2 is, so Alpha 2's names were the first guess for each and each was checked here; FBNeo's training
script and hit box script for `sfa3` (peon2/fbneo-training-mode) named some first.

- **Steps**: FF8000 2 to 8 as the board starts (the warning), then 0 and its show; 2 in a game. FF8004 2 choosing, 4 the
  match; FF8008 6 a round; FF800C 6 while it is fought (2 and 4 its introduction, 8 on its end). Traced with `--watch`
  and `--trace` through attract mode and a versus game.
- **The show** (FF8000 with no coin): 0C and 0E the notices, 10 the introduction (FF8006 its count of frames; the title
  comes in from white at 0x5B0, frame 2337), 14 the title and a demonstration fight, 18 the score rankings, 1A-26 more
  rankings and the advisory, 2A another fight, 2E the rankings, then 0C again. FF0E76 and FF0E77 are the loop's own
  counts (stepped at 0043EC; FF0E77 picks a page of the show: 025886); the demonstration fights' fighters are drawn at
  random (004B3A), so no counter picks them.
- **Credits** FF8076, coins FF80C3 (dumps after one to six coins; two coins a credit on a new board), as in Alpha 2.
  **Players in** FF80A8, **who has chosen** FF80AC (a bit each; traced through the choice).
- **Free play**: the coin mode FF8097 (from the settings record, 0018F2); mode 0x12, FREE PLAY in the test menu's list,
  sets FF809C and the coin words FF8072/FF807A to 0101 (0018F6 on).
- **The board's settings**: the test menu's System and Game pages, each option stepped with Right and the variable read
  after each press (`board.cpp` has the table). Read at the board's start by 00188A (called at 0010D4, the hook at
  0010DA); a game begins at 008CCE (which copies the difficulty to A5+BE and the max round to A5+108).
- **Clock** FF8109 (99 at 0090D0), the frames of its second FF810A, taken at 009BFE. **Result** FF810C, time over FF810D.
- **Players**: records at FF8400 and FF8800, 0x400 apart (and two more, for the dramatic battle: 01BF22 clears four).
  +04-+07 the steps (+05 00 in control, 02 reeling, more held; +06 0 stand, 2 crouch, 4 walk, 6 jump, 0A attack, 0C the
  guarding pose, 0E the Alpha counter), +0B facing right, +0D the first colour set (player 1 0, player 2 5), +10/+14
  place (floor 0x28), +1C the frame, +31 in the air, +38 the opponent, +3C a jump's way, +40/+44 speeds and +48/+4C their
  pulls (traced through a jump: 01B0C4 flies by them), +50 health (taken at 0240D2; below 0 knocked out, 023F1C) and +52
  what the bar shows (0x90 full), +54 the reel kind, +55 the hit's reaction (FF guarded), +5F hit stop, +A6 half width
  (the keep-in), +102 the character, +11E the super combo gauge (0x90 full in every ism: 02EFDC), +125 the computer's
  byte (027C94), +128 the colour choice, +132 the ism (FF X, 0 A, 1 V), +B9/+BA the V-ism's custom combo and its time,
  +2CB-+2CE the stun (timer, built up, the amount that stuns, stunned: 023C30), +37D bit 0 holding away.
- **Characters** (+102): each number seen in a fight with player 2's byte held at it. 0 Ryu, 1 Ken, 2 Akuma, 3 Charlie,
  4 Chun-Li, 5 Adon, 6 Sodom, 7 Guy, 8 Birdie, 9 Rose, 0A M. Bison, 0B Sagat, 0C Dan, 0D Sakura, 0E Rolento, 0F
  Dhalsim, 10 Zangief, 11 Gen, 13 Gen's other stance, 15 Balrog, 16 Cammy, 18 E. Honda, 19 Blanka, 1A R. Mika, 1B Cody,
  1C Vega, 1D Karin, 1E Juli, 1F Juni; 12, 14 and 17 are not characters of this version; from 20 on the board resets.
- **Stages**: the word A5+100 (the stage's number doubled), from A5+174 as a round is set up (0090B8, the hook for a
  wanted stage); the match is made at 00B778 (the hook for wanted characters at 00B7EE), a character's stage from the
  table at 00BBA8. 26 stages: 00-11 (0A the final stage), 15 (M. Bison's, Balrog's, Juli's and Juni's), 16, 18-1D; the
  others are copies.
- **The hit test** 022F86 (A6 the attacker, A1 the victim): the attack box from the frame's +09 in the table at +A0
  (0x20 an attack), the hurt boxes by the record's +C8-+CA in the tables at +90-+98 (8 bytes a box), the push box +CB in
  +9C; boxes compared at 0246FC (x turned about when facing right). The frame's +08 names a set of four box numbers
  (02A77A). Things thrown: two pools of twelve 0x100 records, FF9400 and FFA000, laid out as a player's.
- **The guard-range test** 02AB24 (holding away or auto guard; things thrown within 0x50; the other's attack by its
  reach and 0x58): `guard.cpp` asks it without the holding away for the dummy and the coach. **The guard decision**
  023E26: the victim's frame +0D its pose (1 standing, 2 crouching, 3 in the air), the attack's +15 a bit for each pose
  that cannot guard it.
- **The player's step**: 027C94 for a fighting player (a person's and the computer's: 0329AA the computer's input), the
  dispatch on +05 at 027CBA (the hook for the session's view and the ring out). Animations: 02A770 sets a frame (0x14
  bytes; +00 its time, +01 bit 7 a jump, bit 6 the move's end), 02A7EA steps it; the four lists at +2B0-+2BC (the
  reelings +2B8, 02A75E), made from the table at 0DD412 (three blocks by ism) by 02FC4A.
- **Reeling**: the reaction +55 dispatched at 028BE6 (0D a sweep's knock-down, 12 knocked down into the air as a
  knock-out is); the kind +54 by the table at 0292A0 (4 hit in the air, 8 knocked down: its steps 2 flying, 6 landing,
  8 lying, counted by +DF, 0A getting up; 0C stunned, its time the word +3A, less what 02B56A counts of the stick and
  buttons).
- **Quick rise** 029884 (a person's player, 027CAE): two kicks newly pressed while the knock-down's flight (02020202) is
  under 0x50 above the floor, not in the X-ism, not stunned, and not after a hit that forbids it (+258: sweeps and the
  like). **Tech** 029D08: within 8 steps of being thrown, back or forward with two punches or two kicks (+29B). Throws are
  forward or back with two punches; **the throw test** 02E3FE takes a box of the thrower's attack table (each
  character's own code gives its number: Ryu's 4D) and tests it against the other's push box.
- **Camera**: record FF8280 (+20/+24 where it is to be, +50/+54 its shake, +40-+46 its ends, 256 to 640 across), copies
  FF8026, FF82A0, FF82A8; the players stand from 280 to 1000 and begin at 544 and 736. +263 set leaves a player out of
  its view (018700). The planes behind: FF8200 (8-pixel) and FF8300 (32-pixel) scroll screens.
- **Sound queue**: A5+5E80, sixteen bytes a request, the next place at A5+6E80, sent from A5+6E82 at 0030D4; FF05 stops
  the music, FF07 comes before a tune.
- **Objects by colour set** (hiding each set in turn, `SFA3_HIDE_SETS`): 00-04 player 1, 05-09 player 2, 10-13 the
  stage's own pieces, 14 shadows, 19 dust, 15-18 and 1A-1F the panel (the faces beside the bars are in the players' sets,
  tiles 16C00-177FF: by tile). The panel is objects; every scroll screen is the stage's.
- **Pools of objects** (01C046 on, taken at 01C1C8 on, given back at 01C206 on): seven, with what each giving back
  clears (`livetag.cpp`); an object's maker at +36.
- **The panel** (an object of the last pool, kind 6, FFDC00 in a fight): the names under the bars at +56 (0B9442, from
  0B97EC, 0B986C, 0B98EC) and the faces at +46 (0B958C, from 0B996C, 0B9A7C).
- **The setting up of a player** (state 0, 027B66): clears +80-+FF and +180-+3FF, sets the health, the character's own
  setting up by +102 (027C54), the entrance only with +159 set (027C72).
- **Colours**: the table at 32C094 (a word by character), six sets of five palettes, the set `(ism + 1) * 2 + choice`
  (XP XK AP AK VP VK); 01F95C works it out as a round starts, keeps it at +14C and where it goes at +150 (player 1
  90C000, player 2 90C0A0). Effects from 3329D4 and 34A7B4. PalMod file `sz3.09c`.
- **Hidden characters**: the choice's grid, 8 x 8 at 01E724 (25 faces and four cells that choose at random); Balrog,
  Juli and Juni are on no cell, and the board gives them by its time release (A5+EE, from the settings memory). The
  feature changes the character at 01E764, where the game takes the cursor's.
- **Music**: the table at 0x9006 of the sound program (driver 1.71's music and effects begin alike: songs 00-2F are the
  music, `musicBelow`, with or without an entry). A fight's tune is a character's theme (003A30, the table at 003ABA),
  not the stage's; the stage viewer's tune is put in at 003AB2. Named with `CPS2RECOMP_SOUNDTRACK_LOG` over two full
  ladders (E. Honda, Ryu), a lost game, a challenger and attract mode, and `--music-test`. Not placed: 1C, 2A, 2E.
- **Random number** FF80AE (01BDA6).

## Training mode

The toolkit's session (`fight/session.h`) and its aids, with this game's facts in `src/training.cpp`:

- Frame data from the player's step (027CBA); a hit or a guarded one from the reeling it begins. Ryu's standing jab:
  startup 4, active 4, recovery 6. Not compared with a published table: SuperCombo's Alpha 3 pages were behind a bot
  check when this was written.
- Boxes as the hit test reads them; the throw range from the box the character's throw was last tested with
  (`ThrowTested`, 02E3FE).
- Readouts: stun (+2CC against +2CD), the V-ism's custom combo time (+BA while +B9), combo.
- Fixed damage: the random part (a byte of a row of 32 at 0C4B3A, 024146) replaced by the row's average.
- The dummy, checked headless with scripted sessions (`training_attacks.txt` and others made for the purpose):
  guards 24 of 26 hits with "always" (the other two were throws); with "Alpha counter" it goes into its counter (state
  02000E) and hits player 1 nine times where it was hit twice; teched throws took 5 where they took 20; quick rise: 3
  rises from the 3 knock-downs that allow one, in 20,000 frames of made-up attacks; mashing: a stun of 136 frames took
  55. "Played by the game" sets +125.
- Guarding: stands unless the attack's entry (+15) says a standing guard cannot take it and a crouching one can (a
  low). Hits reach the session from 0240D2 (TakeDamage), for the combo readout and "after a hit". Checked with
  scripted strings (walk in, standing jab, 45 frames on a crouching short, every 135 frames; and a frame trap of two
  crouching shorts 24 frames apart, every 200): Always guarded 24 jabs standing and 24 lows crouching; After a hit
  let every jab land and guarded 23 of 23 lows (and in the trap: 17 first lows land, 17 second guarded); First hit
  only the reverse; Standing only let all 24 lows through; Crouching only guarded every low (a jab misses a
  crouching Ken).
- Infinite guard gauge: +24D (what guarding has taken, 023A5E; drained a step at a time by 027E10; past the size +24C a
  crush, 023A66) kept at 0, and +24C kept at the largest seen for the character and ism (a crush, 029024, and an Alpha
  counter, 02EE72, make it smaller). Checked: 37 heavy attacks guarded in a row; off, three crushes took the bar from 60
  to 16 and 10 attacks landed after them; on, none, and the bar stayed at 60.
- Air recovery (02B330, called in reels 6 and 8 while flying): two punches, kept at +27A once pressed, taken when the
  attacker's move is over (02BB5A clears the victim's +26A/+26B) and the player is high enough (0x30 neutral, 0x10
  with the stick held); a juggle (reel 6) only while still going up. The stick (+37D, 02B4BC) sets the flight: neutral
  keeps the hit's speed across, forward 1.0 across and 4.0 up, back 3.0 across and 6.0 up. Checked from a savestate
  before a standing medium punch hit a jumping dummy: off, no recovery; neutral lands 47 px on; forward is hit again
  by the next punch; back lands 98 px on.

## Alternative modes and the tag team battle

The toolkit's modes (`fight/alternative.h`, `fight/battle.h`), with this game's facts in `src/modes.cpp`,
`src/battle.cpp`, `src/livetag.cpp`, `src/model.cpp`:

- Ring out: one flung out is put in the knock-down's flight (02020202, kind 8) with the in-air reeling's animation
  (12), flies by 01B0C4 and animates by 02A7EA, through the floor; the winner drops in onto the next stage. Seen: E.
  Honda knocked over the end, Ryu landing on the next stage, the result and the mode's end.
- The tag style: a swap gives back what the outgoing character made, sets the character and state 0; the game sets the
  newcomer up with no entrance (027C72 sent to 027C3C) and it is put back where it was (027C68); its colours, name and
  face follow the next frame. A bar emptied with a partner to come in leaves the health at 0 and knocks down (reaction
  12). Checked: `SFA3_SWAP` swaps in a versus fight (Cammy, Chun-Li, Juni: colours, names, faces), and a team battle
  against the CPU's team to its end (Balrog down, Juli in, the CPU's team winning). The fighters a battle picks at
  random are only this version's characters (`choosable`).
- No tag calls found for this game yet (`TagCall`: none).
- Model viewer: the A-ism block's four lists as A-D; a list's length runs to the next list or the first frame after it
  (Ryu: A 76, B 50, C 49, D 70). The viewer sets +263 for player 2 so the camera leaves it out.
- The stage viewer's stages: the planes' and pieces' objects are kept, the panel left out.

## The computer player: scripts (2026-10-26)

Alpha 2's interpreter (games/sfa2/PROGRESS.md, "The computer player: scripts"; Vampire Savior's design), at 030A80,
called each step for a player the game plays: `tools/aiscripts.py sfa3` prints every character's scripts (44,385
instructions in 28 characters'; `--character NAME`). Game data: nothing decoded is kept in the repository.

- The computer's part of a player's record is Alpha 2's, some fields moved: where the interpreter is `+230`, the banks'
  places `+220` to `+22C`, frames left `+21A`, the gap `+21C`, the tier `+213`, the level `+127`, the first if's depth
  `+215`, the answer chosen `+21E`, the answers that may break in `+240`. The character is `+102`, its ism `+132` (0 the
  A-ism, FF the X-ism, 1 the V-ism), the gauge `+11E`. Tables: plans `0DF492`, tactics `0DF512`, reactions `0DF592`,
  answers `0DF612`, the characters' test of a special `0DF412`; chances `0C835A` (an end), `0C83DA` (broken off),
  `0C965A` (a go to), `0C845A` (by character: breaking in, the Alpha counter).
- Alpha 3's own, beside Alpha 2's:
  - A throw, operation 16: the throw's state at once (02040000), its word's two bytes to `+81` and `+80` (the scripts'
    throws are mostly 0000). Jump kind 06: a jump into an air throw the same way; kind 04: a jump into a V-ism custom
    combo in the air (half the gauge, none on; else a plain jump). No script has kind 04.
  - The super combo by the ism: the A-ism's at a level of gauge, the X-ism's (and a character's with `+123` set) at a
    full gauge, as a level 3; for the V-ism a frame's wait instead. Operation 0A kind 02: the V-ism's custom combo
    (half the gauge), its special and timers (`+244`, `+2A6`, `+2A8`) by its word.
  - Tests 3E to 4C: the A-ism (3E), not (40), the X-ism (42), not (44), the V-ism (46), not (48), `+5B` 1 (4A), a custom
    combo on (4C); test 06 wants a full gauge but for the A-ism.
  - A move word: bits 0 to 3 held (`+83`, bit 0 also the way), bit 11 set for a special (its number from bit 4,
    punch or kick bit 10, strength from bit 13), clear a normal (bits 4 to 7 to `+2C0`, stance from bit 8).
  - A normal or a special is cancelled into the next instruction's at once when it may be (032CD6, 032D08: in a custom
    combo, the next a normal or a special; from a chain, a super combo too by the gauge).
  - The second if (operation 14) is the game's, but no script of this version has one. Its two skipping routines know
    only Alpha 2's operations and tests (a throw there would be stepped over wrongly), and `aiscripts.py --check` says
    none is inside such an if; the first if's (032AEE) knows them all.
- Checked: every script decodes, nothing left over; the places the game kept against them (`--against`, every rewrite
  left out, `--cpu blocks`): 14,729 places in seven runs on both sides (the ladder, the attract mode, the endless
  battle, the ring out, the tag style), every one an instruction found and every step one the scripts allow (but three
  written by the tag style's copy of a record, 000CD2).
- As C++ (`src/computer.cpp`, after Alpha 2's): ComputerThinks (030A80) and Chance, BrokenOff, Reactions, AnswerNow
  (030A66, 032258, 0329AA, 032AE0). The two places the game jumps into from outside (03139C, the custom combo's start;
  032C9A, a jump picked at random out of a reeling) stay the game's. Checked with `--check-rewrites` (Alpha 3 has no
  other rewrites of its players: the game's own code makes every call): about 1.45 million calls of ComputerThinks,
  2.1 million of Reactions, 149,000 of Chance, 5,800 of BrokenOff and 1,700 of AnswerNow, none differing, CPU against
  CPU at levels 0 to 31, every tier and plan, each ism held (`+132`), the gauge held full or at half, and Gen and
  Chun-Li for the scripts only they have (`check_rewrites.sh`'s computer section). Every instruction and test the
  scripts have was reached, but for Reactions' answer 2 (the other lying down near) taken.
- The tag style and the ring out (as Alpha 2's: `Description::computerPlaysModes`, the coach standing aside; the
  port's tactics in Alpha 3's words, its throw instruction among them, the tag operation 18 and the tests 4E, 50, 52).
  CPU against CPU, 12 seeds of 40,000 frames each way:
  - The ring out: nobody playing it, side 1 won 47% of 281 ring outs; side 1 playing it itself 53% (301), side 2
    itself 64% (255, from 53%). The coach: 53% and 45%.
  - The tag style (team battles of three, about 75 matches each): with nobody tagging but when a bar empties side 1
    won 44%. Tags cost the side that makes them in this game, the coach's too (side 1 43%, side 2 36%): Alpha 2's tactic
    (back off, then tag) won 39% and 24%, tagging only while the other is down or far 32% and 30%, only while it is
    knocked down 40% and 42%. The last is the computer's here: the one coming on jumps in, and Alpha 3's computer hits
    it.
- Only headless: nobody has played against it yet.

## HSFA's address index (2026-11-03, issue #244)

[HSFA](https://github.com/Rickow/HSFA) (MIT, no game data), which translates the PS2 *Street Fighter Alpha
Anthology*'s games back to 68000, keeps an index of about 9,200 Alpha 3 addresses (`re/connaissances/sfa3_arcade.tsv`,
in French: each one's role and whether it was measured, deduced or assumed). `tools/hsfa_index.py` makes it into
`work/sfa3/hsfa_index.tsv` (not committed: a list made from the program), at our addresses and checked against our
listing.

- **Nothing moves.** Their set, `sfa3ud`, is USA 980904 too, the Phoenix Edition's deciphered copy: every word of its
  program is ours, deciphered (code, and what is read through the pc) or raw (other data), but for 3,608 bytes of
  Phoenix changes in 16 places (the vectors, words of the start and the settings code from 000922 to 0019E1, three
  in 01094C-010B63, and its own code from 0FF000). So the index's addresses are ours; one entry is a Phoenix change and
  41 are their translation's or emulator's own.
- **Their routines against our listing**: 9,045 entries at an instruction of ours (or data); 97 in code our
  translation has never reached (the IRQ4 handler, test menu pages, 079300-07AE00); 12 inside one of our
  instructions, most where their MIPS translation was cut up, three "measured" ones a few bytes off (003130, 01F7E4,
  07CB98: the instruction is at 00312A, 01F7E2, 07CB96).
- **Agreements** (every fact they also have): the random number (FF80AE, 01BDA6), the stage (A5+100, A5+174), coin mode
  and free play (A5+97, A5+9C), time release (A5+EE), the full health set at 027BAE, the animation lists (+2B0-+2BC,
  02A708/02A758, frames of 0x14 with their time at +32, 02A770 and 02A7EA), the reelings' table by +54 (0292A0), the
  boxes (+90-+A0), the opponent (+38), the character (+102), the facing (+0B, +2C9), the script's place (+230, +220),
  the wait at 030E68, the game speed's values (A5+A3; ours also has free select).
- **Ours made more exact by theirs**, each seen in the game: the fourth animation list's routine is 02A710 (the attacks':
  it takes the attack's +23E and +293 from the character's table first), not its tail 02A74E; the players' records
  run in an order drawn by the random number each step (0277EE); A5+90 is the region (doubled), picking the factory
  settings at 00235A (USA's: coin mode 9, difficulty 4, game speed 2, regulation 1, what our runs' variables hold).
- **New names, checked**: regulation A5+BB (1 here; held at 0, palette 25's blood colours go from FFFF/FCCC to
  FF00/FC00), the round-end pose bits A5+10B (029E10), the ending A5+1C6 (from A5+132 at 00A92C: 18, E. Honda's, at the
  ladder's end).
- **Where theirs is loose**: the tables at 0DCB12 are not all code entry points: the first table's are the characters'
  attack tables (data at the end of each one's code, read by 02A710); 027B98 is not a routine's start (the player's
  set-up is 027B66, case 0 of 027B3E); FF851C is the controls' copy made once a step (count its writes for game steps,
  as they do), not a count.

## Widescreen

- The rewrites above, with the margin in them; with invisible walls the margin is 0 (the game's memory the board's at
  every 4,000 frames of the ladder: `check-widescreen`).
- The planes behind are drawn moved by what their wide view would reach past their painting, from their places at the
  camera's two ends on each stage (`kPlaneEnds`: measured with a training session walked to both ends with invisible
  walls; the left end worked out from the place at 376, as the planes move in proportion to the camera). Sagat's two
  planes and Dhalsim's back plane are left as they are: their maps were never drawn beyond the board's view, so a moved
  view showed holes inside them; the holes at their ends are filled.
- Measured with the filling off (`SFA3_NO_FILL`): 13 stages had unpainted columns at an end; with the planes moved, 2
  (Ken's 10, Akuma's 7) and Sagat's and Dhalsim's left to the filling. With it on, 0 on all 26.

## Checks

Run on 2026-10-25, macOS (Apple silicon), each alone:

- `make GAME=sfa3 check FRAMES=40000 MOVIE=games/sfa3/movies/ladder.txt ARGS="--features free_play,no_damage,one_hit"`:
  translated code does what the interpreter does, two runs alike. The ladder movie reaches E. Honda's ending and the
  staff roll (about frame 22,000-30,000) and the show again.
- `make GAME=sfa3 check FRAMES=12000 MOVIE=games/sfa3/movies/versus.txt`: the same.
- `make GAME=sfa3 check-blocks` over the ladder: 349,695,862 translated blocks and 139,810,603 of the sound driver's
  checked against the interpreter, no difference.
- `make GAME=sfa3 check-release FRAMES=40000`: two runs of the interpreted build alike; its pictures the translated
  build's but for 4 frames (150 allowed), ending on the same one.
- `make GAME=sfa3 check-rewrites`: CameraAcross, CameraUp, KeepIn and KeepBothIn over the versus, ladder, two made-up
  two-player, training and throws movies (up to 30,388 calls each), none differing; their cycles within a few of their
  originals'.
- `make GAME=sfa3 check-widescreen`: all 26 stages painted to both ends, the walls' memory the board's, and the ladder in
  widescreen with nothing in the interpreter.
- **No interpreter fallback**: 0 instructions in the interpreter over the ladder and versus movies, made-up one-player
  ladders (seeds 41, 42) and two-player games (43, 44), 20,000 frames of attract mode, the test menu with made-up input,
  training with its aids, survival, ring out, the endless battle, the team battle, the model viewer, the stage viewer,
  the sound test, the hidden characters and the ladder in widescreen.
- **Features off = no sources**: `--cpu interp --no-rewrites --features -free_play` over 8,000 frames of the versus
  movie, and the same with `src/` moved aside: byte-identical hash logs.
- **Straight to the title** on and off: identical hash logs over 6,000 frames.
- **Save state**: made-up play saved at 3,000 and loaded with the same input after it: frames 3,001-6,000 alike.
- **Run-ahead**: 1 and 2 frames give the hash log of 0, over the versus movie and a training session.
- **Board**: every choice of the ten settings (35) read back from its variable at frame 299; with choices made the test
  menu shows the board's own values, and the settings memory is the same as with none.
- **Launcher**: its three pages drawn with no window.

## Not yet

- Nobody has played it by hand, heard its sound, or seen it in a window.
- The frame data against a published table.
- The tag style's calls (each character's sound as it tags out).
- The coached computer's walk in a ring out (`RingOutMayWalkBack`, which Alpha 2's walk rewrite asks): the coach's
  plans run, the game's own walking is not told.
- The music's 1C, 2A and 2E; the board's time-release levels beyond the hidden characters (01E7B0: a hidden ism).
- The colour editor's "Stage in front" and Line search, Save set and Load; PalMod itself.
