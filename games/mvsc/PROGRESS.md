# Marvel vs. Capcom: progress

The European arcade release of 1998-01-23 (ROM set `mvsc`, the parent set), *Marvel Vs. Capcom: Clash of Super
Heroes*, on Capcom's CPS-2 board. Started 2026-10-28 (issue #135). The last of the Marvel games on the board, made from
Marvel Super Heroes vs. Street Fighter's program, on the Marvel games' shared engine (`fight/marvel.h`): each side is a
team of two characters, swapped in and out during the fight, with a special partner to call in.

## The ROM set

`mvsc` was chosen because it is the set competitive play uses: FBNeo's (and so Fightcade's) set of that name, the one
the FBNeo training-mode scripts are written for. Sets of the same date (Asia, Brazil, Hispanic, Japan, USA 980123) differ
in their region; older revisions exist (971222 USA, 980112). The owner's own copy (a merged `mvsc.zip` among their
MiSTer backups, on the Windows PC) has the same files as the Internet Archive's (item VintageArcade, `mvsc.7z`); all 21
files, the key included, match MAME's CRC32s (`src/mame/capcom/cps2.cpp`, `ROM_START( mvsc )`, the good dump of
`mvce.04a`).

The graphics are two rows of four 4 MB sockets. Six buttons a player (MAME's `cps2_2p6b`). Sound driver 1.30.

## Where it stands (2026-10-28)

- Boots and plays on the toolkit's own machine: the warning, the notices, the introduction, the title, the attract
  mode's fights, the character choice (two characters, the mode, the speed, then the special partner), versus fights,
  the whole one-player ladder to Onslaught and the ending, the staff roll, game over and the show again (the ladder
  movie: Onslaught by frame 15,000, game over by 22,500). Seen in pictures only: nobody has played it in a window or
  listened to it yet.
- 68000 program: translated to C++ (183,126 instructions from 1,513 entries: `tools/portmap.py` from Marvel Super Heroes
  vs. Street Fighter's, `make explore`, and the runs below with `--record-entries`). No instruction falls back to the
  interpreter over runs `explore` never made: the ladder movie (and in widescreen), made-up ladders, versus, two-player
  play, 20,000 frames of attract, the test menu, the sound test, and every mode (training, attract mode three ways, the
  stage and model viewers, the colour editor, survival, endless battle and its scroll, ring out, tag-team co-op and 2 vs
  2). Six places the modes' other machines reach (the camera's jump tables, and a return the ring out comes back to)
  were added by hand, as for Marvel Super Heroes vs. Street Fighter: those machines record no entries. The interpreter
  stays only as the fallback for a place no run has reached.
- Sound: driver 1.30, translated. `--stats` counts one instruction of the sound driver with no translation in every
  run, as the other games' builds do: the toolkit's, not this game's.
- The features every game has: free play (Enhanced; the game's own), straight to the title, the board's settings
  (the Board page), cheats (no damage for each side, one-hit knockouts, infinite time), save states, run-ahead,
  arcade timing, a custom soundtrack (its music named), the in-game menu and the launcher.
- Everything the other Marvel games have: a training mode on the toolkit's session (`fight/session.h`: hit boxes,
  frame data, its meter and log, input display, combo readout, health refills, the dummy; and the game's own gauge
  refill and partner's refill), the Alternative modes page (`fight/alternative.h`: attract mode three ways, the stage
  viewer), the fighting modes (`fight/battle.h`: survival, endless battle, ring out), the model viewer (`model/`),
  custom colours and the colour editor (`fight/colours.h`), widescreen and invisible walls, the zoom, the hidden
  characters with no code, and tag-team co-op and 2 vs 2 (`fight/tag.h`).
- Not added, as the user's rules say: the tag team battle (the game has its own tag team), and the CPU coach (the
  game's computer plays every mode itself).
- **On the Marvel games' shared engine** (`fight/marvel.h`), with nothing new in it: the game keeps its tag team as
  Marvel Super Heroes vs. Street Fighter does (records of their own, `playerAt`) and its camera the same way (a copy of
  its place, `cameraWorkX`/`Y`), so `src/engine.cpp` is a description of where this game keeps the same things; the
  frame copy is the toolkit's `marvel::CopyFrameBoxSets` (001DFC, the same instructions).
- No readable rewrites: what the features change is done in hooks that run the game's own code unless a feature is
  on, each a few lines.

## The game's memory and routines (found by experiment)

Variables sit below A5 = FF8000; the facts are in `src/mvsc.h`, each with how it was found. `tools/portmap.py --from`
Marvel Super Heroes vs. Street Fighter's work folder (with its guessed entries) `--to work/mvsc` placed 51,385 of
155,293 of that game's instructions here and named 9,587 places, which gave most of what is below a first guess; each
was then confirmed here. In short, Marvel Super Heroes vs. Street Fighter's variables 0x800 lower again (FF4800 there
is FF4000 here), and the players' records 0x20 further on from +0x250 (health +270, and so on) and 0x10 from +0xA0
(the action +B0, the push box +B4, held at the edge +B9):

- **Where the game is**: FF4000, a byte, 8 while a round is fought; FF4100 set once a game is played, FF4101 who is in;
  FF4104, the game's step (4 choosing, 8 a match), FF4106 the match's. The show runs as a task (its record at FF2666,
  step byte FF266A); the title's scene takes its first step at 0D1302 (frame 1,420 from switching on).
- **Credits** FF4499; **free play** the game's own coin mode 12 (FF44A5), which sets FF449B (007F36).
- **The board's settings**: the record at FFF648 is read into variables at the board's start (007E7A, called from
  000FAC; the hook at its return, 000FB2, puts the choices in). Each option was changed in the test menu, saved, and the
  variables read after the next start; the names read from pictures. The game has a tenth, Flickering (+17, FFF9B8);
  the event sets continue and join-in off.
- **Players**: four records of 0x400 from FF3000 (player 1's first character, player 2's, then their second ones); as in
  Marvel Super Heroes vs. Street Fighter a tag does not swap them, and the longs at FF40C8 and FF40D0 are each side's
  character on point (traced over the tag-team movies). +0C/+10 where a character stands, +4B facing, +52 the character,
  +6C the box table, +70-+7C the box numbers, +82/+83 attacks hit / can be hit, +85 hit-stop, +B0 the action, +B4 the
  push box (the FBNeo hitbox script's places for this set), +270 health (`sub.w d5,0x270(a6)` at 01B74E and 01BEA0),
  +272/+274 the gauge, +277 the colour choice, +27A what comes back while resting, +280 which pick it is, +281 1 while
  it rests, +284 the partner's record, +288 its palettes.
- **Reading the panel**: each record's step (00D0D8) reads its side's panel at 01366C when a person plays it (+03 not 0,
  01367E): the word at FF444C or FF444E through the table at 0136F4 (01368E), kept as held (+D0, the last at +D2) and
  pressed. The words are made at 000D2E: the stick and punches in the low byte, the kicks in bits 8-10.
- **Duo Team Attack** (down, down-back, back with a strong punch and a strong kick): its length is set at 016E02, a
  word from the table at 016E2A by the gauge's levels (0, 192, 384, 576) and half the bar added, into the record's +50
  (the levels and the bar go to nothing); then each side's time left, FF4034 and FF4036, is taken one off a step at
  015F68 by the routine at 015F34, which clears the side's bit of FF4038 and sets it again (015F6C) while there is time
  and both characters stand. At its end (015FA0) 015FF0 decides which of the two stays in by what each is doing (both
  records' first bytes, +B0 against 40 and 2A, +16B, +06, +B1), the checks the Dreamcast's game makes at the same place.
  FF4488 counts the game's steps (00AB60; seven steps in six frames in the movie's fight), and the game does some things
  every other step by its bit 0 (00CAA4). Traced over `movies/cross_fever.txt` (576 from three levels; 388 and 387 from
  two levels and a little bar in a fight that filled the gauges by itself); Cross Fever (`crossfever.cpp`) hooks 01368E
  and 015F68.
- **The gauge**: 01CD1E adds D0 to A6's side's gauge (+272 the bar, 0x90 a level; +274 the levels, three at most) while
  a round is fought, the side is not in its Duo Team Attack and the record's +11C is 0; a new level's sound goes through
  D4D86 (the table at 01CDBE), and the partner's record is given the same (01CDEC). 01CEAA, once a step of a match (from
  00CEE2, the only call), gives the resting characters their health back; Cross Fever's gauge is added there.
- **Moves and taunt**: 018DB6 looks for a record's moves by its character (from 018DD6, the tables at 018DF0 and
  018E4C) unless FF400F has bit 2 or 4, the record is off point outside its side's Duo Team Attack, or its side's bit of
  FF4031 is set (014ACC sets it for a tag on its way). 00D4AC, while a round is fought, clears the controls of a record
  off point outside the attack and goes past its reading. Start is the taunt (01A196: the record's +26E against its last
  step's +26F, the side's bit).
- **The starting order**: at the step from the choosing to the match, 00BB2C swaps a side's picks (FF40B0 and FF40B2,
  player 2's FF40B4 and FF40B6) and their colours (FF40C4 on) when FF4100 is above 0, the side is in and its panel has
  the three punches held. The round's records are made from the picks, +280 by place, so after a swap +280 does not say
  who chose the character (`tag.cpp` keeps whether each side swapped). While a side chooses, 00BD28 keeps the character
  under its cursor as the first pick.
- **The Dreamcast's Cross Fever** (its 0.BIN, read with a disassembler; the board's routines matched by the work each
  does): its flag (8C4EC06C) is tested in 50 places, and those that change a fight are what `crossfever.cpp` does. Each
  character's stream is picked by its +275 (the board's +281: 8C029E40); each side's selector (8C4EC06D, 8C4EC06E) is
  turned over wherever a fight swaps the bytes: a tag (8C02CAA2), the partner coming in for a character knocked out
  (8C02CBFC), the end of a Duo Team Attack (8C02D234, 8C02D294) and a Variable Counter, at its landing (8C02D5EE, the
  board's 0165C6; the bytes are swapped later, at 8C02636A, while neither character reads its controls); and where a
  side starts with its second character (8C03AA0C, 8C03AB3C). Every other write of a +275 sets the four by place as a
  match's records are made (8C0291FE, 8C029262, 8C0293E8), both selectors having gone back to 0 on the way from the
  last match to the choosing (8C054692; a match is one round), or is Onslaught's (8C02CC6C, from his own code at
  8C05FAA4: his second form coming in; and 8C02D3BA), whose fight Cross Fever, a versus mode, never has. So each stream
  stays with its person through a match and into the next, as each record's person does here. The Duo Team Attack's
  time is taken off on odd steps (8C02CF20). Every seventh step of a match the gauge routine (8C035DA2, the board's
  01CD1E) is called with 1 for each side's first character (8C03606A): a count of its own (8C1E24A8) goes up each time
  8C036052 runs (once a step of a match from its intro on, 8C0221E8's call: the board's 01CEAA from 00CEE2) and goes
  back to 0 at seven, in Cross Fever only (it is never cleared otherwise, so the first add comes at once). The moves of
  a character off point are passed by only for its side's held bit (8C03025C, the board's 018DB6), and the Dreamcast's
  step (8C024106, the board's 00D1EA) has no clearing of its controls (the board's 00D4AC). The taunt tests the point
  person's controller's Start as held (8C032674); in the Dreamcast's versus modes (8C4EBBAC set) the record's held word
  must also be that player's taunt button alone (8C4EBF9A, light punch unless changed), as Start alone pauses the
  Dreamcast's game, in Cross Fever as in the rest. The rest: the four players' choosing and menus, what comes after a
  match, the controllers' rumble, a pause when a controller is pulled out, and places shared with the Dreamcast's other
  modes (its versus and training): each of the tests that never ran in a fight there is one of these, read one by one.
- **Characters**: the word +52, each number put in player 1's picks and seen in a fight with its name: 02 War Machine,
  04 Captain America, 06 Hulk, 08 Wolverine, 0A Gambit, 0C Venom, 0E Spider-Man, 10 Roll, 12 Ryu, 14 Captain Commando,
  16 Chun-Li, 18 Jin, 1A Zangief, 1C Strider Hiryu, 1E Morrigan, 20 Mega Man, 24 Red Venom, 26 Orange Hulk, 28 Gold War
  Machine, 2A Shadow Lady, 2C Lilith; 22 is Onslaught (player 2's picks in its fight) and 00 its second form, which put
  in the picks fight as Ryu (the game makes their fights its own way), so they are not in the model viewer; 2E on reset
  the board.
- **Choosing**: a 0x30 record a player from FF4180 (player 2's at FF41B0), +02 the side, +0C the character under the
  cursor; its first byte steps to 7 once the characters, the mode and the speed are chosen, then on for the special
  partner. A character is taken at 00BCDA (the hidden characters' hook). Picks: FF40B0 on, as before.
- **Stage**: FF4113 (0-9, named from pictures); taken at 00C2CA from a table (0E324C, or 0E326C) by a count; Onslaught's
  fights are on 9 and 10 (10, space, keeps no camera record, and 11 is none: not offered); the hook at 00C2F8 puts a
  mode's stage and characters in.
- **Camera**: as Marvel Super Heroes vs. Street Fighter's: one of three records by stage (the hitbox script's table),
  its step (0CB2A2 across, 0CB4E2 down) working on a copy (+76/+78), moved the way FF4408 and FF4409 say, kept between
  +68/+6A and +6C/+6E (0CDBB6, 0CDBDA) and put back (0CDBCE, 0CDBF2). The ends across are -64 to 320 on most stages,
  -64 to 304 in the bathhouse and 960 to 1344 in Onslaught's city; the floor is the camera's lowest place + 0xD8. A
  character is kept inside the picture at 013AD6 (the planes' table at 02563A).
- **Panel and objects** (from the object lists of rounds on every stage): colour sets 00-0B the fighters (player 1's
  first character 00, player 2's 03, their second characters 06 and 09: 013186 on), 0E the shadows, 0D, 13-15 and
  1E-1F the panel, 16-1D the stage's own pieces; the partners' marks on the panel are tiles 1CC80-1CCFF.
- **Colours**: a long a character at 1C7268, two sets of three palettes, 0x60 apart (Onslaught's two forms share one).
  PalMod's file is `mvc.06a`.
- **Animations**: groups of words of offsets from a list a character at 0E676C; frames of 0x10 bytes, copied into the
  record by 001DFC.
- **Music**: each character's theme is 0C and its number (word / 2); the screen before a match plays player 2's first
  character's, the round its second's (traced over the ladder with `CPS2RECOMP_SOUNDTRACK_LOG` and the characters'
  words). 0C is Onslaught's fight's. Others: 01 the opening, 02 player select, 04 before each stage, 05 the winner, 07
  the QSound logo, 0B before Onslaught, 2E after it, 20 the ending, 1D the staff roll, 0A game over, 2F the rankings.
- **The round's result**: FF4005. **The sound queue**: FF27D8, the next place at FF27D2 (0D4C7A). **The random
  number**: a long divs then the number kept at 001932; a hook at 001930 starts a block there.

Facts were confirmed with the help of the FBNeo training mode's scripts (peon2/fbneo-training-mode: `games/mvsc`,
`hitboxes/marvel-hitboxes.lua`), each found again here; no code was taken.

## The checks

(2026-10-28, all on the Mac.)

- `make GAME=mvsc check FRAMES=40000 MOVIE=games/mvsc/movies/ladder.txt ARGS="--features free_play,no_damage,one_hit"`:
  translated code does what the interpreter does over the whole ladder; two runs alike.
- `make GAME=mvsc check FRAMES=12000 MOVIE=games/mvsc/movies/versus.txt`: the same over versus.
- `make GAME=mvsc check-blocks MOVIE=games/mvsc/movies/ladder.txt FRAMES=40000`: every translated block checked against
  the interpreter, and the sound driver's (308,022,572 blocks and 140,046,696 of the sound driver's, no difference).
- `make GAME=mvsc check-release FRAMES=40000`: two runs of the build with no translated code alike; its pictures the
  translated build's but for 18 frames (each one frame early), and the same last picture.
- `make GAME=mvsc check-widescreen`: every one of the ten stages walked to both ends in widescreen, no unpainted
  columns; invisible walls leave the game's memory the board's at every 4,000 frames of the ladder; the ladder in
  widescreen with no instruction left to the interpreter.
- The tag-team movies (X-Men vs. Street Fighter's with the fight 400 frames later, as this game's choosing is longer):
  `make GAME=mvsc check FRAMES=6600 MOVIE=games/mvsc/movies/coop.txt ARGS="--features free_play,tag_team=1 --poke FF3274=03"`
  and `make GAME=mvsc check FRAMES=5600 MOVIE=games/mvsc/movies/two_vs_two.txt ARGS="--features free_play,one_hit,tag_team=2"`:
  translated as interpreted, two runs alike; the point character's long traced through the tags, and the label under
  the bars following it (pictures every 100 frames).
- `make GAME=mvsc check-rollback` and `check-online`: pass (3,000 frames, 940 rollbacks; the online match the same as
  both players' controls replayed). `make check` and `tools/audit.sh` pass.
- By hand: `--stats` over the runs above and every mode (no instruction to the interpreter; the ring out and the
  endless battle run a dozen times each, their choices at random); features off and no sources at all give the same
  `--hash-log` over 6,000 frames of versus (`--cpu interp`); a state saved at frame 3,000 and loaded into a run of the
  same input: frames 3,001-6,000 alike; run-ahead 1 and 2 give run-ahead 0's hash log over 8,000 frames of versus;
  "Straight to the title" on and off give the same hash log; every Board choice read back from its variable (the event
  setting continue and join-in off too), the test menu showing the board's own with choices in, and the settings memory
  the same with and without them; no damage (player 2's team's health stays 0x90 through versus), infinite time (the
  clock at 99), one-hit knockouts (the ladder); the hidden characters (Start held on Chun-Li: Shadow Lady); frame data
  over a training session (`frame_data.csv`); pictures of every mode, the zoom at 2x and on both, the special partner
  called (medium punch and kick), and the launcher's three pages.

Cross Fever and four players online (2026-11-08, on Linux, in a tree after `make explore`):

- `make GAME=mvsc check FRAMES=5740 MOVIE=games/mvsc/movies/cross_fever.txt ARGS="--features free_play,tag_team=2,cross_fever --poke FF3274=03,FF3674=03,FF3A74=03,FF3E74=03"`:
  translated as interpreted, two runs alike; and the ladder, versus, co-op and 2 vs 2 checks above pass again. With
  Cross Fever off, those four movies' hash logs are the same as without these sources (the same entries). `check-blocks`
  over the Cross Fever movie: 41,263,709 blocks and 19,968,999 of the sound driver's, no difference; over a tags scene
  36,197,699 and 17,420,294; over a scene whose side 0 starts with its second character 39,175,304 and 18,651,291; over
  a Variable Counter scene 32,418,731 and 15,455,822; over a fight whose gauges fill by themselves (no pokes) 38,472,136
  and 18,313,522.
- Traced over the Cross Fever movie (`--trace FF3000-FF4FFF`): in both teams' Duo Team Attacks the four records' held
  words are the four people's (P1 001, P2 004, P3 002, P4 008), and each side's time goes down every other step from
  576; with Cross Fever off the partners hold what the character on point does and the time goes down every step.
- Scenes from a state saved at the fight's start (the gauges full), each checked frame by frame from the trace (while a
  round is fought, every record's held word, whenever it changes, is its own person's for that frame; no side's time
  goes down in a frame whose steps were all even) and run again with `--rollback-test 9`, drawn and not (the same hash
  logs): the partner's strong punch and kick while resting (no tag) and then the one on point's (a tag, the newcomer
  moved by its person at once); the resting partner's medium punch and kick (no special partner: FF32C6 stays 4) and
  the one on point's (3); the resting partner's Duo Team Attack (none) and the one on point's; one-hit knockouts in
  both teams' attacks (the knocked-out team's attack ends, the other's goes on, the round won); specials and hypers by
  both partners in the attacks (pictures: the hits counted on both sides); the clock running out in both attacks (the
  round decided, the next one played); one-frame presses by each of the four in the attacks (every one in its
  record's held word on its frame); co-op vs. computer, P2's character moved by P2 in P1's attack, and no gauge added
  to side 0 in it. With Cross Fever, the tags and special partner scenes translated as interpreted.
- The gauge, from the fight's start of the Cross Fever movie without its pokes: both sides' bars one up every seven
  steps (FF4488), copied to the partners, a level from the start in 867 frames (the movie's speed is seven steps in
  six frames), the level's sound in the sound queue for each side; with Cross Fever off, none.
- Beside the Dreamcast's game itself (Flycast with its own BIOS stand-in, four controllers driven by a script, the
  memory read every frame), in a Cross Fever match from the start: both gauges one up every 6 frames (the count at
  8C1E24A8 going round every seven steps from the match's intro on), as here; the Duo Team Attack 36 frames from the
  levels taken to the attack's start, 576 taken off on odd steps over 1,021 frames, the partner walking and doing a
  special by its own controller in it, and at the end, both standing, the one who started it leaving (here 36, 1,023
  and the same one leaving); the resting partner's strong punch and kick, medium punch and kick, and the attack's
  command doing nothing, the one on point's tagging, calling the special partner and starting the attack; in a
  Variable Counter's entrance the partner's held word its own controller's, and six kinds of input by it changing
  nothing; Start alone pausing, and the taunt button with Start, both held, taunting again and again (the partner's
  doing nothing); each match starting at level 1, as the board's does.
- The starting order: player 1 holding the three punches as the choosing ends (00BB2C, the game's step turning to 8)
  starts side 0 with player 3's character, and player 3 moves it (Tag team alone and with Cross Fever); player 1, once
  tagged in, moves their own. Checked frame by frame as above with the swap, and the same straight and rolled back
  (50 rollbacks). Before, player 1 moved player 3's character.
- The taunt: Start held for 600 frames by the person on point taunts once with Tag team alone and every 50 frames or so
  with Cross Fever; the partner's Start does nothing.
- A Variable Counter (back, down-back, down with a strong punch and a strong kick, blocking): while the partner comes
  in, its held word is its own person's (the board clears it); none of twelve kinds of input by that person then
  (specials, hypers, a tag, the special partner, Start, held directions) changed what it did.
- Online, a match of four whose movie fills the gauges by play, with both teams' Duo Team Attacks: the host's and the
  joiner's hash logs the same over 5,697 frames (packets 50 ms late, give or take 25, one in twenty lost), the record
  replayed the same, and in it every check above. The same movie split into one a player and played at four machines
  (one player at each, joining in turn): the four hash logs the same over 5,690 frames, and the match the same as two
  machines of two playing it, hash for hash over 5,693 frames and in its record of the four. (A build that let
  `--movie` be each machine's controllers for this run only: online play refuses movies.)
- A stand-in of the panel hook that gives every record the side's own word: the same hash logs as the board's code
  over versus and 2 vs 2.
- `check-rollback FRAMES=12000`, and with `FEATURES=tag_team=2,cross_fever`: pass (3,801 rollbacks each). The Cross
  Fever movie from a state saved at frame 4,160 (the gauges full: `--poke` and rollback don't mix), straight and with
  `--rollback-test 9`: the same over 5,740 frames (107 rollbacks), drawn and not.
- `check-online` at 3,000 frames: two players (the two the same over 2,996 frames), `FOUR=1` (2 vs 2 and Cross Fever on
  the host, two machines of two: 2,998), `MACHINES=3` (the host's two and two machines of one: 2,991) and `MACHINES=4`
  (four machines of one, players 1 to 4 as they came: 2,987), each with every name and every other machine's line of
  the chat, and the record of the four replayed the same; `ARCADE=1`, alone and with `MACHINES=4` (the match begun after
  243 frames of the host's play: 2,996 and 2,990). Co-op vs. computer online with Cross Fever (one player at each
  machine) the same over 2,995. Two players: the same rollbacks and waits as the build before four machines, the two
  run in turn.
- Four machines by a code, through the introduction service run on this machine (`online/src/index.js` in Node, its
  Durable Objects stood in for in memory): one that joined at the service but never reached the host and one that
  reached it and was killed both had their places given back (the host's `gone`, after 30 seconds and after 5 of
  quiet), one more turned away while the match was full, then the fourth let in and the match of four begun. The
  service alone: 27 checks (a host from before `players` as it was, room, full, the numbers, `gone`, the lobby, the
  same code again).
- A fifth machine turned away ("That match is full."), and a machine of two when one place was left; one leaving in a
  match of four: every machine says who left; one killed: the host says it lost the connection to it, and the other
  two say so. In a window (SDL's offscreen video): one leaving, the host waiting again with its address, the other two
  back by themselves and a new one taking the place, the match again with all four; the in-game menu's Host a match
  with three machines joining (the match from the board's reset; after it the host's own game, still hosted).
- The ladder at 606 and 638 frames a second against 585 and 610 without these sources, the two run in turn.

Four machines, longer and on a worse connection (2026-11-09; each joining machine started once the host had the one
before, as `check-online` now does: started half a second and 0.7 apart, a host slow to start put them out of order):

- Made-up play at each of the four over 15,000 frames, with 2 vs 2, Cross Fever and the rest that changes play on
  (packets 50 ms late, give or take 25, one in twenty lost): two matches and the choosing between them, 80 changes of
  who is on point; the four hash logs the same over 14,989 frames and the host's record of the four replayed the same.
  On the replay's trace, while a fight is on, every record's held word, whenever it changed, was its own person's
  (45,872 frames of the four records; in 140 of them with the stick taken off, as the board does to any character
  stopped at the screen's edge: 013CE2, from 013B4C).
- Packets 140 ms late, give or take 80, and 15 in 100 lost (a ping of 300 to 400 ms: the game slows, waiting, rather
  than run more than 8 frames on guesses): made-up play over 6,000 frames, the four the same over 5,987 (1,710
  rollbacks at the host) and the record replayed the same, with the held words as above (62 changes of who is on
  point); and the Cross Fever movie split by player (with the build above that lets `--movie` be each machine's
  controllers), the same over 5,689 frames, both teams' Duo Team Attacks (455 and 794 frames) with every record's held
  word its own person's and the time taken off on odd steps only.
- That movie with player 1 holding the three punches as the choosing ends: side 0 starts with player 3's character, at
  all four machines the same over 5,688 frames, each record moved by the person who chose its character (player 3's on
  point first); player 1's Duo Team Attack command, from the partner now, does nothing, and side 1's goes as before.

## Not checked by a person

Nobody has played it in a window, used a gamepad with it, or listened to it: every check above is headless. The
custom soundtrack files, the dummy's record and replay, and the arcade timing have not been tried by hand. Roll's and
Zangief's themes are named by their numbers, not heard. Attract mode's "fights only" shows parts of the introduction
between the fights too: this game's show mixes them more than the earlier games'.

## Next

- Online play is the toolkit's; this game has not been tried over a network (four players at up to four machines
  neither: localhost only, and the introduction service's change run in Node, not on Cloudflare).
- Cross Fever: the Dreamcast's four-player choosing and menus (here the board's choosing, a person a pick).
