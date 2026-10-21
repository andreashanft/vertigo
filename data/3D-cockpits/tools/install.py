"""
Copy the textured cockpit models from out/ to where Vertigo looks for them: the folder
3d-cockpits in Vertigo's data directory (or the setting "cockpit/directory", or --dest).
Plain Python 3, no Blender needed.

  python3 tools/install.py [--dest DIR] [--data DIR] [--out DIR]

Copies out/NNN/cockpit_NNN_textured.glb (made by compose_texture.py) as cockpit_NNN.glb. Then
start the game with --cockpit 3d, or press Alt+V in a mission.
"""

import argparse
import os
import shutil
import sys

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, HERE)
import cockpitdata  # noqa: E402


def main():
    parser = argparse.ArgumentParser(description=__doc__.strip().splitlines()[0])
    parser.add_argument("--dest", help="target folder (default: see above)")
    parser.add_argument("--data", help="Vertigo data directory")
    parser.add_argument("--out", default=os.path.join(HERE, "out"), help="the pipeline's output folder")
    args = parser.parse_args()

    dest = args.dest or cockpitdata.settings().get("cockpit/directory")
    if not dest:
        dest = os.path.join(cockpitdata.data_dir(args.data), "3d-cockpits")
    os.makedirs(dest, exist_ok=True)

    installed = 0
    for cid in sorted(os.listdir(args.out)):
        if not os.path.isdir(os.path.join(args.out, cid)):
            continue
        src = os.path.join(args.out, cid, f"cockpit_{cid}_textured.glb")
        if os.path.exists(src):
            target = os.path.join(dest, f"cockpit_{cid}.glb")
            shutil.copyfile(src, target)
            print(f"{os.path.relpath(src, HERE)} -> {target}")
            installed += 1
        else:
            print(f"cockpit {cid}: no textured model yet (run bake_maps.py and compose_texture.py)")
    if not installed:
        sys.exit("nothing installed")


if __name__ == "__main__":
    main()
