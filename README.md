# Vertigo

An open-source clone of the *Massive Development* video game [Archimedean Dynasty](https://en.wikipedia.org/wiki/Archimedean_Dynasty), also known as [Schleichfahrt](https://de.wikipedia.org/wiki/Schleichfahrt_%28Computerspiel%29).

This implementation requires the game assets from an original version of the game (either English or German).

If you use Nix with flakes enabled, you can build and start Vertigo by running:
```
nix run github:pnordhus/vertigo
```

# Compiling

Vertigo needs SDL 3, OpenAL and GLM, and draws with OpenGL 3.2 (core profile)
or OpenGL ES 3. The build also has tests of the game's own types for text,
data values, images, JSON and CD images (`ctest`).

```
mkdir build
cd build
cmake ..
make
```

## macOS (Homebrew)

```
brew install cmake sdl3 glm
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
make
```

## iPad

The iPad version needs Xcode, CMake 3.24 or newer and GLM's headers
(`brew install cmake glm`), and runs on iPadOS 18.6 or later. CMake downloads
SDL's source release (checked against its SHA-256) and builds it for iOS;
OpenAL and OpenGL ES are the system's.

```
cmake -S . -B build-ios -G Xcode -DCMAKE_SYSTEM_NAME=iOS
cmake --build build-ios --config Release -- -sdk iphonesimulator
xcrun simctl install booted build-ios/Release/Vertigo.app
```

builds the app for the simulator and installs it there. For an iPad, open
`build-ios/vertigo.xcodeproj` in Xcode and choose a team for signing, or pass
`-DVERTIGO_DEVELOPMENT_TEAM=<team ID>` (and if needed
`-DVERTIGO_BUNDLE_ID=<identifier>`) to the first command and build with
`-sdk iphoneos -allowProvisioningUpdates`.

The game data goes into Vertigo's folder in the Files app (On My iPad ›
Vertigo), which the Finder also shows when the iPad is connected to a Mac.
Copy a GOG installation of the game there, and Vertigo builds the game data
from its CD image `game.gog` and the installed files, in the folder `Game Data`
(580 MB, kept out of iCloud backups); then the installation can go. The game
data itself (see below) works too, directly in Vertigo's folder or in a folder
of its own. In the simulator, copy it into the app's container:

```
cp -R <GOG installation or game data> "$(xcrun simctl get_app_container booted com.vertigo.Vertigo data)/Documents/"
```

Test mode takes its options there too, e.g.
`xcrun simctl launch booted com.vertigo.Vertigo s00101 --timeout 60`. The
simulator renders OpenGL ES in software: the 3D cockpit is too slow there, and
now and then a triangle close to the camera goes missing.

Missions need a hardware keyboard. On keyboards without Esc and function keys,
Cmd+. is Esc and Cmd with the number row's 1 to 0, - and = is F1 to F12 (on
every system; Ctrl on Windows and Linux); iPadOS can also make Caps Lock an
Esc key (Settings › General › Keyboard › Hardware Keyboard › Modifier Keys).
The on-screen keyboard comes up for a new game's name, which moves above it.

When the app is interrupted (a call, Siri, the app switcher) or goes to the
background, a mission pauses; P on the keyboard goes on. The game's sound
mixes with other apps' audio, such as music, and is silent with the silent
switch.

## Game data

Vertigo needs the complete game data, including the movies. A GOG installation
only contains part of it; the rest is in the CD image (`game.gog`, a raw
`MODE1/2352` track). Point Vertigo to a directory containing `DAT`, `GFX`,
`SFX`, `TXT` and `VFX` from the CD's `SF` directory, with the installed files
copied on top. The case of the file names doesn't matter, also on file systems
that tell upper and lower case apart. Vertigo asks for the directory at the
first start, and again whenever the remembered one lacks part of the data.
Given a GOG installation, or a directory with a CD image of the game (`.iso`,
or `.bin` of a `MODE1/2352` track), it offers to build that directory itself,
as `Game Data` next to the saved games. `--data <directory>` uses another
directory for one run, without remembering it.

## Saved games and settings

Vertigo keeps its saved games (`save`) and its settings (`settings.ini`) in
`~/Library/Application Support/Vertigo/Vertigo` on macOS,
`%LOCALAPPDATA%\Vertigo\Vertigo` on Windows and
`~/.local/share/Vertigo/Vertigo` on Linux, and in the app's own folder on the
iPad.

## Training

The main menu's Training has the original's four training missions, each with
its briefing and its own boat, outside the story. Afterwards the game goes back
to the menu.

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
  `--timeout` don't take the focus; on macOS they run in the background,
  without a Dock icon and without keyboard input other than `--keys`.
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
- `--screenshot 3=front.png` saves the window as PNG at the given mission
  seconds (not with `--headless`).
- `--fixed-step` advances the mission by 20 ms per frame instead of by the
  real time, so runs with a window repeat exactly, frame by frame (test times
  are then mission seconds). Screenshots of two builds can be compared pixel
  by pixel this way, e.g. after changes to the renderer.
- `--click 2=320,300` clicks at the given seconds at a point of the 640×480
  screen (the mouse moves there first).
- Without a mission, `--timeout`, `--keys`, `--click` and `--screenshot`
  work in the main menu and the game it starts, in seconds since the start,
  e.g. `vertigo --click 1=320,240 --screenshot 3=menu.png --timeout 4`.
- `--seed 5` picks the random numbers (default 1), so a run repeats exactly.
  Compare runs only between builds of the same build type: an optimized
  build rounds floating point differently, and missions can end differently.
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
- On the iPad the texture is halved to 4096x4096 (with 8192x8192 an iPad Pro
  11" stopped drawing).
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
