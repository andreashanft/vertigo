# Roadmap: implementing todo.txt

This plan covers the items in [todo.txt](../todo.txt). It is based on a code
review of this repository and on an analysis of the original German game data
(CD release) and of strings and tables in the original `FIGHT.EXE`
(October 2026). Reference data gathered during that analysis is collected in
the [appendix](#appendix-reference-data).

Most todo items need groundwork that doesn't exist yet; without it they
couldn't be built or tested in a real mission. Right now:

- About half of all units with orders (1,110 of 2,345) never appear in
  missions, because their `dtyp` has no object file mapping.
- Enemies can't shoot, and the player can't be hit or die.
- There are no torpedoes.
- Three of the mission-condition events ("attacked", "arrived", "boarded")
  never fire.
- Esc counts as winning the mission.

So the plan builds those basics first.

## What the research established

- All 11 AI order types are identified, with their names and settings, from
  strings inside the original `FIGHT.EXE`: 6 is Ambush, 7 is CardinalPatrol
  (towers), 8 is CardinalAttack, 9 is GotoWithArrive, 10 is GotoWithInfiltrate
  (boarding), 11 is Explode. Each unit has up to 8 order slots, run in
  sequence, each with its own "stop when…" condition.
- The missing mapping from unit type numbers (`dtyp`) to object files was
  recovered from `FIGHT.EXE`. That alone restores the missing units.
- Currents are fully defined in 20 scenarios (1,266 cells, `[CurrentNNNN]`).
  Nothing reads them yet.
- Every HUD item already has its layout section and images in the data,
  unused. The "slave monitor" is an armor and system status page, not a camera
  view.
- The beeps and voice lines for lock, threat and alert exist in the sound list,
  unnamed.

## Phase 0 – Test tooling (do first)

- **Test-mode options.** A mission started from the command line
  (`vertigo s00101`) always uses the chapter 1 boat, which has no turrets. Add
  options to pick the boat and equipment, and an exit code for success or
  failure.
- **Mission check.** A mode that loads all 71 missions and lists skipped units,
  unread settings and conditions that can't resolve. Rerun it after every
  phase.
- **Debug overlay.** Shows each unit's current order, current arrows and sonar
  ranges.

## Phase 1 – Combat and mission basics

| # | Step | Effort | Unlocks |
|---|---|---|---|
| 1.1 | Load every unit type: complete the type table ([appendix A](#a-dtyp--object-file)), use a placeholder instead of skipping a unit, always register objectives | S | 63 missions get their missing units (**done**: cargo ships are placeholder boxes until 1.6, tanks are static until 1.6) |
| 1.2 | Missing condition types ("any unit of faction X attacked", order chaining, protect-only objectives, nav-point objectives), a real failure path, fix the endless "mission failed" voice. Ship together with 1.1 | S–M | Missions become winnable (**done**; "attacked" fires on any hit until 1.4) |
| 1.3 | Moving units: collision box follows the boat, orientation used for drawing and hits, terrain collision | M | All AI orders (**done** for boats and bombers; tanks follow in 1.6, collisions between units come with the orders) |
| 1.4 | Damage: shots know who fired them, the player can be hit and die, "attacked" event fires, explosion when destroyed | M | Friendly fire, alert, combat orders, explosions (**done**: attacker passed through guns, projectiles and damage; "attacked" only for hits by another unit; the player is hit per side (front, left, right, back) and destroyed when a side drops below zero; destroyed units explode, sized by the unit) |
| 1.5 | Mission end: Esc menu, failure and death screens (texts already exist), restart / abort / load. Keep "skip mission" as a debug key until missions are winnable | M | Proper campaign flow (**done**; the player can only be destroyed by `--destroy` until 1.4) |
| 1.6 | Tanks (ground-following, 22 missions) and cargo ships (built from blocks, 36 missions; start with static hulls) | M / M–L | Escort targets, dust clouds (**tanks done**: tanks and crawlers drive on the ground, tilt with it at most `MaxClimbingAngle`, and run their orders; since milestone 6 their turret (the `Gun` module or the `GunTurret`) turns towards the target at 1.5 rad/s and the gun fires along it. Cargo ships are built from their blocks in milestone 5) |
| 1.7 | Enemy guns, using the gun and volley settings in the object files | M | Orders 4, 6, 7, 8 (**done**: `UnitGun` with `[StaticGun]` muzzles and `[VolleyGun]` rhythm; the mission's `gun` key picks the model (0–13, 14 is "No Gun"); boats fire from the nose, tanks along their heading, towers from a turret that turns towards its target at the turret's rates. Who fires when is up to the orders) |
| 1.8 | Torpedoes: 22 types, magazines, fire key, lock (none / noise / optic, 0–6 s), homing, decoys, enemy torpedo towers | L | Torpedo HUD, trails, cluster torpedo, turrets (**done**; the data only has lock types 0 and 1) |

## Phase 2 – Currents (M, can run alongside Phase 1)

- Read the `[CurrentNNNN]` cells into a map grid. The direction from `card` is
  the same compass mapping the game uses elsewhere; values 8 and 9 are probably
  down and up.
- Apply the drift to the player (the boat's `Current` value is already loaded,
  never used) and to AI boats, smoothed at cell edges.
- Tests: `s00303` (should push you back), `s00406`, `s01806`, `s01801`.
  Current speeds aren't in the data and have to be measured in the original.

**Done:** `Scenario::current()` blends the 2×2-cell grid bilinearly between
block centres (full strength in the middle of a current area, fading out over
one block at its edges). The player (`[Kinematics] Current`) and AI boats
(`[Fluid] Current`) drift with it; tanks don't. Speeds are the estimates from
appendix D (`CurrentSpeedSlow`/`CurrentSpeedFast` in `scenario.cpp`), 8 is
down and 9 up. The debug overlay draws an arrow per cell near the player and
shows the current at the player's position.

## Phase 3 – AI orders

Design: each unit runs its orders slot by slot. One class per order type; a
shared steering layer handles seek, arrive, hold, follow-at-offset and
circling, with terrain look-ahead.

| otyp | Order | Behaviour | Fires event | Effort | Test |
|---|---|---|---|---|---|
| 1 | Wait | hold position | – | S | s01602 |
| 2 | Goto | fly to a map cell at a given height | – | S | s00204 |
| 9 | GotoWithArrive | arrive and stop | "arrived" | S | s00204 |
| 3 | Patrol | random points within a radius and height band | – | M | s02007 |
| 5 | Escort | keep an offset to the escorted unit | – | M | s01001 |
| 7 | CardinalPatrol | tower scans its sensor range and fires at hostiles | – | M | s01602 |
| 8 | CardinalAttack | tower attacks one given target | – | S | s00704 |
| 4 | Attack | approach, attack and break off, using the `[Attack]` settings in each boat's file | – | L | s00203 |
| 6 | Ambush | wait; attack when the target comes near or it is attacked | – | M | s00201 |
| 10 | GotoWithInfiltrate | dock at the target for a set time | "boarded" | M | s00406 |
| 11 | Explode | self-destruct after a set time | destroyed | S | s00202 |

Order of work: 1, 2, 9 → 3 → 5 → (needs 1.4 and 1.7) → 7, 8 → 4 → 6 → 10 → 11.

**Combat orders (milestone 3):** 7 and 8 for towers (nearest hostile within
`rad`, on the side `car` 0–7 or all around for 8–10; or the unit `ene`), turning
the turret and firing once it points at the target, leading moving targets; 4
for boats (approach a side of the target at the align radius, attack run with
an aim error from `AttackProbabilityHit`, break off, using the `[Attack]`
settings; no closer than 35 m, and the way goes round the target) and tanks
(drive at the target, fire when aligned); patrol while the target isn't there;
6 waits at its point until the target is within 300 m or the unit is hit,
then attacks; 11 destroys the unit after `dur` (`con` 1: counted from being
boarded). Hostility is the attitude matrix (2 = hostile), the player is
faction 8 (every mission makes the enemies hostile to 8). 10 (milestone 4) flies to the unit `ene`, docks on top of it for
`dur` seconds and then completes its "boarded" event. Cargo ships (milestone 5) run the boat orders. Patrol and Escort units
(milestone 5) also attack hostile units within their sensor range `senr`
(100 m if unset) and return to their order once the target is gone or more
than 2.5 sensor ranges away. The sensor type `sen` (optical, passive,
active) isn't told apart yet. Cargo ships leave this to their turrets.

**Done:** orders 1, 2, 9, 3 and 5 for boats and bombers (`fight/orders.cpp`),
with "stop when" conditions per slot. A slot ends when its break condition is
met, or when it is finished: Goto and GotoWithArrive on arrival (20 m), Wait,
Patrol and Escort after `dur`. After the last slot the unit holds its
position. Heights are above ground with a look-ahead of 60 m. Units with other
orders hold their position for now; `--check` lists them ("order not
implemented"), as well as the orders of units that can't move yet (tanks,
crawlers, towers, buildings, cargo ships: "orders not run").

## Phase 4 – HUD

| Item | What it is | Effort | Needs |
|---|---|---|---|
| Pause | P key stops mission time and sounds, shows the PAUSE image (**done**) | S | – |
| Voxel radar | 120×100 terrain view ahead of the boat, inside the master monitor | M | – |
| Slave monitor | Armor damage silhouette plus a weapons/ammo status table, and F1–F4 detail pages (**done** except F1–F4 and the lamps: armor page from the four side images per damage level, laid out as a 64×64 silhouette; systems page with ARMO/GUN/TORP/TUR1/TUR2, ammunition and the gun states RDY/ARM/FIR/JAM/EMP/ENE; with two monitors one shows each page, I switches) | M | Full rows need 1.8 |
| Icons | Current, radiation (probably shallow depth; unverified), locked (**lock and current done**; radiation open) | S each | Phase 2, 1.8 |
| Alert / threat | ALERT when hit, THREAT for an incoming homing torpedo, with matching beeps and voice (**done**: ALERT blinks for 3 s after a hit, with the alert beep; THREAT blinks while homing torpedoes follow the player, with the threat beep and the voice for each new one) | S | 1.4, 1.8 |
| Friendly fire | Escalating radio warnings; the third friendly kill fails the mission (**done**: friends are units not hostile to the player's faction 8 that aren't destroy objectives or mines; warnings 67–70 at most every 3 s, kills 71, the third kill 89 and failure; `~` texts are red) | M | 1.4, 1.5 |
| Torpedo lock + countdown | Lock ring, countdown from the torpedo's lock time, beep and "torpedo ready" voice (**done**: the target must stay locked and within 40° of the nose; the ring turns yellow when complete) | S | 1.8 |
| Turret targeting | Turret software types, turret states, the `o` and F5–F12 order menu, target brackets (**done**: orders attack my target / sync / search and destroy / stop; Claw targets attacking torpedoes, Punisher hostile vessels, Eraser both; scan, lock time, accuracy and volleys from GLOBWEAP `[SoftwareN]`; states from `[TurretStateN]`; torpedoes can be shot down; brackets green scanning, yellow locking, red firing; turrets turn at 1.5 rad/s, as the turret models do, and fire once they point at the target (milestone 6)) | L | 1.4, 1.8 |

## Phase 5 – Effects

- **Particle system first (M):**
  - Fix the effect-file parsing; speed and debris lifetime, drag and sinking
    are currently ignored or uninitialised.
  - Replace one heavyweight object per particle with lightweight pooled
    particles drawn in batches.
  - Add limits and distance culling.

  **Done** (milestone 6): `Particles` holds explosions, debris, bubbles,
  torpedo trails and dust in one pool of 3,000 and draws them in one batch
  per texture. Bubbles and dust only use three quarters of it; nothing is
  spawned or drawn beyond 220 m (the fog ends at 200 m) or drawn behind the
  camera. Projectiles stay objects, as they collide.
- **Explosions with debris and bodies (M, needs 1.4):** explosion sized to the
  object, 4–10 debris parts, the three "Cadaver" sprites for crewed vessels,
  sparks on hits. Test: `s00105`. **Done** (without the particle pool): the
  billboard loader reads `LifeTime`, `Friction`, `Lift` and `Min/MaxVelocity`;
  `Debris` pieces fly off, slow down, sink (`Lift` × 4 m/s²) and settle on the
  ground until their `LifeTime` ends.
- **Dust clouds (S):** ground-dust sprites behind moving tanks and crawlers.
  Needs 1.6 and moving units. **Done** (milestone 6): moving tanks and crawlers stir up
  the "bodenstaub" sprites (EXPLOSIO 18–20, by the vehicle's size) behind
  their tracks, more often the faster they go; the dust plays slowly and
  drifts up. Which sprite the original uses is still to be checked.
- **Torpedo trails (S, needs 1.8):** the data's "torpedo-bubble" sprite,
  spawned every ~60 ms. **Done.**
- **Cluster torpedo (M, needs 1.8):** the Ripper (torpedo 18). The data has no
  submunition values, so the burst has to be designed: several flare shots, or
  area damage of 7600 within ~31 m. **Done** as area damage: the Ripper bursts
  on impact, at the end of its run or within 25 m of a unit, damaging
  everything within 31 m except its launcher (7600, half at the edge).

## Phase 6 – Nearby engine sounds (M)

- Add 3D audio: the listener follows the camera, and cockpit and voice sounds
  stay attached to the listener.
- Give each unit its own looping sound; play the 4 nearest engines.
- Pick the sample by unit class (enemy, bomber, biont and big-ship engines
  exist); raise pitch with speed, as for the player's engine.

**Done:** sounds are attached to the listener by default; the listener follows
the player; enemy gunfire, hits and explosions play where they happen;
`EngineSounds` loops the engines of the 4 nearest moving units within 300 m
(scouts enemy engine 1, bombers/tanks/crawlers enemy engine 2, bionts the
biont engine, cargo ships the big-ship engine), pitch rising with speed.

## Milestones

1. **Every mission loads and can end properly:** Phase 0, steps 1.1, 1.2 and
   1.5, plus Pause.
2. **The world moves:** step 1.3, orders 1/2/9/3/5, currents, tanks, voxel
   radar, dust clouds.
3. **The world fights back:** steps 1.4 and 1.7, orders 7/8/4/6, explosions,
   alert, friendly fire, slave monitor, engine sounds. (**Done**, without the
   particle pool, the slave monitor's F1–F4 pages and lamps, and THREAT,
   which needs torpedoes.)
4. **Torpedoes:** step 1.8, lock HUD, threat and locked icons, trails, cluster
   torpedo, turret targeting, orders 10/11. (**Done**; turrets aim instantly,
   tank turrets don't aim yet, the radiation icon is open.)
5. **Cargo ships and a balancing pass** against the original. (**Cargo ships
   done**: `CargoShip` builds the hull from its blocks, places the turrets on
   it and runs the boat orders; see appendix A. **Difficulty done**: Easy,
   Normal or Hard in the main menu (DEEPTEXT 107–109), Normal by default and in
   test mode, `--difficulty` to change it. A unit whose `diff` key (0–2) is
   above the level stays out of the mission. Only boats, bombers and tanks
   carry it: 151 of them need Normal, 112 Hard, and before this every
   mission was played as Hard. An absent unit counts as achieved in "destroy
   these" objectives, is in no faction and never fires its own events.
   **Balancing so far:** `AttackProbabilityHit` is now the chance that an
   attack run is on target; the other runs aim past the hull. Before, the aim
   error was smaller than the player's boat, so nearly every run hit. Ship
   turrets engage within their sensor range `senr`. Test runs use a fixed
   random seed (`--seed`). Over 3 seeds × 71 missions at Normal, with the
   idle chapter-1 boat, 14 of 213 runs end with the player destroyed within
   60 s, down from 20. The remaining ones are a tower at the start (S00704),
   torpedo runs, and chapter-22 warships (S02204) that need a late-game boat.
   Still open, and only measurable in the original: the energy pool and its
   recharge for energy guns (`EnergyDemand` and the gun state "EnergyDelay"
   exist, the pool doesn't), tower accuracy, and cargo-ship placement and
   turret size.)
6. **Loose ends:** what earlier milestones left out: voxel radar, dust
   clouds and the particle pool, the slave monitor's F1–F4 pages and lamps,
   turrets that turn at their rates (player and tanks), collisions between
   units. The radiation icon and the sensor type wait for the alignment with
   the original, which comes near the end.

## Open questions and how to settle them

| Unknown | How |
|---|---|
| Current speeds; whether directions 8/9 are down and up | Measure the player's drift in the original, via the GOG DOSBox install or position logging in sfwin32 |
| Unclear order settings (`car` for towers and ambush, `con` for Explode, `sle`, `diff`), mission end types, one objective type (`wccond 9`) | Study the relevant `FIGHT.EXE` routines one at a time with sfwin32's tools, then write our own code (no copying) |
| How engine volume and pan fall off with distance | Log the original's volume, pan and pitch calls in sfwin32's `audio.c` |
| Radiation trigger, cluster burst, alert trigger, which dust sprite, debris units | Watch the original (e.g. `s00701` for radiation) |

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

## iPad port

Nothing in the engine rules out an iPad version (assessed October 2026): the
code is portable C++, the video and sound decoders are our own, matrices are
computed with glm, and the 566 MB of game data fit easily. As on the desktop,
players bring their own game data. Most steps pay off on the desktop too, so
they can land on master one by one.

| # | Step | Effort | Notes |
|---|---|---|---|
| P1 | Drop SDL2 | S | Only `SDL_GetTicks` and `SDL_iconv_string` (Windows file names) are used; replace them with `QElapsedTimer` and Qt's own conversion |
| P2 | Qt 6 | M | `QGLWidget` is gone in Qt 6. Use `QGuiApplication` with a `QOpenGLWindow`; Qt Widgets is then not needed, as the window is the only widget besides the `QFileDialog` folder picker. Replace `QRegExp`, `QString::SkipEmptyParts`, `qsrand`, `QDesktopServices::storageLocation` and `QGLWidget::convertToGLFormat`. Settles the Qt 5 risk below |
| P3 | Shader renderer | M–L | About 280 fixed-function calls in 22 files: matrix stack, lighting, fog, alpha test, `glBegin`/`GL_QUADS`, `glPushAttrib`. Replace the GL matrix stack with a glm one and draw with one or two shaders (texture, light, fog, alpha discard): one code path for OpenGL 3.2 core on the desktop and OpenGL ES 3 on the iPad. A first step towards the modern renderer (see risks) |
| P4 | Case-insensitive data access | S | The data folders are upper case (`GFX/IMG/...`), the code opens lower-case paths (`gfx:img/...`). That only works on file systems that ignore case, like macOS's default; iPadOS's doesn't. Resolve names case-insensitively in `util::File` and the Qt search paths, or lower-case the files on import |
| P5 | iOS build | S | `qt_add_executable` with an Info.plist, icons and signing. Qt 6 for iOS comes from the Qt online installer, not Homebrew. Test in the iPad simulator first |
| P6 | Data import | S (M with the CD image) | Replaces the folder dialog: show the app's Documents folder in the Files app (`UIFileSharingEnabled`, `LSSupportsOpeningDocumentsInPlace`) and look for `DAT`, `GFX`, `SFX`, `TXT` and `VFX` there. Optionally import straight from `game.gog` and the GOG installation |
| P7 | App lifecycle and audio session | S | Pause the mission and save the settings when the app goes to the background (`closeEvent` never comes on iOS), and make no GL calls while in the background. Set an `AVAudioSession` category and suspend the OpenAL context during interruptions (calls, Siri) |
| P8 | Keyboard | S | A hardware keyboard works as is, but older Magic Keyboards for iPad have no Esc and no F keys (F5–F12 are the turret orders), so add second bindings. Bring up the on-screen keyboard for save-game names |
| P9 | Game controller | M | GameController.framework through a small Objective-C++ bridge (Qt 6 has no gamepad module), or SDL3. The sticks feed the analog `m_left`/`m_right`/`m_up`/`m_down` in `Scenario`. There are more actions than buttons, so turret orders and torpedo selection need a menu or button combinations. Useful on the desktop too |
| P10 | Touch controls | L | Virtual stick and buttons over a cockpit that fills the 640×480 screen. Menus and stations already work, as Qt turns taps into mouse clicks; hover highlights only show with a trackpad or Pencil |

Order of work: P1 → P2 → P3 → P4 on the desktop, then P5–P8 (playable with a
hardware keyboard), then P9 and P10.

Platform notes:

- OpenGL ES (deprecated since iOS 12) and OpenAL (deprecated since iOS 13) are
  still in the iOS 27 SDK (checked with Xcode 27.0, October 2026). Should
  Apple remove them, ANGLE's Metal backend runs the same OpenGL ES 3 code, and
  OpenAL Soft replaces OpenAL.
- The game is 4:3, iPad screens are between 4:3 (13" Air) and about 3:2
  (mini), so the bars at the sides stay small. During missions, defer the
  system's edge gestures so a swipe from the bottom doesn't leave the game.

Distribution: the GPL is generally read as incompatible with the App Store
and TestFlight terms (VLC was removed in 2011 for this reason), and an
exception would need every copyright holder. So players install it
themselves:

- built from source in Xcode: a free Apple ID signs it for 7 days, a paid
  developer account for a year;
- as an unsigned IPA built by CI, installed with AltStore or SideStore
  (re-signed every 7 days with a free Apple ID);
- in the EU through AltStore PAL, which needs a developer account and Apple's
  notarization.

## Risks

- Turning on enemy fire before the AI is tuned will make early missions
  unfair, so keep it behind an option until then.
- Collision cost in large missions (`s0220x`) will need a spatial grid.
- How cargo-ship blocks fit together is unverified.
- Keep renderer changes self-contained so the later modern renderer and VR
  work stays easy.
- Qt 5 support ends in Homebrew in May 2027.

Effort: S is up to a day, M a few days, L one to three weeks, all rough and
for one developer.

## Appendix: reference data

Distances and speeds in the object files are in 1/32 m (and 1/32 m/s); one
map cell is 16 m.

### A. dtyp → object file

Decoded from the class switches in `FIGHT.EXE`. Entries marked ✓ were in
`Scenario`'s type map before step 1.1; since then all of them are (number of
stock movables in parentheses).

| typ | Class | dtyp → `VFX/SOBJECTS` file |
|---|---|---|
| 2049 | Scout → SimpleBoat | 1 ✓, 8 ✓, 9 ✓, 10 ✓, 11 prscout1 (56), 12 ✓, 13 prscout2 (73), 14 moghboot (95), 15 indrboot (20), 39 ✓, 67 ✓, 88 anscout3 (60), 89 anscout4 (76; also the default) |
| 2050 | Bomber → SimpleBoat | 16 ✓, 40 ✓, 41 ✓, 42 prbomber (18), 43 biobombr (52), 44 russbomb (46) |
| 2051 | Tank (new class) | 2 tank0 (29), 3 tank1 (57), 4 tank2 (38), 65 tank3 (11); 66 tank4 unused |
| 2052 | GunTower → TurretBase | 5 ✓, 6 guntow1 (12), 7 ✓, 90 guntow3 (96) |
| 2059 | TorpedoTower | 36 ✓; 37/38 have no files and are unused |
| 2058 | Mine | 27 ✓, 28 ✓; the mine keys `dis` and `num` are ignored today |
| 2062 | Building → SimpleObject | 45 build0 (73), 46 build1 (145), 47 ✓, 48 ✓, 49 build4 (7), 50 ✓, 51 build6 (19), 52 build7 (55) |
| 2063 | CargoShip (new composite class) | 68–77 cship0–9, 78–87 bship0–9, 91 bship10 (72 in total) |

Cargo ships are assembled from `CargoBlockN` sections (`Name`,
`ConnectReference`, `ConnectSide` with L1 R2 F4 B8 T16 B32), block scales
312/1600, with turrets from `bblock` `[TurretN]` attached by `PatchNumber`.
As built (`fight/boats/cargoship.cpp`): a block touches the side of its
reference block, with right as +x and front as +y. The half-hulls have their
open seam at +x, which confirms this, and the blocks end up one model unit
apart. A turret stands at `RelativePositionX/Y` in the block's model units, on
the hull surface found straight below it (`Z` +1000), or hangs upside down
under the hull (`Z` −1000). `Alpha` turns it counter-clockwise from the bow, in
the ship's frame for both. Turrets use their own `[Size] Scale`/32, about
1.2 m. The mission position is the middle of the hull seen from above, so
long ships turn around their middle and their sterns don't sweep over nearby
units. Turrets fire at hostile units on their side of the hull within `senr` and the
gun's range.

Damage is not documented, so this is a guess. A hit damages the nearest turret
(within 3 m) or block. A block or turret whose shield runs out explodes and is
removed, together with the turrets on it. The ship's own `[Defense]` shield
takes every hit, and the ship is destroyed when that shield or all its blocks
are gone. When a block goes, the blocks no longer joined to the largest
remaining part (by touching boxes) break off, sink tumbling, and explode on
the ground. The kinematics in the ship files copy the bomber's, so big ships turn
at most 0.15 rad/s and climb at most 5°.

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

- `refx`/`refy` are map cells; `hei`/`mih`/`mah` are height units (≈1.125 m
  in 60 of 71 maps); `vel` is a percentage of the unit's MaxVelocity; `dur` is
  in seconds (100000 or 1000000 means forever); `rad` and the escort offsets
  look like metres (inferred).
- "Arrived" (conditions 43/44, objective 7) completes when a GotoWithArrive
  reaches its point. "Boarded" (45/46, objective 8) completes on the `ene` of a
  GotoWithInfiltrate. "Attacked" with `bcdep` 2 refers to a faction (IFF),
  not a single unit.
- `car` takes 0/2/4/6/10 (mostly 10) with no obvious pattern yet. Explode
  `con=2` is used 71 times; `con=1` only once (S00202, probably "after being
  boarded"). `sle` and `diff` are read by `FIGHT.EXE` but unclear; `ai` is
  never read.
- `[Attack]` in boat and bomber files drives the attack manoeuvre:
  `AlignProbabilityFront/Back/Left/Right`, `Min/MaxAlignRadius`,
  `Min/MaxAlignGamma`, `Min/MaxAlignTime`; `AttackProbabilityTurn`,
  `AttackProbabilityHit`, `Min/MaxAttackTime`; `Min/MaxFlightRadius`,
  `Min/MaxFlightTime`, `FlightPointProbabilityUL/UR/DL/DR`,
  `FlightProbabilityHit`; `*ProbabilityTorpedoFire` per phase. Tanks use
  Hook/Cross/Retain/Change/Hit variants. Also relevant: `Min/MaxClimbingAngle`,
  turret `[Kinematics]` `MaxAngleVelocityA/B`.

### C. Conditions and events

| Event | Where it should complete | Objectives (wccond) |
|---|---|---|
| destroy | `ActiveObject::destroy` (works) | 2, 5, 9 |
| attack | first hit by another object, including AI hits | 3 |
| identify | sonar (works) | 4 |
| paralyze | shock shield below zero (completes, but has no effect) | 6 |
| finish ("ist angekommen") | own GotoWithArrive reaches its point | 7 |
| board ("wurde infiltriert") | target of another unit's GotoWithInfiltrate | 8 |

Handled since step 1.2:

- Faction references (`bcdep` 2, `bcref` is the faction's `iff`): 1 means
  every unit of the faction is destroyed (S01501 "the Crawlers are gone"),
  39 its negation, 2 any unit of the faction attacked. A faction without units
  is never destroyed (S00402 refers to faction 1, which has no units).
- 0 and 47 both mean "no condition", in either slot.
- "Objective achieved" on an objective that can only fail (wccond 5: protect a
  unit, or stay out of an area) means "not failed yet". This spawns the waves
  in S01301 and S01805 and drives the sensor areas in S00405.
- wccond 2 on a nav point means "reached" (S00303, S02006).
- Mission failure plays its message once and ends the mission 10 s later.

Since step 1.5, "destroyed" on the player's id means the player died (S00401
sends pirates). Still reported by `--check`: 12 references to objectives that
no unit defines (S00404, S01802, S02003, S02006).

### D. Currents and radiation

- `[CurrentNNNN]`: `typ=3072`, `px`/`py` (cells on a 2-cell grid), `pz=0`,
  `card` 0–9, `spd` 0 or 2. 1,266 entries in S00202, S00204, S00303, S00401,
  S00405, S00406, S00701–S00705, S00902, S00903, S01306, S01801–S01804,
  S01806 and STRAI2.
- Direction: (sin(card·45°), cos(card·45°), 0), checked against map geometry
  and briefings (S00406, S01806, S01801). 8 and 9 are vertical.
- Speeds are not in the data; start with about 5 units/s for `spd` 0 and 12
  for `spd` 2 and tune against the original.
- Affected: the player (`[Kinematics] Current`), AI boats (`[Fluid] Current`
  0–1; possibly ignored by the original). Not affected: torpedoes, mines, gun
  projectiles.
- Radiation has no data key. All radiation warnings (M00701/02/03/05, M01801)
  belong to maps with `[Surface] depth` 200 or 300, where the player starts
  124–167 m below the surface, so it is probably "too close to the surface".

### E. HUD data not used yet

- Cockpit sections in `VFX/COCKPIT/00n.DES`: `[VoxelRadar0]`,
  `[HUDAlertBlock]`, `[HUDThreatBlock]`, `[HUDPauseBlock]`,
  `[HUDSymbolBlock1]`, `[SlaveMonitor0/1]` (none in 000/Hiob), `[Camera]`
  (`ViewAngle` 105; the code hardcodes 60°), `[Flake]`, the monitors' `Type`
  and `Lamp*` keys (lamp files `*.D16` hold an off and an on frame).
- `[VoxelRadar0]`: offset 21,34 relative to the master monitor, 120×100,
  `WorldMinY` −5, `WorldMaxY` 20, `Projection` 30, `FlightY` 16, shading
  table `VFX/TEXTURE/VOX_000.S16`.
- Images in `GFX/IMG/COCKPIT`: `HUDALERT`, `HUDPAUSE`, `HUDTHREA`/`HUDTHREY`,
  `HUDSYM1` (lock), `HUDSYM2` (radiation), `HUDSYM3` (current), `HUDTURR`
  (turret target bracket), `HUDTURGR/RE/YE`, `HUDLOCAA`/`HUDLOCPA` (lock
  rings, loaded but never drawn), armor silhouettes
  `{GATO,HIOB,SUCC,ZORN}{1-4}{F,L,R,B}`, 48×48 weapon pictures, `SHP*` 60×60
  observation pictures. Font `CPITSMHC.FNT` is unused.
- Slave monitor pages: armor state (64×64 silhouette, damage level 1–4 per
  side) and system state (NAME/AMMO/STAT for ARMO, GUN, TORP, TUR1, TUR2,
  with states from GLOBWEAP `TurretState0–8`: ARM, SCA, FIR, JAM, EMP, LOC,
  STP, SYN, VOL). `Player::damage` currently splits sides by top/bottom rather
  than left/right.
- Keys from `HELP0-2.TXT`: `p` pause, `o` turret menu, F1–F4 status pages,
  `i` armor/system status, F5–F12 turret orders.

### F. Texts (`TXT/FIGHTEXT.DES`)

- 11/12/13: mission accomplished / failed / "DU WURDEST ZERSTÖRT".
- 31/32: "MISSION NICHT ERFÜLLT", "AUTOPILOT NICHT VERFÜGBAR".
- 38–48: turret order menu. 49–56: Esc menu (continue, abort game, restart
  mission, confirmations). 57/58, 61–66: death and game-over menu.
- 67–70: escalating friendly-fire warnings; 71: friendly object destroyed;
  89: mission failed after three friendly kills. 70, 71 and 89 start with `~`,
  which the radio message widget should treat as red (it only handles `#`).

### G. Sounds (`SFX/SND/SMPMAP.DES`, `NumOfVoices = 8`)

| # | Use |
|---|---|
| 6 / 7 / 9 / 10 | threat / lock-on / torpedo lock-on / alert beeps |
| 67 / 72 / 77 / 81 / 82 | voices: target locked / system collapse / torpedo ready / lock-on located / torpedo threat |
| 4 / 21 / 23 / 24 / 34 / 35 | engines: torpedo / turret / enemy 1 / enemy 2 / biont / big ship |
| 1 / 2 / 14 / 15 / 16 / 17 | big explosion / metal hit / torpedo start / ground hit / small explosion / EMS |

The sound system is 2D today (no listener or source positions), with 32
OpenAL sources and one shared instance per sample.

### H. Weapons and effects data

- Enemy weapons: `[StaticGun]` (Model, Ammunition, Relative{i}XYZ),
  `[VolleyGun]` (State{i} a/p, Duration, Tolerance), `[TorpedoMagazine]`
  (Relative{i}XYZ; TORTUR0 has 6 tubes), `[BuzzerMagazine]` (Type, Eject*),
  scenario keys `gun`, `tor`, `ntor`, `eng` ("Armament").
- Torpedoes: `globweap.des` `[TorpedoMagazine0-7]` (items 1025–1032) and
  `[Torpedo0-21]` (items 1–22: `locktype` 0 none / 1 noise / 2 optic,
  `lockdelay` 0–6 s, `imagename`, `sfvname`/`sfvnumber`); torpedo bodies in
  `VFX/SOBJECTS/TORPEDO.DES` (`[ClusterN]`, `[SensorN]`, `[KinematicsN]`,
  `[OffenseN]`). Turret software: GLOBWEAP `Software0–9` (hit, scan, lock,
  volley shots, volley delay).
- Paralysis effectively needs the Flash or Leech torpedoes; gun shock damage
  is far below the shock shields.
- Cluster torpedo: `TORPEDO.DES` entry 18 "Ripper" (ripper.mod, LifeTime 6,
  MaxVelocity 1800, KineticStrength 500, comment "KineticStrength Area =
  7600"). Candidate submunitions: `SHOOT.DES` 7/8 (flare / invisible flare,
  Range 1000, Velocity 300, KineticStrength 7600).
- `EXPLOSIO.DES`: 0–2 explosion with flash (S/M/L), 3–5 puff, 9–11 sparks,
  12–14 ground impact, 15–17 ECM impact, 18–20 and 24–26 ground dust, 21–23
  torpedo bubble.
- `DEBRIS.DES`: 0–2 plates, 3 engine, 4–8 small parts, 9–13 large parts,
  14–16 "Cadaver 1–3" (dead bodies), 17 shrapnel, 22 torch (decoy, Noise 4).
  Uses `LifeTime`, `Friction`, `Lift`, `Min/MaxVelocity`, which the billboard
  loader ignores today.

### I. Mission result

`FIGHT.EXE` writes `dat\save\fightout.des` with `[Global]` completed / failed
/ primary / user / secondary, and `Destination` = AutoEnd | InstantEnd |
<station> | RestoreAutoSave | RestartScenario | AbortGame. A sample file is in
the GOG installation (`DAT/SAVE/FIGHTOUT.DES`).
