"""Real DLL transport must preserve a map/message response larger than 64 KiB."""
import argparse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path
import secrets
import socket
import subprocess
import threading
import time
import urllib.request

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--fubar',type=Path,required=True)
p.add_argument('--dll',type=Path,required=True)
a=p.parse_args()
token=secrets.token_hex(16)
payload={'ok':True,'messages':[{'text':'x'*90000}]}
class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        if self.headers.get('Authorization') != 'Bearer '+token:
            self.send_error(401); return
        data=json.dumps(payload).encode()
        self.send_response(200)
        self.send_header('Content-Length',str(len(data)))
        self.end_headers()
        for start in range(0,len(data),1024): self.wfile.write(data[start:start+1024])
    def log_message(self,*args): pass

server=ThreadingHTTPServer(('127.0.0.1',0),Handler)
thread=threading.Thread(target=server.serve_forever,daemon=True)
thread.start()
with socket.socket() as probe:
    probe.bind(('127.0.0.1',0)); port=probe.getsockname()[1]
env=dict(os.environ,FUBAR_SDR_TOWN_TOKEN=token,SDRTOWN_CONTROL_DLL=str(a.dll.resolve()))
startup=subprocess.STARTUPINFO(); startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
proc=subprocess.Popen([str(a.fubar.resolve()),'--cli','--web-only','--port',str(port),
    '--sdr-town-port',str(server.server_port),'--allow-radio-control','--duration','3'],
    env=env,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,startupinfo=startup)
try:
    deadline=time.monotonic()+10
    while True:
        assert proc.poll() is None,'FUBAR exited before request'
        try:
            with urllib.request.urlopen(f'http://127.0.0.1:{port}/api/sdr-town/inmarsat-messages',timeout=5) as r:
                actual=json.load(r)
            break
        except ConnectionError:
            assert time.monotonic()<deadline,'Website unavailable'
            time.sleep(.1)
        except urllib.error.URLError as e:
            if isinstance(e,urllib.error.HTTPError): raise
            assert time.monotonic()<deadline,str(e)
            time.sleep(.1)
    assert actual==payload, str(actual)[:160]
    assert proc.wait(timeout=10)==0
finally:
    if proc.poll() is None: proc.kill(); proc.wait(timeout=10)
    server.shutdown(); server.server_close(); thread.join(timeout=5)
print('PASS: fragmented 90 KiB response preserved through DLL and public boundary')
