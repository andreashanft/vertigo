# Serves the glyph editor and stores its edits next to it (edited.json, vertigo-mono-edit.svg, vertigo-mono-styled.svg).
import http.server, json, os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
class H(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *a, **k): super().__init__(*a, directory=HERE, **k)
    def end_headers(self):
        self.send_header('Cache-Control', 'no-store'); super().end_headers()
    def do_POST(self):
        if self.path != '/save': self.send_error(404); return
        body = json.loads(self.rfile.read(int(self.headers['Content-Length'])))
        json.dump(body['data'], open(os.path.join(HERE, 'edited.json'), 'w'), ensure_ascii=False)
        open(os.path.join(HERE, 'vertigo-mono-edit.svg'), 'w', encoding='utf-8').write(body['svg'])
        if 'styled' in body: open(os.path.join(HERE, 'vertigo-mono-styled.svg'), 'w', encoding='utf-8').write(body['styled'])
        self.send_response(204); self.end_headers()
    def log_message(self, *a): pass
http.server.ThreadingHTTPServer(('127.0.0.1', int(sys.argv[1]) if len(sys.argv) > 1 else 8765), H).serve_forever()
