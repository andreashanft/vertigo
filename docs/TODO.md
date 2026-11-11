# TODO

What is still open on the way to 1.0, the feature-complete clone. What the
original does is in [KNOWLEDGE.md](KNOWLEDGE.md); this file only lists work
and questions. Effort marks: S is up to a day, M a few days, L one to three
weeks, for one developer.

## Work items

Most important first. The beta's aim is that the story can be played from a
new game to the ending as in the original; the mechanics and the look are
close now, what is left is mostly the campaign as a whole and how its
missions feel.

| # | Item | Effort | Notes |
|---|---|---|---|
| T1 | Campaign playthrough | L | The story from a new game to the ending on the desktop, saving after each mission. Per mission: won at the first try or not, the time it took, damage and money afterwards, and anything off (triggers, radio messages, stuck units, the station afterwards). Besides: the chapter changes, dialogues and tasks, the depot, loading a save, the death and failure menus, the ending's movie. The open mission (TRAINING) starts any single mission again with a fully fitted boat |
| T2 | Difficulty and feel | M | From T1's notes and side by side with the original on a few missions; S00101, S00201, S00402, S00903, S01202 and S02001 have escorts, ambushes, towers, bombers and big fights between them. A measure both games give without any tooling: how long a boat that does nothing lasts at a mission's start, on each difficulty, and how many units attack it at once. In Vertigo's seeded idle runs (normal, 60 s) the idle boat is destroyed in 15 of the 71 missions, in 12 of them with every seed: in S02001 after 15–23 s, in S00201, S00402, S00903, S01202 and S01302 after 19–28 s. The levers: the attacker lists per difficulty, the sensor ranges, the attack settings, the hit spheres, the guns' damage and the player's armour (KNOWLEDGE.md, chapters 4 to 6) |
| T3 | Mission end types | S | `[End] Type` (KNOWLEDGE.md 2.7): 1 AutoEnd in 26 missions, 2 InstantEnd in the four training missions, 3 the freighter st1703 after S00704, 4 st9901 after S02205. Vertigo uses it only to leave out the "accomplished" voice; every mission waits for Alt+A and the autopilot menu, which lists the area's open stations. The freighter, station 3 of chapter 7, is closed at the chapter's start and opened by dialogue 007008; whether the story gets there without type 3 is for T1. Watch what AutoEnd and InstantEnd look like in the original |
| T4 | Platforms | M | macOS, Linux and the iPad are played; Windows is built, packed and tested by CI but not played. Play a few missions on each |
| T5 | S00705: Schwartz's infiltration | S | The AD Augmented Mod's notes say Schwartz (2066) infiltrates only the first control unit and waits until Marquez is dead; in Vertigo he infiltrates all three in turn. Watch the original: a slot with a break condition may not end on its own there |
| T6 | iPad: OpenGL ES and OpenAL | M | Both are deprecated but still in the iOS 27 SDK. ANGLE and OpenAL Soft would replace them |

## Open comparisons

Questions about the original that are not settled yet. Each is settled by
watching the original and reading its data, then implementing the behaviour
independently; the answer goes into KNOWLEDGE.md.

Missions and the campaign:

- Whether buying new armour mends the sides.
- What AutoEnd and InstantEnd look like (T3).
- Whether the training boats have Titanium Armor (their setups' armour 3,
  counted from 0 like the other items; KNOWLEDGE.md 12.4).
- Whether the autopilot menu lists the start station.
- `wccond 9` (56 units, s00303 and s02006 among them).
- Whether a slot with a break condition ends on its own (T5).
- A squadron is the units with the same name. Where several big ships share
  one (the six freighters of Rette Hilfstransport, the two of S01203), the
  original counts every destroyed block towards the squadron's "destroyed"
  count, so a condition on that squadron can be met after a few blocks;
  Vertigo counts each ship once, when its first block goes. Whether the
  mission reacts that early in the original.

Units' behaviour:

- Which of `car` 2 and 6 is left (Vertigo turns 2 left).
- A tower's salvo in detail (KNOWLEDGE.md 4.3): whether its mountings fire
  in the same frame or one after the other (Vertigo's one per step, as a
  round's barrels), and whether an energy gun on a tower fires all the
  mountings in one go too (Vertigo's fires one shot per rearming time,
  through the mountings in turn).
- How many homing torpedoes the player can have on it at once (every AI
  unit takes at most 4; Vertigo keeps 8 for the player).

Sensors, HUD and views:


World, physics and effects:

- Why one of S00202's two upward current cells does nothing.
- How an active mine moves while it chases: how fast, and how far a current
  carries it. Its drive takes currents at a factor of 0.2 against its own
  drag, so a current would carry it along noticeably if its chase is driven
  by a force, and hardly at all if by a set speed; Vertigo's chases at a set
  0.3 of `MaxVelocity` (0.94 m/s) and ignores currents, and passive mines
  stay put in both. Watching one active mine chase through a current settles
  it (KNOWLEDGE.md 3.3).
- What else the original scales with the radiation's strength between 100
  and 40 m below the surface.
- Whether the keyboard ramps the stick value when turning the boat.
- Whether the ground dust is additive with `Transparency = 1`.
- The length of the fade at a mission's start and end (about a second in the
  original; Vertigo fades in over 1.5 s and out over 2 s).
- Whether the hidden hull of a destroyed boat drifts on once it is gone (the
  death view's camera would follow it; Vertigo stops it).

Sound:

- How engine volume and pan fall off with distance.
- Whether the mission music loops, pauses in the menu and how loud it plays.

## Known approximations

Deviations a player may notice. Each is deliberate or waits for one of the
comparisons above.

- Some explosions' sizes: the mines' explosions keep their own size (within
  about 10 % of the original's), the destruction sequence's come in sizes
  Vertigo's effects don't take, and Vertigo sizes the explosions by the
  sprites' extent, which may not be the size the original measures them by.
- The boat's turn rates relax at `turnticks` ticks per second (`settings.ini`
  `[Fight]`, default 30, matched by hand).
- The escort is pulled into its place rather than moved there outright.
- The bombers' turrets sit where a ray from above or below meets the hull
  rather than on the file's polygon.
- Tanks stand on Vertigo's smooth ground; the original takes each corner's
  height between the map's points.
- The outside views' camera distances are in the watched unit's radii.
- The agile torpedoes' steering ticks run at 30 a second.
- The Torch's torch lights the ground about four times as strongly, as
  Vertigo's lights fall off with the square of the distance and the
  original's linearly.
- The terrain's shading bands fall in slightly different places along slopes,
  as Vertigo's vertices and normals are its spline's, finer than the
  original's grid; the ground runs straight between the corners only in a
  lifted cell and its eight neighbours.
- The mission runs in steps of 1/60 s, one a frame at 60 Hz and one every
  other frame at 120 Hz. So what the original does once a frame (a gun fires
  on the frame after the one in which it is ready, a round's barrels one a
  frame, a patrol's new point every frame) goes as at 60 frames a second on
  every display; the original has no rate of its own, as it updates once a
  frame at whatever rate the computer reaches (KNOWLEDGE.md 1). A frame
  between two steps shows the view, and the units, torpedoes and shots, a
  part of a step further along their way, so the picture moves evenly on
  every display; the units' own turning, the particles and the instruments
  go in the steps.
- The player's turrets hold their fire while a friend is in the line, in
  front of the target or behind it; the original's fire through friends.
  This one is kept on purpose.
- Vertigo's ground plans can't be attacked, so their attacker lists have no
  effect.
- PAUSE shows over the views from outside too.
- The master monitor's terrain radar draws every building as its model, in
  more detail than the original's reduced picture.
- The lamps beside the monitors light up with the monitor's mode; the
  original's never change.

## New features

Feature ideas beyond the original, not needed for 1.0.

| # | Item | Notes |
|---|---|---|
| F1 | Upscaled art for owners of the game | Share the AI-upscaled art in a way that it can be used only when the original data is present |

### F1: Upscaled art for owners of the game

Distribute an encrypted package that uses the original game data as a key to open it. Its download
link can then be public, and sharing the link should not violate any copyright law.

1. Encrypt each upscaled image with a key taken from its own original image,
   for example `SHA-256("vertigo-art-v1" ‖ the source's pixels and palette)`,
   with AES-256-GCM or ChaCha20-Poly1305. The file keeps its public name, the
   64-bit `gfx::Upscaled::key` of its source, so the game knows which file
   belongs to which image.
2. Publish the encrypted pack anywhere: a GitHub release asset, a static page
   or a shared folder. No database, expiring links or server logic.
3. Decrypt it in Vertigo, which already has the original data loaded: an
   Options button such as **Upscaled graphics: IMPORT PACK**, and
   `vertigo --import-pack FILE` for the terminal and the iPad's workflow. For
   each image it computes the key, decrypts the matching file and writes it
   to `Game Data Upscaled`, as the upscaler does.
4. Building the pack: A script uses the installed data and the according AI-upscaled set,
   mapping the set's files to their sources by their names. The key must be
   derived as in the game.

#### English and German Data Sets

The language versions sort themselves out. Images that are the same in the
German and English versions (most textures, models and backgrounds) open for
everyone. Images with German lettering open only with the German data, and
English owners keep their original images for those, which is right, as the
German upscale shows German text. Upscales of the English images can be added
to the pack later without changing anything else.

#### Caveats

- A tiny or plain image (one colour) could be guessed. Leave images under a
  size threshold unencrypted, or mix a few large files that are the same in
  both versions (the main menu's art) into every key.
- Anyone can still pass on the decrypted images; no download scheme prevents
  that.
- It's a technical barrier, not a licence: the pack is still derived from the
  original's art.

#### Alternatives considered

A hash challenge on a web page. It is weaker and needs more: one owner can publish
every file's hash, and stopping shared links needs signed, expiring URLs and a
running server.

## Notes

- Renderer changes stay self-contained so that a modern renderer and VR work
  stay easy.
- The cargo-ship block layout is inferred from the data.
