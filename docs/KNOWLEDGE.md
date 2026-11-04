# Knowledge: how Schleichfahrt works

What is known about the original game, Schleichfahrt / Archimedean Dynasty,
from its data files and its behaviour. Vertigo recreates the game from this;
what is still open or only approximated is in [TODO.md](TODO.md). Where a
point says what Vertigo does, Vertigo deviates on purpose; everything else
describes the original.

## Contents

- [1. Units of measure and conventions](#1-units-of-measure-and-conventions)
- [2. Scenarios and their units](#2-scenarios-and-their-units)
  - [2.1 Unit types and object files](#21-unit-types-and-object-files)
  - [2.2 Cargo ships](#22-cargo-ships)
  - [2.3 Unit keys](#23-unit-keys)
  - [2.4 Orders](#24-orders)
  - [2.5 Conditions and events](#25-conditions-and-events)
  - [2.6 Mission failure and friendly fire](#26-mission-failure-and-friendly-fire)
  - [2.7 Mission end and result](#27-mission-end-and-result)
  - [2.8 Quirks of single missions](#28-quirks-of-single-missions)
- [3. The world](#3-the-world)
  - [3.1 Terrain shading](#31-terrain-shading)
  - [3.2 Buildings and the ground under them](#32-buildings-and-the-ground-under-them)
  - [3.3 Currents](#33-currents)
  - [3.4 Radiation and the surface](#34-radiation-and-the-surface)
- [4. The player's boat](#4-the-players-boat)
  - [4.1 Turning](#41-turning)
  - [4.2 The energy pool](#42-the-energy-pool)
  - [4.3 Firing the gun](#43-firing-the-gun)
  - [4.4 The booster](#44-the-booster)
  - [4.5 Jams](#45-jams)
  - [4.6 Armour, wear and the Fixer](#46-armour-wear-and-the-fixer)
  - [4.7 Damage between missions](#47-damage-between-missions)
- [5. Units' behaviour](#5-units-behaviour)
  - [5.1 Sleeping units](#51-sleeping-units)
  - [5.2 Attacker lists](#52-attacker-lists)
  - [5.3 Order slots and squadron events](#53-order-slots-and-squadron-events)
  - [5.4 Turning and banking](#54-turning-and-banking)
  - [5.5 Flying to a point and patrolling](#55-flying-to-a-point-and-patrolling)
  - [5.6 Sensors](#56-sensors)
  - [5.7 Choosing a target](#57-choosing-a-target)
  - [5.8 The boats' attack manoeuvre](#58-the-boats-attack-manoeuvre)
  - [5.9 Shooting](#59-shooting)
  - [5.10 Ambush](#510-ambush)
  - [5.11 Escort](#511-escort)
  - [5.12 GotoWithInfiltrate and Explode](#512-gotowithinfiltrate-and-explode)
  - [5.13 Bombers and their turrets](#513-bombers-and-their-turrets)
  - [5.14 Towers](#514-towers)
  - [5.15 Tanks and crawlers](#515-tanks-and-crawlers)
  - [5.16 Big ships](#516-big-ships)
  - [5.17 Paralysis](#517-paralysis)
- [6. Weapons](#6-weapons)
  - [6.1 Gun models](#61-gun-models)
  - [6.2 The units' weapons](#62-the-units-weapons)
  - [6.3 Torpedo data](#63-torpedo-data)
  - [6.4 How a torpedo flies](#64-how-a-torpedo-flies)
  - [6.5 Decoys and buzzers](#65-decoys-and-buzzers)
  - [6.6 The Cluster Bomb and the Torch](#66-the-cluster-bomb-and-the-torch)
  - [6.7 End of a torpedo's life](#67-end-of-a-torpedos-life)
  - [6.8 The player's torpedo magazine](#68-the-players-torpedo-magazine)
  - [6.9 The player's turrets](#69-the-players-turrets)
- [7. Contacts, mines, debris and destruction](#7-contacts-mines-debris-and-destruction)
  - [7.1 The contact table and factions](#71-the-contact-table-and-factions)
  - [7.2 Mines](#72-mines)
  - [7.3 Debris and trash](#73-debris-and-trash)
  - [7.4 A boat's destruction](#74-a-boats-destruction)
  - [7.5 The death view](#75-the-death-view)
  - [7.6 Ground units' destruction](#76-ground-units-destruction)
  - [7.7 Big ships' destruction](#77-big-ships-destruction)
- [8. Effects](#8-effects)
  - [8.1 Explosion and debris entries](#81-explosion-and-debris-entries)
  - [8.2 Sprite sizes](#82-sprite-sizes)
  - [8.3 Sprite blending](#83-sprite-blending)
  - [8.4 Tank dust](#84-tank-dust)
  - [8.5 Explosion sizes](#85-explosion-sizes)
  - [8.6 Fades](#86-fades)
- [9. Cockpit and HUD](#9-cockpit-and-hud)
  - [9.1 Cockpit description files](#91-cockpit-description-files)
  - [9.2 Images and fonts](#92-images-and-fonts)
  - [9.3 Symbols, ALERT, THREAT and PAUSE](#93-symbols-alert-threat-and-pause)
  - [9.4 The voxel radar](#94-the-voxel-radar)
  - [9.5 The master monitor](#95-the-master-monitor)
  - [9.6 The slave monitors and their pages](#96-the-slave-monitors-and-their-pages)
  - [9.7 Sonars](#97-sonars)
  - [9.8 Target lock and keys](#98-target-lock-and-keys)
  - [9.9 Help, radio log and briefing](#99-help-radio-log-and-briefing)
  - [9.10 Terrain colours and fog](#910-terrain-colours-and-fog)
  - [9.11 Torpedoes on the radars](#911-torpedoes-on-the-radars)
- [10. Views from outside](#10-views-from-outside)
- [11. Texts and sounds](#11-texts-and-sounds)
  - [11.1 FIGHTEXT](#111-fightext)
  - [11.2 Turret states](#112-turret-states)
  - [11.3 Samples](#113-samples)
- [12. Menus, saved games and training](#12-menus-saved-games-and-training)
  - [12.1 Main menu, credits and difficulty](#121-main-menu-credits-and-difficulty)
  - [12.2 Loading and saving](#122-loading-and-saving)
  - [12.3 The autopilot menu](#123-the-autopilot-menu)
  - [12.4 Training](#124-training)
  - [12.5 Music](#125-music)

## 1. Units of measure and conventions

- Distances and speeds in the object files are in 1/32 m (and 1/32 m/s); one
  map cell is 16 m. Heights in the orders (`hei`, `mih`, `mah`) are metres
  over the ground, 32 of the game's units each, not the map's height units.
- The game has 71 scenarios: 67 of the story and the four training missions
  `strai0`–`strai3`.
- Difficulty is 0 easy, 1 normal, 2 hard.
- Angles in the object files are radians unless a key says otherwise.
- `card` and `car` values count in eighths of a turn: 2 is a quarter turn,
  4 half, 6 three quarters.

## 2. Scenarios and their units

### 2.1 Unit types and object files

The `dtyp` of a unit picks its object file in `VFX/SOBJECTS` (the number of
stock movables of each in parentheses).

| typ | Class | dtyp → file |
|---|---|---|
| 2049 | Scout → SimpleBoat | 1, 8, 9, 10, 11 prscout1 (56), 12, 13 prscout2 (73), 14 moghboot (95), 15 indrboot (20), 39, 67, 88 anscout3 (60), 89 anscout4 (76; also the default) |
| 2050 | Bomber → SimpleBoat with turrets ([5.13](#513-bombers-and-their-turrets)) | 16, 40, 41, 42 prbomber (18), 43 biobombr (52), 44 russbomb (46) |
| 2051 | Tank | 2 tank0 (29), 3 tank1 (57), 4 tank2 (38), 65 tank3 (11); 66 tank4 unused |
| 2052 | GunTower → TurretBase | 5, 6 guntow1 (12), 7, 90 guntow3 (96) |
| 2059 | TorpedoTower | 36; 37/38 have no files and are unused |
| 2058 | Mine | 27 passive (`MINE0.DES`), 28 active (`MINE1.DES`); a mine entry is a column of `num` mines `dis` metres apart upwards, the lowest `hei` metres over the ground (407 mines in 10 scenarios) |
| 2062 | Building → SimpleObject | 45 build0 (73), 46 build1 (145), 47, 48, 49 build4 (7), 50, 51 build6 (19), 52 build7 (55) |
| 2063 | CargoShip | 68–77 cship0–9, 78–87 bship0–9, 91 bship10 (72 in total) |

Bombers behave like boats and carry turrets on top
([5.13](#513-bombers-and-their-turrets)). Ground plans (the `GP*` clusters)
can't be destroyed.

### 2.2 Cargo ships

A cargo ship is assembled from `CargoBlockN` sections (`Name`,
`ConnectReference`, `ConnectSide` with L1 R2 F4 B8 T16 B32), block scales
312/1600, with turrets from `bblock` `[TurretN]` attached by `PatchNumber`: a
block touches the side of its reference block, with right as +x and front as
+y (the half-hulls' open seam at +x confirms this); a turret stands at
`RelativePositionX/Y` on the hull surface below it (`Z` +1000) or hangs under
it (`Z` −1000), turned by `Alpha` counter-clockwise from the bow, at its own
`[Size] Scale`/32. The mission position is the middle of the hull. The block
layout is inferred from the data. The turrets are the bombers' kind and
follow the ship's order ([5.16](#516-big-ships)).

Each block is a sonar contact and a target of its own. A big ship takes hits
as a whole on its own `[Defense]` shield (cargo ships 20,000–60,000, Big Fat
Mama 10,000,000). The block hit takes them on its own shield as well
(`BBLOCK*.DES`, e.g. 10,000): when 60 % and again 80 % of it are gone it
shows 6 to 11 small explosions over 1.5 s (entries 8, 5, 0 and 5 alike), each
0.05 to 0.1 of the block's radius (at most 20 m) in size, spread over 0.8 of
that in the plane of the screen about a point 0.6 radii from the block's
origin towards the eye and raised by 0.15 to 0.5 of its height, and then it
is whole again; a block never goes on its own. What happens when the ship's
shield is used up is in [7.7](#77-big-ships-destruction).

The master monitor shows a picture chosen by the ship file's comment:
`SHPSEACO` (bship0), `SHPTRITO` (1, 8), `SHPPOLAR` (2, 9), `SHPINDER` (3, 10),
`SHPBIONT` (4, 7), `SHPBFM` (5, 6), `SHPCONTF` (cship0–9). Big ships turn at
most 0.15 rad/s and climb at most 5°. Their orders are in
[5.16](#516-big-ships).

### 2.3 Unit keys

- `sle`: 1 (or no key) lets the unit sleep while the player is far away
  ([5.1](#51-sleeping-units)); 0 keeps it awake. `sle` is 0 on units that
  have to travel across the map (800 of 949 scouts, 195 of 244 bombers, 72 of
  135 tanks) and 1 on nearly all towers, mines and turrets.
- `diff`: the lowest difficulty at which the unit exists; a missing key is 0.
- `inte` and `eint`: the unit's armour and energy defence in percent of its
  type's.
- `eng`: the engine model 0–5 ([4.2](#42-the-energy-pool)); the object files'
  `[Engine] Type` is never read.
- `gun`, `tor`, `ntor`: the gun model, the torpedo type and the number of
  torpedoes ([6.2](#62-the-units-weapons)).
- `sen` and `senr`: the sensor type and its range in metres
  ([5.6](#56-sensors)); without the keys a passive sonar of 100 m.
- `card`: turns the player and active buildings a quarter turn per two steps
  (0, 2, 4, 6; any other value leaves them as 0, and STRAI3's player has 7),
  cargo ships one eighth turn per step. Ground plans are in
  [3.2](#32-buildings-and-the-ground-under-them).
- `pz` is read for every unit as a height in cells and is 0 everywhere.
- `ai` (0–3 on some boats, bombers and tanks) is never read.
- `iff` is the unit's faction. Objects without an `iff` (the plain buildings)
  have no faction ([7.1](#71-the-contact-table-and-factions)).

### 2.4 Orders

Each movable has up to 8 order slots `o<N>…` (N = 0–7), e.g. `o0typ`,
`o0refx`. Every slot has a break condition: `o<N>bccond1/2`, `o<N>bcdep1/2`,
`o<N>bcref1/2`, `o<N>bcop`, `o<N>bcdel`. Condition code 47 (or 0 in older
files) means "no condition".

| otyp | Name | Keys | Movables (slots) / scenarios |
|---|---|---|---|
| 1 | Wait | dur | 930 (997) / 66 |
| 2 | Goto | refx refy vel hei | 451 (816) / 51 |
| 3 | Patrol | refx refy rad mih mah vel dur | 965 (1297) / 58 |
| 4 | Attack | Patrol keys + ene | 435 (506) / 52 |
| 5 | Escort | esc xof yof zof mih mah vel dur | 64 / 15 |
| 6 | Ambush | refx refy car ene mih mah vel dur | 39 / 12 |
| 7 | CardinalPatrol | rad car dur | 420 / 36 |
| 8 | CardinalAttack | rad car dur ene | 42 / 4 |
| 9 | GotoWithArrive | refx refy vel hei | 55 / 28 |
| 10 | GotoWithInfiltrate | ene mih mah vel dur | 21 / 13 |
| 11 | Explode | dur con | 72 / 16 |

- `refx`/`refy` are map cells; `hei`/`mih`/`mah` are metres over the ground;
  `vel` is a percentage of the unit's MaxVelocity; `dur` is in seconds
  (100000 or 1000000 means forever); `rad`, the escort offsets and the
  escort's `ran` (how far from its place it fights, 150 m if missing, as it
  is in every mission) are metres.
- `car` takes 0/2/4/6/10, a quarter turn ([5.14](#514-towers)). Explode
  `con=2` is used 71 times, `con=1` once (S00202)
  ([5.12](#512-gotowithinfiltrate-and-explode)).
- `[Attack]` in boat and bomber files drives the attack manoeuvre
  ([5.8](#58-the-boats-attack-manoeuvre)):
  `AlignProbabilityFront/Back/Left/Right`, `Min/MaxAlignRadius` (game units,
  32 per metre), `Min/MaxAlignGamma` (radians), `Min/MaxAlignTime`;
  `AttackProbabilityTurn`,
  `AttackProbabilityHit` (the chance to attack at once when fired at),
  `Min/MaxAttackTime`; `Min/MaxFlightRadius`, `Min/MaxFlightTime`,
  `FlightPointProbabilityUL/UR/DL/DR`, `FlightProbabilityHit` (the chance to
  break off when hit); `*ProbabilityTorpedoFire` per phase. The files' own
  comments (in German) describe them. Tanks use Hook/Cross/Retain/Change/Hit
  variants ([5.15](#515-tanks-and-crawlers)). Also `Min/MaxClimbingAngle`
  (read but never used) and the turret `[Kinematics]` `MaxAngleVelocityA/B`.
- The boats' and tanks' object files have a negative `MaxAngleVelocityC`
  (−1.0; the torpedoes −1.8) beside their turn rates; it does nothing.

### 2.5 Conditions and events

| Event | Completes | Objectives (wccond) |
|---|---|---|
| destroy | armour below zero | 2, 5, 9 |
| attack | first hit by another unit | 3 |
| identify | sonar | 4 |
| paralyze | energy defence below zero ([5.17](#517-paralysis)) | 6 |
| finish ("ist angekommen") | own GotoWithArrive reaches its point | 7 |
| board ("wurde infiltriert") | target of another unit's GotoWithInfiltrate | 8 |

- "Arrived" (conditions 43/44, objective 7) completes when a GotoWithArrive
  reaches its point. "Boarded" (45/46, objective 8) completes on the `ene` of
  a GotoWithInfiltrate, once the boat has sat at its target for `dur`
  seconds.
- Faction references (`bcdep` 2, `bcref` is the faction's `iff`): 1 means
  every unit of the faction is destroyed, 39 its negation, 2 any unit of the
  faction attacked. A faction without units is never destroyed, so it counts
  as alive.
- 0 and 47 both mean "no condition", in either slot.
- "Objective achieved" on an objective that can only fail (wccond 5) means
  "not failed yet" (the waves in S01301 and S01805, the sensor areas in
  S00405). wccond 2 on a nav point means "reached".
- "destroyed" on the player's id means the player died.
- A unit counts as destroyed for the mission only when its hull is gone, at
  the end of its destruction sequence ([7.4](#74-a-boats-destruction)); the
  player's boat at once. A big ship counts as destroyed when its first block
  is gone ([7.7](#77-big-ships-destruction)).
- A unit set off by an Explode order is not destroyed for the mission's
  conditions: its armour is untouched ([5.12](#512-gotowithinfiltrate-and-explode)).
- The squadron events: "attacked" on the first hit on any of its units,
  "arrived" and "boarded" once every unit has, "destroyed" once every one is
  destroyed.

### 2.6 Mission failure and friendly fire

- Mission failure plays its message once and ends the mission 10 s later.
- Friendly fire: the game notes each frame in which the player's shots or
  torpedoes hit a unit friendly to the player (faction 8), destroy
  objectives too (in s01805 three of the friendly tanks the mission wants
  destroyed fail it); ramming counts only on a big ship. Every tenth such
  frame shows one of FIGHTEXT 67, 68 and 69 at random, after the unit's name
  in brackets, on the radio line in green, unless another message is on it
  (then it is lost). Each friendly kill goes only to the radio log, in
  amber: 71 with the name, at the second also 70 ("one more and you are
  out"), at the third 89, and from the third on the mission counts as
  failed, even an accomplished one.
- The player is faction 8, which no unit in the missions shares, so the
  player's shots hit friends.

### 2.7 Mission end and result

- The game writes `dat\save\fightout.des` with `[Global]` completed / failed
  / primary / user / secondary, and `Destination` = AutoEnd | InstantEnd |
  <station> | RestoreAutoSave | RestartScenario | AbortGame.
- The scenario's `[End] Type` (0 in most) picks the destination of an
  accomplished mission: 1 AutoEnd, 2 InstantEnd, 3 the station st1703, 4 the
  station st9901; otherwise the destination is what the player chose in the
  mission menu or the autopilot menu. Type 1 is in 26 scenarios, 2 in the
  training missions, 3 in S00704 and 4 in S02205. The freighter st1703,
  station 3 of chapter 7, is closed at the chapter's start and opened by
  dialogue 007008.
- The mission's end lines are FIGHTEXT 11–13; the "accomplished" voice plays
  only with type 0.
- Once the mission is accomplished, Alt+A opens the autopilot menu
  ([12.3](#123-the-autopilot-menu)).

### 2.8 Quirks of single missions

- Five missions reference 13 condition codes for objectives that no unit
  has (S00404, S01802, S02003, S02006). They never come true, and all five
  missions can still be won.
- s01802 lost two tanks' trigger area when it was made from s01801.
- s02006 kept a radio message of a dropped objective: the plant's "direct
  hit" line waits on objective 7 failed, which s02005 has.
- s00404's eight late units wait for area 2087 *or* objective 4, so only the
  area calls them.
- s02003's area 2049 waits for objective 13 *or* generator 2071's loss, and
  the collector 2093's first order for objective 15; both never happen.
- s00402: radio 5124 ("wir werden ihn rächen") waits for scout 2091's loss
  *and* faction 1 alive, but every pirate is faction 5. A faction without
  units counts as alive, so the line plays at its delay.
- S00705: only the player can destroy Marquez; what Schwartz (2066)
  infiltrates is open (TODO.md T5).
- STRAI3's freight containers name a reference cell 23 cells away, on a steep
  slope, and hang at its height, one of them over another
  ([3.2](#32-buildings-and-the-ground-under-them)).
- S00202 has three pipelines that run 13 m over the bottom of the dip they
  cross, and two upward current cells of which one does nothing.

## 3. The world

### 3.1 Terrain shading

The cockpit description's `[Camera]` section holds the numbers:
`AmbientBrightness` 2.0, `CueingBackPlane` 1000, `LightBrightness` 5.0,
`LightPower` 1.0, `LightFrontPlane` 0 and `LightBackPlane` 3650, in units of
1/32 m (31 m and 114 m).

Every vertex of the terrain gets a brightness of ambient × (1 − d/cueing),
not below zero, plus light × ((back − d)/(back − front))^power × cos, where d
is the distance from a point 8 m above and 4 m ahead of the eye and cos the
cosine between the vertex's vector and the direction to that point, not
clamped; the light part is nothing beyond its back plane. The vertex's vector
is the normalised sum of the normalised normals of the six triangles around it
(the `gouraud.xyz` file the surface descriptions name would replace that, but
none exists).

The brightness is a multiple of the texture: 2 is the shading table's last
row (twice the palette), 1 its sixteenth, so the row is 16 × brightness
rounded down, kept between 0 and 31, chosen per pixel from the brightness
interpolated across the triangle; row r blends (r + 1)/32 of twice the
texture with the rest of the table's own colour F
([9.10](#910-terrain-colours-and-fog)). So the ground at the eye is twice its
texture, at 16 m once, at 31 m the ambient part is gone, and from there only
the light counts: at 60 m a wall facing the boat keeps 1.9 (row 30), flat
ground 11 m below 0.45 (row 7), and at 90 m nothing of the flat ground is
left. A fade at a mission's start and end scales the whole brightness
([8.6](#86-fades)). The energy shots' lights add to the vectors of the cell's
corners under them, so they brighten the ground through the same rows.

The models follow the same rule, clamped and rounded the same way, and the
polygons pick the row per pixel from the brightness interpolated between the
corners, but with their own light: models with `ShadingMode f` (every unit,
turret, shot and explosion, buildings, most ground plans and the container
freighters' blocks) are lit from the eye itself, so every face turned to the
viewer is lit, undersides too; those with `ShadingMode g` (the battleship
blocks and a few ground plans) from a point 4 m above and 8 m behind the
eye. There is no other light on them, neither a headlight nor an ambient
colour. Their own shading tables (`MOD_nnn.S16`) hold the palette itself in
the last row, not twice it, with the same blue F in the first, so a model at
the eye shows its texture as it is and one 60 m ahead, facing the boat,
keeps 1.9 of it clamped to the texture. The model files carry no normals.

### 3.2 Buildings and the ground under them

- **The square.** A ground plan of size `siz` covers a square of `siz` + 1
  cells; cluster offset 0, 0 is its corner with the lowest cell numbers
  (`px`, `py` − `siz`). `card` turns the square clockwise about its middle, a
  quarter turn per two steps (2 east, 4 south, 6 west, as the currents
  count), and every cluster's model with it; the cluster's own `Cardinal`
  turns its model on top of that. So a turned plan of more than one cell
  keeps its middle, and a line of pipe segments turned by 6 runs from its
  elbow northwards, not southwards.
- **Heights.** A cluster with `HeightReference` below 1000 stands at the
  height map's value of the scenario's `refx`/`refy` cell (with `-1`, of its
  own cell), whether it lifts the ground or not; with `1000 + n` it sits on
  cluster n's roof. A cluster with `o` keeps that height over lower or higher
  ground. The pipelines rely on it: each segment (`GP1X1_8`, `rohr_005`)
  names the cell of the line's first piece as `refx`/`refy`, so the line runs
  level from the top of that piece, an elbow (`GP1X1_7`, `rohr_004`, with
  `p`) rising from its flattened ground, across dips and rises.
- **Flattening.** A cluster with `SurfaceLifting = p` (and every one with
  `-1`) sets the four corners of its cell, its own and the ones east, south
  and south-east of it, to that height. Clusters two cells apart therefore
  flatten a four-by-four area together, but a single cluster leaves its
  western and northern neighbours alone. Gun towers, torpedo towers and
  active buildings (`typ` 2052, 2059, 2062) flatten their own cell to the
  height of its corner in the same way. Where lifted cells next to each
  other differ (pipes stepping up a slope) they share corners, so their
  ground slopes.
- **Darkening.** Each terrain vertex carries the vector the light part of the
  shading takes its cosine from ([3.1](#31-terrain-shading)). A cluster with
  `p` or `l` sets the vectors of its cell's four corners to 0.35 of their
  length, as do torpedo towers and active buildings; gun towers set them to
  0.25. So beyond the ambient part's reach (31 m) the ground under and around
  buildings and towers keeps only that share of its light, fading back to the
  full share over the neighbouring cells. A unit does this when it appears: a
  tower that waits for its enable condition leaves the ground as it is until
  then. In STRAI3 this darkens 449 corners. When a building or tower is
  destroyed the ground under it turns light again; the flattened ground
  stays.
- **The height map** places the boat at s00202's start 57–60 m over ground
  at 48 m, and at s00101's start 57 m over the ground.

### 3.3 Currents

- `[CurrentNNNN]`: `typ=3072`, `px`/`py` (cells on a 2-cell grid), `pz=0`,
  `card` 0–9, `spd` 0 or 2. 1,266 entries in 20 scenarios. The direction is
  (sin(card·45°), cos(card·45°), 0); 8 is up and 9 down. A cell covers the
  two map cells from `px`,`py` upwards, with no fade at its edges.
- The push is an acceleration of 200 units/s² (6.25 m/s²) for `spd` 0 and
  400 (12.5 m/s²) for `spd` 2, with no falloff, held across the boat by its
  drag and along it only by its engine (no cap). So with the boat at rest in
  current cells, `spd` 2 takes it from rest to about 41 m/s (HUD 141 km/h)
  within about 5 s facing along the current, to about 19 m/s facing across
  it; `spd` 0 to about 5 m/s; downward cells push 9 m/s (`spd` 2) or 4.5 m/s
  (`spd` 0), upward ones about 5 m/s (`spd` 0). The engine's state doesn't
  matter, and a boat that leaves the cells coasts and slows at 3.5–4.7 m/s².
- Affected: the player (`[Kinematics] Current`) and torpedoes at a factor of
  1. The missions' units don't drift, nor do shots. Passive mines stay where
  they are laid, in a current too; an active mine's drive has a current
  factor of 0.2, and whether that shows while it chases is open.
- The HUD shows the current symbol (`HUDSYM3`) while the boat is in a cell
  ([9.3](#93-symbols-alert-threat-and-pause)), and the voxel radar colours a
  current's four map cells as ground, dark teal for `spd` 0 and light cyan
  for `spd` 2 ([9.4](#94-the-voxel-radar)).

### 3.4 Radiation and the surface

Radiation has no data key: it is a depth rule, the same on every map. Its
strength is 1 − depth/100 m, from 0 a hundred metres down to 1 at the
surface.

- Above 99 m (a strength over 0.01) the red radiation symbol (`HUDSYM2`)
  blinks at 1 Hz in the first slot of `[HUDSymbolBlock1]` and the depth
  readout turns red.
- All five monitors flicker: each refreshes at its own rate (master monitor
  with the voxel radar every 0.1 s, slave monitors 0 and 1 every 0.2 and
  0.18 s, round monitors 0 and 1 every 0.21 and 0.23 s), and a refresh draws
  it with a chance of 1 − 0.7 × the strength (0.3 at the surface) and leaves
  it blank otherwise until the next one, so the slower monitors stay dark
  longer.
- No damage.
- Less than 40 m below, the throttle is held at 1 m/s (the booster does
  nothing) and the boat is pushed down at 1 g until it is more than 70 m
  below again; the throttle stays at 1 m/s after that. The boat never comes
  closer than 10 m to the surface. So a boat at full speed straight up coasts
  from 40 m to about 25 m before it sinks back.
- Less than 60 m below, `ALERT` blinks out of phase with the symbol, the
  voice "system collapse" (sample 72) plays on entry and every 15 s and the
  alert beep (sample 10) every 0.8 s. The same ALERT, voice and beep are the
  combat alert, together with the marker under fire when the attackers are
  what brought the alert on.

## 4. The player's boat

### 4.1 Turning

The pitch and yaw rates relax towards the stick's share of `MaxAngleVelocity`
by the boat's `AngleFriction` fraction (0.4 for pitch, 0.7 for yaw on the
Hiob) once per update: rate += (target − rate) × friction; the frame time
only turns the boat by the rate. Each update the nose also drops by 0.01 ×
its own `dir.z` × the pitch maximum, and the roll turns the boat by roll ×
`RollCenterVelocity` × the yaw maximum. So a pressed key reaches the maximum
rate only asymptotically and the rate dies the same way after the release,
which is the momentum felt in the game. A joystick's value goes in as it is.

### 4.2 The energy pool

The player's boat and every unit with a `gun` has an energy level from 0 to
1, full at the start, refilled by its engine model from empty in 5 s (engine
0), 3 s (1), 1 s (2–4) or 0.01 s (5). The player's model is the boat's engine
(`engine` 0–2 for the Small, Medium and Big Block), a unit's its `eng` key.
The HUD's energy bar shows the level, never less than 4 pixels.

### 4.3 Firing the gun

- **Energy guns.** Models 0–5 (the ones with `energydemand` above 0) draw
  from the pool; models 6–13 use rounds, and the energy guns show "ENER" in
  the place of their rounds. A gun fires its barrels in turn: models 0 to 13
  have 1, 2, 1, 2, 4, 1, 2, 2, 2, 3, 4, 2, 2 and 2 barrels. An energy gun
  fires one barrel per trigger pull with the rearming time between. A shot
  takes `energydemand` divided by the barrels off the level (never refused)
  and the gun is ready again once its `rearmingtime` has passed and the level
  is above the whole `energydemand`. So a full pool allows a burst at the
  rearming rate, after which the gun settles to one shot per (`energydemand`
  / barrels) × recharge time: a Plasma Death II (0.3, 2 barrels, 0.075 s) on
  engine 2 fires at its rearming rate for about 4 s and then every 0.15 s, on
  engine 0 every 0.75 s.
- **Rearming time and frames.** The rearming time is the gun's `rearmingtime`
  exactly, counted from the shot. A gun is ready once the time is past it,
  but the player's trigger is read at the start of a frame, before the guns
  count down, so the main gun and the turrets in sync fire on the frame
  after the one in which they become ready. At 60 frames a second a 0.075 s
  gun fires every 0.1 s and a 0.15 s gun about every 0.18 s.
- **Rounds.** An ammo gun with more than one barrel fires a round: one
  barrel per frame while the trigger is held, all its barrels in turn, and
  only after the last one the rearming time. A released trigger leaves the
  round where it is, and the next pull goes on with it. So the rate climbs
  with the frame rate: a Vendetta III (4 barrels, 0.075 s) fires about 17
  shots a second at 30 frames, 27 at 60 and 34 at 120. A round takes one of
  the gun's rounds (the `defaultammo` or a unit's `ammunition`), counted
  when its last barrel has fired.
- **Mountings and barrels.** A gun has one or two mountings (the player's
  gun one, 3 m ahead; a unit's from its `relative` keys), and each round
  fires from the next mounting. The barrels sit around it: two 0.75 m to
  either side; the Vendetta II's third 0.75 m above; the four of the Vendetta
  III and the Plasma Death III 0.75 m to either side, the second and third
  0.8 m lower. The order is right, left, (above or lower left, lower right,)
  left.
- **Units** fire the same rounds. Boats, bombers, gun towers and turrets (the
  cargo ships', TANK2's) fire before their gun counts down, so like the
  player on the frame after it is ready again; a tank's fixed gun counts down
  first and fires on the frame it is ready.
- **Towers, vehicles and turrets** fire each round from all their mountings
  (the `Relative` keys, on each arm of a two-armed gun tower). With one of
  the turret models 11–13 a mounting fires one shot, so a tower's salvo has
  as many shots as it has barrels: one from GUNTOW1, two from GUNTOW0, four
  from GUNTOW2's two arms. A mission's `gun` can put a boat gun model 0–10 on
  them instead, and then every mounting fires all of that model's barrels: a
  one-barrel tower with a Vendetta III fires four shots a salvo, GUNTOW2
  sixteen.
- The main gun and each turret's gun count their barrels on their own, both
  from the same barrel, so a cut-short burst or a volley of an odd number of
  shots leaves them firing from opposite barrels. The displays count one
  round per turn through all barrels (a Maximum Impact II starts with 900).

### 4.4 The booster

The booster draws from the same pool: on only while the level is at least
0.8, 0.15 at once, 1.2 per second while it runs (about 4 s on engines 2–4),
off below 0.05. It doubles the top speed and raises the acceleration
2.5-fold, works from a standstill, and the engine's pitch follows the speed.
Its sound is sample 29.

### 4.5 Jams

Jams are decided before each shot: the gun's condition (1 when new, the saved
`Defect`, at least 0.2) against a random number times the gun's
`defectdamping`; it jams when the product reaches the condition, so never
while the condition is at or above the damping (0.2–0.8). A jam lasts
`defectdelay` × 0.5 to 1.5, then the gun is ready at once. The torpedo
magazine jams the same way ([6.8](#68-the-players-torpedo-magazine)).

### 4.6 Armour, wear and the Fixer

- **Armour.** The player is hit per side (front, left, right, back). A side's
  full armour is the boat's `kineticshield` plus the fitted armour's. The
  armour symbol on the HUD shows a bar per side
  ([9.3](#93-symbols-alert-threat-and-pause)).
- **Wear.** Hits on the armour wear the systems down. Once a side is below
  45 % of its full armour, what a hit takes from it, counted from 45 % down,
  comes off the systems behind that side 2.2 times over: the front's off the
  gun's and the torpedo magazine's conditions, the left, right and back
  sides' off both turrets'. A condition never goes below 0. A hit that takes
  the front from 50 % to 30 % costs the gun and the magazine a third of their
  full condition each (0.15 × 2.2). The lower a condition, the more often that
  gun, turret or magazine jams. Shock hits lower the boat's energy defence,
  but nothing on the boat reacts to it.
- **The Fixer** (item 9218) works one step a second on one system at a time:
  the armour, the gun, the torpedo magazine, turret 1 or turret 2, in that
  order. It starts on the armour. Left to itself it takes the first system
  below a quarter of its full state, else the first below a half, else the
  first below three quarters (the armour counting by the side it is mending,
  or as intact while it mends none); Alt+I forces one (the cycle automatic,
  armour, gun, torpedo, the turrets the boat has) and the system state page
  marks the mended and the forced system. Each step adds a 160th of the full
  state: to the weakest armour side (a 160th of that side's armour), or to
  the gun's, magazine's or turret's condition. A system at three quarters or
  more is left alone and the Fixer moves on to the next; the armour hands
  over once every side is at three quarters. The forced system is forced only
  until it reaches that mark.
- **The system state page** is in [9.6](#96-the-slave-monitors-and-their-pages).

### 4.7 Damage between missions

The save file keeps each armour side (`[Defect]` `FArm`, `LArm`, `RArm`,
`BArm`) as a share from 0 to 1 beside the gun, the torpedo magazine and the
turrets, and the depot repairs them, so a mission's damage stays with the
boat. What the Fixer mends stays with the boat as well.

## 5. Units' behaviour

### 5.1 Sleeping units

A unit with `sle` 1 (or no key) is put to sleep while its horizontal distance
from the player exceeds the player's sonar range plus 3 map cells (48 m on
most maps): its engine sound stops and it is not updated at all until the
player comes back, when it starts its current order slot again. Boats don't
sleep while dying, exploding or in an Explode order; towers sleep even while
they fight, and a bomber's or a big ship's turrets rest while it sleeps.

### 5.2 Attacker lists

Every target keeps a list of its attackers. A unit that takes the player as
its target joins the player's list, 4 units on easy, 6 on normal, 8 on hard;
every other unit's list takes 4 at any difficulty, a ground plan's too, and
towers and turrets count as attackers as well. A unit that finds the list
full gets no target. A unit takes at most 4 homing torpedoes on it
([5.9](#59-shooting)). The player's list also drives the marker under fire
and the "lock-on located" voice ([9.3](#93-symbols-alert-threat-and-pause))
and the `u` key ([9.8](#98-target-lock-and-keys)).

### 5.3 Order slots and squadron events

Order slots end in two ways: the break condition is checked every frame, and
the orders finish on their own (Wait, Patrol, Attack, Ambush, Escort,
CardinalPatrol and CardinalAttack after `dur`, Goto and GotoWithArrive at
their point, GotoWithInfiltrate once the boat has sat at its target for
`dur` seconds, when the target counts as boarded). A unit's 8 slots are taken
in turn, skipping empty ones, round and round. Boats skip CardinalPatrol and
CardinalAttack slots at once; a tower's Wait faces the centre of its last
cardinal order. The squadron events are in
[2.5](#25-conditions-and-events).

### 5.4 Turning and banking

A boat turns like a damped spring: an error in heading or pitch beyond 1°
accelerates the turn by its `MaxAngleAcceleration` times the error over π,
its `AngleFriction` damps the turn rate, and the rate is held to
`MaxAngleVelocity` (an ANSCOUT1 turns 0.8 rad/s per radian of error, builds
up within about 0.8 s and overshoots by about 9 %). Within 1° only the
damping acts, so the boat stops turning without sitting exactly on its aim.
The pitch goes for the true elevation of the point: `Min/MaxClimbingAngle`
are read but never used, so a boat under or over its target pitches straight
at it. The speed is the order's, with no slowing down in turns. The hull
banks by as many radians as it turns radians a second, following within
about a second (69° for an ANSCOUT1 at its full rate); `MaxAngleVelocityC`
does nothing. Bombers turn and fly exactly as boats do.

### 5.5 Flying to a point and patrolling

- **Flying to a point** (Goto, GotoWithArrive, the patrol points, the align
  and flight points, the ambush point). The point is `hei` (or `mih`) metres
  over the ground. The boat flies straight at it, height and all, as long as
  the front bottom corners of its hull, half a cell plus the way it flies in
  0.75 s ahead, stay within a band of `mih` plus 0.6 hull radii to `mah` over
  the ground under them (Goto and GotoWithArrive have no top). Below the band
  it turns aside towards the corner with more room and up, to a point
  31.25 m off at the band's floor; above it, it goes down to the top; both at
  once, it turns aside. In patrols, attacks and ambushes other boats ahead
  bend the way: one within 3.5 hull radii (plus its own) of the boat pushes
  the steering point away, fully within 1.5. It slows down proportionally
  within 2 cells (or its hull radius) of a point and counts as there within
  1.25 cells, its hull radius or the way it flies in a second, whichever is
  largest.
- **Patrol.** The first point is the area's centre; a new one is picked when
  the point is reached, and once the time it takes to fly the radius at the
  order's speed has passed since the patrol began (or resumed after a
  fight), a new one every frame, so the boat wanders: a random point of the
  circle at least half the radius away (an Attack order patrolling its area:
  0.3 of the radius from the last point), its height wandering by up to half
  the horizontal way, clamped to the `mih` (plus 0.6 hull radii) to `mah`
  band.

### 5.6 Sensors

Every unit has one; the default is type 1 with 100 m. The range check is the
same for all: the other unit within the range.

| Type | Hears | Takes |
|---|---|---|
| 0 sight | a unit in range with nothing of the terrain between the two | the nearest |
| 1 passive sonar | a unit while its noise over the squared distance beats 1500 over the squared range, in noise units of 500 per `[Noise] Level` step: a level‑3 unit up to the range, level 2 to 82 %, level 1 to 58 % of it | the loudest over its distance |
| 2 active sonar | everything in range; adds 500 (one level) to the unit's own noise | the nearest |
| 3 | buzzers only | the nearest buzzer, whoever dropped it |
| 4 | buzzers, or others by the passive rule | the nearest hostile buzzer, else the loudest |

The sensor looks for hostiles every 1.5 s (0.7 s for types 3 and 4) and keeps
what it found while that lives. Types 1 and 4 also hear a hostile Torch's
torch at noise level 4 ([6.6](#66-the-cluster-bomb-and-the-torch)); no other
type takes buzzers or torches, and dummy buzzers draw nobody. The scenarios
set `sen` and `senr` on most boats, always type 1 or 2 (`sen 2` with 60–200 m
is common). Towers and turrets use theirs the same way.

### 5.7 Choosing a target

Patrol, Attack and Ambush orders fight; Wait, Goto, GotoWithArrive and
GotoWithInfiltrate never do; an escort defends only itself and its charge
([5.11](#511-escort)). Each frame: the order's enemy (`ene`) if the sensor
has it within its range; else, in the moment the unit is hit, whoever fired
that hit (a shot's or a torpedo's owner, any unit, not only the player) if
hostile and within twice the range; else what the scan found. While
fighting, the range counts double: the target is dropped once it is beyond
that (or no longer heard or seen by the type's rule), and the order goes on
where it was with its time still running; the enemy takes over again once in
range; whoever hits the unit takes over if nearer than the current target.
"Hit" is a change of the armour in that frame; shells without shock strength
count. A unit that finds the target's attacker list full gets no target
([5.2](#52-attacker-lists)).

### 5.8 The boats' attack manoeuvre

Boats and bombers fight in three phases with their `[Attack]` keys.

- *Align* – pick a side by the four align probabilities (ahead within 30°,
  left or right between 30° and 150°, behind within 30°), a distance between
  the align radii and an elevation between the align gammas, and fly to that
  point in the target's frame at the order's speed; when the boat is on that
  side at a distance between the radii the run starts; after the align time a
  new side is picked; when hit, with `AttackProbabilityHit` the run starts at
  once, otherwise a new side is picked.
- *Run* – lasts an attack time; with `AttackProbabilityTurn` the boat stops
  and turns on the spot, otherwise it flies at the target at 40–100 % of its
  speed, bent away from other boats; once its hull's way ahead leaves the
  band of `mih` plus a hull radius to `mah` over the ground it turns on the
  spot instead for the rest of the run. It ends when the target has come
  within a flight radius, when the time is up, or, with
  `FlightProbabilityHit` decided at the start, with the first hit taken after
  the boat has fired.
- *Flight* – to one of four points by the UL/UR/DL/DR probabilities, 850 m
  left or right, 500 m ahead and 500 m above or below the target in its
  frame, for a flight time; after 1.5 s the point drops to the target's
  height, and a boat sitting in the target's line of fire (within 8°) for
  1.5 s swaps sides; then it aligns again.

An ambusher skips the first alignment and runs at once.

### 5.9 Shooting

- A boat fires whenever its gun is ready, its `[VolleyGun]` is in an active
  phase and the target lies within 14° of its nose, at any distance, whatever
  the terrain between, at the target's position with no lead (a ground unit
  0.4 hull radii higher). The volley starts again at its first phase whenever
  the target leaves the 14°. The shots leave from the `[StaticGun]` mountings
  along the nose; the mission's `gun` replaces the file's model, with that
  model's own ammunition. What makes a shot hit is the turning
  ([5.4](#54-turning-and-banking)): a boat that has turned towards a
  standing player keeps hitting it.
- A torpedo is tried, once per phase when that phase's
  `*ProbabilityTorpedoFire` came up, whenever the tube has reloaded, and
  every try reloads it (2–3.5 s), whether a torpedo goes or not: it goes from
  the next `[TorpedoMagazine]` tube when the nose points at the target within
  18° in the horizontal plane (`tor` 0 and 1), or within 45° for the others,
  as long as the target has fewer than 4 homing torpedoes on it.
- The buzzer magazine ([6.5](#65-decoys-and-buzzers)) works only in patrols,
  attacks, ambushes and escorts, looking every frame until it has thrown its
  first buzzer.

### 5.10 Ambush

The boat flies to its point, then lies there without any noise (so passive
sonars don't hear it), level and pointing the way `car` says, its centre held
at twice its hull's depth over the ground by a push of 0.05 to 0.5 g up or
down, until the sensor finds something; being hit makes the shooter known at
double range, as everywhere. Losing the target sends it back to its point to
lie in wait again.

### 5.11 Escort

The place is the escorted unit's position plus the offsets in its frame,
each axis measured from the face of the escorted unit's hull box to the face
of the escort's, so (7, −2, −6) m puts it 7 m clear to the side, 2 m behind
the tail and 6 m below. While closing in, a place ahead of the escorted unit
(`yof` > 0) is taken 1 m behind it. The escort closes in at the order's
speed, slower over the last 75 m and never faster than 1.1 times the
escorted unit (so the escort of a unit standing still doesn't move), counts
as in place within its hull, 1.25 cells or its top speed in a second, and
then matches the escorted unit's heading, pitch and speed, pulled into its
place; it leaves formation beyond 1.3 of that. It doesn't look for targets:
it takes on whoever hits it, whoever hits its charge, or the first of its
charge's attackers it detects, all at double range, and fights only within
`ran` (150 m) of its place; the order ends when the escorted unit is
destroyed.

### 5.12 GotoWithInfiltrate and Explode

- **GotoWithInfiltrate.** The boat flies to a point beside its target, on
  the side it comes from, half its own hull radius clear of the target's, and
  sits there with its nose to the target, counting `dur` down while it is
  within 1.5 times its top speed in a second (or its hull, or 1.25 cells) of
  that place; leaving it doesn't reset the count. Then the target counts as
  boarded.
- **Explode.** `con` (with `dur` in seconds): 0 explodes when `dur` is over
  unless the unit has been boarded by then; 1 waits to be boarded and
  explodes `dur` seconds after that; 2 explodes when `dur` is over no matter
  what. The order freezes the boat where it is. The explosion is the
  destruction sequence of a destroyed unit ([7.4](#74-a-boats-destruction));
  the armour is untouched, so the unit is not "destroyed" for the mission's
  conditions. Of the ground units only buildings follow an Explode order; a
  big ship's Explode is its block sequence ([7.7](#77-big-ships-destruction)).

### 5.13 Bombers and their turrets

Bombers fight, fly and obey their orders exactly as boats do; on top of that
each carries the gun turrets of its file's `[Turret0]` to `[Turret7]`
(ANBOMBR1 4, ANBOMBR2 8, ATBOMBER 3, PRBOMBER 4, BIOBOMBR 6, RUSSBOMB 4),
each on the hull at `RelativePositionX/Y` (z ±1000: on top or hanging under
it), facing `Alpha` (counter-clockwise from the bow).

- A turret (`TURRET*.DES`) is a unit of its own, of the bomber's faction,
  with its own `[Defense]`, `[Noise]` and `[Sensor]` (`sen 1`, 90–120 m);
  hits on it are attacks on the bomber, and losing it doesn't count as a
  loss of the bomber's squadron.
- It follows the bomber's order: on Wait, Ambush, the cardinal orders or
  none it rests, facing its mount's forward, without looking for anyone; on
  Patrol (for the order's `dur`), Goto, GotoWithArrive, GotoWithInfiltrate
  and Escort it sweeps the half circle about its mount's forward (to an
  angle, a pause of 0.5–2.5 s once there, then a step of 15°–90°, back from
  the edges) and takes on whoever hits it or what its sensor finds, dropping
  a target it no longer detects (Goto and the like keep the first one); on
  Attack it faces forward and takes the order's enemy whenever it detects
  it, else whoever hits it or what it finds, chosen anew every frame.
- It tracks within 90° of its mount's forward (beyond, it turns back to it),
  its yaw a damped spring as a boat's (at a fixed rate on TURRET9 and
  BTURRET7, `SoftBodyAdjust = no`), its guns at `MaxAngleVelocityB` from 2°
  down to straight up, leads the target exactly with the shot's speed and
  fires both arms' guns within 14° of it in its volley's rhythm, never with
  its guns at their lowest.
- Paralysed, on its own or with the bomber, it turns level along its mount
  for good; its armour gone, it blows up in two seconds and is gone. A
  sleeping bomber lets its turrets rest.
- A bomber's destruction is in [7.4](#74-a-boats-destruction).

### 5.14 Towers

- `car` (CardinalPatrol, CardinalAttack, Ambush) is a quarter turn: 2 is 90°
  one way, 4 is 180°, 6 the other way; 0, odd values and the frequent 10 are
  straight ahead. A tower sweeps about that heading; a boat in ambush points
  its nose that way.
- Towers look with their own sensor (`sen`, `senr`; the order's `rad` is
  never read), keep a target while they detect it and take whoever hits them
  if nearer; CardinalAttack takes its enemy (the first living unit of `ene`
  at the order's start) whenever it detects it and otherwise any hostile its
  sensor finds. Without a target both orders sweep: the turret turns to the
  centre (the `car` heading), pauses 0.5–2.5 s once there, then steps 15°–90°
  either way; for `car` 0, 2, 4 and 6 the sweep and the tracking keep within
  the half circle about the centre, bouncing off its edges (a target beyond
  it sends the turret back to the centre), for any other `car` all round.
  After losing a target the turret turns back to the centre first.
- A gun turret (`GUNTUR*.DES`) turns as a bomber's, its guns from 5° down
  (15° when turned to an angle), leads exactly and fires each arm's gun
  within 14° at any distance and whatever the terrain.
- A torpedo turret turns only about the vertical and tries a torpedo every
  2–3.5 s from its next tube (TORTUR0 has 6) along its level forward: within
  18° for `tor` 0 and 1, within 21.6° for the others while the target has
  fewer than 4 homing torpedoes on it, and only with the terrain clear
  between the tube and the target, although its active sonar hears through
  hills.
- Towers sleep while they fight, too. A tower blowing up
  ([7.6](#76-ground-units-destruction)) looks for no one, but its turret goes
  on as it was: tracking the target it had and firing or launching at it
  while that lives, for the two seconds until the tower is gone; a paralysed
  tower's turret stays still.
- Towers flatten and darken their cell when they appear
  ([3.2](#32-buildings-and-the-ground-under-them)).

### 5.15 Tanks and crawlers

Tanks drive on the ground and fight in two phases with their own `[Attack]`
keys.

- *Align*: the point lies at a distance between the align radii, 20–30° round
  the target from the tank's present bearing; which way round is decided by
  the probabilities `AlignProbabilityHook` (against the way the target turned
  in the last frame), `Cross` (round towards the target's tail), `Retain` and
  `Change` (by which way the two vehicles point relative to each other, one
  or the other). The tank drives there at the order's speed, half of it at
  both ends of the way, until it is within its top speed in a second of it
  or the align time is up; when hit, `AlignProbabilityHit` says whether it
  aligns anew or attacks at once.
- *Attack*: it drives at the target for an attack time (or until it is
  within its top speed in a second), at a speed between its `MinVelocity`
  and `MaxVelocity` times the order's percentage, so most tanks back away in
  about a third of their runs, nose on the target.
- A fixed `Gun` module (all tanks but TANK2) points along the hull and only
  tilts, to the target's elevation over the hull's plane, from 5° down to
  straight up, as a damped spring by the hull's `MaxAngleAccelerationB`,
  `AngleFrictionB` and `MaxAngleVelocityB`, and only at a target ahead, else
  back to level at once; it fires only in the run. TANK2's `GunTurret`
  (TANTUR2, whose `[StaticGun]` and `[VolleyGun]` it fires, not TANK2's own)
  turns within 90° of the front, leads the target and fires by itself the
  whole fight. Both fire within 14°, whatever the terrain.
- Goto, GotoWithArrive and Patrol drive at full speed to the end; a tank is
  there within its top speed in a second (Goto), its hull (GotoWithArrive) or
  twice its top speed (a patrol point) horizontally; a patrol point lies on
  the arc, inside the area, of a circle round the tank between half the
  radius and the far side of the area, and is given up after four times the
  time to drive there.
- In patrols, attacks and ambushes the way bends around units ahead (one
  within three of the tank's radii plus its own pushes the steering point
  away, fully within one), and a tank tilted more than its climbing angle
  (15°; a crawler 5°) looks at the ground a cell to either side, 1.5 cells
  ahead, and makes for a point one cell to the side that is flatter there,
  if either is flat enough, else lower.
- A ground vehicle stands at the mean height of the ground under its base's
  four corners, each corner's height taken between the map's points, tilted
  by their differences front to back and right to left, without limit, at
  once every frame.
- The crawlers (`GVEHICLE.DES`) never leave their orders to fight. Tank dust
  is in [8.4](#84-tank-dust).

### 5.16 Big ships

A big ship's orders differ from a boat's: Patrol and Attack take it to the
order's cell centre at `mih` over the ground, where it stops once within reach
and waits out `dur` (so the Big Fat Mama, whose patrol is its own cell, never
moves); Escort, Ambush, the cardinal orders and GotoWithInfiltrate leave it
where it is (no scenario gives a big ship an Escort or GotoWithInfiltrate);
Wait, Goto and GotoWithArrive work as for boats, and Explode runs its block
sequence ([7.7](#77-big-ships-destruction)). Its turrets are the bombers'
kind ([5.13](#513-bombers-and-their-turrets)), on the blocks
(`BBLOCK*.DES`), handed the ship's order the same way; they blow up with
their block.

### 5.17 Paralysis

A boat whose energy defence collapses (a Flash Shark or a Leech) shows "ist
paralysiert" and starts turning about its three axes at random rates of up
to 0.227 rad/s each, without explosions or debris; the hit itself shows only
the torpedo's blue flash. For 5 s it brakes at three times its
`MaxOrientationRetardation`, then it stops turning and hangs still where it
is, neither sinking nor going away, so it can still be boarded, identified or
destroyed; destroying it starts the destruction sequence. A tank or crawler
brakes to a stop for 5 s and then stays as it is for good; a gun tower turns
its gun to the front and lowers it to −14° for good; a torpedo tower's turret
stops where it is and never launches again; a bomber's turrets are paralysed
with it; a big ship stops dead; buildings have no energy defence to collapse.
None of them blows up. Paralysis effectively needs the Flash or Leech
torpedoes; gun shock damage is far below the shock shields.

## 6. Weapons

### 6.1 Gun models

`GLOBWEAP.DES` has 14 gun models: 0–10 for boats (0 Plasma Gun I to 10
Vendetta III, 0–5 energy guns), 11–13 for turrets and vehicles. Their
barrels, mountings, energy use and rearming are in [4.3](#43-firing-the-gun).
`SHOOT.DES` holds the shots: entries 3 to 5 are shells without shock
strength (the guns that use rounds), 7 and 8 the Cluster Bomb's shards.

### 6.2 The units' weapons

- Object file sections: `[StaticGun]` (Model, Ammunition, Relative{i}XYZ),
  `[VolleyGun]` (State{i} a/p, Duration, Tolerance), `[TorpedoMagazine]`
  (Relative{i}XYZ), `[BuzzerMagazine]` (Type, Eject*).
- Scenario keys: `gun`, `tor`, `ntor`, `eng` (the engine model 0–5).
- Turret software: GLOBWEAP `Software0–9` (hit, scan, lock, volley shots,
  volley delay; [6.9](#69-the-players-turrets)).
- No unit of the missions fires Subsonics or Torches.

### 6.3 Torpedo data

- `globweap.des` `[TorpedoMagazine0-7]` (items 1025–1032) and
  `[Torpedo0-21]` (items 1–22: `locktype` 0 none / 1 noise / 2 optic,
  `lockdelay` 0–6 s, `imagename`, `sfvname`/`sfvnumber`); torpedo bodies in
  `VFX/SOBJECTS/TORPEDO.DES`.
- The weapons' own voice files (`sfvname`: `cockpit\gunNN`, `tomaNN`,
  `torNN`) are on neither the installation nor the CD, so they never play.
- `TORPEDO.DES` entry 18 "Ripper" is the cluster torpedo (LifeTime 6,
  MaxVelocity 1800, KineticStrength 500, comment "KineticStrength Area =
  7600").
- `Range`, `Min/MaxClimbingAngle` and `VelocityIncrease/Decrease` (0 on all
  types) change nothing. `MinVelocity` changes nothing.

### 6.4 How a torpedo flies

All 22 types share one behaviour, with their `TORPEDO.DES` values.

- A torpedo leaves its tube along the boat's nose at the boat's speed
  (whichever way the boat moves) plus 8 m/s.
- **Straight types** (Stanley I and II, Subsonic I and II, Cluster Bomb,
  Torch: `locktype` 0) speed up along the nose to `MaxVelocity` at
  `MaxOrientationAcceleration`. The Subsonics, slower than most boats, are
  mostly launched faster than that and brake to it.
- `MaxOrientationRetardation` is the braking; all homing types and the
  Subsonics give it as a negative value, the Stanleys, the Cluster Bomb and
  the Torch, which are never launched faster than their top speed, as a
  positive one.
- **Homing types** turn their nose towards the target's centre: yaw and
  pitch each speed up by `MaxAngleAcceleration` A or B times the error over
  π (nothing within a degree), lose `AngleFriction` A or B of their angular
  speed per second and never turn faster than `MaxAngleVelocity` A or B.
  Their speed along the nose makes for a wanted speed at
  `MaxOrientationAcceleration` or `MaxOrientationRetardation`: `MaxVelocity`
  at the distance of the launch, falling towards 1.5 times the target's
  speed plus 0.3 of `MaxVelocity` at the target, so that they slow down
  closing in.
- **Agile types** (Tiger Shark II, Man Eater II, Hammerhead, Flash Shark III,
  Leech I and II) fly straight for 0.7 s, then turn their velocity itself
  towards the target: each tick the share `Agility` of it points at the
  target. Their wanted speed falls from `MaxVelocity` to the target's speed
  plus 0.4 of `MaxVelocity`. (A spin of 5 rad/s about the axis is set aside
  for the Hammerhead, but in the branch of the ordinary homing types, which
  the Hammerhead never takes, so it never shows.)
- Every torpedo's speed across its nose dies away by `OrientationFrictionX`
  and Z per second (it slides in a turn); along the nose there is none. The
  water's currents push it as they push the boats.
- Every half second a homing torpedo looks whether the ground is between it
  and its goal (at once after the launch, the agile types once they turn).
  While it is, it steers round it: over the ground it would hit within its
  way of 1.5 s plus two hull radii, and climbing steeply (1000 units, 31 m
  above it) while the ground under its front, half a map cell and 0.75 s
  ahead, comes within half a hull radius. It finds that ground by walking
  the map cells.
- There is no proximity fuse: a torpedo goes off when it touches the ground,
  a unit, a piece of debris or a shot (its armour is 1). Whatever it touches
  takes its strength off the torpedo's armour, and the Subsonics' armour of
  10,000,000 outlasts all of it: a Subsonic deals its 500 each time it goes
  into a unit and passes on through it, and goes off only on the ground. A
  torpedo never touches the boat that launched it, and boats don't bounce off
  torpedoes.
- The Flash Sharks and Leeches go off on the ground in a large flash
  (`EXPLOSIO.DES` 2) and elsewhere in a large ECM impact (17), sized to 1.6
  (Flash Shark I, Leech I), 2 (Flash Shark II, Leech II) or 2.4 (Flash Shark
  III) times their hull's radius; the Torch in a large flash 0.9 times its
  radius, leaving its torch; the Cluster Bomb in a large puff (5) 0.9 times
  its radius and its shards; all others in a medium explosion (1) at its own
  size.
- Bubbles (`EXPLOSIO.DES` 22) trail it every 0.16 s (twice the entry's
  `DisplayTime`), from half a second after the launch, while its engine runs.
- A spent torpedo can still hit a unit ([6.7](#67-end-of-a-torpedos-life)).

### 6.5 Decoys and buzzers

- **Decoys**: with the type's `InfluenceProbability` (1, or 0.9 and 0.8 for
  the Tiger Sharks and Man Eaters) a homing torpedo can be drawn away once.
  Which decoys draw it depends on its `sen`, within `senr` (80 m): the Tiger
  Sharks, Man Eaters and Flash Sharks (3) take the nearest buzzer, whoever
  dropped it, so the player's own buzzers draw the player's own; the Thresher
  and Bull Sharks (4) take the nearest buzzer of a hostile faction, or else
  whatever is loudest by its noise over its distance squared, so a hostile
  torch draws them when it is louder than their target; all others (99) are
  never drawn away. A drawn torpedo makes for the decoy and is spent once the
  decoy is gone.
- **The player's buzzers** leave the tail upwards and backwards (twice the
  boat's up less its nose) at 12.5 m/s on top of the boat's own velocity;
  nine in ten are buzzers (`DEBRIS.DES` 18), one in ten a dummy (20) that
  draws nothing. They sink slowly and last 16 to 25.6 s. Buzzers are debris
  like the rest ([7.3](#73-debris-and-trash)): they draw torpedoes while they
  last and go out on the ground.
- **The units' buzzers**: a boat's `[BuzzerMagazine]` looks every
  `EjectDelay` (at most every second before its first buzzer) whether a
  torpedo after it is within `EjectRange` (80 m); if so, with
  `EjectProbability` it ejects one from `Relative*` (the tail if none is
  given) at normalize(2 × up − nose) × `EjectVelocity` on top of its own
  velocity, of its faction. `Type` 0 throws one that works four times in ten
  (`DEBRIS.DES` 19, else the dummy 21), 1 nine times in ten (18, else 20), 2
  none (the tanks, the bionts, some scouts).

### 6.6 The Cluster Bomb and the Torch

- **Cluster Bomb**: it runs straight like the Stanleys. Where it goes off,
  six shards (`SHOOT.DES` 7) fly out 60° apart and then twelve (`SHOOT.DES`
  8) 30° apart starting 15° on, each from a metre out round the burst, level
  and a little upwards (a fifth of their way); a second later the same again
  from 6.25 m out, turned by 30°. The shards fly 31 m at 9.4 m/s with a
  strength of 7600 each, following the terrain: their way over the ground
  stays as launched, while their speed up or down is 10 m/s towards one of
  their radii over the ground, never lower than half a radius. Every 0.35 s
  each shard of the first ring leaves a large puff (`EXPLOSIO.DES` 5) behind
  it, 5 m from centre to edge times the share of its way it has flown, at
  least 0.6, which makes the burst a cloud of fire.
- **The Torch** flies like a Stanley and is no decoy itself. Wherever it goes
  off, spent or not, it leaves its burning torch (`DEBRIS.DES` 22) of the
  launcher's faction, which goes straight up at 21.9 m/s, rises 21.7 m in
  2.2 s, sinks again and lasts 16 to 25.6 s or until it comes down to three
  of its radii over the ground; there it hovers, bobbing and no longer
  drifting, flickering between 0.9 and 1.1 of its size, and it carries a
  light of 700 units' reach (the shots' 701) whose brightness flickers every
  frame between 0.5 and 1.5 times 2.11 (the shots have 5). It never goes out
  on the ground or when hit; once its time is up it shrinks away within a
  second. From the seabed it is back down after about 5 s.
- A unit with a passive sonar (`sen` 1) hears a hostile torch like a boat, at
  noise level 4, and takes it for its target when it is the loudest thing it
  hears while it looks for one; it fights the torch until it goes out. A unit
  already fighting doesn't look, and towers don't either.

### 6.7 End of a torpedo's life

When its `LifeTime` is up, or its target is gone, the engine stops and the
homing with it. The torpedo sinks at half a g, its nose following the way it
moves a little below it, and it keeps its way along the nose, so it noses
over and falls faster and faster until it hits the ground, where it goes
off. A spent torpedo can still hit a unit.

### 6.8 The player's torpedo magazine

The magazine holds its torpedoes in a ring, one loaded in the tube and one
selected. Its states are GLOBWEAP's `[TorpedoMagazineStateN]`: ready,
rearming, jammed, empty and waiting.

- Tab (with a click, sample 5, whether or not anything leaves) fires the
  loaded torpedo while the magazine is ready or waiting; it may jam first as
  a gun does (the condition, at least 0.2, against a random share of the
  magazine's `defectdamping`; jammed for `defectdelay` times 0.5 to 1.5, then
  ready).
- Enter (also a click) selects the next torpedo: one of the loaded type is
  simply ready unless the magazine is waiting or rearming, any other makes it
  wait. After a launch the selection moves on past itself and the magazine
  waits as well.
- Waiting lasts 2 s; then the selected torpedo is loaded with the revolver's
  sound (sample 20) for the magazine's `rearmingtime`, and once it is in, the
  voice says "torpedo ready" (77).
- A homing torpedo's lock only runs while the magazine is ready, for the
  loaded torpedo's `lockdelay`; it starts again after every wait and loading.
  The heading plays no part: the locked target may be behind the boat, it
  only has to stay a contact of the sonar, and a contact lost and found
  again starts the lock from the beginning.
  A lock's completion plays the torpedo lock beep (9) and the "target locked"
  voice (67). The HUD shows the lock and countdown in green after the
  target's distance on its line ("72M  0:5S", seconds to one decimal with a
  colon).
- The magazine's condition wears with hits on the front armour
  ([4.6](#46-armour-wear-and-the-fixer)).

### 6.9 The player's turrets

- **Software** (GLOBWEAP `Software0–9`) decides everything a turret does on
  its own: Claw V1.6–V3.6 look for torpedoes, Punisher V1.0–V2.2 for vessels,
  Eraser V0.8–V1.4 for torpedoes first and vessels second. The candidates are
  the torpedoes after the player and the units attacking the player
  ([5.2](#52-attacker-lists)), on the turret's half round the boat (the
  mounting's `side`: 0 above the boat, 1 below), in sight from the boat's
  centre and contacts of the sonar; the nearest is taken, and if there is
  none, the target the player had locked when the attack order was given. In
  sight means nothing of the terrain on the line between the two centres.
  `scan` is the pause before the next search when one finds nothing, `lock`
  the seconds a turret shows LOC on its new target before it fires, `vollsho`
  + 1 the shots of a volley (each when the gun is ready again, ARM between
  them) and `volldel` the VOL pause before the next volley at the same
  target. `hit` scatters the aim: each shot goes for a point up to (1 −
  `hit`) times the target's size off its centre on each axis (half that for
  ground units and buildings), led exactly for the target's motion and the
  shot's speed; a shot that can't catch up isn't fired but counts. There is
  no turning: the direction is worked out at each shot. A friend in the line
  of fire doesn't stop a turret: it fires through a friendly freighter.
- **Keeping the target:** a turret leaves it (back to SCA, searching again
  after `scan`) once it is on the other half, out of sight or gone; Claw and
  Eraser leave a vessel as soon as a torpedo is after the player, Punisher
  keeps only a unit attacking the player, Eraser a vessel only while none or
  this one attacks.
- **Orders** (F5–F12, the turret menu with `o`, FIGHTEXT 38–48): stop (STP)
  and sync (SYN) always work; attack my target and search and destroy need
  software and are refused without (the order's click still sounds); both
  search at once (SCA). A turret starts searching (SCA); one without software
  does nothing until it is set to sync. In sync each frame the fire key is
  held fires the turret's own gun straight ahead from its mounting whenever
  it is ready, from all its barrels at once (one round), ARM until it is
  again. An empty gun shows EMP and lets go of the target; a jammed one shows
  JAM whatever the turret does.
- **Sound:** the turret engine (sample 21) sounds once when a lock-on begins,
  not while the turret tracks or fires.
- **Aim cross:** `HUDTURR` (42×42, four diagonal strokes, red outside to
  yellow inside) sits on each turret's target from the lock-on through
  firing, rearming and the volley pause; for a target off to the side its
  centre sits on the HUD's edge towards it and the HUD clips the outer half,
  so only the two inner strokes show. Never while scanning, stopped, in sync
  or empty.
- The turret and overview pages are in
  [9.6](#96-the-slave-monitors-and-their-pages).

## 7. Contacts, mines, debris and destruction

### 7.1 The contact table and factions

Contact counts only between objects of different factions whose classes
`SOBJECTS/COLLIDE.DES` pairs. In a pair of objects the faster one does the
testing and must have contacts of its own, so a pair only touches when the
faster of the two pairs with the other. On contact each takes the other's
offense (strength) off its armour.

- The player is a faction of its own (8), which no unit in the missions
  shares. The contact table pairs the player with shots, debris, torpedoes
  and mines. Ramming a boat, a building or a tower damages the player and
  the rammed unit all the same, and a friend rammed a few times gives the
  friendly-fire warning ([2.6](#26-mission-failure-and-friendly-fire)).
- Objects without an `iff` in the mission (the plain buildings) have no
  faction, so every shot touches them. Trash without a faction is faction
  10, as debris is.
- Shots and torpedoes carry the faction of whoever fired them and pass
  through the units, mines and ship segments of their own side.
- A unit of a mine's own faction passes through it.

### 7.2 Mines

`dtyp` 27 is a passive mine (`MINE0.DES`), 28 an active one (`MINE1.DES`,
`MaxVelocity = 100`). Both have 1 point of armour and an offense of 2000 and
1500.

- A passive mine never moves. An active mine waits where it was laid until
  the player comes within 50 m of that place, then moves straight at the
  player at 0.3 times its `MaxVelocity` (about 0.9 m/s), keeping 1.5 times
  its radius above the ground; once the player is more than 56 m from the
  place it goes back there at the same speed. Only the player draws it.
- A mine blows up as soon as its armour is gone, shot or rammed. Its contacts
  are the player, boats, bombers, tanks, ground vehicles, towers, turrets,
  buildings, ship modules, debris, shots and torpedoes, never another mine.
  On contact the mine goes up and the unit loses 2000 or 1500 armour.
- Every mine turns about the vertical at 0.5 to 1 rad/s from a random
  heading, and nods: its pitch is 0.25 rad times the cosine of a phase that
  runs at a random 0 to 0.5 rad/s.
- It goes in one explosion of its kind, sized to 1.5 times its radius and
  without debris, `EXPLOSIO.DES` entry 5 (Explosion1L) for a passive mine,
  entry 1 (Explosion0M) for an active one, each with its sound
  ([8.1](#81-explosion-and-debris-entries)). Running into a mine hits from
  below the window, so its explosion is mostly hidden by the cockpit.
- The sonar never lists a mine, active or passive, so mines show on none of
  the radars and the HUD and can't be locked; they have to be seen.
- A minefield's mines are attacked when one of them is and destroyed when
  all are (s02205's objectives).

### 7.3 Debris and trash

- **Debris** (`DEBRIS.DES`, faction 10) has 10 points of armour and a
  strength of 3 to 12, and takes part in contacts once it is a second old. It
  has no contacts of its own, so it never runs into anything itself. A unit,
  shot, torpedo or active mine that is faster than a piece and runs into it
  takes the piece's strength, and the piece takes its strength. A shot
  (armour 1) stops there, and a torpedo (armour 1) goes up there. A piece
  goes out in an explosion and its sound when its armour is gone (entry 4, a
  medium puff), when it touches the ground (entry 12, a ground impact, as a
  shot's) or when its time is up (entry 5, a large puff). A piece slows down
  by its `Friction` (per second, of its speed) and sinks by its `Lift` in g
  (9.81 m/s²), and lasts 1 to 1.6 times its `LifeTime`, so that nearly every
  piece flung out of a destroyed boat sinks to the ground and goes out
  there. Resting mines, towers and buildings are never faster, so debris
  never touches them.
- **Trash** (`TRASH.DES`, armour 1, strength 2 to 5) never moves and has no
  faction in the scenarios, which makes it faction 10. It pairs only with
  shots, debris, torpedoes and mines, so the player and the other boats pass
  through it, and an active mine that runs into it goes up with it.
  Destroyed, it goes out in a large puff (entry 5) and leaves no debris.
- The voxel radar draws debris green ([9.4](#94-the-voxel-radar)).

### 7.4 A boat's destruction

A boat whose armour is gone, and one whose Explode order goes off, runs
through the same sequence, the player's boat too. Explosions are EXPLOSIO.DES
entries at their own size, each at a random spot of the hull: up to half a
hull radius to either side, up to half of a spread s (in hull radii)
lengthwise and up to s upwards. Debris is DEBRIS.DES entries flung out 0.1 to
0.6 hull radii from the centre, in any direction round and between two
elevations, at the piece's own speed.

| At | Explosions (entry, s) | Debris (entries, elevation) |
|---|---|---|
| 0 s | 0 (0.6), 3 (0.6), 3 (0.6) | the player's boat: 17, 17, 17, 1, 3, 3 at ±36° |
| 0.4 s | 4 (0.5) | – |
| 0.9 s | 1 (0.4), 4 (0.4), 4 (0.7) | 2, 2, 1, 3, 3 at 36° to 72° |
| 1.3 s | 5 (0.3) | – |
| 1.6 s | 5 (0.6), 2 (0.6) | the player's boat: 17, 17, 17, 0, 1, 2 at 36° to 72° |
| 2.0 s | 2 (0.1), 2 (0.4) | 2, 2, 2, 1, 3, 3 at ±36°, then the wreck |

- At every step up to 1.6 s the hull gets a new random acceleration of up to
  30 units/s² (about 0.94 m/s²) on each axis and a new angular acceleration
  of up to 0.2 rad/s² about each of its own axes, so it drifts and spins up
  more and more. Another boat's engine brakes it to a stop along its nose at
  `MaxOrientationRetardation`; the player's boat drifts on.
- One boat in five instead keeps its course at 70 % of its speed, its
  angular acceleration drawn anew every frame within 3 rad/s², and after
  1.6 s it waits until it hits the ground (or five seconds) before the last
  burst.
- At 2.0 s the hull is gone and the wreck stays: the player's boat leaves one
  body (DEBRIS.DES 16 half the time, 15 or 14 otherwise), another boat none,
  16, 15 and 16, or all of 14 to 16, each as likely. The bodies stay where
  the hull was, each 0.1 to 0.6 hull radii out from its centre as the other
  pieces start, do not drift as debris does, and after their `LifeTime`
  (16 s, up to 1.6 times that) shrink away within a second; a hit makes one
  go out in a medium puff (4), the ground in a ground impact. The player's
  boat also blows up (entry 2) at the front and back of its hull.
- The unit counts as destroyed for the mission only when its hull is gone
  (at 2.0 s, later for one that keeps its course); the player's boat at once.
- Explosions of entries 0 to 2 sound as big explosions, the others as small
  ones.
- The master monitor shows "Explodiert" (FIGHTEXT 72) through the sequence
  and keeps the lock until the hull is gone.

A bomber runs the boats' steps without their kicks: its hull gets no random
push or spin at the steps (a runner still spins). Half its turrets (the first
ones in its file) blow up with the first step and the rest with the fifth,
each in two seconds of its own explosions (entries 0 and 3 twice, 4, 0 and 3
twice with debris 17, 2 and 1, 4, 1, then 4 and 1 with debris 0, 17, 2 and
1). Its last step flings debris 2, 17, 1, 3 and 3 and, each case one time in
four, the wreck bodies 16, 15 and 16, 14 to 16, or none (the bio bomber none
at all). Then the hull hangs still where it is until its last turret is gone,
blows up at both ends (entry 2, half its length fore and aft, moved towards
the player by its radius) and only then is gone and counts as destroyed.

### 7.5 The death view

The player's boat is then watched from outside, without the cockpit: the
world fills rows 64 to 415 and columns 23 to 612 of the 640×480 screen, the
rest of those rows in the water's dark blue; above and below are black bars
with a grey line in rows 60 and 419. "DEATH VIEW" (FIGHTEXT 18) is at column
8, row 405, and the menu at the end has no box, all in the green font and
centred over the world: "GAME OVER" (FIGHTEXT 58) at row 152, the options at
rows 172 and 187 (abort and restart, FIGHTEXT 64 and 65). The camera starts
in the hull and keeps moving a tenth of a second's worth (a factor of 10 per
second) towards its place three hull radii out, 45° above the hull and
circling it at 45° a second, measured in the hull's own frame, and it always
looks at the hull. Once the hull is gone it comes down level (a factor of
1.5 per second) and closes in from three hull radii to half a radius at 1.25
radii a second. Eight seconds after the hull is gone the mission ends. The
other outside views are in [10](#10-views-from-outside).

### 7.6 Ground units' destruction

Tanks, crawlers, gun and torpedo towers and buildings (typ 2062) have the
boats' bursts at the same times (the table in
[7.4](#74-a-boats-destruction)) about their middle, and their own debris:

| Unit | 0.9 s, at 36° to 72° | 2.0 s, at 0° to 36° | Bodies (0° to 90°) | Gone |
|---|---|---|---|---|
| Tank | 2, 1, 3, 3 | 2, 2, 1, 3 | 14, 15 and 16 | 2.0 s |
| Crawler | 2, 2, 1, 3, 3 | 2, 2, 2, 1, 3, 3 | 14, 15 and 16 | 2.0 s |
| Tower | 2, 2, 1, 3, 3 | 2, 2, 2, 1, 3, 3 | none | 2.0 s |
| Building | 2, 2, 1 | 2, 1, 2, 1 | none | 2.7 s |

A tank or crawler brakes to a stop meanwhile and no longer turns. A building
shakes by up to 5 cm about its place, tilts at up to 0.07 rad/s about both
level axes, plays sample 18 three times in five at the start, and goes up in
a big blast (entry 2, sized 0.35 of its radius) at 2.6 s on its side towards
the player, half way up. Each counts as destroyed when it is gone, and the
darkened ground under a building or tower turns light again (the flattened
ground stays). Under fire a building shows 5 to 11 small explosions (entry
8, 5 or 0, one, two and one in four) on its side towards the player within
1.5 s, sized 0.05 to 0.1 of its radius, when its armour falls below 40 % and
again below 20 %. Ground plans can't be destroyed.

### 7.7 Big ships' destruction

When a big ship's shield is used up it brakes to a stop at its retardation,
its turrets keep firing, and its blocks blow up in their order, the first at
once and each next one 0.5 to 1.5 s after the one before. A block takes
2.7 s: it shakes by up to 5 cm, sinks faster and faster (10 to 40 units/s²)
and turns slowly (up to 0.07 rad/s about two axes), with the boats' bursts
about the block's model origin within its farthest vertex and the debris 2,
2, 1 at 0.9 s (36° to 72°) and 2, 1, 2, 1 at 2.0 s (0° to 36°) plus one body
(14 or 15 two times in five each, 16 once), sample 18 two times in three at
the start, and 0.6 s after the last step a big blast (entry 2) of 0.35 of
that radius, half the block's height up and its flat radius out towards the
eye, which at its largest covers the whole block. Half of the block's
turrets (rounded down) go up at the start and the rest at 1.6 s, each in its
own two seconds of explosions, and the block goes only with its last turret.
Ship and blocks stay sonar contacts and targets until each is gone; the ship
counts as destroyed when its first block is gone (in a squadron of several
ships every block that goes is counted), and is gone with its last. An
Explode order runs the same sequence, and a paralysed big ship stops dead.

## 8. Effects

### 8.1 Explosion and debris entries

- `EXPLOSIO.DES`: 0–2 explosion with flash (S/M/L), 3–5 puff, 9–11 sparks,
  12–14 ground impact, 15–17 ECM impact, 18–20 and 24–26 ground dust, 21–23
  torpedo bubble.
- Each entry has its sound: 0–2 play the big explosion (sample 1), 3–8 the
  small one (16), 9–11 the metal hit (2), 12–14 the ground hit (15), 15–17
  the EMS explosion (17).
- `DEBRIS.DES`: 0–2 plates, 3 engine, 4–8 small parts, 9–13 large parts,
  14–16 "Cadaver 1–3", 17 shrapnel, 18 and 19 buzzer, 20 and 21 dummy
  buzzer, 22 torch (decoy, Noise 4); keys `LifeTime`, `Friction`, `Lift`,
  `Min/MaxVelocity`.
- `BUBBLE.DES` 0 is the ambient bubble, 0.5 m.

### 8.2 Sprite sizes

A sprite is the frame's pixels on a square, each pixel 1/32 m, times the
`[Size] Scale`; the `[Anim] Scale` is not used. Each explosion gets 0.7 to
1.3 of that at random, its frames last 0.75 to 1.25 of `DisplayTime`, and all
but the ground impacts and the ground dust are rolled at random; the torpedo
bubbles (21–23) keep their frames' size, whatever their scale, and so do the
ambient bubbles. The shots are exact, a piece of trash is between half and
the full size, and the debris varies by type (10 to 40 %). So the small
explosion is 3.6 m across, the large one 14.5 m, a plasma shot 1 m, and the
sparks of a hit 0.8 to 2.4 m.

### 8.3 Sprite blending

`Transparency` is a toggle in the desktop's options, read from `CONFIG.DES`
(1 there) and handed to the fight in `FIGHTIN.DES`; the GOG installation's
last `FIGHTIN.DES` has 0. With 1 the explosions, the shots and the bubbles,
the torpedo trails' (entries 21–23) and the ambient ones (`BUBBLE.DES`: dark
blue over the water, lighter over the ground), are additive: each channel
becomes about what was behind plus the sprite's colour, clipped at full, so
overlapping sprites get brighter up to white. Exactly, in RGB565, twice
(half the colour behind + 0.5125 of the sprite's colour from its
`MOD_nnn.S16` table, as the models take theirs). Palette entry 0 is left
out. The ground dust (18–20 and 24–26), the debris and the trash cover what
is behind them at their full colour (brown dust over the water), as all
sprites do with `Transparency = 0`. Vertigo always blends as with 1 and
draws the sprites 2 m towards the camera, so that an explosion doesn't vanish
in the hull it bursts on, but the ambient bubbles where they are.

### 8.4 Tank dust

While a tank drives, one puff every 0.2 to 0.3 s behind one of its tracks,
picked at random, 0.6 to 1.1 of its half width out from the middle;
explosion 20 (upright) or 26 (rolled) at random, sized by its speed over its
top speed times its model's height times 0.8 to 1.0, then 0.7 to 1.3 as
every explosion, its frames 0.75 to 1.25 of `DisplayTime`. The puff stays
where it was made. The crawlers (`GVEHICLE.DES`) do the same every 0.16 to
0.3 s, always with explosion 20.

### 8.5 Explosion sizes

Where the game gives an explosion a size of its own (the building's and big
ship's last blast, a building's explosions under fire, the dust, the mines'),
that size is the sprite's scale, which comes out in metres as with the mines.
An explosion "of a size" is scaled so that its last frame's farthest corner
lies that far from its centre (for entries 0 to 2 a third of their first
frames' reach).

### 8.6 Fades

P fades the world to the water's dark blue: the shading's brightness falls
evenly to nothing in half a second, so that the terrain and the objects all
take the tables' darkest row, while the mission still runs; only then do time
and sound stop. Resuming starts the mission at once and fades the brightness
back up in half a second. The same fade also runs over about a second in and
out at the mission's start and end.

## 9. Cockpit and HUD

### 9.1 Cockpit description files

Cockpit sections in `VFX/COCKPIT/00n.DES` (000 Hiob, 001 Gator; the SMALL
layouts in `00nSMALL.DES`, the TRANS layouts hold the radio message's row):

- `[Front]` (window, `VFX_FlightX/Y` the view's centre).
- `[Camera]` (`ViewAngle` 105, the light and fog values of
  [3.1](#31-terrain-shading)).
- `[VoxelRadar0]` (offset 21,34 in the master monitor, 120×100, `WorldMinY`
  −5, `WorldMaxY` 20, `Projection` 30, `FlightY` 16, `ShadingTable`
  `VOX_000`).
- `[HUDAlertBlock]`, `[HUDThreatBlock]`, `[HUDPauseBlock]`,
  `[HUDSymbolBlock1]`.
- `[SlaveMonitor0/1]` (none in 000/Hiob), the monitors' `Type` and `Lamp*`
  keys (`*.D16`, RGB565 rows twice the lamp's width, the lamp off on the left
  and on on the right).
- `WS_TopLeftY`, `WS_SpacingY`, `WS_NameX`, `WS_AmmoX` and `WS_StatusX` place
  the weapons overview ([9.6](#96-the-slave-monitors-and-their-pages)).
- `[Flake]` is unread.

The in-mission help is `VFX/COCKPIT/HELP0-2.TXT`.

### 9.2 Images and fonts

- Images in `GFX/IMG/COCKPIT`: `HUDALERT`, `HUDPAUSE`, `HUDTHREA`/`HUDTHREY`,
  `HUDSYM1` (under fire), `HUDSYM2` (radiation), `HUDSYM3` (current),
  `HUDTURR` (a turret's aim cross), `HUDTURGR/RE/YE` (never shown),
  `HUDLOCAA`/`HUDLOCPA` (lock rings), armour silhouettes
  `{GATO,HIOB,SUCC,ZORN}{1-4}{F,L,R,B}`, 48×48 weapon pictures, `SHP*` 60×60
  observation pictures.
- Font `CPITSMHC.FNT` is unused. The cockpit font draws the Fixer's `*` as
  an ×.
- Credits: `gfx/pic/title/credits0`–`3.r16` (640×480, RGB565).
- Main menu arrows: `IMG/TITLE/ARR1_UP`/`ARR1_DO` (up; normal and pressed)
  and `ARR2_UP`/`ARR2_DO` (down).

### 9.3 Symbols, ALERT, THREAT and PAUSE

- `[HUDSymbolBlock1]` (96×48) holds three 24×24 symbols at offsets 0, 28 and
  56: the radiation symbol, the current symbol and the marker under fire.
  Each blinks on its own clock from the moment it is switched on, half a
  second shown and half a second hidden. The torpedo lock has no symbol
  there: the lock ring and the voices announce it.
- **Marker under fire:** a red ring with a cross (`HUDSYM1`) in the third
  slot while any unit fights the player (one of the attacker list), blinking
  from the moment it appears; the voice "lock-on located" (sample 81) when
  the first unit turns on the player.
- **Armour symbol:** a bar per side: green, yellow below 75 %, red below
  50 %, gone below 25 %, white when hit.
- **ALERT:** while a side's armour is below 10 %, with the voice "system
  collapse"; also in the radiation ([3.4](#34-radiation-and-the-surface)).
- **THREAT:** blinks while homing torpedoes are after the player; the nearest
  of them decides its colour: yellow (`HUDTHREY`) for one a buzzer can draw
  away (`sen` 3 or 4: the Thresher, Bull, Tiger and Flash Sharks and the Man
  Eaters), red (`HUDTHREA`) for the others (the Hammerhead, the Big Bangs,
  the Leeches).
- **PAUSE** (`HUDPAUSE`, 119×32) stands still while the game is paused,
  centred in `[HUDPauseBlock]`: in the Gator's cockpit 001 at y 188, 35
  pixels above the view's centre; in the Hiob's 000 at y 237, right over the
  crosshair. The pause fade is in [8.6](#86-fades).
- **Aim help:** drawn ahead of a moving target where the shot has to go,
  clamped to the HUD frame.
- **Lock ring:** name, distance, shield bars and sensor icon. Heading tape,
  the lamps beside the sonar monitors, D/H/V/N and R readouts.

### 9.4 The voxel radar

Always shown in the lower part of the master monitor, the target's picture
above it: a patch of terrain from 5 cells behind the boat (`WorldMinY`) to 20
ahead (`WorldMaxY`) and 15 to each side, turning with the heading, seen in
perspective from behind at `Projection` 30° so that the far edge is about
half as wide as the near one and the boat sits in the lower third (`FlightY`);
no background, so the monitor's grid shows through; light grey ground, low
ground dark, with `VOX_000.S16`; buildings as their models in orange, units as
vertical lines from the ground (red hostile, green friendly, blue neutral,
grey the locked target), debris green, the boat orange, the ground of water
currents dark teal (the shading table's entry 65) for `spd` 0 and light cyan
(66) for `spd` 2. Torpedoes are in
[9.11](#911-torpedoes-on-the-radars).

### 9.5 The master monitor

- The target's picture above the voxel radar; the big ships' pictures are in
  [2.2](#22-cargo-ships).
- The second line shows what the target does, with `FIGHTEXT` 77 to 84 for
  its orders (`GOTO`, `PATROLLING`, `ATTACKING`; attacks, waits, guards,
  under way, escorts, infiltrates), 81 while paralysed and 72 "Explodiert"
  through its destruction sequence, which keeps the lock until the hull is
  gone.
- `G:` is its gun's short name and `T:` its torpedoes, `R:` its sensor range
  ([9.7](#97-sonars)). A hostile target's name is red.
- The mission menus (the Esc menu, the autopilot menu) are FIGHTEXT texts
  ([11.1](#111-fightext)).

### 9.6 The slave monitors and their pages

- The first slave monitor shows a weapon page (item name, state as a word,
  the torpedo's long name, its picture; F1–F4 pick gun, magazine or a
  turret), the second the armour page (`ARMOR STATE`, the silhouette with
  each side's percentage). Hiob's single monitor shows the weapon page. I
  switches between the armour page and the system state page, never back to
  the weapon page. Each F key clicks (sample 5).
- **Overview:** F1–F4 again on the page they show (once more goes back) shows
  `NAME AMMO STAT` (green) at `WS_TopLeftY` and a row each, `WS_SpacingY`
  apart from two pixels lower, at `WS_NameX`, `WS_AmmoX` and `WS_StatusX`:
  the gun (short name, "ENER" or its rounds, its state), the magazine (short
  name, the selected torpedo's short name, RDY or its state) and the turrets
  ("TURR", rounds, state). Rounds are green above 50 and red up to 50,
  "----" in red when none are left; a state is red unless ready, the
  turrets' always.
- **Turret page:** the gun's long name (GLOBWEAP, "MaximumImpact II"), the
  software and the state written out, all in red at all times, the gun's
  picture below; a boat without the turret shows an empty monitor. The gun
  page's state is red while rearming, firing, jammed or empty; the magazine
  page's while not ready.
- **System state page:** `SYSTEM STATE` centred at the top, then a row of
  10 px for each system, `ARMO`, `GUN`, `TORP` and the turrets the boat has,
  the rows centred as a whole (a five-letter name and a 30 px bar). The bar
  is 15 ticks 2 px apart: the condition's share of the 30 px, in pairs, as
  6 px ticks in a bright colour, the rest as 2 px ticks in a dim one. Name
  and bar are green above half the full state, yellow above a quarter, red
  below (bright/dim green 66,214,58 / 66,130,58, yellow 239,214,0 /
  173,134,0, red 255,32,0 / 165,12,0). The armour counts by its weakest side.
  With a Fixer a green `*` stands 8 px left of the system it mends and a
  yellow one 16 px left of the forced system, and a centred `AUTOMATIC` line
  follows two pixels below the rows, with the yellow mark when nothing is
  forced.

### 9.7 Sonars

The four sonars (items 6145–6148, Spy I–IV) take 12, 10, 8 and 6 seconds to
come up and differ in four abilities: Spy I has none, Spy II tells friend from
foe and shows the target's armour, Spy III and IV also show its weapons (`G:`
and `T:` on the master monitor) and what it does (the second line). The
target's sensor range (`R:`) shows with any sonar once the contact is active.

Without friend-or-foe detection every contact is drawn as a friend: green on
the master monitor's radar and the round radars, and the HUD takes the locked
target for a friend too; the master monitor's text still colours a hostile
target's name red, and `u` still works. With it the master monitor's radar
draws friends green and everything else red, and the round radars draw
friends and hostiles with their two symbols and skip neutral contacts
altogether. Chapter 1's Spy I has no IFF, so every contact shows as a friend
there. Each block of a cargo ship is a contact of its own; mines are never
contacts, nor are the bombers' turrets, which the sonar skips outright. The
big ships' turrets are of another kind the sonar doesn't skip, but they make
no noise, so a passive sonar never hears them; whether the active sonar
lists them is open.

### 9.8 Target lock and keys

- `u` locks the nearest of the units attacking the player that the sonar has
  as a contact, so a hostile unit that has not attacked yet is not locked. L
  locks what is in the reticle, N the next nav point.
- Keys from `HELP0-2.TXT`: `p` pause, `o` turret menu, F1–F4 status pages,
  `i` armour/system status, Alt+I the Fixer, F5–F12 turret orders, `u`
  nearest enemy, Tab fires a torpedo, Enter selects the next, H the help, M
  the radio log, B the briefing, Alt+A the autopilot menu, the number pad the
  outside views ([10](#10-views-from-outside)).
- Tab and Enter click (sample 5).

### 9.9 Help, radio log and briefing

Three pages lie over the view, the mission going on underneath.

- **Key help** (H): `VFX/COCKPIT/HELP0-2.TXT` in capitals, four columns at
  63, 142, 357 and 436 of the 640×480 art, H turns the three pages, Esc
  closes.
- **Radio log** (M; Esc or M closes). Both the help and the log are a dimmed
  box from 54 to 577 across and 92 to 406 down between two dotted lines, the
  title (FIGHTEXT 21/60) spaced out below the top line, up to twenty rows of
  12 px from 142 down, the footer (FIGHTEXT 20/59) right-aligned at the
  bottom right; the help's footer has the game's version and date to the
  left of the text. The log keeps the last 24 entries and lists each radio
  message at the time it was shown (`mm:ss :` and the text; a leading `#`
  shows it in red, a `~` in amber) and the units' events ("<name> wurde
  angegriffen / zerstört / identifiziert / paralysiert / infiltriert", "ist
  angekommen", FIGHTEXT 5–10) in amber. An event is logged only for a unit
  with a name, when the mission uses that event for it (something depends on
  it) and its count is reached. The mission's end lines (FIGHTEXT 11–13) go
  through the same rule without a unit, so they are not logged in normal
  play; only a hidden switch that logs every event (a Ctrl key combination)
  logs them.
- **Briefing** (B): the mission's briefing in the same box, titled FIGHTEXT
  24 with FIGHTEXT 23 as the footer; B or Enter turns its two pages (Enter
  does not select a torpedo then), Esc closes, and it opens on the first.
  The first page has the mission file's `[B]` text, up to twenty lines each
  centred. The second has the targets (FIGHTEXT 0, its underline 1 and `[P]`)
  in rows 0 to 9 and the tips (FIGHTEXT 2/3 and `[S]`) in rows 10 to 19,
  eighteen characters (108 px) in from the log's left edge; the headings
  stand even without lines, and no mission has more than eight of either.
  All of it is in capitals.

### 9.10 Terrain colours and fog

The map's shading tables (`VFX/TEXTURE/S000_nnn.S16`, like the radar's) hold
32 rows of 256 RGB565 colours, row r holding (r + 1)/32 of twice the palette
and the rest of a blue F ≈ (−70, −2, 37) that takes the red away; the row per
pixel follows the rule of [3.1](#31-terrain-shading), for the models too from
their own tables (`MOD_nnn.S16`, the palette itself in the last row). Only the
sprites keep a fog by distance from 30 to 140 m.

### 9.11 Torpedoes on the radars

Every torpedo on its way is a sonar contact, friend or foe and without being
detected; its colour blinks between 248,12,0 and 160,0,0 (RGB565 `F860` and
`A000`), 0.15 s each, so the voxel radar shows it as a blinking red column.
The units keep their colours (hostile `A060`, friendly or without IFF `4407`,
the locked target `8C71`). The HUD frames a torpedo as it frames the units:
green for the player's own and a friend's, red for a hostile one, and green
for all without IFF.

## 10. Views from outside

The keys are in the in-mission help (`VFX/COCKPIT/HELP2.TXT`), the views'
names are FIGHTEXT 14–19 (chase, fly around, pass by, look, death, look at).

- **Keys.** On the number pad: 0 the chase view of the player's boat (CHASE
  VIEW, 14), 5 its look-at view (LOOK AT VIEW, 19), 1 and 7 the same of the
  locked target, which do nothing without one and stay on the target until
  it is destroyed. The same key again goes back to the cockpit, another goes
  straight to its view. 9 and 3 zoom in and out in 19 steps evenly apart,
  from the first zoom 4 nearer and 14 further; 4, 6, 8 and 2 move the look-at
  view's camera left, right, up and down round the boat in steps of 22.5°
  (16 for a full turn either way), and it pitches with the boat; in the
  chase view they do nothing. The zoom and the place round the boat are
  kept, the same for the player's boat and the target. The help's torpedo
  camera (Entf) does nothing, and the fly-around, pass-by and look views
  (15–17) can't be called up.
- **The mission goes on.** The boat is flown, fires and locks as from the
  cockpit, and the help, the radio log and the briefing open over the view.
  The sounds are heard where the camera is.
- **The picture.** As the death view's ([7.5](#75-the-death-view)): black
  bars with a grey line in rows 60 and 419 and the view's name at column 8,
  row 405, but the world fills rows 64 to 418 across the whole width.
  Nothing marks the contacts or the target in the world. The radio message
  shows in the bar below the picture; ALERT didn't show as the boat was
  destroyed.
- **Instruments.** Only the chase view of the player's boat has a few,
  without the cockpit's monitors and whatever pages these show: the table of
  all weapons (the weapon page's overview) from column 14, row 78, the system
  state page centred on column 584 from row 72, THREAT centred on column 320
  in rows 82 to 96, and the first sonar range (360 M) in a square of 64 by 64
  pixels round column 601, row 376, that darkens the world to half. The two
  pages are where the cockpits' SMALL layouts (`VFX/COCKPIT/00nSMALL.DES`)
  have their status monitors, 96 by 71 pixels at 8, 8 of the picture, the
  second 160 pixels further right in the wider picture.
- **The camera** stays level: the boat rolls about 30° in a turn under it,
  and in a loop the camera keeps upright while the boat turns over. It
  follows the boat's movements like a spring, even its rocking at rest, and
  goes through the ground and buildings. The chase view's camera is behind
  the boat, 20° above its course, and the boat shows a little below the
  middle; at the nearest zoom it is just over the rear of the hull, looking
  along it to the nose. At the first zoom the Gator at rest is 185 pixels
  wide, at the furthest 49. The look-at view's camera starts in front of the
  boat, 45° above it, looking at it, the Gator 154 pixels wide.
- **Other boats** bank into their turns by as many radians as they turn
  radians a second, following within about a second (69° for an ANSCOUT1 at
  its full rate, [5.4](#54-turning-and-banking)).

## 11. Texts and sounds

### 11.1 FIGHTEXT

`TXT/FIGHTEXT.DES`:

| # | Use |
|---|---|
| 0–3 | briefing: targets heading and underline, tips heading and underline |
| 5–10 | the units' events (attacked, destroyed, identified, paralysed, infiltrated, arrived) |
| 11/12/13 | mission accomplished / failed / "DU WURDEST ZERSTÖRT" |
| 14–19 | the outside views: chase, fly around, pass by, look, death, look at |
| 20/21 | the help's footer and title |
| 23/24 | the briefing's footer and title |
| 31/32 | "MISSION NICHT ERFÜLLT", "AUTOPILOT NICHT VERFÜGBAR" |
| 38–48 | turret order menu |
| 49–56 | Esc menu |
| 57/58, 61–66 | death and game-over menu (58 "GAME OVER", 64 abort, 65 restart, 66 load the last game); the autopilot menu follows |
| 59/60 | the radio log's footer and title |
| 67–69 | friendly-fire warnings, one at random |
| 70 | "one more and you are out", at the second friendly kill |
| 71 | friendly object destroyed |
| 72 | "Explodiert" (a target in its destruction sequence) |
| 77–84 | what the target does (its orders; 81 paralysed) |
| 89 | mission failed after three friendly kills |

On the radio line a text starting with `#` is red; `~` only colours the
log, where a leading `#` is red and a `~` amber.

`TXT/DEEPTEXT.DES` holds the menus' and the notebook's texts
([12](#12-menus-saved-games-and-training)).

### 11.2 Turret states

From GLOBWEAP `TurretState0–8`: ARM, SCA, FIR, JAM, EMP, LOC, STP, SYN, VOL
([6.9](#69-the-players-turrets)).

### 11.3 Samples

`SFX/SND/SMPMAP.DES`, `NumOfVoices = 8`:

| # | Use |
|---|---|
| 6 / 7 / 9 / 10 | threat / lock-on / torpedo lock-on / alert beeps |
| 5 / 20 | keyboard click (Tab, Enter and the F keys among others) / the magazine loading a torpedo ("revolver reload") |
| 18 | a building's or block's collapse |
| 29 | booster |
| 67 / 72 / 77 / 81 / 82 | voices: target locked / system collapse / torpedo ready / lock-on located / torpedo threat |
| 4 / 21 / 23 / 24 / 34 / 35 | engines: torpedo / turret / enemy 1 / enemy 2 / biont / big ship |
| 1 / 2 / 14 / 15 / 16 / 17 | big explosion / metal hit / torpedo start / ground hit / small explosion / EMS |

## 12. Menus, saved games and training

### 12.1 Main menu, credits and difficulty

- The main menu cycles the difficulty. The option pages are in the medium
  font, 15 pixels apart.
- CREDITS, between TRAINING and the options, shows
  `gfx/pic/title/credits0`–`3.r16` (640×480, RGB565) full screen, twelve
  seconds each, then goes back to the menu; a key or a click ends them at
  once.

### 12.2 Loading and saving

The texts are `TXT/DEEPTEXT.DES` 35–51, 110 and 113.

- **Main menu.** SPIELSTAND LADEN (110) lists the saved games by slot, each
  by its name in the large font, 20 pixels apart from y = 282, with the
  arrows `IMG/TITLE/ARR1_UP`/`ARR1_DO` (up; normal and pressed) and
  `ARR2_UP`/`ARR2_DO` (down) at the bar's right end, x = 465, y = 304 and
  345; they show half transparent when there is nothing to scroll. A click
  asks SPIEL LADEN ? (41) with `--- NAME ---` and JA / NEIN (42, 43), 30, 30
  and 20 pixels apart from y = 288, without the way back to the main menu.
- **Notebook.** LADEN / SPEICHERN (17) opens L A D E N  U N D  S P E I C H E
  R N (35) with SPIEL LADEN (37), SPIEL SPEICHERN (38) and ZURÜCK at y = 70,
  90 and 110 of the page. SPIEL SPEICHERN (44) lists 16 slots 10 pixels
  apart from y = 50, each by its name or F R E I (50) when free, with ZURÜCK
  below; a slot asks Bitte Namen eingeben (46) at y = 80 over the name with
  the cursor on a dotted line at y = 110, and SPEICHERN / ABBRUCH (47, 48) at
  y = 170 and 190. The name starts as the slot's, so a free slot left
  unchanged is saved as "F R E I". SPIEL LADEN (39) lists only the used
  slots; a click asks SPIEL LADEN ? (41) at y = 60 with `---- Name ----` at
  80 and JA / NEIN at 120 and 140.
- **Scrolling.** The notebook's long pages (the missions) have the arrows of
  its font (characters 0xE3–0xE6 up, 0xE7–0xEA down) on the bottom line,
  y = 270, with ZURÜCK beside them; the font draws ( and ) as [ and ].
- The save file's `[Defect]` section is in
  [4.7](#47-damage-between-missions).

### 12.3 The autopilot menu

Once the mission is accomplished, Alt+A opens the autopilot menu (FIGHTEXT,
after the game-over menu's texts), which lists the area's open stations;
with one destination the autopilot goes there at once. What the scenario's
`[End] Type` does instead is in [2.7](#27-mission-end-and-result).

### 12.4 Training

TRAINING in the main menu lists the four training missions (`strai0`–`strai3`
with their setups `dat/save/ftrai0`–`3.des`), each starting with its
briefing. Their `[End] Type` is 2, InstantEnd.

### 12.5 Music

A scenario's `[CD] Track` n plays the CD's audio track n+1 (1–4; 0 none).
The tracks are the CD's four audio tracks after its data track; a GOG
installation keeps them in its CD image (`game.gog`) with its `.ins` cue
sheet.
