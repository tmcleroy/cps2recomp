# Arcade accuracy: the proof

CPS-2 Recomp runs every game at its arcade board's speed by default, slowdown included. For the Street Fighter II
games that's not a claim: it's measured, frame by frame, against a cycle-accurate model of the board.

![Super Turbo, Turbo 4: the FPGA core, CPS-2 Recomp, and 1.6.0-beta.3, the same fight](evidence/ssf2t-turbo4-vs-jtcps2.gif)

*Super Street Fighter II Turbo at Turbo 4. Left: the board, as jotego's FPGA core simulates it. Middle: CPS-2 Recomp.
Right: CPS-2 Recomp 1.6.0-beta.3, before the board's timing. Same inputs, same moment of the same fight: the board
and CPS-2 Recomp show the same frame.*

## What it was measured against

The reference is [jotego's FPGA cores](https://github.com/jotego/jtcores): **jtcps2** for the CPS-2 and **jtcps1**
for the CPS-1, the cores MiSTer players use. They rebuild the boards' chips from their schematics and measurements of
real boards, clock by clock. Here they ran in simulation (Verilator), so every frame of their memory could be read
and compared with CPS-2 Recomp's.

The comparison is of **the same fight**:

- the same inputs on the same game frames (each side's boot is a few frames different; that offset is measured from
  memory that's identical byte for byte before any input, and the inputs are moved by it);
- the same game state: the board's memory at character select is copied into CPS-2 Recomp, so the random number,
  the stage and every timer are the board's.

Then the round clock is read on both sides at every frame. A turbo setting is the game fitting in extra work
wherever a frame has time to spare, so the round clock's pace is the processor's speed, measured where players feel
it.

## The results

**Super Street Fighter II Turbo** (CPS-2), every turbo setting: frames per second of the round clock (more is slower).

| Turbo | The board (jtcps2) | CPS-2 Recomp | Difference |
| --- | --- | --- | --- |
| 1 | 32.118 | 32.118 | none |
| 2 | 31.579 | 31.474 | 0.3% |
| 3 | 30.923 | 30.923 | none |
| 4 | 30.067 | 30.600 | 1.8%* |

The game's state is the board's byte for byte through character select, the VS screen, the round's start and the
first seconds of the fight, at every setting. \*At Turbo 4 the two part a few seconds in (one game tick a frame
apart, where a frame's spare time sits right at the turbo's threshold), and the scripted fight then goes another
way; the speed after that point is of two different fights.

**Street Fighter II: Hyper Fighting** (CPS-1, at the board's 12 MHz), the same fight over 16 seconds of the round
clock: the board 32.867 frames a second, CPS-2 Recomp 32.933, its last tick one frame from the board's.

**Street Fighter II: The World Warrior** and **Champion Edition** (CPS-1): the same game state as the board through
the character select and into the fight, and the board's pace through the fight with no drift (World Warrior to
frame 2,169, Champion Edition for 1,000 frames of fighting).

**Street Fighter Alpha 2** (CPS-2), the attract mode from power-on: the board's game state at 5,705 of 5,710 frames.

Super Turbo at Turbo 1: [evidence/ssf2t-turbo1-vs-jtcps2.gif](evidence/ssf2t-turbo1-vs-jtcps2.gif) (and `.mp4`).
Hyper Fighting: [evidence/sf2hf-timing-vs-jtcps1.gif](evidence/sf2hf-timing-vs-jtcps1.gif).

## What the measuring found

Matching the board this closely took finding where CPS-2 Recomp's model of the 68000 and the board was off, by
tracing every bus access of the FPGA core and every block of CPS-2 Recomp over the same frames:

- the board's acknowledge: a clock of wait on each access outside its fast memory, and the 68000's own extra
  accesses (its refetch after a change of its status register, its read before CLR and Scc) waited for too;
- the video chip's DMA taking the bus for the palette, the tile rows, the row scroll and, on the CPS-1, the objects;
- the 68000's cycle counts where they're easy to get wrong: long ADD, SUB, AND, OR, ADDA and SUBA from a register
  or an immediate (8, not 6), EOR into a register (4, not 8), MOVE to `-(An)` (as `(An)`);
- a CPS-1's video RAM writes taking a clock more (as jtcps1 has them).

All of it is on by default (**Game → Slowdown → Arcade perfect**). **None** gives the processor every clock instead,
for anyone who wants a game without its slowdown.

## Repeat it

Everything here can be run again: the harness patches, the movies and the scripts are described in
[ARCADE_ACCURACY.md](ARCADE_ACCURACY.md) ("The Street Fighter II family: the same fight on both sides" and "Repeating
it"), with `tools/reference/compare.py` for the numbers and `CPS2RECOMP_POKE` for giving CPS-2 Recomp the board's
state.

## What it doesn't cover

The reference is a model of the boards, the most exact one there is, but not a board itself. CPS-1 video RAM's
extra clock is jtcps1's, and whether a board has it isn't settled by the model. The port's own additions (training
mode, widescreen, the cheats) run some of their code outside the board's timing, close but not exact. The other games
in the collection follow the same rules and haven't each been measured yet.
