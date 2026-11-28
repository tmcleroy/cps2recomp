# Training mode (the shoot-'em-ups)

The shoot-'em-ups' counterpart of the fighting games' [training mode](TRAINING.md). Giga Wing, Progear and Dimahoo have it; the
shooters to come will share it (it lives in the toolkit, `shmup/`). The pictures here are Giga Wing's; Progear's
aids are the same, but its readout has the shot's power, and its summaries count the points scored (it has no
multiplier). Dimahoo's readout has its magic gauge, and its summaries count the points scored too.

**Alternative modes** in the in-game menu has a **Training mode** page: choose where and with what, then **Start**. The game
is taken there unseen in a moment (the board's start, the choice of fighter and the way to a boss are run as fast as
they go, your ship out of harm), and you play from there. Its readout and summary are switches on the same page, and
on the in-game menu's Training tab; they work only in training mode. The hit boxes are on the Cheats page and work in
every mode, the board's own game too.

![The Training mode page of the in-game menu](screenshots/gigawing/practice-menu.png)

## Where, and with what

| Choice | What it does |
| --- | --- |
| Stage | Any of the seven stages |
| Begin at | The stage's start, or **the boss**: met a few seconds before it comes on (the last stage is its boss from the start) |
| Fighter | Any of the four, or the hidden, unfinished fifth |
| Ships, bombs, power | As the game has them, or what you choose: up to nine ships, eight bombs, the shot's power from lowest to full |

## Again and again

- **Start over** (Backspace, or a gamepad's left shoulder): training mode put back exactly as it began, at once. The start is
  kept in memory as a save state the moment it is reached, so there is no wait and nothing is different: the same
  enemies, the same shots.
- **Replay** (Backslash, or the left trigger held with the left shoulder): your last attempt shown again from the
  start, the controls as you pressed them. Touch the controls to take over from there.
- **The cheats** work in training mode too (the other modes leave them out, as they keep scores): invincibility to learn a
  boss's patterns without dying, infinite ships, infinite bombs.
- **Slow motion and frame advance** are the menu's own: speed down to 1/8, pause, and a frame at a time. Save states
  (nine slots) work as everywhere.

## Rewind and a practice point

Two switches on the Game page of the in-game menu , off unless chosen. They are the
port's, so every game in the collection has them, in training mode or anywhere else, but never online.

| Switch | Keys (gamepad) | What it does |
| --- | --- | --- |
| Practice point | `]` sets it (the left shoulder held half a second), Backspace goes back (the shoulder tapped), Backslash replays (the shoulder with the left trigger held) | The game as it is that instant, kept in memory and put back at once, as often as you like: a point in the middle of a stage or just before a boss's last pattern, not only where training mode began. Replay shows your last attempt from the point again, the controls as you pressed them, until you press a button |
| Rewind | hold R (the left trigger) | The game runs backwards, three times as fast as play, through its last half a minute or so, without sound; let go and play on from there |

Until a practice point is set, Backspace and Backslash are training mode's Start over and Replay, as above. With
rewind on, the left trigger rewinds while the game runs (stopped, it still goes on a frame at a time), in place of a
tag game's Tag. Neither adds a frame of lag: keeping a state takes a fifth of a millisecond, every third frame, and
half a minute of them is a few dozen megabytes ([how](FEATURES.md#rewind-and-the-practice-point)). Gone back to, the
game is exactly as it was: `make GAME=<id> check-rewind` sets a point and goes back to it, holds rewind, and replays,
with and without run-ahead, and the game comes out frame for frame as a run that simply played on (Giga Wing, Progear,
Dimahoo, Street Fighter Alpha 3 and Cadillacs and Dinosaurs).

![Training mode begun at stage 1's boss: the readout with the Reflect Barrier charging and the boss's clock, and the boss's hit area](screenshots/gigawing/practice-boss.png)

## The aids

| Aid | What it shows |
| --- | --- |
| Player hit boxes (Cheats page) | Each ship's true hit box, green in a black edge: a 6-pixel square in the middle of the ship. It is the only part of the ship an enemy shot can hit |
| Enemy hit boxes (Cheats page) | Each enemy body's box (orange), which only the middle of your ship can touch |
| Bullet hit boxes (Cheats page) | Every enemy shot's hit point (yellow), which is the shot's tip, not its middle |
| Readout | The scoring as it happens, at the left: the multiplier, the medal chain (what the next gold medal adds to the multiplier), the shot's power, the Reflect Barrier's charge while it charges or runs down, and the boss's clock |
| Summary | When a boss comes, when it is over and at a stage's end: how long it took, what the multiplier gained, the ships lost and the bombs used |

The boxes are drawn with the objects the board shows, not a frame or two ahead of them: the board shows the object
list the game made in the frame before, and Giga Wing makes its list every other frame from the places of the frame
before that. In Dimahoo the stage is wider than the screen and pans with your ship; the boxes pan with it.

![The true hit box: a 6-pixel square in a ship many times its size, and the points where the shots hit](screenshots/gigawing/practice-hitbox.png)

![A boss's summary: how long it took, the multiplier gained, ships lost and bombs used](screenshots/gigawing/practice-summary.png)

![Training mode in stage 2: the readout, the hit box, the shots' points and the enemies' bodies](screenshots/gigawing/practice.png)

## The other modes

Score attack, caravan, boss rush and the stage viewer are on the same Alternative modes page: [MODES.md](MODES.md#the-shoot-em-ups).
