# Hyper Street Fighter II: progress

The ROM set is `hsf2`, *Hyper Street Fighter II: The Anniversary Edition (USA 040202)*, the parent set, the one
Fightcade and FBNeo's training mode (peon2/fbneo-training-mode, `games/hsf2`) play. Every file's CRC32 was checked
against MAME's `cps2.cpp`, the key included. It came from the Internet Archive's VintageArcade item (it was on none of
the user's devices). The sound driver is Capcom's 1.06b (`hs2.01`), run translated. The program fills all eight
sockets (4 MB) and the graphics one row of four 8 MB files.

## Where it stands

- **Native**: the 68000 program translated by `tools/recomp.py` to `code_end` 400000 (the whole program: the
  characters' own routines run to the end of the ROM). The Z80 program translated. No emulator is linked.
- **Rewritten as readable C++** (`src/widescreen.cpp`, checked by `check_rewrites.sh`): the camera's place across
  (0D9CCC) and up (0D9E18), and keeping a player inside the picture (00443E): Super Turbo's rewrites, its routines
  being these instruction for instruction but for where their variables went.
- **Features**: free play (the game's own), straight to the title, the Board page, cheats, the custom soundtrack's
  names, the training session and its aids, the Alternative modes page (attract mode, the stage viewer, the model
  viewer, survival, endless battle, ring out, the tag team battle in both styles), custom colours and the colour
  editor (the Super Turbo version's), widescreen and invisible walls, Akuma with no code, the difficulty fix, the
  other regions' sets (`hsf2a`, `hsf2j`).

## How it was found: Super Turbo's program lined up with this one

Hyper's program grew out of Super Turbo's (games/ssf2t, set `ssf2xjr1`). Its listing (`work/hsf2/program.lst`) and Super
Turbo's were lined up instruction by instruction: each run of six instructions alike in both (numbers left out) and
found once in each anchored a pair, extended both ways while the instructions stayed alike (66,000 of Super Turbo's
109,000 instructions paired). Each pair then said where a routine went (Super Turbo's address, Hyper's), and the
numbers that differed in an instruction alike in shape said where a variable went: a displacement from A5 (or an
address in RAM) read by the same instruction in both is the same variable, counted over every pair (the player's
record at A5+44E there and A5+33C here: 29 instructions; the stage word, 28). The same for the offsets into records
(a player's record is laid out alike to +2B4, read by the same instructions; the bytes after it moved). Every fact
taken that way was looked at again here (`src/hsf2.h` says how each was found). The game's own parts (the menus between
the fights, the versions, the show) were traced here. The tools were scratch scripts, not kept.

## What was found, and how

All in `src/hsf2.h` with the routine or the experiment each came from. A5 is FF8000.

- **Tasks**: the program is a few tasks taking turns (the kernel at 0013C0 starts one in the table at A5-7D7E, 0x20 a
  slot; its first byte is not zero while it runs). Slot 0 is the board's start, 0x20 the show (008476), 0x40 the
  title with credits in, 0xA0 the game (009D40, when Start is pressed). Each keeps its step in the same words from A5,
  so the slot says what is going on. Read from RAM dumps of each.
- **Steps** (the game's task): FF8000 2 while the players choose (the speed, the version, the character), 4 during a
  match, 8 once the one player has lost (continue). In a match FF8004 is the match's step: 2-4 the versus screen, 6 a
  round's introduction, 8 the round, 0A its end, 0C the next round's start; in 8, FF8008 is 0 while the round is called
  ("FIGHT", about 120 frames, the players' routines not run) and 2 while it is fought. Taking 8 alone for "fought" broke
  the ring out: the next stage's match was taken as ready during its call, and its players, not running, never took the
  winner in, so the winner stood on the old stage until the picture cut away. Super Turbo has them one word lower. Traced
  with `--watch FF8000-FF800F` through a one-player game and a two-player one.
- **The show** (its task): FF8000 2 and 4 the anniversary's mark and Capcom's, 6 the introduction (the old games'
  titles, then this one's: its own step is 0x20 as the title comes in, frame 2501), 8 the health warning, 0A the
  QSound notice, 0C a demonstration fight, 0E the rankings; the show begins at frame 837. Pictures every 40 frames.
- **Credits** FF830A, **players in** FF8316 (dumps after coins and Starts).
- **Free play**: the coin mode (FF82C4) is read at the board's start (00762A, the record from A5-62F2 copied at
  0076F8); mode 0x24, which the test menu never offers, sets FF82C0, tested wherever credits are counted, and the coin
  words FF829E/FF82A6 come from the table at 007912 (0101 for 0x24). A new board's mode is 0x12, two coins a credit.
- **The board's settings**: the test menu's System Configuration (third row of the older menu; no System/Game pages),
  each option stepped with Right and the variable read after each press: difficulty FF82BC (0-7: 1-8; 3, 4/medium),
  continue FF82BD (on), demo sound FF82BE (on), game speed FF82C2 (0-3 turbo 0-3, 4 free select; free select), max
  round FF82C6 (0 four, 1 six, 2 two; four). The names read from pictures of the rows. The hook is 008374, where the
  board start's call of 00762A returns; a game begins at 00B1AE.
- **Clock** FF8BFC, BCD (FBNeo's script), the frames of each second at FF8BFD: 40 a second, taken at 00DA76.
- **Players**: records at FF833C and FF873C, 0x400 apart (cleared 0x300 bytes each at 0039E8 at every round: the
  bytes from +300 are the match's). To +2B4 Super Turbo's layout. After it: +301 side, +328 a person, +32A the
  **version** (00 Super Turbo, 02 Super, 04 World Warrior, 06 Champion Edition, 08 Hyper Fighting: the PL TYPE cursor,
  +90, through the table at 00C902; each chosen in turn and read back), +32B the character (Super Turbo's numbers; 16
  Akuma), +32C the colours (0-5 the button, 6 Start, 7 a button held), +344 the opponent's record, +349 rounds won,
  +34A the opponent's character, +8C chosen (00C364). +310/+312 the buttons now and before.
- **Damage**: 0C26A0 (a player's attack) and 0C2C90 (a thrown thing's), D6 off A2; throws D4 off A3 at 0049AE and
  004A52, or put by at +78 (004AFE); and two more throws' at 004E3E and 004F4C (Super Turbo has them too, but its
  made-up play never reached them), hooked at 004E18 and 004F2E where D4 is final. Found as the writers of player 1's
  health over made-up ladders with no damage: with the hooks only the hits' routines (writing nothing off) and the
  round's set-up write it, three seeds of 30,000 frames.
- **Hits**: a hit decided at 0C26D4 (0C2CDA for thrown things), A2 the victim. The boxes (0C38FE-0C3968, 0C398E) are
  words now: across, up, half width, half height, eight bytes a box (0x14 an attack's), from tables at +0 head, +2 body,
  +4 legs, +6 a fourth tested only after one of those is hit, +8 attack, +A push; Super Turbo's were bytes. Drawn in
  training they sit on the fighters.
- **The player's step**: 066BBE (a person's) and 06A246 (the computer's); a player's routine begins at 066964.
- **Stun**: +5E built up, +5C its timer; stunned at the table at 06D830 by +2B1.
- **Stage**: FF8B64 (FBNeo's script writes it), written by the choosing at 003F9A; the match's first step is 00B1FA
  and the stage is set up at 0DA446.
- **Sound queue**: FF0D04 (A5-72FC), its next place FF0D00 (006ECC puts one in).
- **Camera**: record FF8CBC, across FF8CC2, up FF8CC6, ends A5+B50/B52 across and B54/B56 up; the camera up is held by
  FF8D7E, FF8C11 (the round's end) and FF8C0E (a super's flash).
- **The round's result** FF8C03: 1 player 1, 2 player 2, FF a draw. The round goes to the player at A6 at 00D666; a
  time over compares the healths at 00D5F8.
- **Objects by colour set**: hiding each set in turn along the ladder, a picture every 150 frames: player 1 01-05,
  player 2 06-0A (0C3DF4 copies their palettes to 900020 and 9000C0), shadows 18, the panel 00, 0C, 0D, 0E, 13, 15, 16,
  17, 19, the stage's pieces the rest (0B, 1A-1F seen).
- **Colours**: a table at 0C3E70 by version, then a long a character, then a word a choice: eight sets for each Super
  Turbo character (Akuma two), two (punch, kick) for the old versions; five palettes a set, from 0C3F9E to 0CD036.
- **Animations**: a table at 0056EE by version, a long a character (the Super Turbo version's 17th Akuma's, 18th the
  computer's Akuma), each the list itself (005462 reads it through A4: the data view); frames are Super Turbo's.
- **Akuma**: the game's code (the cursor held on a series of characters, ending on Ryu or Ken: 00C3FA on) sets +88;
  then the three punches pressed together within half a second of choosing make the player character 16 with
  colours 0 (00C2CE). The feature does that itself when the three punches chose the character. His face and name on
  the panel are blank either way (the game's own path, the flag poked, gives the same).
- **Music**: the table at 0x9006 of `hs2.01`, 0x280 entries. Super Turbo's numbers for the stages, their faster
  versions (16 on), the final fight, the endings and the staff roll, whichever versions fight; the introduction 33, the
  choosing 34, versus 35, winner 36, continue 37.
- **Random number** FF0CB2 (A5-734E: 003276 steps it).
- **The computer**: its step 06A142 (called at 06697E, returning to 066986); its set-up 06BD16, 06C922, 06A1C8, with
  two words a character from 316E3E.
- **Difficulty by region** (`src/difficulty.cpp`): the computer's difficulty reads a row from a table at 005CB6, the
  row the board's difficulty (FF82BC) times 64 plus a stage index times four. A0 holds the table: 0x315E00 (005C8A),
  which a block at 005C90-005CB4 swaps for 0x316000 when the region word FF82FA is 2 (USA), 4, 6 or 8 (Asia), not for
  Japan's 0 -- so those regions do not follow the setting. bankbank's ROM patch NOPs that block; the **Difficulty fix**
  feature does the same by putting 0x315E00 back in A0 at 005CB6 when it is on (off by default). Only A0 is touched, so
  it is identical to the patch; nothing is kept between frames, so no save state holds it, and online the host's choice
  is used as any feature's is.
- **The tag style's swap** (`src/livetag.cpp`): Super Turbo's, with the round's 0x300 bytes cleared but for what a
  round's start alone puts in; the objects the set-up makes (kinds 16 and 1A) given back to the pool (60 of 0xC0 from
  FFA754, its list A5+1342, its count A5+132B); the panel made again through 011B48; a character the player's version
  has not got taken in the Super Turbo version.
- **Other regions**: `hsf2a` (Asia 040202) and `hsf2j` (Japan 040202) differ from this set's program in one word, the
  region at 0012A0, which 00126A reads from the ROM as data: they run on this translation as they are, and online the
  host sends that word. `hsf2j1` (Japan 031222) is an earlier revision: not taken.

- **The Alphas' training aids**, as Super Turbo's (`games/ssf2t/PROGRESS.md`), with this game's places, all hooked at
  places that already began blocks of translated code: the random part of a hit's damage (0C2FD4 on: 0C2FEE, 0C2FF0)
  and a throw's (004C82, 004C84); the throw test's boxes (00478C on, Super Turbo's offsets); the stun (+124, 068EC2);
  softening (004870: only when the one thrown and the thrower are both of Super Turbo's version, +2BC and +2BD); the
  guard decision (0C3044 on), which goes by the attacker's version: Super Turbo's way for Super Turbo's and Super's
  (a jump table by the attack's +0F at 0C307E, its routines 0C310C most, 0C3124 standing only, 0C312A and 0C3132
  crouching only, 0C3130 either), the classic games' for the World Warrior's, Champion Edition's and Hyper Fighting's
  (0C3138: the attack's +0B through a row of four bytes for each pose, the rows at 0C31B4). An attack box here keeps
  those two at +0F and +0B (Super Turbo's +0B and +07).

## Checks

Run on 2026-11-03, macOS (Apple silicon), each alone:

- `make GAME=hsf2 check FRAMES=40000 MOVIE=games/hsf2/movies/ladder.txt ARGS="--features free_play,no_damage,one_hit"`:
  translated code does what the interpreter does, two runs alike. The ladder movie (Super Turbo's Ryu) reaches the
  final fight, Ryu's ending and the staff roll.
- `make GAME=hsf2 check FRAMES=12000 MOVIE=games/hsf2/movies/versus.txt ARGS="--features free_play"`: the same.
- `make GAME=hsf2 check-blocks` over the ladder: 363,255,791 translated blocks and 189,997,474 of the sound driver's
  checked against the interpreter, no difference.
- `make GAME=hsf2 check-release`: the interpreted build makes the same pictures twice, the same as the translated
  build's with `--cpu interp`, and the translated build's are `--cpu blocks`'s; the interpreted build's pictures part
  from the translated build's in 31 frames of the 40,000 (interrupts taken between instructions, not blocks), and both
  play the whole ladder.
- `make GAME=hsf2 check-rewrites`: CameraAcross, CameraUp and KeepIn over the versus, ladder and two made-up two-player
  movies (13,000 to 55,000 calls each), none differing. Their cycles are this game's averages (388, 324, 199: Super
  Turbo's 418, 322 and 215 charged 33, 0 and 16 too many here), a few cycles off on average.
- `make GAME=hsf2 check-widescreen`: all sixteen stages painted to both ends in the wide picture (no unpainted column),
  the game's memory with invisible walls the board's at every 4,000 frames of the ladder, and the ladder in widescreen
  with nothing in the interpreter.
- `make GAME=hsf2 check-rollback` and `check-online`: the same over 3,000 frames with 400 rollbacks (the game's own
  features; all the play features; the frames run again not drawn), and two copies playing each other the same as an
  offline replay. With `JOIN_ROM` the Asia and the Japan sets join a USA host and play the same (after the table's
  words were added to `online_words`: before, the Asia set came apart at frame 2,400).
- `make check`: the audit, the no-game build and the launchers.
- **No interpreter fallback**: 0 instructions in the interpreter over the ladder movie, the versus movie, made-up
  one-player ladders (seeds 41, 42, 90) and two-player games (43, 44), 20,000 frames of attract mode (each show), the
  test menu with made-up input, the sound test (seeds 1-12), training, survival, endless battle, ring out (six seeds),
  the tag team battle in both styles (eighteen seeds), the stage and model viewers and the ladder in widescreen, after
  the 27 places those runs reached that made-up play had not were added to the entries.
- **Features off = no sources**: `--cpu interp --no-rewrites --features -free_play` over 8,000 frames of the versus
  movie, and the same with `src/` moved aside: byte-identical hash logs.
- **Save state**: made-up play saved at 3,000 and loaded with the same input after it: frames 3,001-6,000 alike.
- **Run-ahead**: 1 and 2 frames give the hash log of 0, over the versus movie and a training session.
- **Board**: every choice of the five read back from its variable at frame 299; with choices made the test menu shows
  the board's own values, and the settings memory is the same as with none.
- **Other regions**: `hsf2a` and `hsf2j` boot with their own region's screen (ASIA, JAPAN), the region word holding 8
  and 0 (the USA's 2), and play the versus movie with nothing in the interpreter; the USA set's hash log is unchanged.
- **Hand-picked**: free play from the show; straight to the title (hash logs the same with and without); the cheats
  (no damage, one-hit knock-outs up the whole ladder, the clock held); training's hit boxes on the fighters; attract
  mode's three shows; Akuma with the switch (and without it, none); the camera zoom on one fighter and on both.
- **Launcher**: its three pages drawn with no window.

- **The training aids above** (2026-11-27): with them off, the ladder, the versus movie and two training sessions give a
  build of main's hash logs byte for byte. Seen working in headless sessions, Super Turbo's version and (player 1) Hyper
  Fighting's: fixed damage, the guard (standing only lets a crouching kick through, crouching only a jump kick, in both
  ways of deciding), softening (Super Turbo's versions only, as the game has it), the stun mash, the computer, the answer
  with a recording.

## Not yet

- The other characters' endings in the music's names.
- Nobody has played it by hand, heard its sound, or seen it in a window.
