# Ball battle

(Called "the court" in the sources: `fight/court.h`.)

A ball game for the fighting games, on the in-game menu's **Alternative modes** page (the Vampire Savior games:
Vampire Savior, Vampire Savior Definitive, Vampire Savior 2 and Vampire Hunter 2, which share its sources; Street
Fighter Alpha, Alpha 2 (with Zero 2 Alpha, which shares its sources) and Alpha 3; and the Street Fighter IIs: Super
Turbo with the games that build its sources (Super Street Fighter II, The World Warrior, Champion Edition, Hyper
Fighting and its CSL editions), and Hyper Street Fighter II; and Marvel vs. Capcom). The stage is a court cut in two by a line down the middle of the picture. Each fighter keeps
to its own half, and the two of them hit a big ball that flies over the whole court. Get the ball past the other
fighter and off their end of the court for a point. First to 3, 5 or 10 wins.

## Playing

- **Hitting the ball.** Any attack that touches the ball hits it. Harder attacks send it faster: light, medium, hard,
  then special moves well beyond any of those (held longer, glowing, and knocking down whoever they hit), then moves
  made with a gauge (ES and EX moves, supers).
- **Aftertouch**: hold up or down while your hit holds the ball (the moment it stands still) to send it higher or
  lower, up to about 25 degrees, the longer held the more; it never turns back. Where the attack meets the ball decides
  the angle: strike it below its middle and it goes up, above its middle and it goes down (a spike), squarely and it
  flies straight. It always goes toward the other half. Fireballs and other thrown attacks hit it too, a little softer.
- **Chip it up.** A light kick low (a crouching light kick) under a ball on the floor pops it up gently, to about
  head height, to be struck again. Only the light kick does it (the button pressed last tells it from a light punch).
- **Combos.** After a light or medium hit the ball drifts slowly for a moment (a quarter of a second), so the next
  attack of a chain combo can catch it; a hard hit, a special or a gauge move sends it straight off.
- **Spin.** A ball struck above its middle spins forward and kicks on when it bounces. Struck below, it spins back and
  checks up.
- **Three in a row.** Three hits by the same fighter in a row (each more than a moment apart, and not too long) make a
  **power shot**: much faster, glowing orange.
- **Knock-downs.** The ball bounces off a fighter softly, but passes through one in hitstun (reeling from a hit, or
  knocked down) until they recover. A hot ball (a power shot or a super) or one that is simply
  fast enough, hit by the other fighter, **knocks down** whoever it hits.
- **The line.** The line only separates the fighters: the ball flies over it. A fighter only hits the ball over their
  own half, never reaching across for it; but fireballs and other thrown attacks hit it anywhere.
- **The court** is as much of the stage as is drawn, edge to edge: the fighters walk to its ends and the ball goes out
  there. In Vampire Savior that is up to 768 columns (twice the board's picture, the camera held in the middle), less
  where a stage's back planes end sooner (Tower of Arrogance and the other games' stages 512); the fighters start in the middles of their halves. A gentle ball dropping on a fighter (a serve on a
  head) rolls off towards the line, never off the back to its own end.
- **Points.** The ball off an end of the court is a point for the other fighter; a ball lying still stays in play
  until someone strikes it. The next serve drops on the half of whoever lost the point.
- **The ball hits you**: a ball the other fighter sent that hits you makes you reel, light, medium or hard by its
  speed, unless you are blocking (holding back): then you guard it, and a hot ball guarded does not knock you down
  either. The CPU blocks too, at the medium and hard levels.
- **The fighters hit each other** (near the line, or with what they throw), as in a fight, but nobody loses health.
  **Nobody is caught**: throws and command grabs take no one (Anakaris's coffin reaches across the court).
- **The gauge** fills as it always does when attacks are thrown, so supers come round.
- Nobody loses health and the clock stands still.

The menu's **Ball battle** page has Start and these choices:

| Row | Choices |
| --- | --- |
| First to | 3, 5 or 10 points |
| Player 2 side | The CPU, or player 2 |
| CPU level | Easy, medium or hard |
| Camera | **Whole court** (the default: the whole court always) or **Follow the play** (as close as shows both fighters and the ball, to 1.3x a 16:9 picture; out to the whole stage, black above it, when they are apart) |
| Widescreen (recommended) | The Display page's own Widescreen, here too: the court shown 16:9 (else 4:3) |
| Fighter 1, Fighter 2 | Someone at random, or a character |
| Stage | The menu's **Stage** page, as for every way of playing: the game's choice (at random, of the game's own), or any stage, the game's own, then the other games' (guest stages, each needing that game's ROM set) |

## How it is made

`fight/court.{h,cpp}` holds the game itself, the same for every fighting game: the ball's flight and bounces, what an
attack does to it, the rules and the score, the CPU's side, the ball and the line drawn, the score shown, and the
menu's page. It runs as one of the battle modes' kinds (`fight/battle.cpp`, `Kind::Court`). The battle starts the
match (the fighters from the page's rows and the stage from the Stage page, nobody played by the game's own computer, no clock, no
knock-out), holds the camera in the middle of the stage, and calls the court before and after each frame.

- **The court** is the picture: its left and right edges in the stage (wide or not) are the ends, and the line is
  their middle. The ceiling is the picture's top, below the game's panel (`pictureTop`).
- **Units** are pixels and frames (a sixtieth of a second), floats in the court's own state. Heights are the game's
  (y upwards; `floor` is a fighter's standing height).
- **Contacts** come once a frame from the game's sources (`Description::shapes`): every attack box that is out (the
  fighters' and what they threw, each with its power and an id that stays the same while the same attack is out) and
  the fighters' bodies (the boxes they can be hit in). The strongest attack touching the ball strikes it, once per
  attack (`id`). Bodies bounce it softly and never let it rest on them. Attack boxes reach `kStrikeSlack` (14) pixels
  further for the ball than for a fighter, and the CPU's reach further still by level (`kComputerSlack`): a big ball is
  met generously, and the CPU's play is not exact.
- **Drawn** as round boxes over the picture (`HudBox::round`, `port/hud.cpp`), a frame behind like the game's sprites
  (`shown`). The overlay is zoomed with the picture, the zoom by distance too: `Port::Windowed` draws the boxes on the
  panel-less picture it zooms.
- **The court's width**: the game's wide picture, forced while the court runs and as wide as the stage is drawn (vsav's
  `widescreen.cpp`: `court::On()` counts as the feature on and played in, its side each stage's own from `kCourtSides`,
  up to `Video::kMaxSide`, 192 columns, where the widescreen feature's is `Video::kWideSide`, 64; the camera's ends both
  put in the middle). The floor reaches 192 a side on every stage, but the planes behind it go by more slowly and their
  pictures are only as wide as the board's camera needs, so on most stages one ends first (black past it, or its
  picture again). Each stage's side was measured from its court picture at 192 (`--present-every`, the columns where
  every plane is drawn) and made the same both ways for halves alike: 140, 170, 64, 136, 120, 192, 160, 154, 192, 152,
  192, 192. A guest stage is drawn by its own game to 64 a side, so its court is 512. The renderer draws up to
  192 columns a side (`kMaxSide`: its line buffers and the GPU's textures sized to match); the stage's far plane that
  is drawn as the camera moves comes from the stage's plan (vsav's `SideTile`), as for widescreen.
- **The picture shown** (`Game::framedColumns`, `framedFollows`; `Port::Framed`): a picture wider than the window's
  shape is looked at through the court's own camera into a 16:9 frame at three times a 512-column picture's size, made
  on the CPU (the GPU's own picture is not used meanwhile), the panel and the readout laid over it where a 512-column
  picture has them. The camera eases to as close as shows both fighters (`Game::focus`) and the ball
  (`Game::focusAlso`), at most 1.3x, and out to the whole picture, its floor at the frame's bottom and black above. The
  menu is drawn over the whole picture in that frame. Each frame's pixel is the picture's nearest (tables of columns and
  rows made once a frame): a blend of four was too slow on a Steam Machine. A court of 512 columns (a guest stage's, Tower
  of Arrogance) is the widescreen picture itself and needs no frame. Screenshots (`--shot-every`) are the whole picture;
  `--present-every` is the frame. and `Game::zoomWanted` (the court asks for the zoom by distance while it runs, unless its page says Whole
  court; the player's own zoom comes back after).
- **The panel**: its health bars, names and clock (the objects at its top) are taken out of the object list while
  the court runs (`Description::panel`, the game's own test of a panel object, and `panelTop`, the list's y above
  which they are: `court::TakeObjects` as `Video::objectsTaken`); the gauges, below, stay. A panel that is a scroll
  screen is left out by `hiddenLayers` (Street Fighter Alpha's).
- **Save states**: the court's state is one POD (`court::State`, in the battle's).
- **The CPU's side** (`Computer`): where the ball will come down on its half (a copy of the ball flown on, no
  fighters), standing a little behind that spot. It tries each button strength, with that strength's startup frames
  and reach, against where the ball will be when the attack comes out. A fast ball coming at it is blocked (held back) rather than met, decided once for each ball by chance (medium 15% of
  slower balls and 45% of fast or hot ones, hard 40% and 85%). It jumps only for a ball over its own half,
  crouches for a ball on the floor, and now and then (medium and hard) does a quarter-circle-forward special. It plays
  from the court's own controls, never the game's computer, which knows nothing of a ball.

### Tuning

The feel is in the constants at the top of `fight/court.cpp`: `kRadius` 15, `kGravity` 0.13, `kSpeeds` (4.4, 5.7, 7.0,
10.0, 12.5 by power), `kKnockDownSpeed` 9.6, the bounce and grip factors, `kStreak*`, `kRally*` (the ball gets a little
faster each time it crosses the line in a rally), `kFreezes` and `kBallFreezes` (how long the attacker and the ball
are held when it is struck: the hit's weight), `kDeadBall`.

## Adding it to another game

A game needs the battle modes first (`battle::Register`, [FIGHT.md](FIGHT.md#the-fighting-modes)). Then:

0. **The wide picture forced while the court runs, as wide as the stage** (the game's widescreen code: `court::On()` as
   the feature on, its side as far as the stage is drawn (measure each stage: the court picture at `Video::kMaxSide`), the camera's ends both in the middle; then
   `Game::framedColumns` shows it in a 16:9 frame).
1. **`games/<id>/src/court.cpp`** with a `RegisterCourt()` filling `court::Description` and calling `court::Register`.
   Call it before `RegisterBattle()` in the game's `game.cpp`: the battle makes the court's page only for a game that
   described one. Vampire Savior's (`games/vampire_savior/src/court.cpp`) is the model.
   - `shapes`: each player record's and thrown record's attack box now (the same read as the game's own hit test, or
     the training hit boxes', `boxes.cpp`), with its side, its power and an id (record address and the attack's number
     in its entry); and both fighters' hurt boxes as `kBody`.
   - **Power** from the attack's own data. In Vampire Savior, byte `+0x12` of the attack's entry is 1, 2 or 3 for a
     button's light, medium or hard attack and higher for a special move (each its own value: Demitri's fireball 5, its
     ES version 9, his uppercut 7, Lilith's rising blade 0F). So a **super's** power is told by a gauge spent: the
     player's stock byte going down marks the move under way as a super until the fighter can act again. Find the
     byte by logging entries as attacks come out (the `VSAV_COURT_LOG=1` log in vsav's file) while a movie or the CPU
     presses each button and a few specials.
   - `place`: a fighter put at x (its record's place), for the start of a game.
   - `fighterX`, `fighterY`, `halfWidth`, `canAct`, `floor`, `pictureFloor`, `pictureTop`, `wideStage`: as the
     training session's description has them.
   - `freeze`: the attacker's hitstop byte. `playSound` and `hitSounds`: the game's own hit sounds. Find them by
     tracing the sound queue (`--trace` of the ring) while hits land in training: Vampire Savior's are 0100, 0101, 0102
     for light, medium, hard.
   - A game that builds another's sources through a family (vsav2 and vhunt2 build vsav's) needs each new `At(...)`
     place in its `family.h` table (`tools/places.py`, or by hand: FF8135 was added so, between two neighbours that
     stayed put); the build stops on a place a table lacks.
   - `everyFrame`: anything to keep set while the court runs. Vampire Savior's "first attack made" byte (FF8135),
     else the game says FIRST ATTACK when the ball knocks someone down; and both fighters' health kept full.
   - No catches: hook the game's catch test to say no while `court::On()` (vsav: 029406, D0 0 and Z set, the
     `CourtNoCatch` hook), or Anakaris's coffin catches the other fighter across the court (`--watch` on the victim's
     control byte found the routine: 029482 writes the caught state). Vampire Savior 2 and Hunter 2 have it in their
     `family.h` tables by hand (028708, 028736).
2. **Keep the fighters apart.** Where the game pushes the two players apart (vsav's `PushApart` rewrite), while
   `court::On()`: put each one's x through `court::KeepToHalf(side, x, halfWidth)` and push nothing.
3. **Their attacks hit each other** as the game has them: no change (nobody's health goes down: `everyFrame`).
4. **What the ball does to a fighter.** At the player's step (vsav: the `PlayerStep` hook, `CourtStep`), when
   `court::ReactionWanted(side, away)` gives one: put the fighter in the state a hit leaves (vsav: state `02020000`, the
   reaction byte `+54`: 0, 1, 2 a reel on its feet, 4 a knock-down with `+117` = 1 so it lands lying, FF a guard) and
   let the game's own reeling run from there. The game pushes or throws it the way its (here missing) attacker faced,
   so turn its speed across to `away` while it reels. `Description::guarding` (vsav: `+12B` bit 0, holding back, and
   free to guard) is the block, and `Description::stunned` (vsav: the reeling state, not a guard's) lets the ball
   through a fighter in hitstun.
5. Build, and check with the test switches below: rallies, a knock-down (`CPS2RECOMP_COURT_SHOT`), the zoom
   (`--present-every`).

A game whose players are not on a fixed 384-column picture (a tag game's partner records, a game whose camera's x is
not the picture's left edge) needs those cases in its `shapes` and in the battle's camera facts. The toolkit does
not care what the fighters are, only where their boxes are.

### Street Fighter Alpha 2

The second game, and the shortest port: the routines the court changes were already rewritten for the game's other
modes (`games/sfa2/src/court.cpp`). What it took:

- **The push** (`collision.cpp`, `PushApart`), **the throw test** (`throws.cpp`, `ThrowTest`: no while `court::On()`;
  whether every command grab asks it is not checked yet) and **the player's step** (`player.cpp`, `PlayerStepOne` calls `CourtStep`): each
  a line or a few in a routine already in C++.
- **Power**: an attack's entry has the button's strength in `+09`'s low four bits (0 light, 1 medium, 2 hard: what
  `HitLands` keeps at the victim's `+56`), and bit 7 of `+08` set for a special move's. A super is told by the gauge
  (`+9E`) dropping by half a level or more at once, or a Custom Combo running (`+142`). Found with `SFA2_COURT_LOG=1`.
- **What the ball does**: the state a hit leaves (`+04` 02 02 00 00, as `HitLands` writes it), the reaction `+55` (0
  a reel, 4 a knock-down with `+134` set, FF a guard), the strength `+56` (how far `023C0C`'s table pushes it), and the
  way it is pushed `+66` (0 left, 1 right) set away from the ball: no turning its speed about as in Vampire Savior.
- **Sounds**: 0200, 0205, 0206 for light, medium, hard and 0203 for specials and supers, the sounds queued on the frame
  a hit lands (`SFA2_SOUND_LOG` against the health going down in the endless battle).
- **The court's width** (`widescreen.cpp`, `kCourtSides`): no stage shows black past its planes (the game's own
  `fillHoles` and the far planes' plans fill them), so each was judged by eye at 192 a side for stretched, cut or
  repeated pictures: 64, 168, 192, 120, 192, 192, 192, 136, 140, 192, 96, 192, 192, 64, 72, 192, 192, 192 (Ryu's
  mountains are filled stripes past 64, Sakura's tower and Rolento's mural come round again). `SFA2_COURT_SIDE=N`
  forces a side, for measuring. The stage's own `kEnds` are not used on the court.

### Street Fighter Alpha and Alpha 3

Neither has the push or the throw test rewritten, and neither needs it: a hook at a routine's start (or anywhere in
it: `HookAt`, written in the `AddHook` call itself so that the translator finds it) can do the court's part and
return, or send the routine to its own "no" (`c.pc`).

- **Alpha** (`games/sfa/src/court.cpp`): the players' push `00F2FC` hooked (each kept on its half, then `Return`); a
  throw's catch, the two places that write the caught state (`020F80`, `021032`), sent to the test's "no" (`020FBE`);
  the hit's fields as `00F646` leaves them: the reaction `+45`, the strength `+46`, the way pushed `+4D`, hitstop
  `+4F`, knock-down `+134`. An attack's entry is sixteen bytes (a box of four bytes, then `+04` bit 7 special, `+05`
  strength). Hit sounds 0205-0208 (0206 medium, 0207 hard, 0208 specials, as queued by `012BEE` on the frame `00F646`
  lands). **The bars, names and clock are the first scroll screen's**, which past 64 a side shows its map again:
  `court::Description::hiddenLayers` keeps that screen out of the court's picture (health stays full, and there is no
  clock; the gauges are objects and stay). Sides: 176, 192, 128, 192, 128, 192, 80, 176, 192, 192, 80, 192, 192 (the
  train yard's far sheds come round again 558 apart; the Great Wall's mountains are mirrored fill past 128).
- **Alpha 3** (`games/sfa3/src/court.cpp`): the players' push `0249E0` hooked; the throw test `02E3FE` (already
  hooked for the training's throw ranges) sent to its "no" `02E52A`; the hit's fields: reaction `+55` (0D a sweep's
  knock-down), the reel's time `+26A` (0C, 12, 18 by strength), the way pushed `+66`. Strength is the entry's `+09`; a
  special is told by the move (`+06` not 0A or 06). Hit sounds 0204, 0205, 0206 and 0203, queued (`003168`) a frame or
  three after the hit (`023278`). Sides: each stage's painting measured with `SFA3_NO_FILL` (holes left black) at 192,
  and kept within half of each plane's range in `kPlaneEnds`, past which a plane that is a map going round shows
  its picture again (the black alone missed those).

### Street Fighter II

Super Turbo's sources (`games/ssf2t/src/court.cpp`) are built by Super Street Fighter II and the three classic games as
well, each game's places in its `family.h` (found in each listing by the same instructions); Hyper Street Fighter II
has a copy with its own places (`games/hsf2/src/court.cpp`). Hooks only, as in Alpha 3:

- **The push**: the players' push-box routine (Super Turbo's `0BF662`, which `0BF656` calls) keeps each on its half and
  returns. **The throw tests**: where the person's throw test's ways in meet (`004634`) and the computer's own
  (`07E274`, none in The World Warrior and Champion Edition) sent to their "no" (`0047D0`, `07E36A`).
- **What the ball does**: at `07AC78`, where the game looks for a hit at the end of each fighter's step (a person's and
  the computer's), the fields a hit leaves: the reaction `+4B` (`0C` a hit on the feet, `12` a knock-down, `06`
  guarded with `+49` set), the strength `+4C`, the way pushed `+54`, and the game's own sign of a hit, the health below
  what it was before (`+2C`): the game's reeling runs from there (`07ACFC` on, `+55` from `+4B`). `everyFrame` keeps the
  health full (one below while a fighters' hit is still to be found), the stun (`+5E`) at 0, and the first-attack byte
  set (Super Turbo `FF8DEE`, Super `FF8CEE`, Hyper `FF8C1A`; none in the classic games), or FIRST ATTACK pays out
  when the ball knocks someone down.
- **Power**: the attack's entry has the button's strength in `+08`'s low two bits (0 light, 1 medium, 2 hard: what the
  hit keeps at the victim's `+4C`) and bit 7 of `+04` set for a special move's (the chip damage's test); Hyper's entries
  are twenty bytes, the same fields four on. A super (Super Turbo and Hyper) is told by the gauge (`+2B4`) dropping by
  half or more at once.
- **Sounds**: 004C, 004D, 004B for light, medium, hard (004B for specials and supers too) in Super Turbo, Super and Hyper;
  002B, 002C, 002A in the classic games (family.h's `SSF2T_FAMILY_HIT_SOUNDS`), read off the sound queue while the
  training's dummy was hit by each button.
- **The court's width** (`widescreen.cpp`, `kCourtSides`, by the stage number): the board's camera covers 768 columns of
  every stage, so 192 a side is the whole floor; the planes behind end sooner on some, judged by eye at 192: 128, 192,
  144, 192, 128, 128, 192, 192, 192, 160, 96, 192, 64, 160, 64, 144 (Ryu's roofs end, Ken's boats and Chun-Li's street
  come round again, Balrog's casino is 576 wide, Cammy's and Fei Long's planes behind come round again past 64). The
  classic games' twelve stages end sooner on E. Honda's and M. Bison's (black past 128): their `family.h` has
  `SSF2T_FAMILY_COURT_SIDES`. But the classic games are CPS-1s, whose objects are placed across in nine bits (512
  columns: `Video::DrawObjectsCps1`), and the game's own object writer leaves out any outside them (Hyper Fighting's
  `051226` on): a court wider than 512 had its fighters undrawn in its outer columns, or drawn 512 away on the other
  half. So their `family.h` has `SSF2T_FAMILY_COURT_MOST` 64 too, and their court is the widescreen picture on every
  stage (`kCourtMost`; tools/check_online.sh's `BALL` fails a court wider than its board's objects go).
  `SSF2T_COURT_SIDE=N` (`HSF2_COURT_SIDE`) forces a side, for measuring.
- The World Warrior's battle modes offer its eight world warriors only (`SSF2T_FAMILY_CHOOSABLE`): M. Bison or Sagat
  played by a person sent the game off into nothing as the fight began (every battle mode, before the court too).

### Marvel vs. Capcom

Hooks only (`games/mvsc/src/court.cpp`), as in Alpha 3, with three things of its own: a side is a team of two, its heights
grow downwards, and a hit has no simple state to write.

- **One character a side.** The chord matcher (`01A0C2`) never matches a punch and a kick of the same strength (HP + HK:
  the tags at `019456` and `019526` and the team moves; MP + MK: the special partner at `0195CE`), so the partner rests
  off the picture all game. **The push**: `013758` (the four pairs of records pushed apart) keeps each character on
  point on its half and goes on at `01378C`. **The throw test** `0186E6` sent to its "no" (`01870E`); its kept-apart
  distance (`halfWidth` 32 a side of the line) is past the throws' reach. **First attack**: `01D12A` passed over (the
  game says FIRST ATTACK as it sets `FF4011`). **No damage**: the hits' `sub.w d5,0x270(a6)` (`01B74E`, `01BEA0`) take
  nothing.
- **What the ball does** is a hit of the game's own. The hit test leaves each record's hit found in it (`+87` the box
  hit, `+88` who hit it: `0D7270`), and `0D6ED2` deals with them (`01A8C6` each, the attacker's attack entry chosen at
  `01A934`). The court's reaction is put in at `0D6EDA` as a hit by the other side's character, and its entry swapped at
  `01A952` for one of Ryu's (`0F1544` on: his standing light, medium and hard punches, his sweep for a knock-down); the
  entry is the hit's data, so it reels any character as Ryu's own would. A ball guarded goes to the game's guard
  (`01AA08`) at once (`01A9F4`), since the game guards only what its attack lists said was coming. The attacker, far away,
  gets back the hitstop the entry gave it (`0D6E9E`).
- **Power**: the attack's entry (`0E68AE` by the character, `+80` its number, 0x20 bytes) has the button's strength in
  `+0D`'s low two bits; a special move is the character's `+B0` not 28 (2A specials, 2C what they throw); a super is a
  gauge level spent (`+274` going down). **Sounds** 0104, 0105, 0106 and 010A (the game's own, as the ball's hits of
  Ryu's entries queued them).
- **Heights**: the court's are the height over the floor (`Ground()` less `+10`), and the camera's place down is turned
  into one by `Description::cameraHeight` (the camera record's lowest place less its place); the picture's left edge
  is the camera's word + 0x40 (`Description::pictureLeft`). Both are new in the description for this game.
- **The court's width** (`Engine::courtSide` in `fight/marvel.h`, the shared widescreen with the court's side in place
  of 64): the game draws a stage's scroll screens only as far past the board's picture as its camera needs, so as a
  court game begins (under the fade from black) the camera is put at the stage's right end for two frames, then its
  left, then back in the middle, and the game draws the rest (the fighters put back after). Then judged by eye at each
  side: 64, 80, 192, 192, 192, 192, 16, 32, 32, 64 (the bathhouse's camera has 368 columns of room; the haunted hall's
  back wall, the wreckage's far plane, the city street's and the canyon's planes end, Onslaught's city's lights end). The
  wreckage's left 16 and the city street's left 30 columns show past their art in the game's own widescreen too, with
  the camera in the middle. `MVSC_COURT_SIDE=N` forces a side, `MVSC_COURT_NO_SWEEP=1` leaves the camera where it is,
  `MVSC_COURT_LOG=1` logs each attack's entry, the sounds queued and the ball's hits.

### Test switches

| Variable | What it does |
| --- | --- |
| `CPS2RECOMP_COURT_CPU=1` | Player 1's side played by the court's CPU too: a whole game with no input |
| `CPS2RECOMP_COURT_LOG=1` | The ball, the fighters and the score every 6 frames, and each press, hit and bounce (with the machine's frame, the `--shot-every` pictures' numbers) |
| `CPS2RECOMP_COURT_BOXES=1` | The boxes the ball was tested against, drawn (attacks red, bodies blue), and a line per frame |
| `CPS2RECOMP_COURT_SHOT=X,Y,VX,VY` | The first serve sent off from there at that speed, hot, as if player 1 had struck it (a knock-down test: `600,110,10,0.5`) |
| `CPS2RECOMP_BATTLE_SEED=N` | The battle's choices at random the same each run |

For example, a whole CPU-against-CPU game at the hard level, seen every 6 frames:

```
CPS2RECOMP_COURT_CPU=1 CPS2RECOMP_BATTLE_SEED=7 build/vampire_savior/vampire_savior --rom rom/vampire_savior \
  --headless --settings /tmp/court/settings.ini --keys "5:fcourt_level=2,10:ccourt_start" --frames 6000 \
  --shot-every 6:/tmp/court/f
```

## Next

- **Tall stages**: the Marvel games' stages (as guests) with the ball going up past the picture's top and the camera
  following, past the board's picture vertically (the renderer drawing more of the stage than the game asks for).
- **Power-ups** (more power, other physics) and **combos with properties** (three specials in a row giving the ball a
  property of its own), a tip-off or face-off serve.
- **The CPU**: smarter positioning (a lob over a fighter near the line, waiting for a ball to drop), specials of each
  character's own.
- More games: the other Marvel games (Marvel Super Heroes, X-Men vs. Street Fighter, Marvel Super Heroes vs. Street
  Fighter), which share Marvel vs. Capcom's engine code (`fight/marvel.h`).
