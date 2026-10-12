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
