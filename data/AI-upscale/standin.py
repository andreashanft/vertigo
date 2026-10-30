"""Stands in for Real-ESRGAN while "aiupscale.py export" lets Vertigo run its own upscaling: keeps each
original Vertigo hands over (KEY.png, decoded as the game uses it) in originals/general or
originals/text, by the model Vertigo asks for, and hands back a plain 4x copy so that the run
completes. Called as: standin.py ORIGINALS -i IN -o OUT -n MODEL -s 4 -f png -m MODELS"""
import os
import shutil
import sys

from PIL import Image


def main():
    originals = sys.argv[1]
    options = dict(zip(sys.argv[2::2], sys.argv[3::2]))
    kind = 'text' if options.get('-n') == 'realesr-animevideov3' else 'general'
    os.makedirs(os.path.join(originals, kind), exist_ok=True)
    for name in os.listdir(options['-i']):
        if not name.endswith('.png'):
            continue
        path = os.path.join(options['-i'], name)
        shutil.copy(path, os.path.join(originals, kind, name))
        image = Image.open(path)
        image.resize((image.width*4, image.height*4), Image.NEAREST).save(os.path.join(options['-o'], name))


if __name__ == '__main__':
    main()
