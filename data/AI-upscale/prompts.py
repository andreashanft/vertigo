"""The prompts, by kind of image."""

STYLE = ('It is a pre-rendered 3D computer graphics image from a 1996 underwater science-fiction game, '
         'set in deep-sea stations and submarines on the dark ocean floor.')

KEEP = ('Keep the composition, every object with its position, shape and size, the lighting and the colours '
        'exactly as they are. Do not add, remove or move anything.')

PROMPTS = {
    'scene': ('Upscale and restore this image to high resolution. ' + STYLE + ' ' + KEEP +
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
