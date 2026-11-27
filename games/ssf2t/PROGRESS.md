# Super Street Fighter II Turbo: progress

The ROM set is `ssf2xjr1`, *Super Street Fighter II X: Grand Master Challenge (Japan 940223)*, a clone of `ssf2t`:
the revision Super Turbo is played on in tournaments and on Fightcade (issue #88: "whichever one the competitive
community prefers"). Every file's CRC32 was checked against MAME's `cps2.cpp`, the key included. The sound driver is
Capcom's 1.04 (`sfx.01`), run translated.

## Where it stands

- **Native**: the 68000 program translated by `tools/recomp.py` to `code_end` 240000 (the characters' own routines are
  past the first megabyte: the tables at 100000 name them, from 10FF12 to 2156E2; with 100000 3,605 instructions of a
  6,000-frame fight ran in the interpreter). The Z80 program translated. No emulator is linked.
- **Rewritten as readable C++** (`src/widescreen.cpp`, checked by `check_rewrites.sh`): the camera's place across
  (0CC4E4) and up (0CC630), and keeping a player inside the picture (0043F4).
- **Features**: free play (the game's own), straight to the title, the Board page, cheats, the custom soundtrack's
  names, the training session and its aids, the Alternative modes page (attract mode, the stage viewer, the model
  viewer, survival, endless battle, ring out, the tag team battle in both styles), custom colours and the colour
  editor, widescreen and invisible walls, the hidden characters (Akuma and the old versions) with no code. The CPU's
  side is coached in the tag style and the ring out (`fight/battle.cpp`), with the game's own guard test for an attack
  coming.

## What was found, and how

All in `src/ssf2t.h` with the routine or the experiment each came from. A5 is FF8000. FBNeo's training script for
this set (peon2/fbneo-training-mode, `games/ssf2t`) named some of them first; each was looked at again here.

- **Reads relative to the PC see the deciphered program**: `program.bin` holds the deciphered program and
  `program.bin.data` the ROM as it is. A table read with `move.w table(pc,dn)` is in the first; one reached through
  an address register (`lea table(pc),a0` then `(a0,dn)`) in the second. Both kinds are mixed, even in one routine
  (a character's animation list is the first kind, its frames the second).
- **Steps**: FF8000 is 2 in a game (the board's start steps 2 to 10 first, then its show from 0); FF8004 is 0 while
  choosing and 4 in the match; FF8008 is the match's: 2-4 versus, 6-8 a round's introduction, 0A fought, 0C-10 its
  end. Traced with `--watch FF8000-FF800F` through attract mode, a one-player game and a two-player one.
- **The show**: FF8000 2 the introduction and the title (FF8004 16 on: the title from white), 4 to 0C the notices, 0E
  a demonstration fight (a game the computer plays for both), 10 to 16 the rankings; frame 805 its first step.
- **Credits** FF82E9 (dumps after 1, 2, 3 coins; 002418), **players in** FF82F0.
- **Free play**: the coin mode (FF8342) is read at the board's start (00823A; the record at A5-6578, copied at
  00831C); mode 12, which the test menu never offers, sets FF82DF, tested wherever credits are counted, and the coin
  words FF82AE/FF82B6 come from the table at 008536 (0101 for mode 12). The title says FREE PLAY with it.
- **The board's settings**: the test menu's System Configuration, each option stepped with Right and the variable read
  after each press: game speed FF83A9 (0-3 turbo 1-4, 4 free select; turbo 2), difficulty FF82D3 (0-7: 1-8; 4), demo
  sound FF82DD (off), continue FF82DA (on), start mode FF8344, monitor flip, sound. The hook is 008FFC, where the
  boot code's call of 00823A returns; a game begins at 00D69C.
- **Clock** FF8DCE, BCD (99 at 00D780), with the frames of each second at FF8DCF: 40 a second, taken at 00FE74.
- **Players**: records at FF844E and FF884E, 0x400 apart (cleared at 003616). Health +2A (0x90 full) and before the
  last hit +2C. +03 who is in charge (00 standing or walking, 02 crouching, 04 jumping, 06 getting up, 08 guarding, 0A
  a normal or a throw, 0C a special, 0E reeling, 14 down), from a trace of a scripted fight against a player who stood,
  guarded, was hit and thrown. +06 and +0A place (longs, 16 bits of fraction; the floor 0x28), +12 facing right, +1A
  the frame, +34 the box tables, +3C/+3E speeds and +40/+42 their pulls (words, 002F50), +47 hit stop, +49 guarded,
  +181 in the air, +18B attacking, +1C2 half width, +284 chosen, +2B4 super gauge (0x30), +381 side, +38E a person,
  +391 the character, +39E the opponent, +3B1 the colours, +3B6 old, +3BD Akuma.
- **Damage**: 0BE646 (a player's attack: D6 off A2) and 0BEBA6 (a thrown thing's); throws D4 off A3 at 00490A and
  0049B0, or put by at +78 (004A58) and taken as the throw ends. Found as the writers of both healths over made-up play
  with no damage; with the five hooks no writer is left over three games of 30,000 frames each, for both players.
- **Hits**: the tests at 0BE518 (player 1's attack, player 2's, then thrown things at 0BEAC8); the box test 0BF5AE takes a
  box's offsets from the frame's numbers and the record's tables (as FBNeo's hit box script has them); a hit and a
  guarded one meet at 0BE678 (0BEBDA for thrown things), +49 set when guarded (0BEF32-0BEF58).
- **The player's step**: 0782D4, the dispatch on +03 of the fight step (0782A0), for the person's and the computer's.
- **Stun**: +5E built up, +5C its timer; stunned at the table at 07F1C6 by +2B1 (30, 34, 35).
- **Stage**: FF8C4E, the number of whose stage (0-15, in the characters' order; seen with its low byte held at each);
  the choosing writes it (0039A6), the match's first step is 00D714 and the stage is set up at 0CCC5C.
- **Sound queue**: FF0A7E (A5-7582), sixteen bytes a request, the next place at FF0A7A; FF05 stops the music.
- **Camera**: record FF8ECE, across FF8ED4 and up FF8ED8, copies FF8FEA-FF8FF0; ends A5+C54/C56 across, C58/C5A up;
  0x100 to 0x280 across on every stage. Players stop at 0x128 and 0x3D8.
- **The round's result** FF8DD2: 1 player 1, 2 player 2, FF neither (00FC88).
- **Objects by colour set**: hiding each set in turn on every stage: player 1 01-05, player 2 10-14, shadows 0A, hit
  sparks 0E, stage pieces 0B and 1A-1F, the panel the rest (07-08 faces, 0C bars, 0D score and clock, 19 gauges). The
  stage is all scroll screens and pieces; the panel is all objects.
- **Colours**: the table at 0BFAF2 by character (16 Akuma), a list of sets by +3B1, five palettes a set; copied at
  0BFA78 to palettes 1-5 (player 1) and 10-14 (player 2). LP-HK give sets 0-5 (seen choosing with each).
- **Animations**: three tables at 100000/100088/100110 name each character's routine (16 Akuma, 17-20 the old ones,
  21 the computer's Akuma); its list is 0x34 into it; frames are 0x18 bytes (002ED0 sets one, 002EE2 steps it).
- **Hidden characters**: Akuma's code ends at 00E942 (the three punches), the old version's at 015788; the choosing
  is 00E9B8 (a button at 00EA5C, made final at 00E93A).
- **Music**: the table at 0x9006 of `sfx.01`, 0x1FF entries, 68 of them music. Named from the soundtrack log over a
  fight on each stage, the ladder, a two-player game and attract mode.
- **Random number** FF82D0 (a shift register: 002E96).
- **A swap in the tag style** (`src/livetag.cpp`): the record from +0E to +37F cleared but for what a round's start
  alone puts in (facing, health, bar, the place's copies, the super combo gauge), the character (+391, Akuma +3BD and
  old +3B6 off), the other's copy of it (+3A2, 00386E: T. Hawk's moves against M. Bison read it), the half width from
  00502A, the routine's step +02 to 0; the game sets the newcomer up as at a round's start. The thrown things it owns
  are given back to the pool (FFA9A2, 60 of 0xC0; its list FF9580, count FF954C), as 003794 gives them. Less than the
  whole record left a character's own state behind: each character's reading of the controls for its moves keeps it in
  +80-+DF (T. Hawk's went off into the ROM's data on what M. Bison's left there) and the computer's plan its step in
  +200 (07BC22). A swap is made where the player's routine begins (078030), never at a frame's edge: the game's frames
  fill the processor, so an edge can come while it is drawing the player (its frame pointer +1A cleared under it, the
  drawing went round for ever at 0C71B4). The panel's faces and names are made again by sending the game through 0104B2
  and 0102F2 from there. Every pair of the sixteen characters swapped seven times in a fight, twice with other times and
  input: no fault, nothing in the interpreter.
- **The computer and a person**: their moves keep where they are in bytes of their own (+180-+188 and the step +04;
  the computer's own moves count in +80, Cammy's at 0B4B2C), so a fighter changes hands only standing or crouching
  (`HandOver`, `fight/battle.h`'s `handOver`): Fei Long's and Ken's routines went into the middle of their tables
  (095C90, 08031C) when the other took a move over. One the computer played has its reading of the controls cleared as
  a person takes it (`src/computer.cpp`, 078270: Cammy's reading at 08DCBC took the computer's count for its own), and
  one given to the computer mid-round gets the computer's set-up (07D6F4, 07E36E, 07BB82, the words from 24EAE8) but
  not the standing animation that set-up starts.
- **Facing**: +12 is not zero when the player faces right (it was named the other way, which mirrored the input
  display's directions).
- **The guard test** (07AF3A and 07AFBA): a thing thrown (the other's word +1D4, or +2AE) within 0xE0 across coming
  this way or 0x70 going away, or the other attacking (+18B) within 0xE0 (+182, how far apart): the coach's "attack
  coming".

- **The Alphas' training aids** (the throw range, fixed damage, a dummy that guards high or low by the game's rule,
  softens throws, mashes out of stun or is played by the computer), all with hooks at places that already began blocks
  of translated code (a hook elsewhere splits a block, which moves where an interrupt is taken, and with it the game):
  - *Fixed damage*: a hit's damage gets a random byte of a row of 32 (0BEE40: the table at 0E7996, the row by the
    attack's +0A, or +09 above 0x3C health, the pick the random number's low five bits), and a throw's alike (004B86;
    the classic games' throws have none). Hooked where the sum is next used (0BEE74, 004BDC) and where it is made 1
    (0BEE72, 004BDA): the row's average put in the byte's place. 19 every time for Ryu's medium punch (19, 21, 20 at
    random) until the health is low enough for the game's own reduction (0BEE80).
  - *Throw range*: the throw test (00461E, 004604, 00460C; 0046E4) takes the thrower's box from its record (+64 across,
    +66 up, +68 and +6A half sizes: put there for the throw tried, by the character's table at 0B8650, and written by
    nothing else) and the other's place to be thrown by (+6C half width, +6E height, a table by character as a round
    is set up; +2AA across while +2A7 is set, Super Turbo's alone). Drawn from the records, no hook.
  - *Guarding high or low*: the guard decision (0BEEA4) takes the guarding pose (the frame's +11: 1 standing, 2
    crouching) and, through a jump table by the attack's +0B (0BEED8, 38 kinds), one of five routines: most attacks
    (crouching guards them, standing only on the head or body: lows reach the legs), standing only (kind 0C, a jump
    attack, 24, 26, 34, 4A), crouching only (0E, 28), either way. The classic games decide by the attack's +07 through
    a row of four bytes for each pose (Champion Edition's 095608; family.h's kGuardRows). `src/guard.cpp` asks the same
    of the attack coming, for both poses and all three hurt boxes, and the game's guard test (07AF3A) for when.
  - *Softening*: 0047D4, each step of being thrown, Super Turbo's alone: the stick to a side and a medium or heavy
    button new within 0x0E steps (+2BC) sets +2BD, and the throw does half its damage (32 to 16, Ryu's). The one
    thrown stays in its own state meanwhile, +63 FF (1 for the thrower), and its own step does not run: the dummy is
    told it is held by that byte.
  - *Stun*: +124 is 1 while stunned (07A7B2, cleared at 07A7D2); a person shortens it (07A80E: 2 for the stick from
    the middle, 1 for a button new, 2 in Hyper Fighting, and at 3 the step's time off +60). Mashing every other frame:
    130 frames stunned became 43.
  - *The box tables of the other games*: Super Street Fighter II's attack boxes are twelve bytes (Super Turbo's
    sixteen), and the classic games keep an attack's table at +8 and the pushing box's at +A (Super Turbo's +6 and
    +8; +6 there is a fourth box the hit test looks at only after a hit), twelve bytes an attack box: family.h's
    kTableAttack, kTablePush, kAttackBoxSize, read in each game's hit test. Until now the hit boxes drew those games'
    attack and pushing boxes from Super Turbo's places (in Champion Edition no attack box showed at all).
  - *The computer*: +38E to 0 and back when the game lets the player change hands (battle.cpp's HandOver), the
    computer set up by `src/computer.cpp` as for the fighting modes.

## Checks

Run on 2026-10-25, macOS (Apple silicon), each alone:

- `make GAME=ssf2t check FRAMES=40000 MOVIE=games/ssf2t/movies/ladder.txt ARGS="--features free_play,no_damage,one_hit"`:
  translated code does what the interpreter does, two runs alike. The ladder movie reaches Ryu's ending, the staff roll
  (about frame 35,000), game over and the show again (39,000).
- `make GAME=ssf2t check FRAMES=12000 MOVIE=games/ssf2t/movies/versus.txt ARGS="--features free_play"`: the same.
- `make GAME=ssf2t check-blocks` over the ladder: 317,827,857 translated blocks and 165,810,752 of the sound driver's
  checked against the interpreter, no difference.
- `make GAME=ssf2t check-rewrites`: CameraAcross, CameraUp and KeepIn over the versus, ladder and two made-up
  two-player movies (13,000 to 61,000 calls each), none differing; their cycles are their originals' averages.
- `make GAME=ssf2t check-widescreen`: all sixteen stages painted to both ends in the wide picture (no unpainted column),
  the game's memory with invisible walls the board's at every 4,000 frames of the ladder, and the ladder in widescreen
  with nothing in the interpreter.
- `make check`: the audit, the no-game build and the launchers.
- **The tag team battle and the coach**: `CPS2RECOMP_BATTLE_SEED=1 make GAME=ssf2t check` over
  `movies/team_tag_computer.txt` (9,000 frames; the CPU tags), `team_tag_two_vs_two.txt` (9,000) and
  `ringout_computer.txt` (12,000; two ring outs), with the options in each movie's header: translated code as the
  interpreter, two runs alike, nothing in the interpreter. Made-up play for 12,000 frames each, 24 seeds of each kind
  (tag style with teams of two and of three against the CPU, knock-out style, player 2's team), 48 of the ring out
  against the CPU: nothing in the interpreter, no fault.
- **No interpreter fallback**: 0 instructions in the interpreter over the ladder movie, the versus movie, made-up
  one-player ladders (seeds 41, 42, 90) and two-player games (43, 44, 91), 40,000 frames of attract mode, the test menu
  with made-up input, every character's old version and Akuma, training, survival, ring out, the stage and model
  viewers and widescreen. One endless battle on shuffled stages (20,000 frames) still had 19.
- **Features off = no sources**: `--cpu interp --no-rewrites --features -free_play` over 8,000 frames of the versus
  movie, and the same with `src/` moved aside: byte-identical hash logs.
- **Save state**: made-up play saved at 3,000 and loaded with the same input after it: frames 3,001-6,000 alike.
- **Run-ahead**: 1 and 2 frames give the hash log of 0, over the versus movie and a training session.
- **Board**: every choice of the four read back from its variable at frame 299; with choices made the test menu shows
  the board's own values, and the settings memory is the same as with none.
- **Launcher**: its three pages drawn with no window.
- **`make GAME=ssf2t check-release`** passes since #190, which no longer compares the two builds' pictures: the build
  with no translated code (the interpreter, as CI builds it) makes the same pictures twice and the same as the
  translated build's with `--cpu interp`, and plays the whole ladder to the ending and the show, but its pictures part
  from the translated build's in the first fight and do not meet again (36,444 of 40,000). The game is timed to the
  instruction by its interrupts: its tasks wait on counters the vblank interrupt steps, and its frames fill the
  processor (above all at the turbo speeds), so where the interrupt is taken (the interpreter between instructions,
  translated code between blocks) changes when a task gives way, small words of RAM first (frame 874, then 1,480, the
  objects' budget) and the random number from frame 2,691 on, and from there the computer's choices. Neither Turbo 1,
  free select, arcade timing nor a processor twice as fast changes that. It is not a fault of the translation (`check`
  and `check-blocks`, which take interrupts as translated code does, agree exactly); in most other games the frames are
  not full enough for it to show (Alpha and Alpha 2's ladders part too).

- **The training aids above** (2026-11-27): with them off, the ladder (40,000 frames), the versus movie and two
  training sessions (widescreen, and the aids) give the hash logs of a build of main, byte for byte, in all seven games
  that build these sources (and in Hyper Street Fighter II); the fleet worker's queue was stuck (11 jobs queued
  since 2026-11-18), so every check ran on this Mac. Each was seen working in headless sessions: fixed damage
  (one damage every time), the guard (always: nothing hits; standing only: a crouching kick hits; crouching only: a
  jump kick hits), softening (half the damage), the stun mash (stunned for a third as long), the computer (playing,
  then a person again when switched off), the answer with a recording (on the first frame it can act), the throw
  range's boxes in pictures.

## Not yet

- The other characters' endings in the music's names, and continue and game over.
- Nobody has played it by hand, heard its sound, or seen it in a window.
