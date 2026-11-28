# The picture

## Widescreen

The fighting games can be shown and played in **16:9**: the picture is wider at the same height, with everything
the same size, so more of the stage is in view. The extra columns are the game's own stage art, which the board
already holds and never showed; nothing is stretched. It's a switch on the launcher's first page under "Full screen",
and on the in-game menu's Display page. It's **off unless you switch it on**, because it changes how the game plays:
the players can go to the wide picture's edges.

![Widescreen in Street Fighter Alpha 2: Ryu and Ken at Sagat's stage, more of it on each side](screenshots/sfa2/widescreen.png)

**Invisible walls**, on the Display page, keeps the 16:9 picture and plays the game exactly as the board does: the
players are stopped where the board's picture ends, by walls that aren't seen.

## Display

The Display page (in the launcher and the in-game menu) says how the picture fits the window or screen:

| Row | Choices |
| --- | --- |
| Scaling | Sharp (each pixel a crisp block), Whole pixels (every pixel the same whole number of the screen's pixels tall, a border round it), Smooth |
| Picture shape | 4:3 (as the arcade's monitor showed it), Square pixels, Fill |
| Shader | Off, or one of RetroArch's shaders (below) |
| Window size | 3x the board's 224 lines, or 2x to 6x |
| Picture turned | A vertical game only (Dimahoo): Upright (its picture stood up in a tall window, 3:4, as the cabinet's monitor stood on end showed it), or On its side (as the board draws it, for a screen turned on its side) |

## Shaders

The picture through one of RetroArch's shaders, by
[librashader](https://github.com/SnowflakePowered/librashader): a curved CRT, CRT Lottes, CRT Royale, CRT Guest or
a light CRT; scanlines light, strong, as an OSSC draws them, or dark; or xBRZ and ScaleFX to smooth the pixels'
edges (the Display page's **Shader**). **Your own** takes any RetroArch slang preset (`.slangp`) you put in a
`shaders` folder beside the game's settings, with the files it names (a few need a newer OpenGL than macOS has; Linux and Windows have it).
Zoomed in, the camera closes in on the shaded picture, scanlines and all, as on a monitor. The menu is drawn over the picture without the shader. A shader that cannot be made leaves the
picture as it was.

| | | |
| --- | --- | --- |
| ![No shader](screenshots/shaders/off.png)<br>Off | ![CRT (crt-geom-mini)](screenshots/shaders/crt-geom-mini.png)<br>CRT | ![CRT Royale](screenshots/shaders/crt-royale.png)<br>CRT Royale |
| ![CRT Lottes](screenshots/shaders/crt-lottes.png)<br>CRT Lottes | ![Scanlines, light](screenshots/shaders/scanline-fract.png)<br>Scanlines, light | ![xBRZ](screenshots/shaders/xbrz.png)<br>xBRZ |

## Arcade monitor

For a CRT driven at arcade rates (CRT Emudriver on Windows, or a custom resolution elsewhere). Set on the launcher's
Settings page, under **Arcade monitor**; the in-game menu (Display > Arcade monitor) can only turn these back off, so a
mode the monitor can't take is never chosen while playing.

- **Full screen resolution**: *384 x 224* switches the screen, in full screen, to exactly the board's resolution and
  fills it pixel for pixel. *Super resolution* switches it to a mode of the board's 224 lines and 1920 columns or more
  (CRT Emudriver's 2560 x 224, say) and stretches the picture across it: the lines one for one, and the columns, many
  screen columns to each, blended by the monitor itself so that every column comes out the same width, which a
  graphics card can't do at 384. Where the screen has no super resolution of 224 lines (some cards' sets go no lower
  than 240p), one of 240 lines is used instead, 2560 x 240 say: the picture's 224 lines one for one in its middle, 8
  black lines above and below, never stretched. Either way only a mode of exactly 224 lines (or, for a super
  resolution, 240) at the refresh nearest the board's 59.64 Hz (within 5 Hz) is used; if the screen doesn't list one, full screen stays at its own resolution, and the
  log says so. Turn Widescreen off with it. A vertical game is shown as the board sends it, for a monitor turned on
  its side.
- **Screen**: which screen the game opens on and goes full screen on (the main one, or the second, third or fourth in
  the order the system lists them).
- **Keep the board's pace**: off (the default), in full screen at an arcade resolution the game runs one frame per
  screen refresh, each shown as the screen refreshes, instead of on the board's 59.64 Hz clock. The modes a system
  lists seldom run at exactly that rate (most CRT Emudriver modes run at 59.98 or 60 Hz), and on the board's clock
  such a mode shows a frame twice every few seconds (a hitch in a scroll) and tears; synced, it never does, and the
  game runs 0.6% faster at 60 Hz. On: the board's clock, each picture shown as soon as it's made. Online, always the
  board's pace. The log says the screen's measured rate every ten seconds.
- **Make the mode at the board's rate** (Windows): with 384 x 224 or Super resolution, full screen has
  [Switchres](https://github.com/antonioginer/switchres) (GroovyMAME's) make that mode at exactly 59.637 Hz and
  switch the screen to it (CRT Emudriver's custom modes), instead of taking the nearest mode the system lists (most
  run at 59.98 or 60 Hz). The game then plays at the board's own speed when synced to the screen, and on the board's
  pace never shows a frame twice. The desktop's mode comes back when the game ends. If Switchres can't make it, the
  listed mode is used, and the log says why.
- **Monitor**: the monitor whose ranges Switchres keeps to (its presets): Arcade 15 kHz, Generic 15 kHz (a TV or
  consumer CRT), NTSC TV, PAL TV, Arcade 15/25 kHz, Arcade 15/25/31 kHz, Arcade 31 kHz. A mode outside a monitor's
  ranges can harm it: choose the one it is.
- **The frame delay**: synced to the screen, each frame's controls are read and its picture made as late before the
  screen's refresh as it can be, rather than just after the one before: the longest a frame has taken lately, and a
  margin, before the refresh (wider after a refresh is missed, narrower again in time). That takes most of the wait
  for the refresh out of the controls' lag. It needs the system to say when the screen refreshes, so it is on Windows
  only for now; the log's "sync to the screen" line says the delay and how many refreshes were missed.
- **Compact menu**: the in-game menu in small letters with its rows close together, so a page fits in 224 lines. It
  is on by itself in full screen at an arcade resolution.

## The camera's zoom

In the fighting games, hold the left stick's click and press the left shoulder to zoom in on player 1's fighter,
the right shoulder for player 2's (keyboard: hold Left Shift and press Q or E). Each press goes a size up, 1.25x to
2x, then back to the whole picture. With the click held, the d-pad moves the camera; the right stick's click
(keyboard: Left Shift and W) keeps **both fighters** in the picture instead, as close as shows both whole. The zoom is
the window's alone: the game plays the same.

The menu's **Zoom** row (Display page) has these and one more, **By distance**, made for playing: the camera a little
closer than the whole picture as a round starts, a quarter closer than that as the fighters come together, and out to
the whole picture as they part (from four fifths of the way to the ends), with both fighters always in it. The health
bars, the clock and the rest of the game's panel stay where they are and at their size; only the fight is zoomed.
