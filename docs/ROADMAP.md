# Roadmap

Vertigo recreates Schleichfahrt / Archimedean Dynasty from the original's game
data. This file keeps the plan, what of it is done, what is still open, and
the reference data gathered from the data files and from the original
(October 2026). The plan grew out of the former `todo.txt` (currents, the 11
AI orders, the HUD items, effects, engine sounds) and a review of the
repository: every mission loads, is fought and can be won or lost now, on the
desktop and on the iPad. What remains is collected under
[what is open](#what-is-open). Rules for handling the original's executables
are under [working safely](#working-safely-with-sfwin32-and-the-original-executables).

Effort marks: S is up to a day, M a few days, L one to three weeks, for one
developer.

## What is done

**Test tooling.** Test mode runs a mission from the command line with a
chosen boat, equipment and difficulty, headless if wanted, with a seed, a
timeout and an exit code for success or failure; `--trace` prints units every
second, `--keys`, `--click`, `--pad` and `--touch` drive the game,
`--screenshot` saves frames, `--start` and `--near` place the player,
`--destroy` and `--collapse` take units out, `--check` loads all 71 missions
and reports what the loader skips. The debug overlay shows orders, currents
and sonar ranges. Seeded idle runs of all missions (3 seeds, 60 s) are the
regression check: before and after a change the same runs should end the
same way, or differ for a known reason.

**Combat and missions (phase 1).** Every unit type loads (appendix A); the
missing conditions, the failure path and the mission end (Esc menu, death and
failure screens, restart, abort, load, the autopilot's choice of station)
are in. Units move with terrain and object collisions; shots know who fired
them, the player is hit per side and dies, destroyed units explode with
debris and bodies. Tanks drive on the ground with turning turrets; cargo
ships are built from their blocks with turrets on the hull, take hits as a
whole and blow up block by block. Enemy guns use the object files' muzzles
and volleys; 22 torpedo types with magazines, lock, homing, decoys and
torpedo towers; the cluster torpedo bursts as area damage. Difficulty from
the main menu limits which units appear (`diff`) and how many attack the
player at once.

**Currents (phase 2).** The `[CurrentNNNN]` cells push the player and AI
boats as the original's do (appendix D).

**AI orders (phase 3).** All 11 order types run (appendix B), slot by slot
with break conditions, round and round over a unit's 8 slots. Towers sweep
and fire; boats and bombers fight as the original does (appendix J): they
find their targets with the mission's sensors (`sen`, `senr`), align on a
side of the target, run at it turning in place or flying, break off and come
round again with the `[Attack]` settings, fire at anything within 14° of the
nose, and go on with their order when the target is out of double sensor
range. Patrols wander their area, ambushers lie silent until the sensor finds
something, escorts fly in formation and fight within 150 m of their place.
Sleeping units, the attacker lists, `car` and `con` follow the original too.

**HUD (phase 4).** Pause, the voxel radar, both slave monitor pages with
F1–F4 and I, the lamps, the lock, current and radiation symbols, ALERT and
THREAT with their beeps and voices, friendly-fire warnings, the torpedo lock
and countdown, the turret menu and brackets, the armor symbol, the aim help,
the master monitor's activity and weapon lines, `u` to lock the nearest
enemy. The 3D cockpit (glTF models) shows the instruments on its screens
and moves the pilot's head with the boat.

**Effects and sound (phases 5 and 6).** One particle pool for explosions,
debris, bubbles, torpedo trails and dust; 3D audio with the four nearest
engines, mission music from the CD tracks; the terrain is coloured by the
maps' shading tables with the original's rule, and so are the models
(appendix M), energy shots
light the ground, the booster, the energy pool and the jams follow the
original (appendix K), and so does the boat's turning (appendix L).

**iPad port.** Qt is gone (SDL3 and our own types), the renderer is one
shader on OpenGL 3.2 core and ES 3, data access is case-insensitive, the
game data is imported from a GOG installation or the CD image, and the app
has its lifecycle and audio session, second key bindings, game controllers,
touch controls and the 3D cockpit at 120 frames a second on an iPad Pro.
OpenGL ES and OpenAL are deprecated but still in the iOS 27 SDK; ANGLE and
OpenAL Soft would replace them. The GPL keeps it out of the App Store, so
players build it in Xcode, side-load an IPA or use AltStore PAL in the EU.

**Later additions.** Mission music (G1), minefields as columns (G2), the
armour and energy defence percentages `inte`/`eint` (G3), the five missions
with unresolved conditions checked (G4), the station chosen after a mission
(G6), the four training missions in the main menu (G7).

## What is open

| # | Item | Effort | Notes |
|---|---|---|---|
| O1 | The marker under fire (D12) | S | A red ring with a cross in the symbol block's second slot while the player is attacked, sometimes outside the HUD frame; not HUDAIMH, HUDTARG, HUDTURR, HUDPOI* or HUDTHREA. Identify the image, then draw it |
| O2 | Ground sample at s00101's start (D18) | S–M | The original reads 57 m of height there (the cell's own value 250), Vertigo 74 m: its western neighbours hold 236–240 and a building's `setHeight` or the spline's sampling takes the lower value |
| O3 | Turrets with boat guns | S–M | The AD Augmented Mod's changelog says a turret given a boat gun fires that gun's projectiles from each barrel (a one-barrel turret with Vendetta III four a salvo, a four-barrel one 16); Vertigo fires one muzzle per pull. Compare in the original first |
| O4 | `card` on active buildings | S | 51 of 424 carry 2, 4 or 6; buildings turn by `card`·45°. Look at s00401's hydrogen tank 2066 (`card` 6) in the original |
| O5 | s00705 (G5) | S | The AD mod says Schwartz (2066) infiltrates only the first control unit and waits until Marquez is dead; in Vertigo he infiltrates all three in turn. Watch the original: a slot with a break condition may not end on its own there |
| O6 | Tank manoeuvre | M | Tanks drive at their target and fire; the original's `[Attack]` keys for tanks (`AlignProbabilityHook/Cross/Retain/Change/Hit`) are not read yet (appendix J) |
| O7 | Open comparisons | – | How a hit counts for the AI (the original reacts when the energy defence changes, Vertigo on any hit by another unit); what an attacking boat does once its area time is up (the original may pick a new point every frame); the escort offsets measured from hull to hull; GotoWithInfiltrate's approach and docking; `wccond 9` (56 units, s00303 and s02006 among them); which of `car` 2 and 6 is left; why one of S00202's two upward current cells does nothing; how engine volume and pan fall off with distance; the cluster burst, the dust sprite and the debris units; what the monitors' lamps show; whether the original lists the start station in the autopilot menu, loops a mission's track and pauses it in the menu; whether big buildings show as blocks or ground on the radar; whether the original stacks mine columns upwards |

Risks: renderer changes stay self-contained so that a modern renderer and
VR work stay easy; the cargo-ship block layout is inferred from the data.

## Differences to the original's cockpit (screenshots, October 2026)

Compared with screenshots of the original (English version; Hiob, Gator and
Succubus cockpits). All but D12 and the ground sample of D18 are done.

| # | Area | Original, and what Vertigo does now |
|---|---|---|
| D1–D3 | Voxel radar | Always shown in the lower part of the master monitor, the target's picture above it: a patch of terrain from 5 cells behind the boat (`WorldMinY`) to 20 ahead (`WorldMaxY`) and 15 to each side, turning with the heading, seen in perspective from behind at `Projection` 30° so that the far edge is about half as wide as the near one and the boat sits in the lower third (`FlightY`); no background, so the monitor's grid shows through; light grey ground, low ground dark, with `VOX_000.S16`; buildings as their models in orange, units as vertical lines from the ground (red hostile, green friendly, blue neutral, grey the locked target), debris green, the boat orange, water currents as one turquoise sheet over their cells |
| D4–D6 | Slave monitors | The first monitor shows a weapon page (item name, state as a word, the torpedo's long name, its picture; F1–F4 pick gun, magazine or a turret), the second the armor page (`ARMOR STATE`, the silhouette with each side's percentage); Hiob's single monitor shows the weapon page; I switches to the systems table |
| D7–D8 | Master monitor | The second line shows what the target does (`GOTO`, `PATROLLING`, `ATTACKING`), `G:` its gun's short name and `T:` its torpedoes |
| D9 | Aim help | Drawn ahead of a moving target where the shot has to go, clamped to the HUD frame |
| D10 | Cargo ships | Each block is a sonar contact and target of its own |
| D11 | Target lock | `u` locks the nearest enemy; L locks what is in the reticle, N the next nav point |
| D12 | Marker under fire | Open (O1) |
| D13 | Hits and explosions | The sprites' sizes follow the rule of appendix H: a frame's pixels at 1/32 m times the `[Size] Scale`, the explosions at 0.7 to 1.3 of that, rolled and timed at random |
| D14 | Armor symbol | A bar per side: green, yellow below 75 %, red below 50 %, gone below 25 %, white when hit |
| D15 | ALERT | While a side's armor is below 10 %, with the voice "system collapse"; also in the radiation (appendix D) |
| D16 | THREAT | Yellow (HUDTHREY) |
| D17 | Terrain colours and fog | The map's shading tables (`VFX/TEXTURE/S000_nnn.S16`, like the radar's): 32 rows of 256 RGB565 colours, row r holding (r + 1)/32 of twice the palette and the rest of a blue F ≈ (−70, −2, 37) that takes the red away; the row per pixel follows the rule of appendix M, for the models too from their own tables (`MOD_nnn.S16`, the palette itself in the last row). Only the sprites keep a fog by distance from 30 to 140 m |
| D18 | Terrain geometry | The height map is the same (at s00202's start both have the boat at 57–60 m over ground at 48 m); only the ground sample at s00101's start differs (O2) |

Already alike: the lock ring with name, distance, shield bars and sensor
icon; turret brackets; heading tape; the lamps beside the sonar monitors; the
master monitor's target picture, D/H/V/N and R.

## Gaps found after milestone 6

| # | Gap | Notes |
|---|---|---|
| G1 | Mission music | Done. `[CD] Track` n plays the CD's audio track n+1 (1–4; 0 none); building `Game Data` from a CD image copies them out as `SFX/CD/TRACK02.WAV` to `TRACK05.WAV`; `sfx::Music` streams the mission's track round and round at half volume, pausing with the mission; CD Musik AN/AUS in the options (`Sound/cdMusic`). Not compared: whether the original loops, pauses and how loud |
| G2 | Minefields | Done. A mine entry is a column of `num` mines `dis` height units apart, the lowest at `hei`; 407 mines in 10 scenarios were missing before. Attacked when one of them is, destroyed when all are (s02205's objectives). Whether the original stacks them upwards is not compared |
| G3 | Unit keys | Done in the main. `inte` and `eint` are the armour and energy defence in percent of the type's (the AD Augmented Mod's changelog says so, and its changes match); `sle` lets a unit sleep (appendix J); `eng` is the engine model (appendix K); `ai` is never read by `FIGHT.EXE`. Open: `card` on active buildings (O4) |
| G4 | Unresolved conditions | Done. The references `--check` reported are condition codes for objectives that no unit has; they never come true, and all five missions can still be won. s01802 lost two tanks' trigger area when it was made from s01801 (the AD mod fixes it), s02006 kept a radio message of a dropped objective |
| G5 | s00705 | The idle player is destroyed in every seed now; only the player can destroy Marquez. Open: Schwartz's infiltration (O5) |
| G6 | Station after a mission | Done. The original's autopilot menu (FIGHTEXT) opens with Alt+A once the mission is accomplished and lists the open stations; with one destination the autopilot goes there at once. The menus show on the master monitor, also in the 3D cockpit |
| G7 | Training | Done. TRAINING in the main menu lists the four training missions (`strai0`–`strai3` with their setups `dat/save/ftrai0`–`3.des`), each starting with its briefing |

## Open questions and how to settle them

What is still unknown is in [what is open](#what-is-open) (O3–O7).
[reverse-engineering.md](reverse-engineering.md) maps each question to an
instrument: the original under DOSBox-X on the Mac first, a static reading of
`FIGHT.EXE` second, sfwin32 last. Settled this way so far: the currents
(appendix D), the radiation (D), `sle`, `diff`, the attacker lists, `car`,
`con`, Explode, the order slots, the sensors and the boats' fighting (J), the
energy pool, the energy guns, the booster and the jams (K), the boat's turning
(L) and the terrain's shading
(M).

[sfwin32](https://github.com/mnkraus/sfwin32) runs the original `FIGHT.EXE`
machine code on Windows and only replaces the DOS, video and sound services.
It contains no decompiled game logic, but it can log changes to the game's own
variables (`--watch`), trace calls, and see every volume/pan/pitch call the
game makes. Use it to observe behaviour, then implement it independently; don't
copy disassembled logic into this GPL code base.

## Working safely with sfwin32 and the original executables

sfwin32 loads the original DOS executables into memory, patches them and runs
their machine code under a vectored exception handler. Treat both the runner
and the game binaries as untrusted, and never build or run them on a
development machine. It is Windows-only anyway; whether it works under the x86
emulation of Windows on ARM is untested.

Before building or running anything from sfwin32:

- **Verify repo provenance**
  - Only use the code from the official repository,
    <https://github.com/mnkraus/sfwin32>.
  - Check the commit hash and confirm it is the intended source. The commit
    inspected (read-only, nothing built or run) while writing this roadmap was
    `e0861140e62f0934432ad05b0d0813ab48b5d670` (2026-08-23, "Give
    DESKTOP.EXE's text fields a keyboard").
- **Treat the game binaries as untrusted input**
  - Do not run `FIGHT.EXE` / `DESKTOP.EXE` or game assets from unknown sources.
  - Verify checksums or source authenticity (see the table below).
- **Run in an isolated Windows VM**
  - Use a disposable VM or a snapshot-enabled environment.
  - No sensitive data, no banking, no SSH keys, no developer credentials.
- **Keep the VM offline**
  - Disable networking before the first run.
  - Only enable it after the app behaves normally.
- **Run as a non-admin user**
  - Prefer a standard local user account.
  - Avoid Administrator privileges.
- **Review dangerous execution paths**
  - Loader and memory mapping
  - Vectored exception handler
  - Thread/context manipulation
  - CLI options like `--patch`
  - Any modification of executable memory
- **Use a clean snapshot**
  - Take a snapshot before building or executing.
  - Restore immediately if you see unexpected process creation, file writes
    or network activity.
- **Monitor behaviour**, watching for:
  - unexpected child processes
  - file writes outside the game folder
  - network connections
  - registry changes
  - suspicious DLL loads
- **Prefer a fresh test VM for each major change**, especially after changing
  patches or runner settings.
- **If anything odd happens**
  - Stop immediately.
  - Restore the VM snapshot.
  - Reassess the binary and asset sources.

Known-good checksums of the GOG release (Archimedean Dynasty, GOG id
1436955893). The MD5 values match GOG's own hash database shipped with the
installation (`goggame-1436955893.hashdb`, 1,774 entries, all verified
2026-10-13):

| File | SHA-256 | MD5 (GOG hashdb) |
|---|---|---|
| `FIGHT.EXE` | `193b07df63cc67a6d3be151792e0dc72de62401ea15208d8e25363b76de21cee` | `d853239748b8d5c9dfdca4b270d95acf` |
| `FIGHT3.EXE` | `f90440e630c6ee0a877bfcd542ab75ea1268ca83894859e3e97bfae3e4ad9c56` | `fa083f2aa8e2430495a485ee5a424b74` |
| `DESKTOP.EXE` | `540bdc3781f0d035dfaf942dbcb5a54e2a4e72c47ba13585911ea356e4b86e67` | `df53e082660918aa8514c202981d7efc` |
| `DESKTOP3.EXE` | `b9f71901015122247580f54274eed1e794f78c7984515069c15de6c558f9b488` | `9fd7a0daed5a133395d571e0b2ef1b50` |
| `SF.EXE` | `940b707c04960a84762695a95c49346cd714ee1e6ce7195a65222ebf1294106c` | `fbf78a04501d92b0652f3ca4bdacd676` |
| `SF3DFX.EXE` | `40a91282275ce99c6b3e4f653ffba70987a794cce336bbd2dc45e8a6f2b63cca` | `5e0dcb9726ab0a287a5ff7ae512862f4` |
| `game.gog` (CD image) | `58ed1b740bc51f36a4aa8df51969ec6e6a1f2dc2eaabb54fd51183a4556ea3d4` | `7cd5858c1e5e73088a39057bbb025b80` |

The hash database comes with the installation, so it only shows the files are
what that GOG installer shipped; obtain the installer from GOG itself.

## Appendix: reference data

Distances and speeds in the object files are in 1/32 m (and 1/32 m/s); one
map cell is 16 m.

### A. dtyp → object file

Decoded from the class switches in `FIGHT.EXE` (number of stock movables in
parentheses).

| typ | Class | dtyp → `VFX/SOBJECTS` file |
|---|---|---|
| 2049 | Scout → SimpleBoat | 1, 8, 9, 10, 11 prscout1 (56), 12, 13 prscout2 (73), 14 moghboot (95), 15 indrboot (20), 39, 67, 88 anscout3 (60), 89 anscout4 (76; also the default) |
| 2050 | Bomber → SimpleBoat | 16, 40, 41, 42 prbomber (18), 43 biobombr (52), 44 russbomb (46) |
| 2051 | Tank | 2 tank0 (29), 3 tank1 (57), 4 tank2 (38), 65 tank3 (11); 66 tank4 unused |
| 2052 | GunTower → TurretBase | 5, 6 guntow1 (12), 7, 90 guntow3 (96) |
| 2059 | TorpedoTower | 36; 37/38 have no files and are unused |
| 2058 | Mine | 27, 28; `num` mines above each other, `dis` height units apart (G2) |
| 2062 | Building → SimpleObject | 45 build0 (73), 46 build1 (145), 47, 48, 49 build4 (7), 50, 51 build6 (19), 52 build7 (55) |
| 2063 | CargoShip | 68–77 cship0–9, 78–87 bship0–9, 91 bship10 (72 in total) |

Cargo ships are assembled from `CargoBlockN` sections (`Name`,
`ConnectReference`, `ConnectSide` with L1 R2 F4 B8 T16 B32), block scales
312/1600, with turrets from `bblock` `[TurretN]` attached by `PatchNumber`:
a block touches the side of its reference block, with right as +x and front
as +y (the half-hulls' open seam at +x confirms this); a turret stands at
`RelativePositionX/Y` on the hull surface below it (`Z` +1000) or hangs under
it (`Z` −1000), turned by `Alpha` counter-clockwise from the bow, at its own
`[Size] Scale`/32. The mission position is the middle of the hull. A big ship
takes hits as a whole on its own `[Defense]` shield (cargo ships
20,000–60,000, Big Fat Mama 10,000,000); used up, it counts as destroyed,
stops, and its blocks blow up one after another. The master monitor shows a
picture chosen by the ship file's comment: `SHPSEACO` (bship0), `SHPTRITO`
(1, 8), `SHPPOLAR` (2, 9), `SHPINDER` (3, 10), `SHPBIONT` (4, 7), `SHPBFM`
(5, 6), `SHPCONTF` (cship0–9). Big ships turn at most 0.15 rad/s and climb at
most 5°.

### B. Orders

Each movable has up to 8 order slots `o<N>…` (N = 0–7), e.g. `o0typ`,
`o0refx`. Every slot has a break condition: `o<N>bccond1/2`,
`o<N>bcdep1/2`, `o<N>bcref1/2`, `o<N>bcop`, `o<N>bcdel`. Condition code 47
(or 0 in older files) means "no condition".

| otyp | Name in `FIGHT.EXE` | Keys | Movables (slots) / scenarios |
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

- `refx`/`refy` are map cells; `hei`/`mih`/`mah` are metres over the ground
  (32 of the game's units each, read off in October 2026; not the map's height
  units); `vel` is a percentage of the unit's MaxVelocity; `dur` is in seconds
  (100000 or 1000000 means forever); `rad`, the escort offsets and the escort's
  `ran` (how far from its place it fights, 150 m if missing, as it is in every
  mission) are metres.
- "Arrived" (conditions 43/44, objective 7) completes when a GotoWithArrive
  reaches its point. "Boarded" (45/46, objective 8) completes on the `ene` of a
  GotoWithInfiltrate. "Attacked" with `bcdep` 2 refers to a faction (IFF).
- `car` takes 0/2/4/6/10: a quarter turn (appendix J). Explode `con=2` is
  used 71 times; `con=1` once (S00202). `sle` lets a unit sleep (J); `diff` is
  the lowest difficulty the unit appears at; `ai` is never read.
- `[Attack]` in boat and bomber files drives the attack manoeuvre (appendix
  J): `AlignProbabilityFront/Back/Left/Right`, `Min/MaxAlignRadius` (game
  units, 32 per metre), `Min/MaxAlignGamma` (radians), `Min/MaxAlignTime`;
  `AttackProbabilityTurn`, `AttackProbabilityHit` (the chance to attack at
  once when fired at), `Min/MaxAttackTime`; `Min/MaxFlightRadius`,
  `Min/MaxFlightTime`, `FlightPointProbabilityUL/UR/DL/DR`,
  `FlightProbabilityHit` (the chance to break off when hit);
  `*ProbabilityTorpedoFire` per phase. The files' own comments (in German)
  describe them. Tanks use Hook/Cross/Retain/Change/Hit variants (O6). Also
  `Min/MaxClimbingAngle` and the turret `[Kinematics]` `MaxAngleVelocityA/B`.
- `sen` is the unit's sensor type and `senr` its range in metres (appendix
  J); without the keys a passive sonar of 100 m.

### C. Conditions and events

| Event | Completes | Objectives (wccond) |
|---|---|---|
| destroy | armour below zero | 2, 5, 9 |
| attack | first hit by another unit | 3 |
| identify | sonar | 4 |
| paralyze | energy defence below zero (the unit then tumbles away, appendix J) | 6 |
| finish ("ist angekommen") | own GotoWithArrive reaches its point | 7 |
| board ("wurde infiltriert") | target of another unit's GotoWithInfiltrate | 8 |

- Faction references (`bcdep` 2, `bcref` is the faction's `iff`): 1 means
  every unit of the faction is destroyed, 39 its negation, 2 any unit of the
  faction attacked. A faction without units is never destroyed.
- 0 and 47 both mean "no condition", in either slot.
- "Objective achieved" on an objective that can only fail (wccond 5) means
  "not failed yet" (the waves in S01301 and S01805, the sensor areas in
  S00405). wccond 2 on a nav point means "reached".
- Mission failure plays its message once and ends the mission 10 s later;
  "destroyed" on the player's id means the player died.
- 13 references to objectives that no unit has (S00404, S01802, S02003,
  S02006) never come true (G4).

### D. Currents and radiation

- `[CurrentNNNN]`: `typ=3072`, `px`/`py` (cells on a 2-cell grid), `pz=0`,
  `card` 0–9, `spd` 0 or 2. 1,266 entries in 20 scenarios. Direction
  (sin(card·45°), cos(card·45°), 0); 8 is up and 9 down. A cell covers the two
  map cells from `px`,`py` upwards, with no fade at its edges.
- Measured in the original (October 2026, the player's position dumped from
  memory and the HUD's readouts, boat at rest in current cells): `spd` 2 takes
  the boat from rest to about 41 m/s (HUD 141 km/h) within about 5 s facing
  along the current, to about 19 m/s facing across it; `spd` 0 to about 5 m/s;
  downward cells push 9 m/s (`spd` 2) or 4.5 m/s (`spd` 0), upward ones about
  5 m/s (`spd` 0; one of S00202's two upward cells did nothing). The engine's
  state doesn't matter, and a boat that leaves the cells coasts and slows at
  3.5–4.7 m/s². Vertigo makes of this a steady push of 9 m/s² (`spd` 2) or
  3.1 m/s² (`spd` 0) on the boat: its drag across its axis and the cap of twice
  its top speed along it give the speeds above, its own retardation the
  coasting. Affected: the player (`[Kinematics] Current`) and AI boats
  (`[Fluid] Current`); not torpedoes, mines or shots.
- Radiation has no data key: it is a depth rule, the same on every map
  (measured on S00701, S01801 and S00202). Less than 100 m below the surface
  the red radiation symbol (`HUDSYM2`) blinks at 1 Hz in the first slot of
  `[HUDSymbolBlock1]`, the depth readout turns red and the three monitors
  blank out for single frames at random; no damage, no push. Less than 60 m
  below, `ALERT` blinks out of phase with the symbol, the voice "system
  collapse" (SMPMAP 72) plays on entry and every 15 s and the alert beep
  (SMPMAP 10) every 0.8 s. The same ALERT, voice and beep are the combat
  alert, together with a red ring with a cross in the symbol block's second
  slot (O1).

### E. HUD data

- Cockpit sections in `VFX/COCKPIT/00n.DES`: `[Front]` (window, `VFX_FlightX/Y`
  the view's centre), `[Camera]` (`ViewAngle` 105, the light and fog values of
  appendix M), `[VoxelRadar0]` (offset 21,34 in the master monitor, 120×100,
  `WorldMinY` −5, `WorldMaxY` 20, `Projection` 30, `FlightY` 16, `ShadingTable`
  `VOX_000`), `[HUDAlertBlock]`, `[HUDThreatBlock]`, `[HUDPauseBlock]`,
  `[HUDSymbolBlock1]`, `[SlaveMonitor0/1]` (none in 000/Hiob), the monitors'
  `Type` and `Lamp*` keys (`*.D16`, an off and an on frame). `[Flake]` is
  unread.
- Images in `GFX/IMG/COCKPIT`: `HUDALERT`, `HUDPAUSE`, `HUDTHREA`/`HUDTHREY`,
  `HUDSYM1` (lock), `HUDSYM2` (radiation), `HUDSYM3` (current), `HUDTURR` and
  `HUDTURGR/RE/YE` (turret brackets), `HUDLOCAA`/`HUDLOCPA` (lock rings),
  armor silhouettes `{GATO,HIOB,SUCC,ZORN}{1-4}{F,L,R,B}`, 48×48 weapon
  pictures, `SHP*` 60×60 observation pictures. Font `CPITSMHC.FNT` is unused.
- Turret states from GLOBWEAP `TurretState0–8`: ARM, SCA, FIR, JAM, EMP, LOC,
  STP, SYN, VOL.
- Keys from `HELP0-2.TXT`: `p` pause, `o` turret menu, F1–F4 status pages,
  `i` armor/system status, F5–F12 turret orders, `u` nearest enemy.

### F. Texts (`TXT/FIGHTEXT.DES`)

- 11/12/13: mission accomplished / failed / "DU WURDEST ZERSTÖRT".
- 31/32: "MISSION NICHT ERFÜLLT", "AUTOPILOT NICHT VERFÜGBAR".
- 38–48: turret order menu. 49–56: Esc menu. 57/58, 61–66: death and
  game-over menu. The autopilot menu follows.
- 67–70: escalating friendly-fire warnings; 71: friendly object destroyed;
  89: mission failed after three friendly kills. Texts starting with `~` or
  `#` are red.

### G. Sounds (`SFX/SND/SMPMAP.DES`, `NumOfVoices = 8`)

| # | Use |
|---|---|
| 6 / 7 / 9 / 10 | threat / lock-on / torpedo lock-on / alert beeps |
| 29 | booster |
| 67 / 72 / 77 / 81 / 82 | voices: target locked / system collapse / torpedo ready / lock-on located / torpedo threat |
| 4 / 21 / 23 / 24 / 34 / 35 | engines: torpedo / turret / enemy 1 / enemy 2 / biont / big ship |
| 1 / 2 / 14 / 15 / 16 / 17 | big explosion / metal hit / torpedo start / ground hit / small explosion / EMS |

### H. Weapons and effects data

- Enemy weapons: `[StaticGun]` (Model, Ammunition, Relative{i}XYZ),
  `[VolleyGun]` (State{i} a/p, Duration, Tolerance), `[TorpedoMagazine]`
  (Relative{i}XYZ; TORTUR0 has 6 tubes), `[BuzzerMagazine]` (Type, Eject*),
  scenario keys `gun`, `tor`, `ntor`, `eng` (the engine model 0–5, appendix
  K; the object files' `[Engine] Type` is never read).
- `GLOBWEAP.DES` has 14 gun models: 0–10 for boats (0 Plasma Gun I to 10
  Vendetta III, 0–5 energy guns), 11–13 for turrets and vehicles; turrets with
  boat guns see O3.
- Torpedoes: `globweap.des` `[TorpedoMagazine0-7]` (items 1025–1032) and
  `[Torpedo0-21]` (items 1–22: `locktype` 0 none / 1 noise / 2 optic,
  `lockdelay` 0–6 s, `imagename`, `sfvname`/`sfvnumber`); torpedo bodies in
  `VFX/SOBJECTS/TORPEDO.DES`. Turret software: GLOBWEAP `Software0–9` (hit,
  scan, lock, volley shots, volley delay). Paralysis effectively needs the
  Flash or Leech torpedoes; gun shock damage is far below the shock shields.
- Cluster torpedo: `TORPEDO.DES` entry 18 "Ripper" (LifeTime 6, MaxVelocity
  1800, KineticStrength 500, comment "KineticStrength Area = 7600"), bursting
  within 31 m in Vertigo.
- `EXPLOSIO.DES`: 0–2 explosion with flash (S/M/L), 3–5 puff, 9–11 sparks,
  12–14 ground impact, 15–17 ECM impact, 18–20 and 24–26 ground dust, 21–23
  torpedo bubble. `DEBRIS.DES`: 0–2 plates, 3 engine, 4–8 small parts, 9–13
  large parts, 14–16 "Cadaver 1–3", 17 shrapnel, 22 torch (decoy, Noise 4);
  `LifeTime`, `Friction`, `Lift`, `Min/MaxVelocity`.
- Sprite sizes (read off in October 2026): a sprite is the frame's pixels on
  a square, each pixel 1/32 m, times the `[Size] Scale`; the `[Anim] Scale`
  is not used. Each explosion gets 0.7 to 1.3 of that at random, its frames
  last 0.75 to 1.25 of `DisplayTime`, and all but the ground impacts and the
  ground dust are rolled at random; the torpedo bubbles (21–23) keep their
  frames' size, whatever their scale. The shots are exact, a piece of trash
  is between half and the full size, and the debris varies by type (10 to
  40 %). So the small explosion is 3.6 m across, the large one 14.5 m, a
  plasma shot 1 m, and the sparks of a hit 0.8 to 2.4 m. Vertigo had the
  sprites at the `[Anim] Scale` times 4/255 of the frame, about two thirds
  of that for the explosions and half for the shots.

### I. Mission result

`FIGHT.EXE` writes `dat\save\fightout.des` with `[Global]` completed / failed
/ primary / user / secondary, and `Destination` = AutoEnd | InstantEnd |
<station> | RestoreAutoSave | RestartScenario | AbortGame. The scenario's
`[End] type` (0 in most, 1–4 in a few) picks the destination of an
accomplished mission: 1 AutoEnd, 2 InstantEnd, 3 the station st1703, 4 the
station st9901; otherwise the destination is what the player chose in the
mission menu or the autopilot menu.

### J. What the original does with `sle`, `diff`, `car`, `con`, the attackers, the order slots, the sensors and the fighting

Read off in October 2026; Vertigo does all of it unless a point says otherwise.

- **Sleeping units (`sle`).** A unit with `sle` 1 (or no key) is put to sleep
  while its horizontal distance from the player exceeds the player's sonar
  range plus 3 map cells (48 m on most maps): its engine sound stops and it
  is not updated at all until the player comes back, when it starts its
  current order slot again. Boats don't sleep while dying, exploding or in
  an Explode order, a tower not while it attacks. `sle` is 0 on units that
  have to travel across the map (800 of 949 scouts, 195 of 244 bombers, 72
  of 135 tanks) and 1 on nearly all towers, mines and turrets.
- **`diff`** is the lowest difficulty (0 easy, 1 normal, 2 hard) at which the
  unit exists; a missing key is 0.
- **Attackers.** A unit that takes the player as its target joins the
  player's list of attackers, 4 units on easy, 6 on normal, 8 on hard; when
  the list is full the unit gets no target. Ground plans take at most 4 at
  any difficulty (Vertigo's buildings can't be attacked, so this has no
  effect); units on a torpedo run against a target have a list of their own,
  8 for every target.
- **`car`** (CardinalPatrol, CardinalAttack, Ambush) is a quarter turn: 2 is
  90° one way, 4 is 180°, 6 the other way; 0, odd values and the frequent 10
  are straight ahead. A tower starts its patrol with the turret turned by it
  from its front, then every 0.5–2.5 s turns it by a random 15° to 90° either
  way, unbounded; a boat in ambush points its nose that way. Which of 2 and 6
  is left is not checked (Vertigo turns 2 left). Boats skip CardinalPatrol and
  CardinalAttack slots at once.
- **`con`** on Explode (with `dur` in seconds): 0 explodes when `dur` is over
  unless the unit has been boarded by then; 1 waits to be boarded and explodes
  `dur` seconds after that; 2 explodes when `dur` is over no matter what. The
  explosion is three explosion effects at the unit, which then tumbles away
  with a random spin, the same sequence as a unit whose energy defence has
  collapsed; the armour is untouched, so the unit is not "destroyed" for the
  mission's conditions (the AD Augmented Mod found the same).
- **Order slots** end in two ways: the break condition is checked every
  frame, and the orders finish on their own (Wait, Patrol, Ambush, Escort and
  CardinalPatrol after `dur`, Goto and GotoWithArrive at their point,
  GotoWithInfiltrate once the boat has sat at its target for `dur` seconds,
  when the target counts as boarded). A unit's 8 slots are taken in turn,
  skipping empty ones, round and round. The squadron events: "attacked" on
  the first hit on any of its units, "arrived" and "boarded" once every unit
  has, "destroyed" once every one is destroyed.
- **Flying to a point** (Goto, GotoWithArrive, the patrol points, the ambush
  point). The point is `hei` (or `mih`) metres over the ground; on the way the
  boat keeps at least that plus 0.6 of its hull radius over the ground ahead
  and, where a `mah` is given, at most `mah`. It slows down proportionally
  within 2 cells (or its hull radius) of the point and counts as there within
  1.25 cells, its hull radius or the way it flies in a second, whichever is
  largest (so Goto no longer passes through its point at full speed).
- **Patrol.** The first point is the area's centre; a new one is picked when
  the point is reached or after the time it takes to fly the radius at the
  order's speed: a random point of the circle at least half the radius away
  (an Attack order patrolling its area: 0.3 of the radius from the last
  point), its height wandering by up to half the horizontal way, clamped to
  the `mih`–`mah` band. Vertigo picks the point again on the timer where the
  original may pick one every frame once the time is up (O7).
- **Sensors (`sen`, `senr`).** Every unit has one; the default is type 1
  with 100 m. The range check is the same for all: the other unit within the
  range. Type 0 (sight) also needs nothing of the terrain between the two;
  type 1 (passive sonar) hears a unit while its noise over the squared
  distance beats 1500 over the squared range, in the game's noise units of
  500 per `[Noise] Level` step, so a level‑3 unit is heard up to the range,
  level 2 to 82 %, level 1 to 58 % of it; type 2 (active sonar) hears
  everything in range and adds 500 (one level) to the unit's own noise; type
  3 picks up the player's boat only; type 4 the player, or others by the
  passive rule. The sensor looks for hostiles every 1.5 s (0.7 s for types 3
  and 4), keeps what it found while that lives, and takes the nearest (type 0
  and 2), the loudest over its distance (1), the player (3), or the player if
  in range else the loudest (4). The scenarios set `sen` and `senr` on most
  boats (`sen 2` with 60–200 m is common).
- **Choosing a target.** Patrol, Attack, Escort and Ambush orders fight; Wait,
  Goto, GotoWithArrive and GotoWithInfiltrate never do. Each frame: the
  order's enemy (`ene`) if the sensor has it within its range; else, in the
  moment the unit is hit, the player if hostile and within twice the range;
  else what the scan found. While fighting, the range counts double: the
  target is dropped once it is beyond that (or no longer heard or seen by the
  type's rule), and the order goes on where it was with its time still
  running; the enemy takes over again once in range; a player who hits the
  unit takes over if nearer than the current target. "Hit" in the original is
  a change of the energy defence in that frame; Vertigo takes any hit by
  another unit (O7). The attacker lists above still cap how many fight the
  player.
- **The attack manoeuvre** of boats and bombers (bombers behave the same), with the
  `[Attack]` keys: *align* – pick a side by the four align probabilities
  (ahead within 30°, left or right between 30° and 150°, behind within 30°),
  a distance between the align radii and an elevation between the align
  gammas, and fly to that point in the target's frame at the order's speed;
  when the boat is on that side at a distance between the radii the run
  starts; after the align time a new side is picked; when hit, with
  `AttackProbabilityHit` the run starts at once, otherwise a new side is
  picked. *Run* – lasts an attack time; with `AttackProbabilityTurn` the boat
  stops and turns on the spot, otherwise it flies at the target at 40–100 %
  of its speed (if the terrain is in the way it turns instead); it ends when
  the target has come within a flight radius, when the time is up, or, with
  `FlightProbabilityHit` decided at the start, with the first hit taken after
  the boat has fired. *Flight* – to one of four points by the UL/UR/DL/DR
  probabilities, 850 m left or right, 500 m ahead and 500 m above or below
  the target in its frame, for a flight time; after 1.5 s the point drops to
  the target's height, and a boat sitting in the target's line of fire (within
  8°) for 1.5 s swaps sides; then it aligns again. An ambusher skips the
  first alignment and runs at once.
- **Shooting.** A boat fires whenever its gun is ready, the volley timer
  allows and the target lies within 14° of its nose, at any distance and at
  the target's position with no lead (a ground unit 0.4 hull radii higher).
  A torpedo goes, once per phase when that phase's `*ProbabilityTorpedoFire`
  came up, when the nose points at the target within 18° in the horizontal
  plane (45° for homing types) and the tube has reloaded (2–3.5 s); a target
  with 8 torpedo attackers takes no more.
- **Ambush.** The boat flies to its point, then lies there without any noise
  (so passive sonars don't hear it), two half hull heights over the ground,
  pointing the way `car` says, until the sensor finds something; being hit
  makes the player known at double range, as everywhere.
- **Escort.** The place is the escorted unit's position plus the offsets in
  its frame, a negative `yof` moved to −1 m (the escort never sits behind);
  the escort closes in at the order's speed, slower over the last 75 m and
  never much faster than the escorted unit, counts as in place within its
  hull, 1.25 cells or its top speed in a second, and then holds the place
  (the original moves it there outright) matching the escorted unit's
  heading and speed; it leaves formation beyond 1.3 of that. It fights only
  within `ran` (150 m) of its place and ends the order when the escorted unit
  is destroyed. The original measures the offsets from hull to hull (O7).
- **Tanks** keep Vertigo's simple attack (drive at the target and fire); the
  original's tank manoeuvre is not read (O6). GotoWithInfiltrate is not
  compared (O7).

### K. What the original does with the engine, the energy guns and the booster

Read off in October 2026; Vertigo does all of it.

- **The energy pool.** The player's boat and every unit with a `gun` has an
  energy level from 0 to 1, full at the start, refilled by its engine model
  from empty in 5 s (engine 0), 3 s (1), 1 s (2–4) or 0.01 s (5). The
  player's model is the boat's engine (`engine` 0–2 for the Small, Medium
  and Big Block), a unit's its `eng` key. The HUD's energy bar shows the
  level, never less than 4 pixels.
- **Energy guns.** Models 0–5 (the ones with `energydemand` above 0) draw
  from the pool; models 6–13 use rounds and the energy guns show "ENER" in
  their place. A gun fires its barrels in turn, one per trigger pull with the
  rearming time between: models 0 to 13 have 1, 2, 1, 2, 4, 1, 2, 2, 2, 3,
  4, 2, 2 and 2 barrels. A shot takes `energydemand` divided by the barrels
  off the level (never refused) and the gun is ready again once its
  `rearmingtime` has passed and the level is above the whole `energydemand`.
  So a full pool allows a burst at the rearming rate, after which the gun
  settles to one shot per (`energydemand` / barrels) × recharge time: a
  Plasma Death II (0.3, 2 barrels, 0.075 s) on engine 2 fires at its rearming
  rate for about 4 s and then every 0.15 s, on engine 0 every 0.75 s.
- **The booster** draws from the same pool: on only while the level is at
  least 0.8, 0.15 at once, 1.2 per second while it runs (about 4 s on engines
  2–4), off below 0.05. It doubles the top speed and raises the acceleration
  2.5-fold, works from a standstill, and the engine's pitch follows the speed.
- **Jams** are decided once per trigger pull: the gun's condition (1 when
  new, the saved `Defect`, at least 0.2) against a random number times the
  gun's `defectdamping`; it jams when the product reaches the condition, so
  never while the condition is at or above the damping (0.2–0.8). A jam lasts
  `defectdelay` × 0.5 to 1.5, then the gun is ready at once.

### L. How the original turns the boat

Read off in October 2026. The pitch and yaw rates relax towards the stick's
share of `MaxAngleVelocity` by the boat's `AngleFriction` fraction (0.4 for
pitch, 0.7 for yaw on the Hiob) once per update: rate += (target − rate) ×
friction; the frame time only turns the boat by the rate. Each update the
nose also drops by 0.01 × its own `dir.z` × the pitch maximum, and the roll
turns the boat by roll × `RollCenterVelocity` × the yaw maximum. So a pressed
key reaches the maximum rate only asymptotically and the rate dies the same
way after the release, which is the momentum felt in the original. Vertigo
relaxes the rates at `turnticks` ticks per second (`settings.ini` `[Fight]`,
default 30, which matched by hand). Whether the keyboard ramps the stick
value is not known; a joystick's value goes in as it is.

### M. How the original shades the terrain

Read off in October 2026. The cockpit description's `[Camera]` section holds
the numbers: `AmbientBrightness` 2.0, `CueingBackPlane` 1000,
`LightBrightness` 5.0, `LightPower` 1.0, `LightFrontPlane` 0 and
`LightBackPlane` 3650, in units of 1/32 m (31 m and 114 m). Every vertex of
the terrain gets a brightness of ambient × (1 − d/cueing), not below zero,
plus light × ((back − d)/(back − front))^power × cos, where d is the distance
from a point 8 m above and 4 m ahead of the eye and cos the cosine between the
vertex's vector and the direction to that point, not clamped; the light part
is nothing beyond its back plane. The vertex's vector is the normalised sum
of the normalised normals of the six triangles around it (the `gouraud.xyz`
file the surface descriptions name would replace that, but none exists). The
brightness is a multiple of the texture: 2 is the shading table's last row
(twice the palette), 1 its sixteenth, so the row is 16 × brightness rounded
down, kept between 0 and 31, chosen per pixel from the brightness interpolated
across the triangle; row r blends (r + 1)/32 of twice the texture with the
rest of the table's own colour F. So the ground at the eye is twice its
texture, at 16 m once, at 31 m the ambient part is gone, and from there only
the light counts: at 60 m a wall facing the boat keeps 1.9 (row 30), flat
ground 11 m below 0.45 (row 7), and at 90 m nothing of the flat ground is
left. A fade at a mission's start and end scales the whole brightness. The
energy shots' lights add to the vectors of the cell's corners under them, so
they brighten the ground through the same rows. Vertigo computes the
brightness per vertex in the pipeline's table-shading mode
(`gfx::Pipeline::TableShading`), picks the row per fragment and adds the shot
lights' strength to the brightness; its vertices and normals are the spline's,
finer than the original's grid, so the bands along slopes fall in slightly
different places. Against the original's pictures at s00701's start the band
means of the ground differ by 12.5 units RMS over four views.

The models follow the same rule: every vertex gets the brightness of its
normal, turned into the camera, against the same point 8 m above and 4 m
ahead of the eye, clamped and rounded the same way, and the polygons pick
the row per pixel from the brightness interpolated between the corners.
There is no other light on them, neither a headlight nor an ambient colour.
Their own shading tables (`MOD_nnn.S16`) hold the palette itself in the last
row, not twice it, with the same blue F in the first, so a model at the eye
shows its texture as it is and one 60 m ahead, facing the boat, keeps 1.9 of
it clamped to the texture. Vertigo draws the models in the same table-shading
mode with the fog's gain at 1; their normals are the faces' (the model files
carry none), so the flat-shaded objects of the descriptions (`ShadingMode f`,
the ground plans and blocks) differ only in that the brightness still varies
across a face. In the original's pictures at s00701's start the station, the
turret and the tower come out within 3 to 6 units of Vertigo's in each
channel.
