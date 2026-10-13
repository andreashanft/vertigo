# 3D cockpits

Feasibility work for a VR version: the four pre-rendered 640×480 cockpits of Schleichfahrt /
Archimedean Dynasty (`VFX/COCKPIT/000`–`003`) rebuilt as 3D models with Blender scripts, then
textured by projecting the original artwork onto them.

Nothing here ships game data. Every script reads the original files from your own game data
directory and writes its results to `out/` (ignored by git), like the rest of Vertigo.

## Requirements

- Blender 4.2 or newer (tested with 5.2), run headless. On macOS:
  `/Applications/Blender.app/Contents/MacOS/Blender`.
- The game data directory Vertigo uses. The scripts read Vertigo's `datadir` setting
  (`~/Library/Preferences/com.vertigo.Vertigo.plist` or `~/.config/vertigo/Vertigo.conf`);
  override it with `--data DIR` or `VERTIGO_DATA`.

## Usage

```sh
B=/Applications/Blender.app/Contents/MacOS/Blender

# 1. build the model (out/000/cockpit_000.blend/.glb + renders/)
$B -b --factory-startup --python cockpit_000.py

# 2. project the original art onto it (out/000/cockpit_000_textured.blend/.glb + textures/)
$B -b out/000/cockpit_000.blend --python project_textures.py
```

Options after `--`:

| script | option | effect |
| --- | --- | --- |
| `cockpit_NNN.py` | `--quick` | save/export plus the alignment check only |
| | `--no-render` | save/export only |
| `project_textures.py` | `--res 4096` | atlas size |
| | `--art FILE` | project this image instead of COCKPIT.R16's colours (e.g. an upscaled one) |
| | `--suffix _x4` | suffix for all outputs, to compare variants |
| all | `--data DIR`, `--out DIR` | game data and output directories |

Tools:

- `tools/trace_crops.py -- --cockpit NNN --crop name,x0,y0,x1,y1,scale …`: gridded zoom crops of
  the original (grey lines every 10 px, yellow every 50 px) for tracing shapes.
- `tools/export_art.py -- --cockpit NNN`: the art as a plain PNG with the window area filled
  in, ready for an image upscaler (see below).

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
  `masked_hull` builds a hull whose faces turn into glass wherever the original art is
  see-through. That suits the open canopies of 001–003, and every opaque pixel of the art lands
  on some surface.
- **Parts.** `Panel` + `prism` extrude outlines traced in pixel space (ray-cast onto a plane, so
  silhouettes match). `rect_monitor` and `round_monitor` build complete instruments, `beam`
  builds struts and arches, `cable` builds (corrugated) hoses whose loose ends are anchored into
  the hull or floor. `standard_interior` adds floor, seat and a pilot dummy (dummy not exported).
- **Alignment check.** Every build renders the model flat from the eye and compares it with the
  original's see-through mask: `renders/alignment_check.png` shows the original, the model, both
  blended and a diff (red: only the original is opaque, cyan: only the model). The log prints
  the IoU.
- **Panel orientation.** `facing` trades fidelity against ergonomics. At 0 a panel is parallel
  to the original image plane and keeps its 2D look exactly. At 1 it faces the eye, which is
  better to read in VR but looks stretched near the edge of a flat 105° render.

## Texture projection

`project_textures.py` gives every mesh an atlas UV map. It then bakes the original image,
projected from the eye, into it with Cycles. A light at the eye point marks which texels the
pilot can see, so occluded, edge-on and out-of-frame surfaces don't receive art meant for the
part in front of them. Those surfaces get a lit render of the plain model instead, colour-matched
per material to the art's average colour. The result uses one unlit material
(`KHR_materials_unlit`), because the lighting is already painted into the original art.

From the eye, the textured model reproduces the original almost exactly. If the head moves
about ±15 cm it still reads as a solid cockpit; artefacts appear where the geometry is a few
pixels off the painted shapes (art landing on the part behind, stretched side faces).

## Upscaling the art

The original is 640 px across a 105° view, about 6 pixels per degree. VR headsets show
roughly 15–25. Upscaling the source image (not the atlas) before projecting it closes most of
that gap:

```sh
$B -b --factory-startup --python tools/export_art.py -- --cockpit 000
realesrgan-ncnn-vulkan -i out/000/art/cockpit_000_art.png -o out/000/art/cockpit_000_art_x4.png \
    -n realesrgan-x4plus -s 4
$B -b out/000/cockpit_000.blend --python project_textures.py -- \
    --art out/000/art/cockpit_000_art_x4.png --res 8192 --suffix _x4
```

See *Findings* for the comparison of upscalers.

## Findings

### The four cockpits

| cockpit | design | window IoU | triangles |
| --- | --- | --- | --- |
| 000 | oval porthole in a ribbed hull (`Porthole`) | 0.943 | 249k |
| 001 | open canopy: diagonal struts, header arch, side windows continued past the frame | 0.983 | 308k |
| 002 | open canopy: hazard-striped pillars, crown, sill, side struts | 0.985 | 483k |
| 003 | open canopy: arch with ribbed feet, hanging monitors | 0.977 | 287k |

All four have a named screen for every DES monitor section and a closed pod with floor and seat.
Seen from the eye, the textured models are nearly indistinguishable from the originals.

### Limitations and next steps

- **Ghosting.** With `masked_hull`, every opaque pixel of the art also lands on the hull 25–35 cm
  behind the parts. When the head moves, that shows as dark fringes beside struts and pillars.
  Cockpit 003 avoids it by making the hull glass behind the modelled parts; 001 lets parts overlap
  the art outline by about 1 px.
- **Hidden surfaces.** Surfaces the original never shows get a colour-matched fallback. A visible
  stepped seam remains where it meets the projected art.
- **Library candidates.** The cockpit scripts contain helpers worth moving into `cockpitlib`:
  - correct strut and cable widths near the image edges (`w_m`, `across_px`, `fit_across`);
    `m_per_px` ignores the stretch of a flat 105° view;
  - prisms whose bevel doesn't grow the outline (`inset`, `prism_eye`);
  - mirrored traces, DES lamp positions, flat lamp lenses.
- **Not done yet.** PBR materials and normal maps, LODs (standalone headsets need the hoses and
  hull decimated), live screen content, and loading the models in Vertigo (glTF plus OpenXR).

### Upscaling (evaluated on cockpit 000)

The upscalers were run on the exported art. The textures were then baked at 4096 and
8192 px and compared in close-ups rendered at 32 pixels per degree.
`project_textures.py` writes those close-ups as `renders/tex*_detail_*.png`.

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
- **Recommendation.** Real-ESRGAN x4plus with an 8192 px atlas. The smeared text doesn't matter
  because the game draws screen contents at runtime. For final assets, an artist pass over the
  upscaled art, or a model tuned on 90s pre-rendered CG, would remove the painted look.
- **Speed.** The upscale takes seconds per cockpit on an M1 Pro (ncnn-vulkan via Metal).
