# FinalBurn Neo against the FPGA core and the port

`fbneo_run.cpp` runs a FinalBurn Neo libretro core headless the way the port and the jtcores harness run in the
comparison of `docs/ARCADE_ACCURACY.md`: a cps2recomp input movie played into it, its 68000's work RAM
(FF0000-FFFFFF) written in address order at the end of every frame, and work RAM bytes set at a chosen frame (the
reference's state, as `CPS2RECOMP_POKE` does for the port). It holds none of FinalBurn Neo's code: it loads a core
built separately (FinalBurn Neo's licence is not the GPL's, so none of it is in this repository).

    c++ -std=c++17 -O2 -I <FBNeo>/src/burner/libretro/libretro-common/include fbneo_run.cpp -o fbneo_run
    FBNEO_SYSTEM=<folder> ./fbneo_run <core.dylib> <set>.zip FRAMES ram.bin [movie.txt] [FRAME:ADDR=BYTE,...]

The settings memory is `<FBNEO_SYSTEM>/fbneo/<set>.nv`, the same 128 bytes as the port's `--eeprom` file
(Super Turbo's turbo setting at 0x1F and 0x4F). The controls are matched by the names the core gives them.

## The two FinalBurn Neos

- **Upstream**: `github.com/libretro/FBNeo`, `make -C src/burner/libretro`.
- **Fightcade's**: `github.com/fightcadeorg/fightcade-fbneo` has no libretro front end, so its emulation of these
  boards was built into upstream's: a copy of the upstream tree with Fightcade's `src/burn/drv/capcom`,
  `src/cpu/m68k` and `src/cpu/m68000_intf.{cpp,h}` in place of upstream's (`m68kops` and `ctv.h` generated from
  them: `make SUBSET=cps12 generate-files`), built with `make SUBSET=cps12`. Glue it needed: Fightcade's netplay
  version constants (its `burn.h` enum) and `ACB_FULLSCANL` added, its netplay globals defined as offline and
  current (`kNetSpectator = 0`, `kNetVersion = NET_VERSION`), the two members upstream's `cpu_core_config` has after
  `reset` given as null, and the front end's references to drivers left out (Neo Geo, PGM2, the CD systems) and
  Fightcade's Lua memory hook stubbed (no script runs). Everything that times a CPS board is Fightcade's.

Both run Super Turbo at a 68000 of 10.8675 MHz and Hyper Fighting at 8.5 MHz, below the boards' 11.8 and 12 MHz,
to make up for the waits and the video chip's DMA they don't count (their sources: "RN compared MAME/FBA to PCB").

## The runs

Each emulator boots a few frames apart from the core (measured where work RAM is the same byte for byte before
any input), its movie is the core's moved by that, and the core's work RAM just after the first press of the
character select is copied in (where a copy lands in the middle of a task's switch it breaks the run: the frame
after the first press is the one that holds). Then the round clock's ticks side by side: `docs/ARCADE_ACCURACY.md`.
