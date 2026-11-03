"""The prompts, by kind of image."""

STYLE = ('It is a pre-rendered 3D computer graphics image from a 1996 underwater science-fiction game, '
         'set in deep-sea stations and submarines on the dark ocean floor.')

KEEP = ('Keep the composition, every object with its position, shape and size, the lighting and the colours '
        'exactly as they are. Do not add, remove or move anything.')

PROMPTS = {
    'scene': ('Upscale and restore this image to high resolution. ' + STYLE + ' ' + KEEP +
              ' Add only fine, crisp detail and clean edges, and remove noise, dithering and compression artefacts.'),
    # richer colours and more luminous lights, still in the dark of the deep sea (config/vibrant.json)
    'vibrant': ('Upscale and restore this image to high resolution, with richer colours and more luminous lights, like '
                'a modern high-dynamic-range render. ' + STYLE + ' It is so deep that no daylight reaches it: the water '
                'is dark, with no light from above, no sun rays and no light shafts. All light comes from the stations '
                'themselves: their lamps, windows and searchlight beams, which glow in the dark water and light the hulls '
                'and the sea floor near them. Keep the composition and every object, sign and letter with its position, '
                'shape and size exactly as they are. Do not add, remove, move or enlarge anything. Keep the dark '
                'background as it is: do not add buildings, domes, towers, vehicles or submarines, and do not repeat or '
                'echo the outlines of the buildings in it; far objects stay as faint as they are. Make the water a deep, '
                'dark teal and blue, the metal hull plates crisp with clear highlights where the lamps light them, and '
                'the windows, lamps and beams brightly glowing with a gentle bloom. Add fine, crisp detail and clean '
                'edges, and remove noise, dithering and compression artefacts.'),
    # four pictures of a depot movie, an item turning, on a sheet
    'movie': ('Upscale and restore this image to high resolution. It is a sheet of four pre-rendered 3D computer '
              'graphics pictures from a 1996 underwater science-fiction game, in a 2x2 grid: the same piece of submarine '
              'equipment (a torpedo, a gun, a module or the like) turning step by step, on a plain black background. '
              'Draw it the same way in all four pictures, with the same parts, markings and surface detail. ' + KEEP +
              ' Add only fine, crisp detail and clean edges, and remove noise, dithering and compression artefacts.'),
    'texture': ('Upscale and restore this texture atlas to high resolution. It is the texture map of 3D models of '
                'submarines from a 1996 underwater science-fiction game: hull plates, panels, pipes, lights and '
                'markings, laid out side by side. Keep every part, edge and seam exactly where it is, and the '
                'colours and black background as they are. Letters and markings stay exactly as they are, also '
                'where they are mirrored or upside down. Add only fine, crisp surface detail and clean edges, and '
                'remove noise, dithering and compression artefacts.'),
}


def with_text(prompt, texts):
    """The prompt naming the image's text and what else must come out right: strings, or items with
    "text" (and maybe a "note" on where or how it is written), or with "hint" (what a part shows,
    e.g. "a skull emblem, not a face")."""
    if not texts:
        return prompt
    items = [{'text': t} if isinstance(t, str) else t for t in texts]
    written = [t for t in items if 'text' in t]
    hints = [t['hint'] for t in items if 'hint' in t]
    if written:
        quoted = '; '.join(f'"{t["text"]}"' + (f' ({t["note"]})' if t.get('note') else '') for t in written)
        prompt += (f' The image contains this text, which must stay exactly as written, in the same place, '
                   f'font, colour and slant, with sharp, clean letters: {quoted}.')
    if hints:
        prompt += ' It shows ' + '; '.join(hints) + ', which must stay what it is.'
    return prompt
