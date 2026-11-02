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
| T6 | Towers and vehicles with boat guns | S | The AD Augmented Mod's readme: a gun tower or vehicle whose `gun` is a boat gun model (0–10, as some missions have) fires all of that gun's barrels from each of its own, so a one-barrel tower with a Vendetta III fires four shots a salvo and a four-barrel one 16; with the models meant for them (11–13) it fires one per barrel. Vertigo's unit guns fire one of the tower's barrels per pull, in turn, whatever the model. Compare a salvo in the original |
| T7 | Torpedoes as sonar contacts | S | In the original every torpedo on its way is a sonar contact (KNOWLEDGE.md 9.11): the round radars show it blinking red and the player's turrets take it from the sonar. Vertigo draws torpedoes in the voxel radar only, its sonar lists none, and its turrets ask the sight test of the units alone |
| T8 | iPad: OpenGL ES and OpenAL | M | Both are deprecated but still in the iOS 27 SDK. ANGLE and OpenAL Soft would replace them |

## Open comparisons

Questions about the original that are not settled yet. Each is settled by
watching the original and reading its data, then implementing the behaviour
independently; the answer goes into KNOWLEDGE.md.

Missions and the campaign:

- Whether the next mission starts with the damage the boat kept, and whether
  buying new armour mends the sides.
- What AutoEnd and InstantEnd look like (T3).
- Whether the autopilot menu lists the start station.
- Whether the death menu adds loading the last game in the campaign (it
  showed abort and restart; Vertigo always offers all three).
- `wccond 9` (56 units, s00303 and s02006 among them).
- Whether a slot with a break condition ends on its own (T5), and whether
  Attack and CardinalAttack end after `dur` (Vertigo ends them).
- In a squadron of several big ships the original counts every block that
  goes; Vertigo counts the ship.

Units' behaviour:

- Whether an attacking boat's pitch is limited to `Min/MaxClimbingAngle` (20°
  on all scouts and bombers; Vertigo limits it, so a boat right under or over
  its target can't aim at it).
- What an attacking boat does once its area time is up (the original may
  pick a new patrol point every frame; Vertigo picks one on the timer).
- The lower bound of a tank's attack speed (Vertigo takes 40–100 % of the
  order's speed).
- GotoWithInfiltrate's approach and docking.
- Which of `car` 2 and 6 is left (Vertigo turns 2 left).
- How a boat keeps hitting a standing player while its turning leaves up to
  1° of error alone (with Vertigo's 1.5 m hit sphere for the player such a
  boat would miss from about 85 m, so Vertigo turns on until the error is
  gone).
- Whether boats, tanks and gun towers hold their fire while the terrain is
  between them and their target, as the torpedo towers do, and whether the
  player's turrets fire through friends (Vertigo's do neither).
- Whether the player takes damage running into buildings, towers or boats
  (the contact table pairs the player only with shots, debris, torpedoes and
  mines; Vertigo's player takes their strength).

Sensors, HUD and views:

- The sonars' detection ranges.
- Whether the units' event messages show as they happen (they are in the
  radio log).
- What the monitors' lamps show.
- Whether big buildings show as blocks or as ground on the radar.
- What the turrets' aim cross does for a target behind the boat (Vertigo puts
  it on the edge away from the target).
- In the outside views: where the radio message sits in the bar, whether the
  pause shows, what the target's views do when the sonar loses the target
  (Vertigo goes back to the cockpit), and what the camera does the moment a
  view opens. At the furthest zoom Vertigo's boat shows about 10 rows lower
  than the original's, in the look-at view about 16 rows lower.
- How far the other boats bank as they turn more slowly, and whether tanks,
  bombers and torpedoes roll.

World, physics and effects:

- Why one of S00202's two upward current cells does nothing.
- What else the original scales with the radiation's strength between 100
  and 40 m below the surface.
- Whether the keyboard ramps the stick value when turning the boat.
- What a negative `MaxAngleVelocityC` in the boats', tanks' and torpedoes'
  object files does.
- Whether mine columns are stacked upwards.
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

- The currents push steadily; the original's speeds come out of Vertigo's
  drag and speed cap.
- Some explosions' sizes: the mines' explosions keep their own size (within
  about 10 % of the original's), the destruction sequence's come in sizes
  Vertigo's effects don't take, and Vertigo sizes the explosions by the
  sprites' extent, which may not be the size the original measures them by.
- The boat's turn rates relax at `turnticks` ticks per second (`settings.ini`
  `[Fight]`, default 30, matched by hand).
- The escort's place is measured from the escorted hull's box to the escort's
  own, not from hull to hull, so an offset of 7 m to the side puts the escort
  7 m clear of the hull.
- The outside views' camera distances are in the watched unit's radii, and
  the boats' bank follows their share of the full turn rate.
- The agile torpedoes' steering ticks run at 30 a second.
- The Torch's torch lights the ground about four times as strongly, as
  Vertigo's lights fall off with the square of the distance and the
  original's linearly.
- The terrain's shading bands fall in slightly different places along slopes,
  as Vertigo's vertices and normals are its spline's, finer than the
  original's grid; the ground runs straight between the corners only in a
  lifted cell and its eight neighbours.
- The barrels of a round fire 1/60 s apart whatever the frame rate, as the
  original's do at 60 frames a second.
- Vertigo's ground plans can't be attacked, so their attacker lists have no
  effect.
- The sonar lists no torpedoes (T7).

## Notes

- Renderer changes stay self-contained so that a modern renderer and VR work
  stay easy.
- The cargo-ship block layout is inferred from the data.
