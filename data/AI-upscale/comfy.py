"""Talks to ComfyUI's HTTP API: uploads images, queues workflows (API format), fetches the results.
The server is COMFYUI_URL, by default ComfyUI on this machine."""
import json
import os
import subprocess
import time
import urllib.parse
import uuid

SERVER = os.environ.get('COMFYUI_URL', 'http://127.0.0.1:8188').rstrip('/')


class ComfyError(Exception):
    pass


class Comfy:
    def __init__(self, server=SERVER):
        self.server = server
        self.client_id = str(uuid.uuid4())

    def _request(self, path, data=None, headers=None, timeout=60, form=None):
        # through curl: macOS lets it reach other machines on the local network, where it may stop
        # Python's own sockets
        command = ['curl', '-sS', '--max-time', str(timeout), '-w', '\n%{http_code}']
        for name, value in (headers or {}).items():
            command += ['-H', f'{name}: {value}']
        if data is not None:
            command += ['--data-binary', '@-']
        for field, value in (form or {}).items():
            command += ['-F', f'{field}={value}']
        result = subprocess.run(command + [self.server + path], input=data, capture_output=True)
        if result.returncode != 0:
            raise ComfyError(f'{self.server}{path}: curl {result.returncode} {result.stderr.decode(errors="replace")}')
        body, _, code = result.stdout.rpartition(b'\n')
        if not code.startswith(b'2'):
            raise ComfyError(f'{path}: {code.decode()} {body.decode(errors="replace")[:2000]}')
        return body

    def _json(self, path, payload=None, timeout=60):
        data = json.dumps(payload).encode() if payload is not None else None
        headers = {'Content-Type': 'application/json'} if data else {}
        return json.loads(self._request(path, data, headers, timeout))

    def check(self):
        """The server's system information, or an error if it can't be reached."""
        return self._json('/system_stats', timeout=10)

    def upload(self, path, subfolder='vertigo'):
        """Uploads an image to ComfyUI's input folder; returns the name LoadImage takes."""
        result = json.loads(self._request('/upload/image', timeout=120, form={
            'image': f'@{path};type=image/png', 'subfolder': subfolder, 'overwrite': 'true', 'type': 'input'}))
        return f'{result["subfolder"]}/{result["name"]}' if result.get('subfolder') else result['name']

    def queue(self, workflow):
        result = self._json('/prompt', {'prompt': workflow, 'client_id': self.client_id})
        if result.get('node_errors'):
            raise ComfyError(json.dumps(result['node_errors'])[:2000])
        return result['prompt_id']

    def wait(self, prompt_id, timeout=3600):
        """Waits for a queued workflow; returns its history entry."""
        start = time.time()
        while time.time() - start < timeout:
            history = self._json(f'/history/{prompt_id}')
            if prompt_id in history:
                entry = history[prompt_id]
                status = entry.get('status', {})
                if status.get('status_str') == 'error':
                    messages = [m for m in status.get('messages', []) if m[0] == 'execution_error']
                    raise ComfyError(json.dumps(messages)[:3000])
                if status.get('completed', True):
                    return entry
            time.sleep(1)
        raise ComfyError(f'{prompt_id} did not finish within {timeout} s')

    def run(self, workflow, path, timeout=3600):
        """Queues a workflow with one SaveImage, waits, downloads its image to path."""
        entry = self.wait(self.queue(workflow), timeout)
        for output in entry.get('outputs', {}).values():
            for image in output.get('images', []):
                query = urllib.parse.urlencode({'filename': image['filename'], 'subfolder': image.get('subfolder', ''),
                                                'type': image.get('type', 'output')})
                with open(path, 'wb') as file:
                    file.write(self._request(f'/view?{query}', timeout=120))
                return
        raise ComfyError('the workflow saved no image')

    def text(self, workflow, timeout=600):
        """Queues a workflow ending in PreviewAny; returns the text it shows."""
        entry = self.wait(self.queue(workflow), timeout)
        for output in entry.get('outputs', {}).values():
            if 'text' in output:
                return ''.join(output['text'])
        raise ComfyError('the workflow showed no text: ' + json.dumps(entry.get('outputs'))[:500])
