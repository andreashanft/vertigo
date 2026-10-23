# Font editor

A browser tool for redrawing the game's silver font (DPLARGE) as scalable
vectors. Every glyph is a set of polygons in an 11 × 12 unit cell, monospaced,
with the ink starting one unit in. The editor shows the game's glyph under the
shape, compares both at actual and enlarged sizes and measures their pixel
overlap. It also previews the outline, light and shade, and grain layers that
give the vectors the look of the game's font.

| File | |
|---|---|
| `index.html` | the editor |
| `server.py` | serves the editor and saves its edits |
| `glyphs.json` | the starting shapes |
| `vertigo-mono.json` | the current shapes and style, written by the editor |
| `vertigo-mono-edit.svg` | the shapes as an SVG atlas, 16 glyphs per row |
| `vertigo-mono-styled.svg` | the atlas with outline, light and shade, and grain |
| `originals.py` | writes `originals.json`, the game's glyphs to compare against |

The game data is not part of the repository, so `originals.json` is made
locally from an installed copy of the game (the folder that holds `GFX`), and
needs Pillow:

```bash
python3 data/font/originals.py "<game data folder>"
```

Then start the server and open http://127.0.0.1:8765/:

```bash
python3 data/font/server.py
```

The editor saves every change to `vertigo-mono.json` and the two SVGs. Reset glyph
returns a glyph to its shape in `glyphs.json`.

## In Vertigo

With the upscaled graphics on, Vertigo draws the large silver font from
`vertigo-mono.json`, which the build copies next to the program
(`font/vertigo-mono.json`, in `share/vertigo/font` when installed): each glyph
over the box of the game's glyph it replaces, so text keeps the game's widths,
in the editor's style, eight texels per game pixel. The game's small letters
are its capitals, so they take the capitals' vector glyphs; a glyph without a
vector version keeps the game's pixels. After editing, building copies the file
again.
