# Vertigo

An open-source clone of the *Massive Development* video game [Archimedean Dynasty](https://en.wikipedia.org/wiki/Archimedean_Dynasty), also known as [Schleichfahrt](https://de.wikipedia.org/wiki/Schleichfahrt_%28Computerspiel%29).

This implementation requires the game assets from an original version of the game (either English or German).

If you use Nix with flakes enabled, you can build and start Vertigo by running:
```
nix run github:pnordhus/vertigo
```

# Compiling

```
mkdir build
cd build
cmake ..
make
```

## macOS (Homebrew)

```
brew install cmake qt@5 sdl2 glm
mkdir build
cd build
cmake -DCMAKE_PREFIX_PATH="$(brew --prefix qt@5)" ..
make
```

Qt has to be found through its real prefix; the symlinks Homebrew creates in
`/opt/homebrew/lib/cmake` make Qt's CMake files look for `mkspecs` in the wrong place.

## Game data

Vertigo needs the complete game data, including the movies. A GOG installation
only contains part of it; the rest is in the CD image (`game.gog`, a raw
`MODE1/2352` track). Point Vertigo to a directory containing `DAT`, `GFX`,
`SFX`, `TXT` and `VFX` from the CD's `SF` directory, with the installed files
copied on top.

## Testing

Vertigo can start a mission directly, which is meant for testing:

```
vertigo s00101 --boat 2050 --equip GUN=3083,TUR1=3074 --overlay --timeout 60
```

- `--boat` picks the boat (2049 Hiob, 2050 Gator, 2051 Zorn, 2052 Succubus),
  `--equip` installs items (`--list-items` lists them). Otherwise the
  chapter 1 boat is used.
- `--overlay` shows the debug overlay with player state and a label for every
  object (toggle with Ctrl+D, Cmd+D on macOS). Yellow labels are detected by
  the sonar, green ones are enabled but not detected, red ones not enabled yet.
- `--destroy 2064,2068` destroys these units (scenario ids, shown by the
  overlay) one second into the mission, to test objectives and conditions.
  The player's id destroys the player; for areas and nav points it counts as
  reaching them.
- `--trace 2062,2055` prints these units' map cell, height above ground,
  speed, shield and current order every second; the player's id prints the
  player with the water current at its position and the four shield sides.
- `--headless` runs the mission without a window and without sound, in
  simulated time as fast as possible (use it with `--timeout`; all test
  times are then mission seconds). Automated runs with a window and
  `--timeout` don't take the focus.
- `--keys 2=Esc,3=2,4=1` presses keys at the given seconds, e.g. to drive the
  Esc menu (here: restart the mission). `5=Space/0.1` holds a key for 0.1 s;
  keys like Space (fire) or the arrows need that to have an effect.
- `--fire 2050` lets units fire their guns; towers and tanks aim at the
  player.
- `--near 2050` enables this unit and starts the player 40 m from it, looking
  at it, e.g. to watch it with `--screenshot`.
- `--drive 2053=60,2055=40:-0.5` lets boats move at a speed in km/h, turning
  at a fraction of their maximum turn rate (the sign picks the direction),
  instead of following their orders.
- `--screenshot 3=front.png` saves the window at the given mission seconds
  (not with `--headless`).
- The mission ends as soon as it is accomplished (no need to press Alt+A), 10
  seconds after it failed, or 3 seconds after the player was destroyed. The
  exit code tells how it ended: 0 success, 1 failed, 2 aborted (Esc menu or
  window closed), 3 `--timeout` reached, 4 player destroyed. A summary line
  with the time, enabled units and radio messages is printed at the end:

  ```
  $ vertigo s01805 --destroy 2064,2068,2069,2071 --timeout 5
  s01805: timeout after 5.006 s, 43/51 units enabled, 2 radio messages
  ```

In a mission, Esc opens the mission menu (continue, abort the game, restart
the mission, load the last saved game), P pauses, and Ctrl+K (Cmd+K on macOS)
skips the mission as if it had been accomplished, for debugging.

`vertigo --check [mission...]` loads all missions (or the given ones) and lists
everything the loader skips or ignores: unsupported object types, unread
settings and sections, and conditions that can't be resolved. The output for
the German CD data is kept in [docs/mission-check.txt](docs/mission-check.txt);
regenerate it after changes to the loader and compare:

```
build/vertigo --check > docs/mission-check.txt
```

## 3D cockpits

Besides the original 2D cockpit art, Vertigo can show 3D models of the four
cockpits. They are built and textured from your game data by the Blender
scripts in [data/3D-cockpits](data/3D-cockpits/README.md), whose
`tools/install.py` copies them to the folder `3d-cockpits` in the data
directory (or the folder set as `cockpit/directory`).

- `--cockpit 3d` (or `2d`) picks the cockpit for one run; Alt+V switches in a
  mission and saves the choice (setting `cockpit/mode`). Without a model for
  the boat's cockpit the 2D art is shown.
- In 3D the world uses the cockpit art's camera (`ViewAngle` 105° in the
  cockpit's DES file, wider than the 2D cockpit's view) and fills the whole
  window. The instruments are projected onto the model's screens.
- Mouse look: dragging with a mouse button (or on the trackpad) turns the
  pilot's head, up to 105° to the side, 65° up and 55° down; when the button
  is released the head turns back to the front. `--look=60,-10` fixes the
  head at 60° left and 10° down for the whole run, e.g. for screenshots.
- The pilot's head lags behind accelerations and is thrown about by
  collisions, ground contact, hits and nearby explosions, which shows the
  depth of the cockpit. `--shake 0.5` (setting `cockpit/shake`, default 1)
  scales it, 0 turns it off. `--trace` with the player's id prints the head
  offset.
