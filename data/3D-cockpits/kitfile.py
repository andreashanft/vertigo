"""
Writing a cockpit kit: the model Vertigo textures itself from the player's cockpit art
(src/hud/cockpitpaint.cpp). A binary glTF 2.0 file with KHR_mesh_quantization, so it stays small:

  - one node and mesh per part: positions (16 bit), normals (8 bit), atlas UVs (16 bit); the
    material "Cockpit" with the fallback shading as its texture; extras: the part's material
    ("material") and what Blender gave the object (role, des_section, des_rect)
  - the window glass and the HUD plane with blended materials, as in the textured models
  - asset.extras.vertigo_kit: version, cockpit, camera (focal length and optical centre in
    pixels of the 640x480 view) and the atlas size the islands' margins are made for

Positions are the model's (glTF: x right, y up, -z forward, metres, the eye at the origin); all
nodes share one translation and uniform scale that undo the quantization. Plain Python + numpy.
"""

import json
import struct

import numpy as np

VERSION = 1


def _pad(b, fill=b"\0"):
    return b + fill * (-len(b) % 4)


def write(path, parts, shade_jpeg, info):
    """parts: dicts with name, positions (n, 3), normals (n, 3) or None, uvs (n, 2) or None (0..1,
    v down), indices (m,), material ("Cockpit", "Glass" or "HUD"), extras (dict)."""
    allpos = np.concatenate([p["positions"] for p in parts])
    lo, hi = allpos.min(0), allpos.max(0)
    centre = (lo + hi) / 2
    scale = float((hi - lo).max() / 2) * 1.0001
    views, accessors, meshes, nodes, chunks = [], [], [], [], []
    offset = 0

    def view(data, target=None, stride=None):
        nonlocal offset
        data = _pad(data)
        v = dict(buffer=0, byteOffset=offset, byteLength=len(data))
        if target:
            v["target"] = target
        if stride:
            v["byteStride"] = stride
        views.append(v)
        chunks.append(data)
        offset += len(data)
        return len(views) - 1

    def accessor(view_index, ctype, count, type_, normalized=False, mn=None, mx=None):
        a = dict(bufferView=view_index, componentType=ctype, count=int(count), type=type_)
        if normalized:
            a["normalized"] = True
        if mn is not None:
            a["min"], a["max"] = [float(x) for x in mn], [float(x) for x in mx]
        accessors.append(a)
        return len(accessors) - 1

    materials = {"Cockpit": 0, "Glass": 1, "HUD": 2}
    for p in parts:
        n = len(p["positions"])
        q = np.clip(np.rint((p["positions"] - centre) / scale * 32767), -32767, 32767).astype("<i2")
        q4 = np.zeros((n, 4), "<i2")
        q4[:, :3] = q
        attributes = dict(POSITION=accessor(view(q4.tobytes(), 34962, 8), 5122, n, "VEC3", True,
                                            q.min(0) / 32767, q.max(0) / 32767))
        if p.get("normals") is not None:
            nn = p["normals"] / np.maximum(np.linalg.norm(p["normals"], axis=1, keepdims=True), 1e-9)
            b4 = np.zeros((n, 4), "i1")
            b4[:, :3] = np.clip(np.rint(nn * 127), -127, 127)
            attributes["NORMAL"] = accessor(view(b4.tobytes(), 34962, 4), 5120, n, "VEC3", True)
        if p.get("uvs") is not None:
            uv = np.clip(np.rint(p["uvs"] * 65535), 0, 65535).astype("<u2")
            attributes["TEXCOORD_0"] = accessor(view(uv.tobytes(), 34962), 5123, n, "VEC2", True)
        idx = np.asarray(p["indices"])
        if idx.max() < 65536:
            ib, ct = idx.astype("<u2").tobytes(), 5123
        else:
            ib, ct = idx.astype("<u4").tobytes(), 5125
        primitive = dict(attributes=attributes, indices=accessor(view(ib, 34963), ct, len(idx), "SCALAR"),
                         material=materials[p["material"]])
        meshes.append(dict(name=p["name"], primitives=[primitive]))
        node = dict(name=p["name"], mesh=len(meshes) - 1, translation=[float(c) for c in centre],
                    scale=[scale] * 3)
        if p.get("extras"):
            node["extras"] = p["extras"]
        nodes.append(node)
    image_view = view(shade_jpeg)
    doc = dict(
        asset=dict(version="2.0", generator="Vertigo data/3D-cockpits kitfile.py",
                   extras=dict(vertigo_kit=dict(info, version=VERSION))),
        extensionsUsed=["KHR_mesh_quantization", "KHR_materials_unlit"],
        extensionsRequired=["KHR_mesh_quantization"],
        scene=0, scenes=[dict(nodes=list(range(len(nodes))))], nodes=nodes, meshes=meshes,
        materials=[
            dict(name="Cockpit", extensions=dict(KHR_materials_unlit={}),
                 pbrMetallicRoughness=dict(baseColorTexture=dict(index=0), metallicFactor=0.0,
                                           roughnessFactor=1.0)),
            dict(name="Glass", alphaMode="BLEND",
                 pbrMetallicRoughness=dict(baseColorFactor=[0.6, 0.8, 0.85, 0.08], metallicFactor=0.0)),
            dict(name="HUD", alphaMode="BLEND",
                 pbrMetallicRoughness=dict(baseColorFactor=[0.2, 1.0, 0.2, 0.05], metallicFactor=0.0)),
        ],
        samplers=[dict(magFilter=9729, minFilter=9987, wrapS=33071, wrapT=33071)],
        textures=[dict(sampler=0, source=0)],
        images=[dict(name="shade", mimeType="image/jpeg", bufferView=image_view)],
        accessors=accessors, bufferViews=views, buffers=[dict(byteLength=offset)])
    js = _pad(json.dumps(doc, separators=(",", ":")).encode(), b" ")
    body = b"".join(chunks)
    with open(path, "wb") as f:
        f.write(struct.pack("<III", 0x46546C67, 2, 28 + len(js) + len(body)))
        f.write(struct.pack("<II", len(js), 0x4E4F534A) + js)
        f.write(struct.pack("<II", len(body), 0x004E4942) + body)
