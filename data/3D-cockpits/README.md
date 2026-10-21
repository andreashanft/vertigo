# 3D cockpits

Feasibility work for a VR version: the four pre-rendered 640×480 cockpits of Schleichfahrt /
Archimedean Dynasty (`VFX/COCKPIT/000`–`003`) rebuilt as 3D models with Blender scripts, then
textured part by part from the original artwork, with the outlines as vectors.

Nothing here ships game data. Every script reads the original files from your own game data
directory and writes its results to `out/` (ignored by git), like the rest of Vertigo.

## Requirements

- Blender 4.2 or newer (tested with 5.2), run headless. On macOS:
  `/Applications/Blender.app/Contents/MacOS/Blender`.
- The game data directory Vertigo uses. The scripts read Vertigo's `datadir` setting from its
  `settings.ini` (in `~/Library/Application Support/Vertigo/Vertigo` on macOS); override it
  with `--data DIR` or `VERTIGO_DATA`.

## Usage

```sh
B=/Applications/Blender.app/Contents/MacOS/Blender

# 1. build the model (out/000/cockpit_000.blend/.glb + renders/)
$B -b --factory-startup --python cockpit_000.py

# 2. lay out the texture atlas and bake the maps of the geometry (out/000/maps/)
$B -b out/000/cockpit_000.blend --python bake_maps.py

# 3. make the texture from the art (out/000/cockpit_000_textured.glb, textures/, vector/)
$B -b --factory-startup --python compose_texture.py -- --cockpit 000
```

Step 1 takes seconds, step 2 two to three minutes (Cycles on the GPU), step 3 about a minute.
Step 3 needs numpy and OpenImageIO only; Blender's Python has both.

Options after `--`:

| script | option | effect |
| --- | --- | --- |
| `cockpit_NNN.py` | `--quick` | save/export plus the alignment check only |
| | `--no-render` | save/export only |
| `bake_maps.py` | `--res 4096` | atlas size (default 8192) |
| `compose_texture.py` | `--art FILE` | colours from this image instead of COCKPIT.R16: the art upscaled 2×, 4× or 8× (e.g. by Real-ESRGAN) |
| | `--soft` | don't sharpen the art's strong edges |
| | `--suffix _x4` | suffix for the outputs, to compare variants |
| all | `--data DIR`, `--out DIR` | game data and output directories |

Tools:

- `tools/trace_crops.py -- --cockpit NNN --crop name,x0,y0,x1,y1,scale …`: gridded zoom crops of
  the original (grey lines every 10 px, yellow every 50 px) for tracing shapes.
- `tools/export_art.py -- --cockpit NNN`: the art as a plain PNG with the window area filled
  in, ready for an image upscaler (see *Upscaling*).
- `tools/install.py` (plain Python 3): copies the textured models to Vertigo (see below).

## Using the models in Vertigo

```sh
python3 tools/install.py              # out/NNN/cockpit_NNN_textured.glb
build/vertigo s00101 --cockpit 3d     # or Alt+V in a mission
```

Variants side by side, e.g. the upscaled one (`--suffix _x4`, see *Upscaling*) as the default
and the one without in a folder of its own:

```sh
python3 tools/install.py --suffix _x4
python3 tools/install.py --dest "<data directory>/3d-cockpits-vector"
build/vertigo s00101 --cockpit 3d --cockpit-dir 3d-cockpits-vector
```

`install.py` copies one model per cockpit to `3d-cockpits/cockpit_NNN.glb` in Vertigo's data
directory (or the setting `cockpit/directory`, or `--dest`). Vertigo reads the GLB itself and
uses the following:

- **Meshes and texture.** Everything with the projected texture, the `Glass` material only as
  a surface for the HUD, and node transforms. The `HUD_MasterHUD` plane is skipped.
- **Camera.** The eye at rest is the origin. The world and the model are drawn with the camera
  of the cockpit art.
- **Instruments.** The HUD's instruments are rendered into a texture covering the 640×480 view
  and projected onto the model from the eye. At rest that reproduces the 2D layout. When the
  head moves (shake, mouse look), the instruments stay on their screens and panels.
- **Mouse look** shows the parts the original art never showed: the side walls, the ceiling, the
  footwell. They only have the colour-matched fallback, and are the obvious place for more
  modelling and texturing work.

See the main README for the options (head motion, settings).

## How the models are built

- **Camera.** Each cockpit's DES file defines the view: `ViewAngle` (105°, horizontal over 640 px)
  and the optical centre (`VFX_StartX + VFX_FlightX`, `VFX_StartY + VFX_FlightY`). A pixel plus a
  distance gives a 3D point (`cockpitlib.at(px, py, dist)`). Everything is traced in pixels and
  placed at a chosen distance, so the model seen from the eye lines up with the original.
- **Conventions.** Metres; Blender +Z up, the pilot looks along +Y (glTF: +Y up, −Z forward).
  The pilot's eye is the origin (empty `Eye`). Screens are separate meshes named after their DES
  sections (`Screen_MasterMonitor`, `Screen_RoundMonitor0`, …): origin at the screen centre,
  +Z towards the pilot, UVs 0..1, the DES rectangle in the glTF extras. The DES `MasterHUD`
  rectangle becomes a transparent `HUD_MasterHUD` plane.
- **Hull.** `Porthole` builds an ellipsoid hull with one window outline (cockpit 000).
  `masked_hull` builds a hull that turns into glass wherever the original art is see-through.
  That suits the open canopies of 001–003, and every opaque pixel of the art lands on some
  surface. Its faces are cut along the smooth outline of the see-through area (the 0.5 contour
  of the mask blurred by 0.8 px, `cockpitdata.window_field`), so the window's edge is a curve
  rather than the staircase of the faces. Both hulls come with their own UV map, split along
  grid lines (see *The texture*).
- **Parts.** `Panel` + `prism` extrude outlines traced in pixel space (ray-cast onto a plane, so
  silhouettes match). `prism_eye` makes the silhouette exact: the outline is cast onto the front
  face and inset by the bevel, and the sides run along the view rays. `rect_monitor` and
  `round_monitor` build complete instruments, `beam` builds struts and arches, `cable` builds
  (corrugated) hoses whose loose ends are anchored into the hull or floor. `standard_interior` adds
  floor, seat and a pilot dummy (dummy not exported).
- **Widths in pixels.** `beam(w_px=…)` and `cable(r_px=…)` take the width as traced across the
  centre line in the image, also per control point. `fit_across` turns it into metres at the
  member's near side, including the radial stretch of the flat 105° view, and moves the control
  points so both edges straddle the trace. `mirror` copies traces to the other side of the art's
  symmetry axis, which `load` measures per cockpit (319.5 for 001, 320 for 002).
- **Alignment check.** Every build renders the model flat from the eye and compares it with the
  original's see-through mask: `renders/alignment_check.png` shows the original, the model, both
  blended and a diff (red: only the original is opaque, cyan: only the model). The log prints
  the IoU.
- **Panel orientation.** `facing` trades fidelity against ergonomics. At 0 a panel is parallel
  to the original image plane and keeps its 2D look exactly. At 1 it faces the eye, which is
  better to read in VR but looks stretched near the edge of a flat 105° render.

## The texture

The texture is made in two steps. The first knows only the geometry, the second only adds the
art.

**`bake_maps.py`** lays out the atlas and bakes per texel where the eye sees it:

- **Atlas.** The hull keeps the UV map its builder made: the part the art shows in azimuth and
  elevation (or around and away from the window), the rest at 30 % of the texel density, split
  along grid lines so that the seams are smooth curves. Everything else is unwrapped with Smart
  UV Project, faces the art shows separately from the others. At 8192 px that gives
  1800–2500 texels/m where the art lands.
- **Maps** (`out/NNN/maps/`, no art in them): per texel the pixel of the 640×480 view where the
  eye sees it and how directly it faces the eye (`view.exr`), the part (`part.exr`), the model
  lit by the scene lights (`shade.exr`); the model seen from the eye at 8× the art's resolution
  with the part at every sub-pixel (`eye.exr`); and the model with the atlas UVs
  (`cockpit_NNN_atlas.glb`).

**`compose_texture.py`** turns the art into the texture:

- **A colour field per part.** The eye render says which part every pixel of the art shows.
  Every part gets a field of its own from the pixels it covers (its rim counts less: those
  pixels mix in what is behind it), continued smoothly beyond them by push-pull. A texel takes
  its colour only from its own part's field. So the edges between parts are the model's edges,
  sharp at any resolution, and no part shows the colours of the part in front of it, also when
  the head moves.
- **Sharp edges inside the parts** (`vectorize.edge_detail`). The art's strong, long edges
  (panel seams, outlines, stripes, rings) are drawn as crisp lines: along them the colour
  switches between the darker and the brighter side at the edge's 0.5 contour, one texel wide.
  Short or noisy edges (dirt, 16-bit dithering) stay soft, so the surfaces don't break up into
  blobs. `--soft` turns this off.
- **Fallback.** Surfaces turned away from the eye, and those far from any pixel of their part,
  blend into the model lit by the scene lights, tinted with the part's colour from the art.
- **Gaps.** The texels between the atlas islands are filled with the islands' colours, so
  filtering and mipmaps at the islands' edges don't pull in black.
- **Vector version.** `vector/cockpit_NNN.svg` is the art redrawn this way: every part as a path
  (its outline from the eye, as the model shows it), filled with its colour field, the sharp
  edges as paths filled with their sides' colours, the window's outline as a path. Any SVG
  viewer shows it at any size. `renders/eye_8x.png` is the same as the texture shows it from
  the eye, at 8×; from the eye it differs from the original by about 2/255 on average.
- **Upscaled art.** `--art` takes the colours from an upscaled image, e.g. Real-ESRGAN's 4×
  (see *Upscaling*): the parts' edges still come from the model, the surfaces get the
  upscaler's detail. A 4× source takes about three minutes.

## Findings

### The four cockpits

| cockpit | design | window IoU | triangles | texels/m |
| --- | --- | --- | --- | --- |
| 000 | oval porthole in a ribbed hull (`Porthole`) | 0.943 | 249k | 1779 |
| 001 | open canopy: diagonal struts, header arch, side windows continued past the frame | 0.987 | 308k | 2484 |
| 002 | open canopy: hazard-striped pillars, crown, sill, side struts | 0.992 | 488k | 2471 |
| 003 | open canopy: arch with ribbed feet, hanging monitors | 0.978 | 290k | 2346 |

(texels/m: where the art lands, with an 8192 px atlas.)

All four have a named screen for every DES monitor section and a closed pod with floor and seat.
Seen from the eye, the textured models are nearly indistinguishable from the originals.

### The texture compared with the projected one

The first version baked the art projected from the eye into the atlas, from a Real-ESRGAN 4×
upscale, and lit the rest. Compared in Vertigo at 3024×1522, looking ahead and 35°/60° to the
sides:

| | projected (Real-ESRGAN 4×) | part by part, vector edges | part by part, `--art` Real-ESRGAN 4× |
| --- | --- | --- | --- |
| side walls (001–003) | zigzag lines: seams between the islands of the faces the art shows and those it doesn't, along the faces' staircase | none | none |
| window edge of the hull (001–003) | staircase of the hull's faces (about 1.5 px of the art) | smooth curve; window IoU up by up to 0.007 | smooth curve |
| edges of struts and pillars | stair-stepped fringes where the art lands on the hull behind them | the model's edges | the model's edges |
| part behind a part, head moved | the front part's colours (ghosting) | the part's own colours | the part's own colours |
| inside the surfaces | crisp, slightly painted (the upscaler's detail) | softer, dithering visible; strong edges crisp | crisp, slightly painted |
| needs | an ML upscaler | numpy | an ML upscaler |

The combination is the best of the three: the upscaler's surfaces with the model's edges.
Polygon edges are drawn with 4× multisampling in Vertigo.

### Limitations and next steps

- **Hidden surfaces.** Surfaces the original never shows get the tinted fallback: plausible,
  but flat. They are the place for real modelling and texturing.
- **Interior detail without an upscaler.** The surfaces are as soft as 4× bicubic between their
  sharp edges. Fine dirt next to strong edges (002's stripes) can still turn into small blobs.
- **Lamps on the 3D cockpit.** The cockpit lamps the HUD draws are projected with the
  instruments at the HUD's resolution and look blocky up close.
- **Not done yet.** PBR materials and normal maps, LODs (standalone headsets need the hoses and
  hull decimated), and VR (OpenXR, stereo rendering). Vertigo shows the models (see above); a
  VR renderer could reuse its instrument texture, which is already mapped onto the screens.

### Upscaling (evaluated on cockpit 000)

Evaluated with the first, projected version of the texture: the upscalers were run on the art,
the textures baked at 4096 and 8192 px and compared in close-ups rendered at 32 pixels per
degree. To use an upscale now, export the art (its window area filled with the surrounding
colours, so the upscaler draws no dark halos at the window's edge), upscale it and pass it with
`--art`:

```sh
$B -b --factory-startup --python tools/export_art.py -- --cockpit 000
realesrgan-ncnn-vulkan -i out/000/art/cockpit_000_art.png -o out/000/art/cockpit_000_art_x4.png \
    -n realesrgan-x4plus -s 4
$B -b --factory-startup --python compose_texture.py -- --cockpit 000 \
    --art out/000/art/cockpit_000_art_x4.png
```

| variant | result |
| --- | --- |
| larger atlas only (4096 → 8192, original art) | no visible gain: the 640×480 source is the limit |
| Lanczos 4× (classical) | smoother, no new detail |
| Real-ESRGAN `realesrgan-x4plus` | clearly sharper: crisp edges, the 16-bit dithering noise gone, sonar rings, hose ridges and screen grids clean. Side effects: surfaces look slightly painted, tiny on-screen text smears |
| Real-ESRGAN `realesrgan-x4plus-anime` | unusable: erases the master monitor grid, posterises the shading |
| Real-ESRGAN `realesr-animevideov3` | faithful to the original but little gain |

- **4× is the right factor.** The original has about 6 pixels per degree and headsets show
  roughly 15–25.
- **The atlas has to grow with it.** At 4096 px (about 920 texels/m where the art lands) part
  of the gain survives. At 8192 px (about 1900 texels/m) nearly all of it does.
- **Cost of 8192 px.** 256 MB as uncompressed RGBA8, 64 MB as BC7. That is fine for PC VR. For
  standalone headsets use 4096 px.
- **Recommendation.** Real-ESRGAN x4plus with an 8192 px atlas, through `--art` (it takes
  about 8 s per cockpit, composing then 2–3 minutes). The smeared text doesn't matter because
  the game draws screen contents at runtime. For final
  assets, an artist pass over the upscaled art, or a model tuned on 90s pre-rendered CG, would
  remove the painted look.
- **Speed.** The upscale takes seconds per cockpit on an M1 Pro (ncnn-vulkan via Metal).

## Plan: building the cockpits when the game data is imported

Today a player who wants the 3D cockpits needs Blender, runs three scripts per cockpit and
copies the results. The goal: Vertigo builds them itself from the player's game data, on every
platform including the iPad, when it builds `Game Data` (or the first time the 3D cockpit is
shown), in seconds.

Where the pipeline stands now:

- `bake_maps.py`'s output depends on the geometry only. The geometry itself still needs two
  things from the game data: the camera (`ViewAngle` and the optical centre from the DES file,
  three numbers per cockpit) and the art's see-through mask, which shapes `masked_hull` (and
  003's sill).
- `compose_texture.py` needs nothing but numpy: art and maps in, texture out. All of its steps
  (coverage, push-pull, edge detail, sampling, filling the gaps) are simple image operations.

Steps:

1. **Geometry without the game data.** Put the camera numbers and the window outlines (the
   contours `vector/cockpit_NNN.svg` already contains, a few hundred points each) into the
   scripts, like the outlines they already trace from the art. Then the Blender steps run
   without the game data, e.g. in CI. Whether outlines traced from the art may live in the
   repository is the same question as for the scripts' traced coordinates.
2. **A kit per cockpit**, built by those steps and shipped with Vertigo (release download or app
   bundle, about 10 MB each, no art in it): the model with its atlas UVs, one mesh per part, and
   the fallback shading, as vertex colours or a small grey texture. The maps are not shipped;
   Vertigo derives them from the model.
3. **Compose in Vertigo** (C++, about 600 lines, a port of `compose_texture.py` and
   `vectorize.py`):
   1. render the kit from the eye at 8× into an id framebuffer: the parts' coverage of the
      art's pixels;
   2. per part: the colour field (push-pull) and the sharp edges, from `COCKPIT.R16`;
   3. draw every mesh into the atlas in UV space (`gl_Position` from the UVs): the fragment
      shader projects the interpolated position to the art's pixel, samples the part's field
      and edges, blends with the fallback; then fill the gaps and build the mipmaps;
   4. save the texture next to the game data (`Game Data/3d-cockpits/cockpit_NNN.png`, or
      compressed: BC7 on desktops, ASTC on the iPad).
   Expected: a few seconds per cockpit, against a minute in numpy.
4. **Drop the manual steps.** `tools/install.py` goes; Blender and the scripts stay for whoever
   works on the models.

Alternatives considered:

- **Compose at every start, no cache.** The GPU part takes milliseconds, but the fields and
  edges are about a second of CPU work per cockpit; caching is simpler and starts faster.
- **Projective texturing at run time, no atlas.** Avoids the atlas, but ties the cockpit to
  Vertigo's renderer; an atlas works in any engine (VR runtimes, other tools).
- **An upscaler in the import step.** Real-ESRGAN would need a 65 MB network and an inference
  runtime on every platform. The sharp edges give most of the crispness without it; `--art`
  stays for those who run an upscaler themselves.
