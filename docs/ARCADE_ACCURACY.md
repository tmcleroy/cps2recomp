# How close the arcade timing is to the board

The port's **arcade timing** (**Slowdown → Arcade perfect**, `slowdown=0`; the rules are in [TIMING.md](TIMING.md)) takes from the
68000 the clocks a CPS-2 board takes, so the games slow down where and as much as the board does. This page says
what it was compared with, what that comparison shows and leaves open, and what is still to do before the port can
be called arcade perfect. The work still to do is tracked in [#52](https://github.com/tmcleroy/cps2recomp/issues/52).

## What "arcade perfect" would mean here

For the same input from power-on, the port and a board would show:

1. the same game state at every frame: the 68000's work RAM, and the video and object RAM it writes;
2. the same picture at every frame, slowdown included;
3. the same sound, at the same moments;
4. the same frame rate and the same input-to-picture delay.

This needs to hold for every game the port carries, and with the port's own additions switched on where they claim
to keep the board's timing.

## The comparison that was done

**The reference.** [jtcps2](https://github.com/jotego/jtcores) (jotego's CPS-2 core for MiSTer), simulated
clock by clock with Verilator. Its 68000 wait states follow Loic Petit's study of the DL-1827
([petitl.fr/cps2](https://petitl.fr/cps2/DL-1827.pdf)), and jotego matched its video DMA to measurements of a real
board. It is a model of the board, not the board.

**The harness.** This is kept outside the repository, in a local checkout of jtcores at `bfced64` with four
small changes:

- `jtframe/verilator/test.cpp` dumps the 68000's 64 KB of work RAM at the start of every vertical blank, and logs
  that frame's DMA and wait clocks. `sdram.h` gets the `peek` this needs.
- `jtframe_68kdtack_cen.v`: the wait counter's width goes from 6 bits to 16. It overflowed during a long SDRAM hold
  in the simulation, which is something a board never does.
- `jtcps15_sound.v` (`DSP_ALWAYS_READY`): the QSound DSP always reports ready. The simulation can't run the DSP
  without the chip's own internal program, which isn't in any ROM set, and the sound program would otherwise wait
  for it forever.

The port was run on the same input script with the then-current `--features arcade_timing` (now `--features slowdown=0`), dumping its work RAM at the same point
of every frame. A small script (`track.py`) pairs each reference frame with an identical frame of the port's.
Pairs have to sit within 30 frames of the previous match. The stack area (`FF7C00-FF7FFF`) and two bytes
(`FF043C`, `FFDF63`) are left out of the comparison. `diffat.py` lists the bytes that differ at a given frame.

**The run.** Street Fighter Alpha 2 (`sfa2u`), 5,671 frames from power-on: the board's tests, the attract mode, two
real coins (no free play), Start, character select, a whole fight against the CPU to the knockout, and after.

### What it showed

| | Reference frames matched | First lasting divergence |
|---|---|---|
| Arcade timing off | 904 of 5,671 | frame 913 (attract mode, the fighters' introduction) |
| Arcade timing on | **5,639 of 5,671** | none |

- **Off:** the port's 68000 gets through about 1.35 times the board's work in a frame. The board's memory test
  takes 14 frames a step on the reference and 10 or 11 on the port.
- **On:**
  - From frame 80 to the end, the port's state follows the reference at a fixed offset of 11 frames (the port reaches each
    state 11 frames sooner), and it never drifts. The offset builds up during the boot sequence (frames 70 to 80), and its cause was not looked into.
  - The 32 frames that don't match are isolated frames: eight during boot, and 24 during the fight. In
    each one, a routine's saved registers or the stack area was caught at a slightly different point when the
    vertical blank began. The next frame matches again.
  - The DMA per frame comes within 0.3% of the reference's (33,554 clocks against 33,650).
  - With the DMA's scheduler counted (a step at each of its tasks' places, found on the CPS-1, below; 2026-11-20), the
    DMA comes within 0.16% (33,680 clocks against 33,735 over the run), and the work RAM is the reference's at 5,634
    of the 5,671 frames: five more single frames in the fight, the same fixed offset, no drift. The input for this
    run is the `versus` movie with real coins at 903 and 953 and a blank settings memory (all FF), as the
    reference's.

The run also turned up several bugs, which are now fixed and part of the rules:

- the 68000's refetch after a taken short branch;
- the wait clocks of an interrupt;
- a readable rewrite that ran two frames of game work in one at a knockout;
- the settings-memory shortcut, which was timed without the board's waits.

### What it does not show

- **Only SFA2.** Vampire Savior, Super Puzzle Fighter II Turbo, Alien vs. Predator and the Vampire Savior variants
  (`vsav2`, `vhunt2`, `vsav_definitive`) were never compared and passed. They use the same board and get the same
  rules.
  - A first Vampire Savior run (`vsavj`, 6,000 frames, coins at 1,100 and 1,150) was made after the merge. It
    follows the reference at the same fixed 11-frame offset up to reference frame 1,958. It then diverges for good:
    one byte differs at 1,965 (`FF0861`), eight at 1,970, and 139 by 2,100. That is shortly after the coins, in
    the menus or the start of the fight. The cause hasn't been found, and the run itself hasn't been checked for an
    input mismatch.
- **Only work RAM, only at frame boundaries.** Pictures, video RAM, object RAM and the 68000's position inside a
  frame weren't compared. A raster effect or a mid-frame write can be early or late within the frame and still
  leave the same work RAM at the vertical blank.
- **No sound.** The reference's QSound was stubbed to always ready, so the Z80, the QSound and the timing of the
  68000-to-Z80 handshake are the port's own and were never checked. Only the port's previous behaviour was checked.
- **No real board.** Everything was compared with jtcps2. Where jtcps2 differs from a board, so may the port.
  - One known case: the 93C46 settings memory, which both answer at once. A real chip is busy for a few
    milliseconds after a write, by an amount that varies from chip to chip.
- **Not with the port's additions.** Training mode, widescreen, the sources' own modes, and cheats that work
  inside a rewrite keep the readable rewrites, and those don't take the board's waits. The timing there is close,
  not exact.
- **Partly reproducible from this repository.** The comparison (`tools/reference/compare.py`) and the port's side
  (`--ram-log`, `--cpu-clock`) are here, and the harness's changes are described below ([Repeating
  it](#repeating-it)); the harness itself, a jtcores checkout with those changes, is not.

## The CPS-1: Hyper Fighting against jtcps1

**The reference.** jtcps1 (jotego's CPS-1 core, the same jtcores checkout), simulated with Verilator. Its DMA is the
same engine as jtcps2's with the object table's copy on (`jtcps1_dma.v`), and its 68000 waits a clock on every bus
access to the ROM, work RAM and video RAM, as the board's PAL does. This jtcps1 clocks the 68000 at 10 MHz for every
game, where Hyper Fighting's board has 12 MHz (MAME: "verified on pcb"), so the port was run at 10 MHz for the
comparison (a test build only) and plays at 12 MHz. The rules are in time, per access and per word, so they carry
to 12 MHz unchanged.

**The harness.** As for CPS-2, with three more changes: the work RAM dump reads a CPS-1's place in the SDRAM (words
from 0x300000, in order), a bus log for the CPS-1 (the 68000's clocks and the DMA's each frame), and `RAWFRAMES=1`
writing each frame's picture as it is (the stock dump converts in a forked process from one shared file and loses
or mixes frames under load).

**The runs.** Hyper Fighting (`sf2hf`), with the board's DIP switches (FF FC 9F): from power-on with no input (the
board's tests, the warning, the title, the attract mode's demo fight), 4,203 frames; and from power-on with the
`versus` movie and two real coins (the select screen and a two-player fight), 2,606 frames.

| | Board timing (now the default) | Every clock (before) |
|---|---|---|
| DMA a frame (median, at 10 MHz) | 23,754 clocks against 23,767 | none counted |
| Ahead of the core after the boot tests (frame 154) | 14 frames | 48 frames |
| Ahead of the core at the demo fight's start | 17 frames (3 more over 2,826 frames: 0.1%) | 106 frames (58 more: 2.1%) |
| Player fight: frames a round-clock second (core: 34.96) | **34.90** | 30.89 (11.6% fast) |

- In a fight, Hyper Fighting's turbo speed fits in more of the game's work wherever a frame has time to spare, so
  the speed of the game itself follows the CPU's time: with every clock it ran 11.6% fast, with the board's
  timing within 0.2% of the core.
- After the boot tests the game state is the core's, frame for frame, at a fixed offset, until a frame where the
  core slows down and the port doesn't (frame 986, the title's start; three such frames before the demo fight).
  Two bytes flip with where the frame's end catches a routine (`FF0380`, `FF084F`, as `FF043C` and `FFDF63` on
  SFA2) and the stack (`FFFE00`-) are left out.
- A fight with inputs can't be compared press for press: a random number the game advances while it waits for the
  next frame (`FF02BE`) comes out one apart at the second player's Start, and the fights go their own ways from
  there. Its speed can be, as above.

### The evidence

The same input to the same frame, the 68000 at the core's 10 MHz: the FPGA core (jtcps1), the port with the
board's timing (the default), the port with every clock (as before). The core and the port tick the round clock
together; with every clock the fight runs ahead (90, 90, 86 here).

![Hyper Fighting's fight: jtcps1, the port with the board's timing, the port with every clock](evidence/sf2hf-timing-vs-jtcps1.gif)

![The same at 6.3 seconds into the fight, with the numbers](evidence/sf2hf-timing-vs-jtcps1.png)

The whole fight, 18 seconds, the three stacked at three times the size:
[evidence/sf2hf-timing-vs-jtcps1.mp4](evidence/sf2hf-timing-vs-jtcps1.mp4).

### Repeating it

**The reference.** [jtcores](https://github.com/jotego/jtcores) at `bfced64` (its `jt51` and `jt6295` submodules
too), Verilator 5, and jtframe's `setprj.sh` in a bash with GNU's tools first on the path. `jtframe mra cps1
--setname sf2hf --path <folder of MAME zips>` makes `rom/sf2hf.rom` and its DIP file. In
`modules/jtframe/verilator/test.cpp`:

- `WRAMLOG=1`: at the start of each vertical blank, append the 68000's 64 KB of work RAM, big-endian, to
  `wram.bin`: SDRAM bank 0 from word 0x300000 in order on a CPS-1 (`WRAM_OFFSET`); on a CPS-2 from word 0x600000,
  bits 9 and up of the word index one place up.
- `BUSLOG=1`: each frame's 68000 clocks (`u_dtack` `emit && phase`) and those with `BGACKn` low (the DMA's) to
  `buslog.txt`; on a CPS-2 also those with `board_wait` high.
- `MOVIE=file`: a cps2recomp input movie (`frame p1 p2`, the port's bits: joystick and buttons in the low ten,
  Start 0x400, coin 0x800) played into the cabinet inputs.
- `RAWFRAMES=1` (with `-video N`): each frame's picture as 384x224 RGBA to `frames/frame_NNNNN.rgba`, written in
  the simulation's own process.
- For a CPS-2: `jtframe_68kdtack_cen.v`'s wait counter 16 bits wide, and the QSound DSP always ready
  (`jtcps15_sound.v`); a blank settings memory (all FF).

Run from `cores/cps1/ver/sf2hf` (or `cps2/ver/sfa2u`): once `jtsim -verilator -load -time 200`, which writes the
loaded SDRAM to `sdram_bank*.bin` (keep a copy: a later `SDRAMAT` dump overwrites them); then
`WRAMLOG=1 BUSLOG=1 MOVIE=$PWD/versus_coins.txt jtsim -verilator -time 43700` (2,606 frames), with `-video 2606`
and `RAWFRAMES=1` for the pictures. `versus_coins.txt` is `games/sf2hf/movies/versus.txt` with a coin for each
player at 900 and 950, each held 6 frames as the movie's other presses are. About 70 frames a minute on an
M-series Mac with nothing else running.

**The port.** `build/sf2hf/sf2hf --rom rom/sf2hf --headless --frames 2606 --movie versus_coins.txt --cpu-clock 10
--ram-log new.bin`, and the same with `--features slowdown=1` for every clock. For SFA2, `--eeprom` a file of 128
FF bytes and the movie's coins at 903 and 953.

**The numbers.**
`tools/reference/compare.py clock core.bin new.bin old.bin` (frames a round-clock second);
`tools/reference/compare.py track core.bin new.bin --skip 0380 --skip 084F --skip FE00-FFFF` (Hyper Fighting:
its stack and two bytes that flip with where the frame's end catches a routine; SFA2's are
`--skip 043C --skip 7C00-7FFF --skip DF63`).

## The Street Fighter II family: the same fight on both sides

Comparing a fight's speed means comparing the same fight. Three things made earlier fight comparisons compare
different fights, and are now part of the method:

- **The boot offset, exact.** The core boots later than the port (a full `-load` ROM download: 520 frames for Super
  Turbo, 164 for World Warrior, 174 for Champion Edition; Hyper Fighting's preloaded run 14 the other way). Measured
  as the offset at which work RAM is the same byte for byte before any input, not estimated: a movie shifted by an
  estimate a few frames out puts the coins into another moment of the attract mode, and the two runs then play
  different fights.
- **The inputs on the same game frames.** The port's movie is the core's shifted by that offset, so each press lands
  on the same frame of the game on both sides.
- **The same random state.** Super Turbo stirs its random number (FF82D0) with everything it does, so the attract
  mode's few frames of difference leave the two sides different seeds, and a different stage and fight. For a speed
  comparison the core's work RAM at the frame before the first press of the character select is copied into the port
  (`CPS2RECOMP_POKE=FRAME:ADDR=BYTE,...`, the bytes that differ); from there both play the same fight.

Then the round clock's frames (FF8DCE in Super Turbo, FF8ACE in World Warrior, FF8ABE in Champion Edition and Hyper
Fighting) and the game's tick counter (FF801D) side by side:

| Game (core) | Settings | Same game state | Speed in the fight |
| --- | --- | --- | --- |
| Super Turbo (jtcps2) | Turbo 1 | to the round and 150 frames in | 32.118 frames a clock second, the core's 32.118 |
| | Turbo 2 | likewise | 31.474 against 31.579 (the last tick 2 frames sooner over 19 seconds) |
| | Turbo 3 | likewise | 30.923, the core's 30.923 |
| | Turbo 4 | likewise | 30.600 against 30.067 (8 frames later over 15 seconds, after the fights part) |
| Hyper Fighting (jtcps1, 10 MHz) | | to the fight | 34.88 against 34.96 |
| World Warrior (jtcps1) | | to frame 2,169 | the tick gap +3, the same through the fight (no drift) |
| Champion Edition (jtcps1) | | to frame 2,148 | the tick gap +2, the same for 1,000 frames of fight |
| Alpha 2 (jtcps2), attract | | 5,705 of 5,710 frames | in step to the end (3,760 and frame 4,785 before) |

Where the two parted (after 100 to 150 frames of a Super Turbo fight) it is by one tick landing a frame apart where a
frame's spare time is at the turbo's threshold; the scripted fight then goes another way, and the speeds after that
differ either way, not one way. World Warrior and Champion Edition run 2 or 3 more catch-up ticks than the core while
they set up a screen (after Start, at the VS screen), so their fight starts that many ticks sooner and then keeps the
core's pace.

The same fight side by side (the core's pictures from its saved state at the round's start, the port's from the
same frames; left to right the core, this release, and 1.6.0-beta.3 with every clock), Turbo 4 and Turbo 1:
[evidence/ssf2t-turbo4-vs-jtcps2.gif](evidence/ssf2t-turbo4-vs-jtcps2.gif) (and `.mp4`, at the board's 59.64 Hz; the
GIF plays at 50), [evidence/ssf2t-turbo1-vs-jtcps2.gif](evidence/ssf2t-turbo1-vs-jtcps2.gif), and a still,
[evidence/ssf2t-turbo4-vs-jtcps2.png](evidence/ssf2t-turbo4-vs-jtcps2.png).

What the runs found (TIMING.md, "The 68000's own clocks" and "A CPS-1's video RAM"): long ALU operations from a
register or an immediate 2 clocks short, EOR into a register 4 long, MOVE to `-(An)` 2 long, the refetch after a
change of SR or CCR and the read before a CLR or Scc not waited for, and the CPS-1's video RAM writes a clock short.
They were found by tracing every bus access of the core (the harness's `PCTRACE=from:to`) and every block of the
port (an experiment build's) over the same frames, from a saved state of the simulation just before them.

### Other emulators, measured the same way

For context, the same Super Turbo fights (the same inputs, the core's state copied in at character select) were run
on FinalBurn Neo, upstream's libretro core and Fightcade 2's own build of it (`fightcadeorg/fightcade-fbneo`, its
Capcom driver and 68000 built into upstream's libretro front end: `tools/reference/fbneo/README.md`). FinalBurn
Neo takes a different approach to the boards' timing: rather than counting the waits and the DMA, it sets each game's
68000 clock (Super Turbo's 10.8675 MHz, Hyper Fighting's 8.5) to match recordings of real boards. Frames a second of
the round clock:

| Turbo | jtcps2 | CPS-2 Recomp | FinalBurn Neo (upstream) | Fightcade 2 |
| --- | --- | --- | --- | --- |
| 1 | 32.118 | 32.118 | 32.824 | 32.882 |
| 2 | 31.579 | 31.474 | 32.600 (16 seconds) | 32.158 |
| 3 | 30.923 | 30.923 | 31.692 | 31.538 |
| 4 | 30.067 | 30.600 | 30.600 | 30.800 |

All start the round on the core's frame. Hyper Fighting, the same way against jtcps1 at the board's 12 MHz (a
build of it with the CPU's clock enable at 48/4 and the DMA's bus requests kept: `CPS2RECOMP_BOARD12`, a local change
to `jtcps1_main.v`; jtcps1's own 12 MHz is its turbo, which also drops the DMA's requests), 16 seconds of the round
clock: jtcps1 32.867 frames a second, CPS-2 Recomp 32.933 (the last tick a frame from the core's), FinalBurn Neo
32.600 (4 frames), Fightcade 2 32.733 (2 frames). Where the copy of the core's state lands matters: each side's
round must start on the core's frame (1,562), found by trying the frames around the first press.

## Where it stands

For Street Fighter Alpha 2's game logic, the arcade timing matches a cycle-level model of the board frame for
frame. That covers a whole credit, with no drift beyond a fixed 11-frame offset from boot. Nothing has been measured
for the other games, the picture, the sound, or against real hardware, and the one other game tried (Vampire
Savior) diverges after about 2,000 frames.

Hyper Fighting's speed in a fight is jtcps1's within 0.2%, and its game state keeps step with jtcps1's, a few
slowdown frames apart over its attract mode (above).

The Street Fighter II family (above) runs at its core's speed in a fight: Super Turbo at every turbo setting,
Hyper Fighting, World Warrior and Champion Edition, the same fight on both sides.

So the accurate claim is **"the Street Fighter II family and Alpha 2 run at the speed of jotego's FPGA cores, the
same fight frame for frame"**, not "arcade perfect": the cores are models of the boards, not the boards. It is the
default for every game.

## The path to arcade perfect

In rough order. Each step makes the next one's results mean more.

1. **Make the comparison reproducible.** ([#42](https://github.com/tmcleroy/cps2recomp/issues/42)) Put the harness patches, input scripts and comparison scripts under
   `tools/reference/`, with a `make compare-reference GAME=…` that builds the simulation, runs both sides, and
   prints the match table above. Every later step depends on this.
2. **Find the Vampire Savior divergence.** ([#43](https://github.com/tmcleroy/cps2recomp/issues/43)) Re-run it with a checked input script, find the first instruction where
   the two runs part using the bus logs and a per-line trace, and fix the rule that's missing. This is the clearest
   sign that the rules aren't complete yet.
3. **Compare every game.** ([#44](https://github.com/tmcleroy/cps2recomp/issues/44)) Run Super Puzzle Fighter II Turbo, Alien vs. Predator, and the Vampire Savior variants
   over a full credit each, and keep the results as checks that fail when the match count drops.
4. **Explain the 11-frame boot offset.** ([#45](https://github.com/tmcleroy/cps2recomp/issues/45)) The game runs in step after boot, but power-on should match too. Find what
   the port does differently during the tests: the settings memory's first read, the QSound's ready answer, or how
   the simulation's ROM download is counted.
5. **Compare inside the frame.** ([#46](https://github.com/tmcleroy/cps2recomp/issues/46)) Log the 68000's cycle count at every raster interrupt and every write to the
   video registers on both sides, and compare video and object RAM at each vertical blank. This catches timing
   that lands in the same frame but at the wrong line.
6. **Compare the picture.** ([#46](https://github.com/tmcleroy/cps2recomp/issues/46)) jtcps2's simulation writes frames (`frames/`, `test.mp4`), and the port has
   `--shot-every`. Compare them pixel for pixel, slowdown frames included, to cover the CPS-B priority, object
   latency and raster effects as drawn.
7. **The sound side.** ([#47](https://github.com/tmcleroy/cps2recomp/issues/47)) Compare the Z80 and the 68000-to-Z80 handshake timing with the QSound stub, then check the
   QSound itself against a capture from a board or a decapped-DSP emulation. jtcps2 can't do this without the
   DSP's program.
8. **The port's additions.** ([#48](https://github.com/tmcleroy/cps2recomp/issues/48)) Charge the board's waits inside readable rewrites, either by counting the original's
   bus accesses or by recording each rewrite's cost from the interpreter. Then training mode, widescreen and cheats
   keep the timing as well, and `rewritesNeeded` no longer trades accuracy away.
9. **Ground truth from a real board.** ([#49](https://github.com/tmcleroy/cps2recomp/issues/49)) Use a board, or published logic-analyser captures, for the cases a model
   can't settle: slowdown lengths in known scenes (a frame-counted video of a board, for example), the 93C46's busy
   time, and the DL-1827's wait in corner cases.
10. **The CPS-1 with QSound (Cadillacs and Dinosaurs, The Punisher).** ([#50](https://github.com/tmcleroy/cps2recomp/issues/50)) It has the CPS-1's rules
    (its PAL's waits outside F00000-FEFFFF, the CPS-1's DMA), but not the wait for the QSound side's shared memory,
    and was not compared with its reference (jtcps15). The classic CPS-1's few slowdown frames the core has and the
    port doesn't (three in 2,826 frames of Hyper Fighting's attract mode) are to be found too.
11. **Frame rate and latency on the player's screen.** ([#51](https://github.com/tmcleroy/cps2recomp/issues/51)) The game runs at the board's 59.637 Hz, paced by a timer.
    On a 60 Hz display that means a repeated frame about every 2.75 seconds. Use variable refresh where the display
    has it, and measure input-to-picture delay against a board's.
