"""Where things are: this folder, its work folder, the game data and Vertigo's own folder."""
import os
import re
import sys

FOLDER = os.path.dirname(os.path.abspath(__file__))
CONFIG = os.path.join(FOLDER, 'config')
# everything the pipeline makes (ignored by git)
WORK = os.environ.get('VERTIGO_AI_WORK') or os.path.join(FOLDER, 'work')
ORIGINALS = os.path.join(WORK, 'originals')
ESRGAN_SET = os.path.join(WORK, 'esrgan')
OUT = os.path.join(WORK, 'out')
TILES = os.path.join(WORK, 'tiles')
SETS = os.path.join(WORK, 'sets')

TEXTS = os.path.join(CONFIG, 'texts.json')
REGIONS = os.path.join(CONFIG, 'esrgan-regions.json')
STREAKS = os.path.join(CONFIG, 'streaks.json')
NAMES = os.path.join(CONFIG, 'names.json')

UPSCALED = 'Game Data Upscaled'
UPSCALED_ASIDE = 'Game Data Upscaled (Real-ESRGAN)'


def app_dir(home=None):
    """Vertigo's folder for saves, settings and the upscaled set, as the game finds it: in the given
    home, or the user's."""
    if sys.platform == 'darwin':
        home = home or os.environ.get('CFFIXED_USER_HOME') or os.path.expanduser('~')
        return os.path.join(home, 'Library', 'Application Support', 'Vertigo', 'Vertigo')
    if sys.platform == 'win32':
        return os.path.join(os.environ.get('LOCALAPPDATA', ''), 'Vertigo', 'Vertigo')
    if home:
        return os.path.join(home, '.local', 'share', 'Vertigo', 'Vertigo')
    data = os.environ.get('XDG_DATA_HOME', '')
    if not data.startswith('/'):
        data = os.path.join(os.path.expanduser('~'), '.local', 'share')
    return os.path.join(data, 'Vertigo', 'Vertigo')


def data_dir(given=None):
    """The game data: given, or VERTIGO_DATA, or Vertigo's datadir setting."""
    if given:
        return given
    if os.environ.get('VERTIGO_DATA'):
        return os.environ['VERTIGO_DATA']
    settings = os.path.join(app_dir(), 'settings.ini')
    if os.path.exists(settings):
        for line in open(settings, encoding='utf-8', errors='replace'):
            match = re.match(r'\s*datadir\s*=\s*(.+?)\s*$', line)
            if match:
                return match.group(1)
    raise SystemExit('Where is the game data? Pass --data DIR or set VERTIGO_DATA.')


def original(key):
    for kind in ('general', 'text'):
        path = os.path.join(ORIGINALS, kind, key + '.png')
        if os.path.exists(path):
            return path
    raise SystemExit(f'No original {key} (run "aiupscale.py export" first).')


def esrgan(key):
    for extension in ('.jpg', '.png'):
        path = os.path.join(ESRGAN_SET, key + extension)
        if os.path.exists(path):
            return path
    raise SystemExit(f'No Real-ESRGAN copy of {key} (run "aiupscale.py prepare" first).')


def keys():
    """All exported originals."""
    found = []
    for kind in ('general', 'text'):
        folder = os.path.join(ORIGINALS, kind)
        if os.path.isdir(folder):
            found += [name[:-4] for name in os.listdir(folder) if name.endswith('.png')]
    return sorted(found)
