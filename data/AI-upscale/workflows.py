"""The ComfyUI workflows (API format) the pipeline uses, and the model files they load (as named in
ComfyUI's models folders)."""

QWEN_UNET = 'qwen_image_2.1_int8_convrot.safetensors'  # diffusion_models
QWEN_CLIP = 'qwen3vl_8b_int8_convrot.safetensors'  # text_encoders (Qwen3-VL, also reads text)
QWEN_VAE = 'qwen_image_2.1_vae_bf16.safetensors'  # vae


def qwen21(image, prompt, seed, prefix, steps=25):
    """Qwen Image 2.1 drawing the image anew with it as the reference (an edit), at its size (a
    multiple of 32)."""
    return {
        'unet': {'class_type': 'UNETLoader', 'inputs': {'unet_name': QWEN_UNET, 'weight_dtype': 'default'}},
        'cache': {'class_type': 'QwenImage21Cache', 'inputs': {'model': ['unet', 0], 'device': 'auto', 'dtype': 'default'}},
        'clip': {'class_type': 'CLIPLoader', 'inputs': {'clip_name': QWEN_CLIP, 'type': 'qwen_image', 'device': 'default'}},
        'vae': {'class_type': 'VAELoader', 'inputs': {'vae_name': QWEN_VAE}},
        'image': {'class_type': 'LoadImage', 'inputs': {'image': image}},
        'text': {'class_type': 'TextEncodeQwenImage21',
                 'inputs': {'clip': ['clip', 0], 'prompt': prompt, 'negative_prompt': '', 'resolution': 0,
                            'images.image_1': ['image', 0], 'vae': ['vae', 0]}},
        'sampler': {'class_type': 'KSampler',
                    'inputs': {'model': ['cache', 0], 'positive': ['text', 0], 'negative': ['text', 1],
                               'latent_image': ['text', 2], 'seed': seed, 'steps': steps, 'cfg': 1.0,
                               'sampler_name': 'euler', 'scheduler': 'simple', 'denoise': 1.0}},
        'decode': {'class_type': 'VAEDecode', 'inputs': {'samples': ['sampler', 0], 'vae': ['vae', 0]}},
        'save': {'class_type': 'SaveImage', 'inputs': {'images': ['decode', 0], 'filename_prefix': prefix}},
    }


FIND_TEXT = ('Find all legible text in this image: signs, labels, titles, captions, letters and numbers. '
             'Answer only with a JSON array with one object per piece of text, '
             '{"text": "<the text exactly as written>", "bbox_2d": [x1, y1, x2, y2]}, '
             'or [] if there is no text.')

READ_TEXT = ('What text is written in this image? It may be rotated, mirrored or upside down. '
             'Answer only with the text as it reads, nothing else.')


def ask(image, prompt):
    """Qwen3-VL answering a question about the image (boxes come from 0 to 1000)."""
    return {
        'clip': {'class_type': 'CLIPLoader', 'inputs': {'clip_name': QWEN_CLIP, 'type': 'qwen_image', 'device': 'default'}},
        'image': {'class_type': 'LoadImage', 'inputs': {'image': image}},
        'generate': {'class_type': 'TextGenerate',
                     'inputs': {'clip': ['clip', 0], 'prompt': prompt, 'max_length': 1024, 'sampling_mode': 'off',
                                'image': ['image', 0], 'thinking': False}},
        'show': {'class_type': 'PreviewAny', 'inputs': {'source': ['generate', 0]}},
    }
