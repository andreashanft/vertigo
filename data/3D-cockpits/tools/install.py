"""
Copy the textured cockpit models from out/ to where Vertigo looks for them: the folder
3d-cockpits in Vertigo's data directory (or the setting "cockpit/directory", or --dest).
Plain Python 3, no Blender needed.

  python3 tools/install.py [--variant _x4plus8k] [--dest DIR] [--data DIR] [--out DIR]

For each cockpit the first existing file wins: cockpit_NNN_textured{variant}.glb, then
cockpit_NNN_textured_x4plus8k.glb, then cockpit_NNN_textured.glb. Then start the game with
--cockpit 3d, or press Alt+V in a mission.
"""

import argparse
import os
import plistlib
import shutil
import sys

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PLIST = os.path.expanduser("~/Library/Preferences/com.vertigo.Vertigo.plist")
CONF = os.path.expanduser("~/.config/vertigo/Vertigo.conf")


def settings():
    """Vertigo's settings (QSettings: a plist on macOS, an ini file elsewhere)."""
    if os.path.exists(PLIST):
        with open(PLIST, "rb") as f:
            return plistlib.load(f)
    values, section = {}, ""
    if os.path.exists(CONF):
        for line in open(CONF, encoding="utf-8"):
            line = line.strip()
            if line.startswith("["):
                section = line.strip("[]")
                section = "" if section == "General" else section + "."
            elif "=" in line:
                key, value = line.split("=", 1)
                values[section + key.strip()] = value.strip()
    return values


def main():
    parser = argparse.ArgumentParser(description=__doc__.strip().splitlines()[0])
    parser.add_argument("--variant", default="_x4plus8k", help="preferred texture variant (suffix)")
    parser.add_argument("--dest", help="target folder (default: see above)")
    parser.add_argument("--data", help="Vertigo data directory")
    parser.add_argument("--out", default=os.path.join(HERE, "out"), help="the pipeline's output folder")
    args = parser.parse_args()

    s = settings()
    dest = args.dest or s.get("cockpit.directory")
    if not dest:
        data = args.data or os.environ.get("VERTIGO_DATA") or s.get("datadir")
        if not data:
            sys.exit("Vertigo data directory unknown: pass --data DIR or --dest DIR")
        dest = os.path.join(data, "3d-cockpits")
    os.makedirs(dest, exist_ok=True)

    installed = 0
    for cid in sorted(os.listdir(args.out)):
        folder = os.path.join(args.out, cid)
        if not os.path.isdir(folder):
            continue
        for variant in dict.fromkeys((args.variant, "_x4plus8k", "")):
            src = os.path.join(folder, f"cockpit_{cid}_textured{variant}.glb")
            if os.path.exists(src):
                target = os.path.join(dest, f"cockpit_{cid}.glb")
                shutil.copyfile(src, target)
                print(f"{os.path.relpath(src, HERE)} -> {target}")
                installed += 1
                break
        else:
            print(f"cockpit {cid}: no textured model yet (run project_textures.py)")
    if not installed:
        sys.exit("nothing installed")


if __name__ == "__main__":
    main()
