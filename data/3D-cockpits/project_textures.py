"""
Texture a 3D cockpit by projecting the original 2D artwork (COCKPIT.R16) onto it from the pilot's
eye and baking the result into one texture atlas with an unlit material.

Run on a model built by cockpit_NNN.py (writes out/NNN/cockpit_NNN_textured.blend/.glb,
out/NNN/textures/ and out/NNN/renders/tex_*):

  /Applications/Blender.app/Contents/MacOS/Blender -b out/NNN/cockpit_NNN.blend \
      --python project_textures.py -- [--res 4096] [--samples 32] [--art upscaled.png]
      [--suffix _x4] [--no-render] [--data DIR]

  --art     use this image instead of COCKPIT.R16's colours, e.g. an upscaled version written
            by tools/export_art.py + an upscaler (any resolution with a 4:3 aspect); the
            see-through mask still comes from COCKPIT.R16
  --suffix  appended to all output names, to compare variants

How it works
  1. Every mesh gets an "Atlas" UV map (Smart UV Project). Faces that the original image shows get
     full texel density, hidden ones HIDDEN_DENSITY of it.
  2. Cycles bakes, per texel of the atlas (on a joined copy of all meshes):
       art   - the original image, projected through the cockpit camera (eye = origin)
       info  - R: how directly the surface faces the eye, G: inside the 640x480 frame and not
               part of the see-through window region of the original
       vis   - direct light from a point light at the eye: 0 where the eye cannot see the texel
       cg    - the untextured model lit by the scene lights (fallback for hidden surfaces)
       id    - material index (to colour-match the fallback per material)
  3. numpy blends art (where seen by the eye) with the fallback (shading from cg, colour from
     the art's average per material) and feathers the seam. The result is a lighting-baked,
     unlit texture like the original art.
"""

import math
import os
import re
import sys
import time

import bmesh
import bpy
import numpy as np
from bpy_extras.bmesh_utils import bmesh_linked_uv_islands
from mathutils import Matrix, Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cockpitlib as CL  # noqa: E402

arg = CL.arg
ARGV = CL.ARGV
CID = arg("--cockpit") or re.search(r"cockpit_(\d+)", os.path.basename(bpy.data.filepath)).group(1)
CL.load(CID)
F, CX, CY = CL.F, CL.CX, CL.CY
OUT = os.path.dirname(bpy.data.filepath)
RES = int(arg("--res", 4096))
SAMPLES = int(arg("--samples", 32))
SUFFIX = arg("--suffix", "")
RENDER = "--no-render" not in ARGV
HIDDEN_DENSITY = 0.3   # texel density of surfaces the original never shows (relative)
TEX = os.path.join(OUT, "textures")
REND = os.path.join(OUT, "renders")
os.makedirs(TEX, exist_ok=True)
os.makedirs(REND, exist_ok=True)
T0 = time.time()


def log(*a):
    print(f"[texture {CID} {time.time() - T0:6.1f}s]", *a, flush=True)


sc = bpy.context.scene
ATLAS_COLLS = ("Hull", "Instruments", "Cables", "Seat")
obs = [o for c in ATLAS_COLLS for o in bpy.data.collections[c].objects
       if o.type == 'MESH' and o.name != "Window_Glass"]
glass = bpy.data.objects["Window_Glass"]
hud = bpy.data.objects["HUD_MasterHUD"]

# ---------------------------------------------------------------------------------------------
# 1. Apply modifiers and build the atlas UV map
# ---------------------------------------------------------------------------------------------
dg = bpy.context.evaluated_depsgraph_get()
for o in obs:
    if o.modifiers:
        me = bpy.data.meshes.new_from_object(o.evaluated_get(dg))
        o.modifiers.clear()
        o.data = me
    loc, rot, scale = o.matrix_world.decompose()   # bake scale into the mesh (UV density)
    if (scale - scale.__class__((1, 1, 1))).length > 1e-6:
        o.data.transform(Matrix.Diagonal(scale).to_4x4())
        o.matrix_world = Matrix.LocRotScale(loc, rot, None)
for o in obs:
    lay = o.data.uv_layers.new(name="Atlas")
    o.data.uv_layers.active = lay

def in_frame(p, n):
    """Face centre p (world) is inside the original 640x480 view and faces the eye."""
    if p.y < 0.05 or n.dot(-p.normalized()) < 0.05:
        return False
    px, py = CX + F * p.x / p.y, CY - F * p.z / p.y
    return -20 < px < 660 and -20 < py < 500


def uv_stats():
    uva, a3 = np.zeros(2), np.zeros(2)
    n_isl = 0
    for o in obs:
        uv = np.empty(len(o.data.loops) * 2, np.float32)
        o.data.uv_layers["Atlas"].data.foreach_get("uv", uv)
        uv = uv.reshape(-1, 2)
        for p, seen in zip(o.data.polygons, SEEN[o.name]):
            q = uv[p.loop_start:p.loop_start + p.loop_total]
            uva[int(seen)] += 0.5 * abs(np.dot(q[:, 0], np.roll(q[:, 1], 1))
                                        - np.dot(q[:, 1], np.roll(q[:, 0], 1)))
            a3[int(seen)] += p.area
        bm = bmesh.new()
        bm.from_mesh(o.data)
        n_isl += len(bmesh_linked_uv_islands(bm, bm.loops.layers.uv["Atlas"]))
        bm.free()
    d = np.sqrt(uva / np.maximum(a3, 1e-9)) * RES
    return (f"fill {uva.sum():.0%}, {n_isl} islands, texels/m: seen {d[1]:.0f}, "
            f"hidden {d[0]:.0f}")


# Faces the original image shows and faces it doesn't are unwrapped separately, so no UV island
# mixes both; hidden islands then get a lower texel density.
SEEN = {}
for o in obs:
    mw, mn = o.matrix_world, o.matrix_world.to_3x3()
    SEEN[o.name] = np.array([in_frame(mw @ p.center, (mn @ p.normal).normalized())
                             for p in o.data.polygons], bool)
bpy.ops.object.select_all(action='DESELECT')
for o in obs:
    o.select_set(True)
bpy.context.view_layer.objects.active = obs[0]
sc.tool_settings.mesh_select_mode = (False, False, True)
for want in (True, False):
    for o in obs:  # exact face selection (vertex selection would grow into neighbours)
        o.data.vertices.foreach_set("select", np.zeros(len(o.data.vertices), bool))
        o.data.edges.foreach_set("select", np.zeros(len(o.data.edges), bool))
        o.data.polygons.foreach_set("select", SEEN[o.name] == want)
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.uv.smart_project(angle_limit=math.radians(60), island_margin=0.0015,
                             area_weight=0.0, correct_aspect=True, scale_to_bounds=False)
    bpy.ops.object.mode_set(mode='OBJECT')
sc.tool_settings.use_uv_select_sync = True
for o in obs:  # set every island's texel density: full where the art lands, lower elsewhere
    bm = bmesh.new()
    bm.from_mesh(o.data)
    bm.transform(o.matrix_world)
    lay = bm.loops.layers.uv["Atlas"]
    seen = SEEN[o.name]
    bm.faces.index_update()
    for isl in bmesh_linked_uv_islands(bm, lay):
        loops = [l for f in isl for l in f.loops]
        uva = sum(abs(sum((f.loops[i][lay].uv.x * f.loops[i - 1][lay].uv.y
                           - f.loops[i - 1][lay].uv.x * f.loops[i][lay].uv.y)
                          for i in range(len(f.loops)))) / 2 for f in isl)
        a3 = sum(f.calc_area() for f in isl)
        if uva <= 0 or a3 <= 0:
            continue
        k = (1.0 if seen[isl[0].index] else HIDDEN_DENSITY) * math.sqrt(a3 / uva)
        c = sum((l[lay].uv for l in loops), loops[0][lay].uv * 0) / len(loops)
        for l in loops:
            l[lay].uv = c + (l[lay].uv - c) * k
    bm.transform(o.matrix_world.inverted())
    bm.to_mesh(o.data)
    bm.free()
log("before pack:", uv_stats())
bpy.ops.object.mode_set(mode='EDIT')
bpy.ops.mesh.select_all(action='SELECT')
bpy.ops.uv.pack_islands(rotate=True, scale=True, margin_method='FRACTION', margin=2.5 / RES,
                        shape_method='CONCAVE')
bpy.ops.object.mode_set(mode='OBJECT')
log("after pack: ", uv_stats())

area3d = sum(p.area for o in obs for p in o.data.polygons)
area_seen, uv_seen = 0.0, 0.0
uv_area = 0.0
for o in obs:
    uv = np.empty(len(o.data.loops) * 2, np.float32)
    o.data.uv_layers["Atlas"].data.foreach_get("uv", uv)
    uv = uv.reshape(-1, 2)
    for p in o.data.polygons:
        q = uv[p.loop_start:p.loop_start + p.loop_total]
        a = 0.5 * abs(np.dot(q[:, 0], np.roll(q[:, 1], 1)) - np.dot(q[:, 1], np.roll(q[:, 0], 1)))
        uv_area += a
        if in_frame(o.matrix_world @ p.center, (o.matrix_world.to_3x3() @ p.normal).normalized()):
            area_seen += p.area
            uv_seen += a
log(f"atlas: {len(obs)} objects, 3D area {area3d:.1f} m2 ({area_seen:.1f} m2 in the original's "
    f"view), UV fill {uv_area:.0%}, ~{RES * math.sqrt(uv_seen / max(area_seen, 1e-6)):.0f} "
    f"texels/m where the art lands")

# ---------------------------------------------------------------------------------------------
# 2. Source image: COCKPIT.R16. Only the large see-through regions (window) count as "no art";
#    isolated colour-key pixels are genuine black. Window colours are dilated so filtering at
#    the window edge doesn't pull in black.
# ---------------------------------------------------------------------------------------------
see_through = CL.SEE
rgb = CL.ART.astype(np.float64).copy()
log(f"source: {see_through.sum()} see-through px")
filled = ~see_through
for _ in range(64):  # dilate art colours into the see-through area
    acc = np.zeros_like(rgb); n = np.zeros(see_through.shape)
    for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
        f = np.roll(filled, (dy, dx), (0, 1)); c = np.roll(rgb, (dy, dx), (0, 1))
        acc += c * f[..., None]; n += f
    grow = ~filled & (n > 0)
    rgb[grow] = acc[grow] / n[grow, None]
    filled |= grow
art_px = np.concatenate([rgb, (~see_through)[..., None]], -1).astype(np.float32)
src_px = art_px
if arg("--art"):  # e.g. an upscaled image; alpha (see-through mask) from COCKPIT.R16
    up = CL.load_rgba(arg("--art"))
    hh, ww = up.shape[:2]
    yi = (np.arange(hh) * 480 // hh)[:, None]
    xi = (np.arange(ww) * 640 // ww)[None, :]
    src_px = np.concatenate([up[..., :3], (~see_through)[yi, xi][..., None]], -1).astype(np.float32)
    log(f"art: {arg('--art')} ({ww}x{hh})")
art = bpy.data.images.new(f"COCKPIT_{CID}_art", src_px.shape[1], src_px.shape[0], alpha=True)
art.colorspace_settings.name = 'sRGB'
art.alpha_mode = 'STRAIGHT'
art.pixels.foreach_set(np.ascontiguousarray(src_px[::-1]).ravel())
art.pack()

# ---------------------------------------------------------------------------------------------
# 3. Bake setup: every material gets projection / info / id nodes and a bake target node
# ---------------------------------------------------------------------------------------------
mats = sorted({s.material for o in obs for s in o.material_slots if s.material}, key=lambda m: m.name)
MODES = {}
for idx, m in enumerate(mats):
    nt = m.node_tree
    N, L = nt.nodes, nt.links
    out = next(n for n in N if n.type == 'OUTPUT_MATERIAL' and n.is_active_output)
    bsdf = N["Principled BSDF"]
    geo = N.new("ShaderNodeNewGeometry")
    sep = N.new("ShaderNodeSeparateXYZ")
    L.new(geo.outputs["Position"], sep.inputs[0])

    def math_node(op, a, b=None, val=None):
        n = N.new("ShaderNodeMath")
        n.operation = op
        for i, x in enumerate((a, b)):
            if x is None:
                continue
            if isinstance(x, (int, float)):
                n.inputs[i].default_value = x
            else:
                L.new(x, n.inputs[i])
        return n.outputs[0]

    ys = math_node('MAXIMUM', sep.outputs[1], 1e-4)
    u = math_node('MULTIPLY_ADD', math_node('DIVIDE', sep.outputs[0], ys), F / 640, None)
    N[u.node.name].inputs[2].default_value = CX / 640
    v = math_node('MULTIPLY_ADD', math_node('DIVIDE', sep.outputs[2], ys), F / 480, None)
    N[v.node.name].inputs[2].default_value = (480 - CY) / 480
    comb = N.new("ShaderNodeCombineXYZ")
    L.new(u, comb.inputs[0]); L.new(v, comb.inputs[1])
    tex = N.new("ShaderNodeTexImage")
    tex.image, tex.interpolation, tex.extension = art, 'Linear', 'CLIP'
    L.new(comb.outputs[0], tex.inputs["Vector"])

    neg = N.new("ShaderNodeVectorMath"); neg.operation = 'SCALE'
    L.new(geo.outputs["Position"], neg.inputs[0]); neg.inputs["Scale"].default_value = -1.0
    nrm = N.new("ShaderNodeVectorMath"); nrm.operation = 'NORMALIZE'
    L.new(neg.outputs[0], nrm.inputs[0])
    dot = N.new("ShaderNodeVectorMath"); dot.operation = 'DOT_PRODUCT'
    L.new(geo.outputs["Normal"], dot.inputs[0]); L.new(nrm.outputs[0], dot.inputs[1])
    front = math_node('GREATER_THAN', sep.outputs[1], 0.01)
    valid = math_node('MULTIPLY', tex.outputs["Alpha"], front)
    info = N.new("ShaderNodeCombineColor")
    L.new(dot.outputs["Value"], info.inputs[0]); L.new(valid, info.inputs[1])

    idc = N.new("ShaderNodeRGB")
    idc.outputs[0].default_value = ((idx + 1) / 64.0, 0.0, 0.0, 1.0)
    emit = N.new("ShaderNodeEmission")
    diff = N.new("ShaderNodeBsdfDiffuse")
    diff.inputs["Color"].default_value = (1, 1, 1, 1)
    target = N.new("ShaderNodeTexImage")
    MODES[m.name] = dict(nt=nt, out=out, bsdf=bsdf, emit=emit, diff=diff, target=target,
                         art=tex.outputs["Color"], info=info.outputs[0], id=idc.outputs[0])


def set_mode(mode, img):
    for d in MODES.values():
        L = d["nt"].links
        for lk in list(d["out"].inputs["Surface"].links):
            L.remove(lk)
        for lk in list(d["emit"].inputs["Color"].links):
            L.remove(lk)
        if mode == 'cg':
            L.new(d["bsdf"].outputs[0], d["out"].inputs["Surface"])
        elif mode == 'vis':
            L.new(d["diff"].outputs[0], d["out"].inputs["Surface"])
        else:
            L.new(d[mode], d["emit"].inputs["Color"])
            L.new(d["emit"].outputs[0], d["out"].inputs["Surface"])
        d["target"].image = img
        d["nt"].nodes.active = d["target"]
        d["target"].select = True


sc.render.engine = 'CYCLES'
try:
    prefs = bpy.context.preferences.addons['cycles'].preferences
    prefs.compute_device_type = 'METAL'
    prefs.get_devices()
    for dv in prefs.devices:
        dv.use = dv.type == 'METAL'
    sc.cycles.device = 'GPU'
except Exception as e:
    log("no Metal:", e)
sc.render.bake.margin = 8
sc.render.bake.margin_type = 'EXTEND'
glass.hide_render = hud.hide_render = True
# Bake a single joined copy: baking many objects at once costs (objects x texels).
proxies = []
for o in obs:
    me = o.data.copy()
    me.transform(o.matrix_world)
    p = bpy.data.objects.new("bake_" + o.name, me)
    sc.collection.objects.link(p)
    proxies.append(p)
    o.hide_render = True
for o in sc.objects:
    o.select_set(o in proxies)
bpy.context.view_layer.objects.active = proxies[0]
bpy.ops.object.join()
proxy = bpy.context.view_layer.objects.active
proxy.data.uv_layers.active = proxy.data.uv_layers["Atlas"]
for o in sc.objects:
    o.select_set(o == proxy)


def bake(mode, btype, samples=1, pass_filter=None, res=RES, channels=3):
    img = bpy.data.images.new(f"bake_{mode}", res, res, alpha=True, float_buffer=True)
    set_mode(mode, img)
    sc.cycles.samples = samples
    kw = dict(type=btype, uv_layer="Atlas", use_clear=True, margin=max(4, res // 512),
              target='IMAGE_TEXTURES')
    if pass_filter is not None:
        kw["pass_filter"] = pass_filter
    bpy.ops.object.bake(**kw)
    a = np.empty(res * res * 4, np.float32)
    img.pixels.foreach_get(a)
    bpy.data.images.remove(img)
    log(f"baked {mode}")
    a = np.ascontiguousarray(a.reshape(res, res, 4)[..., :channels])  # keep memory down at 8K
    if res != RES:  # smooth fallback lighting: bake at lower resolution and upsample
        a = np.repeat(np.repeat(a, RES // res, 0), RES // res, 1)
    return a


B = {}
B["art"] = bake('art', 'EMIT')
B["info"] = bake('info', 'EMIT', channels=2)
B["id"] = bake('id', 'EMIT', channels=1)
B["cg"] = bake('cg', 'COMBINED', SAMPLES, res=RES // 2)
lights = [o for o in sc.objects if o.type == 'LIGHT']
for o in lights:
    o.hide_render = True
world_strength = sc.world.node_tree.nodes["Background"].inputs["Strength"]
ws = world_strength.default_value
world_strength.default_value = 0.0
eye_ld = bpy.data.lights.new("EyeProbe", 'POINT')
eye_ld.energy, eye_ld.shadow_soft_size = 1000.0, 0.0
eye_light = bpy.data.objects.new("EyeProbe", eye_ld)
sc.collection.objects.link(eye_light)
B["vis"] = bake('vis', 'DIFFUSE', 1, {'DIRECT'}, channels=1)
bpy.data.objects.remove(eye_light)
bpy.data.lights.remove(eye_ld)
for o in lights:
    o.hide_render = False
world_strength.default_value = ws
bpy.data.objects.remove(proxy)
for o in obs:
    o.hide_render = False

# ---------------------------------------------------------------------------------------------
# 4. Compose: art where the eye sees the surface, colour-matched lit fallback elsewhere
# ---------------------------------------------------------------------------------------------
def box_blur(x, r):
    for axis in (0, 1):
        pad = [(0, 0), (0, 0)]
        pad[axis] = (r + 1, r)
        c = np.cumsum(np.pad(x, pad, mode='edge'), axis=axis)
        n = x.shape[axis]
        x = (np.take(c, np.arange(2 * r + 1, 2 * r + 1 + n), axis)
             - np.take(c, np.arange(n), axis)) / (2 * r + 1)
    return x


idf = B["id"][..., 0] * 64
mid = np.rint(idf).astype(np.int32) - 1
exact = np.abs(idf - np.rint(idf)) < 0.02
used = idf > 0.02                                                   # texel covered by the atlas
mid[~exact] = -1
for _ in range(16):  # fill border texels with a neighbouring exact id
    hole = used & (mid < 0)
    if not hole.any():
        break
    for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
        nb = np.roll(mid, (dy, dx), (0, 1))
        take = hole & (nb >= 0) & (mid < 0)
        mid[take] = nb[take]
mid[~used] = -1
facing = B["info"][..., 0]
seen = (B["vis"][..., 0] > 0.05) & (facing > 0.08) & (B["info"][..., 1] > 0.5) & (mid >= 0)
w = box_blur(seen.astype(np.float32), max(2, RES // 1024))
w = np.clip(2 * w - 1, 0, 1) * seen                                  # feather inwards only
w *= np.clip((facing - 0.08) / 0.17, 0, 1)                           # grazing -> stretched art
art_c, cg_c = B["art"][..., :3], B["cg"][..., :3]

core = w > 0.95
lum = lambda x: x @ np.array([0.2126, 0.7152, 0.0722], np.float32)
glob_a, glob_c = art_c[core].mean(0), max(lum(cg_c[core].mean(0)), 1e-5)
tint = np.tile(glob_a / glob_c, (len(mats), 1))  # per material: art colour per unit cg luminance
for i, m in enumerate(mats):
    sel = core & (mid == i)
    if sel.sum() > 2000:
        tint[i] = art_c[sel].mean(0) / max(lum(cg_c[sel].mean(0)), 1e-5)
    log(f"  {m.name:12s} seen texels {int(sel.sum()):8d}  tint {np.round(tint[i], 2)}")
# fallback: lighting/shading from the cg bake, colour from the art's average for that material
cg_m = lum(cg_c)[..., None] * tint[np.clip(mid, 0, None)]
final = w[..., None] * art_c + (1 - w[..., None]) * cg_m
final[mid < 0] = 0
log(f"art covers {seen.sum() / max((mid >= 0).sum(), 1):.1%} of the used atlas")


def to_srgb(x):
    x = np.clip(x, 0, 1)
    return np.where(x <= 0.0031308, 12.92 * x, 1.055 * np.power(x, 1 / 2.4) - 0.055)


def save_png(name, rgb, colorspace='sRGB'):
    img = bpy.data.images.new(name, rgb.shape[1], rgb.shape[0], alpha=False)
    img.colorspace_settings.name = colorspace
    px = np.concatenate([rgb, np.ones(rgb.shape[:2] + (1,), np.float32)], -1)
    img.pixels.foreach_set(np.ascontiguousarray(px, np.float32).ravel())
    img.filepath_raw = os.path.join(TEX, name + ".png")
    img.file_format = 'PNG'
    img.save()
    return img


atlas = save_png(f"cockpit_{CID}_projected{SUFFIX}", to_srgb(final))
mask = np.stack([w, seen * 0.5, (mid >= 0) * 0.15], -1).astype(np.float32)
bpy.data.images.remove(save_png(f"cockpit_{CID}_projection_mask{SUFFIX}", mask, 'Non-Color'))
del B, art_c, cg_c, cg_m

# ---------------------------------------------------------------------------------------------
# 5. One unlit material (exports as KHR_materials_unlit) for every atlas object
# ---------------------------------------------------------------------------------------------
mat = bpy.data.materials.new("Cockpit_Projected")
nt = mat.node_tree
N, L = nt.nodes, nt.links
for n in list(N):
    N.remove(n)
out = N.new("ShaderNodeOutputMaterial")
uvn = N.new("ShaderNodeUVMap"); uvn.uv_map = "Atlas"
tex = N.new("ShaderNodeTexImage"); tex.image = atlas; tex.interpolation = 'Linear'
L.new(uvn.outputs[0], tex.inputs["Vector"])
emit = N.new("ShaderNodeEmission"); L.new(tex.outputs["Color"], emit.inputs["Color"])
diff = N.new("ShaderNodeBsdfDiffuse"); L.new(tex.outputs["Color"], diff.inputs["Color"])
lp = N.new("ShaderNodeLightPath")
mix = N.new("ShaderNodeMixShader")
L.new(lp.outputs["Is Camera Ray"], mix.inputs[0])
L.new(diff.outputs[0], mix.inputs[1]); L.new(emit.outputs[0], mix.inputs[2])
L.new(mix.outputs[0], out.inputs["Surface"])
mat.diffuse_color = (0.3, 0.3, 0.3, 1)
for o in obs:
    o.data.materials.clear()
    o.data.materials.append(mat)
for o in obs:
    o.data.uv_layers["Atlas"].active_render = True
atlas.filepath = f"//textures/cockpit_{CID}_projected{SUFFIX}.png"
glass.hide_render = hud.hide_render = False

for o in sc.objects:
    o.select_set(False)
export = obs + [glass, hud] + list(bpy.data.collections["Markers"].objects)
for o in set(export):
    o.select_set(True)
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUT, f"cockpit_{CID}_textured{SUFFIX}.blend"),
                            compress=True, relative_remap=True)
bpy.ops.export_scene.gltf(filepath=os.path.join(OUT, f"cockpit_{CID}_textured{SUFFIX}.glb"),
                          export_format='GLB', use_selection=True, export_apply=True,
                          export_extras=True, export_cameras=False, export_lights=False,
                          export_image_format='JPEG', export_image_quality=90)
log("saved blend + glb")

if not RENDER:
    sys.exit(0)

# ---------------------------------------------------------------------------------------------
# 6. Renders: from the eye (should reproduce the original), with the head moved, and 360
# ---------------------------------------------------------------------------------------------
sc.view_settings.view_transform = 'Standard'
sc.view_settings.look = 'None'
sc.cycles.samples = 16
sc.cycles.use_denoising = False
sc.render.film_transparent = False
sc.render.image_settings.file_format = 'PNG'
C_RIG = bpy.data.collections["RenderRig"]


def cam(name, loc, rot, angle=105.0):
    cd = bpy.data.cameras.new(name)
    cd.sensor_fit, cd.angle, cd.clip_start = 'HORIZONTAL', math.radians(angle), 0.01
    ob = bpy.data.objects.new(name, cd)
    ob.location, ob.rotation_euler = loc, [math.radians(a) for a in rot]
    C_RIG.objects.link(ob)
    return ob


def render(path, camera, res):
    sc.camera = camera
    sc.render.resolution_x, sc.render.resolution_y = res
    sc.render.filepath = path
    bpy.ops.render.render(write_still=True)
    log("rendered", os.path.basename(path))


hud.hide_render = glass.hide_render = True   # glass only adds transparency noise here
eye_cam = bpy.data.objects["EyeCamera"]
render(os.path.join(REND, f"tex{SUFFIX}_eye_front.png"), eye_cam, (1280, 960))
render(os.path.join(REND, f"tex{SUFFIX}_eye_front_640.png"), eye_cam, (640, 480))
render(os.path.join(REND, f"tex{SUFFIX}_head_moved.png"),
       cam("CamHeadMoved", (0.16, 0.12, -0.06), (88, 0, 12), 100), (1280, 960))
render(os.path.join(REND, f"tex{SUFFIX}_look_left_down.png"),
       cam("CamTexLeft", (0.04, 0.0, 0.0), (68, 0, 38), 100), (1280, 960))
render(os.path.join(REND, f"tex{SUFFIX}_look_right_up.png"),
       cam("CamTexRight", (-0.04, 0.0, 0.0), (104, 0, -40), 100), (1280, 960))
pano = bpy.data.objects.get("CamPanorama") or cam("CamPanorama", (0, 0, 0), (90, 0, 0))
pano.data.type, pano.data.panorama_type = 'PANO', 'EQUIRECTANGULAR'
for o in bpy.data.collections["Seat"].objects:
    o.hide_render = True
render(os.path.join(REND, f"tex{SUFFIX}_eye_360_equirect.png"), pano, (2048, 1024))
for o in bpy.data.collections["Seat"].objects:
    o.hide_render = False

# close-ups at headset-like resolution: 1280 px over 40 deg = 32 px/deg (the original: ~6 px/deg)
for section, tag in (("MasterMonitor", "master"), ("RoundMonitor0", "round0")):
    if section.lower() in CL.DES:
        r = CL.des_rect(section)
        cx, cy = (r[0] + r[2] / 2, r[1] + r[3] / 2) if len(r) == 4 else r[:2]
        c = cam(f"CamDetail_{tag}", (0, 0, 0), (0, 0, 0), 40)
        c.rotation_euler = Vector(CL.unit(CL.ray_np(cx, cy)).tolist()).to_track_quat('-Z', 'Y').to_euler()
        render(os.path.join(REND, f"tex{SUFFIX}_detail_{tag}.png"), c, (1280, 960))

# side-by-side with the original from the eye
img = bpy.data.images.load(os.path.join(REND, f"tex{SUFFIX}_eye_front_640.png"))
ours = np.array(img.pixels[:], np.float32).reshape(480, 640, 4)[::-1, :, :3]
orig = np.where(see_through[..., None], ours, art_px[..., :3])
sheet = np.concatenate([orig, ours], 1)[::-1]
cmp_img = bpy.data.images.new("cmp", 1280, 480, alpha=False)
cmp_img.pixels.foreach_set(np.ascontiguousarray(
    np.concatenate([sheet, np.ones((480, 1280, 1), np.float32)], -1)).ravel())
cmp_img.filepath_raw = os.path.join(REND, f"tex{SUFFIX}_compare_original.png")
cmp_img.file_format = 'PNG'
cmp_img.save()
log("done")
