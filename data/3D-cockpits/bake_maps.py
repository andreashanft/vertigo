"""
Lay out a 3D cockpit's texture atlas and bake the maps that compose_texture.py turns into its
texture. The maps describe the geometry only, no game art goes into them.

Run on a model built by cockpit_NNN.py:

  /Applications/Blender.app/Contents/MacOS/Blender -b out/NNN/cockpit_NNN.blend \
      --python bake_maps.py -- [--res 8192] [--samples 32] [--eye-scale 8]

Writes out/NNN/maps/:
  view.exr        per texel: R, G = the pixel of the 640x480 art the eye sees it at (x right,
                  y down, pixel i covers [i, i + 1)); B = how directly it faces the eye (cosine,
                  -1 behind the eye)
  part.exr        per texel: R = the part (index into parts.json, 0 = no texel), G = material
  shade.exr       the untextured model lit by the scene lights, at half resolution: the shading
                  of surfaces the art doesn't show
  eye.exr         the model seen from the eye at --eye-scale times the art's resolution:
                  R = part (0 = window), G = distance along the view axis
  parts.json      the parts, materials and the camera
  cockpit_NNN_atlas.glb   the model with the atlas UVs and a placeholder texture

How the atlas is laid out
  Every mesh gets an "Atlas" UV map. The hull keeps the UV map it was built with: islands split
  along its grid lines (so their seams are smooth curves, not staircases), full texel density where
  the art lands, HIDDEN_DENSITY elsewhere. Everything else is unwrapped with Smart UV Project; faces
  the art shows and faces it doesn't are unwrapped separately, the hidden ones at HIDDEN_DENSITY.
"""

import json
import math
import os
import re
import sys
import time

import bmesh
import bpy
import numpy as np
import OpenImageIO as oiio
from bpy_extras.bmesh_utils import bmesh_linked_uv_islands
from mathutils import Matrix

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cockpitlib as CL  # noqa: E402

arg = CL.arg
CID = arg("--cockpit") or re.search(r"cockpit_(\d+)", os.path.basename(bpy.data.filepath)).group(1)
CL.load(CID)
F, CX, CY = CL.F, CL.CX, CL.CY
OUT = os.path.dirname(bpy.data.filepath)
RES = int(arg("--res", 8192))
SAMPLES = int(arg("--samples", 32))
EYE_SCALE = int(arg("--eye-scale", 8))
HIDDEN_DENSITY = CL.HIDDEN_DENSITY   # texel density of surfaces the art never shows (relative)
MAPS = os.path.join(OUT, "maps")
os.makedirs(MAPS, exist_ok=True)
T0 = time.time()


def log(*a):
    print(f"[maps {CID} {time.time() - T0:6.1f}s]", *a, flush=True)


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


SEEN = {}
for o in obs:
    mw, mn = o.matrix_world, o.matrix_world.to_3x3()
    SEEN[o.name] = np.array([in_frame(mw @ p.center, (mn @ p.normal).normalized())
                             for p in o.data.polygons], bool)


# The hull comes with a UV map from its builder: islands split along its grid lines, the part the
# art shows at full density and the rest at HIDDEN_DENSITY. It is only scaled here.
natural = [o for o in obs if o.name == "Hull" and "UVMap" in o.data.uv_layers]
for o in natural:
    uv = np.empty(len(o.data.loops) * 2, np.float32)
    o.data.uv_layers["UVMap"].data.foreach_get("uv", uv)
    o.data.uv_layers["Atlas"].data.foreach_set("uv", uv)
# Everything else: Smart UV Project, faces the art shows and faces it doesn't unwrapped
# separately, so no island mixes both
bpy.ops.object.select_all(action='DESELECT')
for o in obs:
    o.select_set(True)
bpy.context.view_layer.objects.active = obs[0]
sc.tool_settings.mesh_select_mode = (False, False, True)
for want in (True, False):
    for o in obs:  # exact face selection (vertex selection would grow into neighbours)
        o.data.vertices.foreach_set("select", np.zeros(len(o.data.vertices), bool))
        o.data.edges.foreach_set("select", np.zeros(len(o.data.edges), bool))
        o.data.polygons.foreach_set("select", (SEEN[o.name] == want) & (o not in natural))
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.uv.smart_project(angle_limit=math.radians(60), island_margin=0.0015,
                             area_weight=0.0, correct_aspect=True, scale_to_bounds=False)
    bpy.ops.object.mode_set(mode='OBJECT')
sc.tool_settings.use_uv_select_sync = True
for o in natural:  # density: full where the art lands
    bm = bmesh.new()
    bm.from_mesh(o.data)
    bm.transform(o.matrix_world)
    lay = bm.loops.layers.uv["Atlas"]
    bm.faces.ensure_lookup_table()
    shown = [bm.faces[i] for i in np.nonzero(SEEN[o.name])[0]]
    uva = sum(abs(sum((f.loops[i][lay].uv.x * f.loops[i - 1][lay].uv.y
                       - f.loops[i - 1][lay].uv.x * f.loops[i][lay].uv.y)
                      for i in range(len(f.loops)))) / 2 for f in shown)
    k = math.sqrt(sum(f.calc_area() for f in shown) / uva)
    for f in bm.faces:
        for l in f.loops:
            l[lay].uv = l[lay].uv * k
    bm.transform(o.matrix_world.inverted())
    bm.to_mesh(o.data)
    bm.free()
for o in (o for o in obs if o not in natural):  # full density where the art lands, lower elsewhere
    bm = bmesh.new()
    bm.from_mesh(o.data)
    bm.transform(o.matrix_world)
    lay = bm.loops.layers.uv["Atlas"]
    seen = SEEN[o.name]
    bm.faces.index_update()
    for isl in bmesh_linked_uv_islands(bm, lay):
        uva = sum(abs(sum((f.loops[i][lay].uv.x * f.loops[i - 1][lay].uv.y
                           - f.loops[i - 1][lay].uv.x * f.loops[i][lay].uv.y)
                          for i in range(len(f.loops)))) / 2 for f in isl)
        a3 = sum(f.calc_area() for f in isl)
        if uva <= 0 or a3 <= 0:
            continue
        k = (1.0 if seen[isl[0].index] else HIDDEN_DENSITY) * math.sqrt(a3 / uva)
        loops = [l for f in isl for l in f.loops]
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

# ---------------------------------------------------------------------------------------------
# 2. Parts: every atlas object is one part; its faces carry the part and material index
# ---------------------------------------------------------------------------------------------
mats = sorted({s.material for o in obs for s in o.material_slots if s.material}, key=lambda m: m.name)
mat_index = {m.name: i for i, m in enumerate(mats)}
parts = []
for i, o in enumerate(obs, 1):
    slot_mats = [s.material for s in o.material_slots]
    mi = np.array([mat_index[m.name] if m else 0 for m in slot_mats] or [0])
    idx = np.empty(len(o.data.polygons), np.int32)
    o.data.polygons.foreach_get("material_index", idx)
    for name, values in (("part", np.full(len(idx), i, np.float32)),
                         ("mat", mi[np.clip(idx, 0, len(mi) - 1)].astype(np.float32))):
        a = o.data.attributes.new(name, 'FLOAT', 'FACE')
        a.data.foreach_set("value", values)
    main = slot_mats[0].name if slot_mats and slot_mats[0] else ""
    parts.append(dict(index=i, name=o.name, collection=o.users_collection[0].name,
                      material=main))

# ---------------------------------------------------------------------------------------------
# 3. Bake (one joined copy: baking many objects at once costs objects x texels)
# ---------------------------------------------------------------------------------------------
sc.render.engine = 'CYCLES'
CL.use_metal()
glass.hide_render = hud.hide_render = True
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
proxy_mats = [s.material for s in proxy.material_slots]


def emission_material(name, build):
    """A material emitting build(nodes, links, geometry) -> colour socket."""
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    N, L = m.node_tree.nodes, m.node_tree.links
    for n in list(N):
        N.remove(n)
    out = N.new("ShaderNodeOutputMaterial")
    emit = N.new("ShaderNodeEmission")
    L.new(build(N, L, N.new("ShaderNodeNewGeometry")), emit.inputs["Color"])
    L.new(emit.outputs[0], out.inputs["Surface"])
    target = N.new("ShaderNodeTexImage")
    N.active = target
    return m, target


def math_node(N, L, op, a, b=None, c=None):
    n = N.new("ShaderNodeMath")
    n.operation = op
    for i, x in enumerate((a, b, c)):
        if x is None:
            continue
        if isinstance(x, (int, float)):
            n.inputs[i].default_value = x
        else:
            L.new(x, n.inputs[i])
    return n.outputs[0]


def attribute(N, name):
    a = N.new("ShaderNodeAttribute")
    a.attribute_type, a.attribute_name = 'GEOMETRY', name
    return a.outputs["Fac"]


# Encoded so that every value is positive: x' = x / 1024 + 0.5, facing' = (facing + 1) / 2.
def build_view(N, L, geo):
    sep = N.new("ShaderNodeSeparateXYZ")
    L.new(geo.outputs["Position"], sep.inputs[0])
    ys = math_node(N, L, 'MAXIMUM', sep.outputs[1], 1e-4)
    px = math_node(N, L, 'MULTIPLY_ADD', math_node(N, L, 'DIVIDE', sep.outputs[0], ys), F, CX)
    py = math_node(N, L, 'MULTIPLY_ADD', math_node(N, L, 'DIVIDE', sep.outputs[2], ys), -F, CY)
    neg = N.new("ShaderNodeVectorMath"); neg.operation = 'SCALE'
    L.new(geo.outputs["Position"], neg.inputs[0]); neg.inputs["Scale"].default_value = -1.0
    nrm = N.new("ShaderNodeVectorMath"); nrm.operation = 'NORMALIZE'
    L.new(neg.outputs[0], nrm.inputs[0])
    dot = N.new("ShaderNodeVectorMath"); dot.operation = 'DOT_PRODUCT'
    L.new(geo.outputs["Normal"], dot.inputs[0]); L.new(nrm.outputs[0], dot.inputs[1])
    front = math_node(N, L, 'GREATER_THAN', sep.outputs[1], 0.01)
    facing = math_node(N, L, 'SUBTRACT', math_node(N, L, 'MULTIPLY',
                       math_node(N, L, 'ADD', dot.outputs["Value"], 1.0), front), 1.0)
    comb = N.new("ShaderNodeCombineXYZ")
    for i, v in enumerate((px, py)):
        L.new(math_node(N, L, 'MULTIPLY_ADD', v, 1 / 1024, 0.5), comb.inputs[i])
    L.new(math_node(N, L, 'MULTIPLY_ADD', facing, 0.5, 0.5), comb.inputs[2])
    return comb.outputs[0]


def build_part(N, L, geo):
    sep = N.new("ShaderNodeSeparateXYZ")
    L.new(geo.outputs["Position"], sep.inputs[0])
    comb = N.new("ShaderNodeCombineXYZ")
    L.new(math_node(N, L, 'DIVIDE', attribute(N, "part"), 4096), comb.inputs[0])
    L.new(math_node(N, L, 'DIVIDE', attribute(N, "mat"), 256), comb.inputs[1])
    L.new(math_node(N, L, 'DIVIDE', sep.outputs[1], 16), comb.inputs[2])  # forward distance
    return comb.outputs[0]


def save_exr(name, a):
    path = os.path.join(MAPS, name)
    spec = oiio.ImageSpec(a.shape[1], a.shape[0], a.shape[2], oiio.FLOAT if a.dtype == np.float32
                          else oiio.HALF)
    spec.attribute("compression", "zip")
    o = oiio.ImageOutput.create(path)
    o.open(path, spec)
    o.write_image(a)
    o.close()


def bake(name, btype, material=None, samples=1, res=RES, decode=None):
    img = bpy.data.images.new(f"bake_{name}", res, res, alpha=True, float_buffer=True)
    if material is not None:
        m, target = material
        for s in proxy.material_slots:
            s.material = m
        target.image = img
    else:
        for s, m in zip(proxy.material_slots, proxy_mats):
            s.material = m
            nt = m.node_tree
            t = nt.nodes.get("bake_target") or nt.nodes.new("ShaderNodeTexImage")
            t.name = "bake_target"
            t.image = img
            nt.nodes.active = t
    sc.cycles.samples = samples
    bpy.ops.object.bake(type=btype, uv_layer="Atlas", use_clear=True, margin=max(4, res // 512),
                        margin_type='EXTEND', target='IMAGE_TEXTURES')
    a = np.empty(res * res * 4, np.float32)
    img.pixels.foreach_get(a)
    bpy.data.images.remove(img)
    a = a.reshape(res, res, 4)[::-1, :, :3]   # top row first
    a = decode(a) if decode else a
    log(f"baked {name}")
    return a


view = bake("view", 'EMIT', emission_material("BakeView", build_view),
            decode=lambda a: np.stack([(a[..., 0] - 0.5) * 1024, (a[..., 1] - 0.5) * 1024,
                                       a[..., 2] * 2 - 1], -1).astype(np.float32))
save_exr("view.exr", view)
del view
part_mat = emission_material("BakePart", build_part)
part = bake("part", 'EMIT', part_mat,
            decode=lambda a: np.stack([np.rint(a[..., 0] * 4096), np.rint(a[..., 1] * 256)], -1))
save_exr("part.exr", part.astype(np.float32))
del part
shade = bake("shade", 'COMBINED', samples=SAMPLES, res=RES // 2)
save_exr("shade.exr", shade.astype(np.float16))
del shade

# the model from the eye, point sampled: which part the eye sees at every sub-pixel of the art
for s in proxy.material_slots:
    s.material = part_mat[0]
cam = bpy.data.objects["EyeCamera"]
sc.camera = cam
sc.render.resolution_x, sc.render.resolution_y = 640 * EYE_SCALE, 480 * EYE_SCALE
sc.render.resolution_percentage = 100
sc.render.film_transparent = True
sc.cycles.samples = 1
sc.cycles.use_denoising = False
sc.cycles.pixel_filter_type = 'BOX'
sc.cycles.filter_width = 0.01
sc.view_settings.view_transform = 'Standard'
sc.view_settings.look = 'None'
sc.render.image_settings.file_format = 'OPEN_EXR'
sc.render.image_settings.color_depth = '32'
sc.render.image_settings.exr_codec = 'ZIP'
tmp = os.path.join(MAPS, "eye_render.exr")
sc.render.filepath = tmp
bpy.ops.render.render(write_still=True)
buf = oiio.ImageBuf(tmp)
e = buf.get_pixels(oiio.FLOAT)
eye = np.stack([np.rint(e[..., 0] * 4096) * (e[..., 3] > 0.5), e[..., 2] * 16], -1).astype(np.float32)
os.remove(tmp)
save_exr("eye.exr", eye)
log(f"eye: {EYE_SCALE}x, {len(np.unique(eye[..., 0])) - 1} parts visible")

json.dump(dict(cockpit=CID, res=RES, eye_scale=EYE_SCALE, f=F, cx=CX, cy=CY,
               materials=[m.name for m in mats], parts=parts),
          open(os.path.join(MAPS, "parts.json"), "w"), indent=1)

# ---------------------------------------------------------------------------------------------
# 4. The model with one unlit material (KHR_materials_unlit) and a placeholder texture, which
#    compose_texture.py replaces
# ---------------------------------------------------------------------------------------------
bpy.data.objects.remove(proxy)
for o in obs:
    o.hide_render = False
placeholder = bpy.data.images.new(f"cockpit_{CID}_texture", 16, 16, alpha=False)
placeholder.pixels.foreach_set(np.full(16 * 16 * 4, 0.5, np.float32))
placeholder.filepath_raw = os.path.join(MAPS, "placeholder.png")
placeholder.file_format = 'PNG'
placeholder.save()
mat = bpy.data.materials.new("Cockpit")
mat.use_nodes = True
N, L = mat.node_tree.nodes, mat.node_tree.links
for n in list(N):
    N.remove(n)
out = N.new("ShaderNodeOutputMaterial")
uvn = N.new("ShaderNodeUVMap"); uvn.uv_map = "Atlas"
tex = N.new("ShaderNodeTexImage"); tex.image = placeholder
L.new(uvn.outputs[0], tex.inputs["Vector"])
emit = N.new("ShaderNodeEmission"); L.new(tex.outputs["Color"], emit.inputs["Color"])
diff = N.new("ShaderNodeBsdfDiffuse"); L.new(tex.outputs["Color"], diff.inputs["Color"])
lp = N.new("ShaderNodeLightPath")
mix = N.new("ShaderNodeMixShader")
L.new(lp.outputs["Is Camera Ray"], mix.inputs[0])
L.new(diff.outputs[0], mix.inputs[1]); L.new(emit.outputs[0], mix.inputs[2])
L.new(mix.outputs[0], out.inputs["Surface"])
for o in obs:
    o.data.materials.clear()
    o.data.materials.append(mat)
    for name in ("part", "mat"):
        o.data.attributes.remove(o.data.attributes[name])
    o.data.uv_layers["Atlas"].active_render = True
glass.hide_render = hud.hide_render = False
for o in sc.objects:
    o.select_set(False)
for o in set(obs + [glass, hud] + list(bpy.data.collections["Markers"].objects)):
    o.select_set(True)
bpy.ops.export_scene.gltf(filepath=os.path.join(MAPS, f"cockpit_{CID}_atlas.glb"),
                          export_format='GLB', use_selection=True, export_apply=True,
                          export_extras=True, export_cameras=False, export_lights=False,
                          export_image_format='JPEG')
log("done")
