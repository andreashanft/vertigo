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
- The mission ends as soon as it is accomplished (no need to press Alt+A) or
  10 seconds after it failed. The exit code tells how it ended: 0 success,
  1 failed, 2 aborted (Esc or window closed), 3 `--timeout` reached. A summary
  line with the time, enabled units and radio messages is printed at the end:

  ```
  $ vertigo s01805 --destroy 2064,2068,2069,2071 --timeout 5
  s01805: timeout after 5.006 s, 43/51 units enabled, 2 radio messages
  ```

`vertigo --check [mission...]` loads all missions (or the given ones) and lists
everything the loader skips or ignores: unsupported object types, unread
settings and sections, and conditions that can't be resolved. The output for
the German CD data is kept in [docs/mission-check.txt](docs/mission-check.txt);
regenerate it after changes to the loader and compare:

```
build/vertigo --check > docs/mission-check.txt
```

## Game data

Vertigo needs the complete game data, including the movies. A GOG installation
only contains part of it; the rest is in the CD image (`game.gog`, a raw
`MODE1/2352` track). Point Vertigo to a directory containing `DAT`, `GFX`,
`SFX`, `TXT` and `VFX` from the CD's `SF` directory, with the installed files
copied on top.
