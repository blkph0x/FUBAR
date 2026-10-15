"""Real executable + real DLL, loopback-only synthetic native service, no RF."""
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
import urllib.error
import urllib.parse
import urllib.request


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--fubar', type=Path, required=True)
    parser.add_argument('--dll', type=Path, required=True)
    parser.add_argument('--browser', action='store_true')
    parser.add_argument('--output', type=Path, default=Path('build-fubar-msvc/hardening-qa'))
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    token = secrets.token_hex(24)
    scenario = {'name': 'large', 'requests': [], 'posted': []}
    rows = [{'aesId': i + 1, 'label': '\u822a' * 200, 'latDeg': 1, 'lonDeg': 2} for i in range(512)]

    class Handler(BaseHTTPRequestHandler):
        def do_GET(self):
            if self.headers.get('Authorization') != 'Bearer ' + token:
                self.reply({'ok': False, 'error': 'Unauthorized'}, 401)
                return
            if self.path == '/fixture-scenario':
                self.reply({'posted': len(scenario['posted'])})
                return
            scenario['requests'].append(self.path)
            name = scenario['name']
            url = urllib.parse.urlsplit(self.path)
            query = urllib.parse.parse_qs(url.query)
            if name.startswith('browser-'):
                is_map = url.path.endswith('/map-status') or (url.path == '/v1/aircraft/sessions' and 'sessionId' in query and query.get('view') == ['map'])
                name = name.removeprefix('browser-') if is_map else 'ui'
                if name.startswith('post-'): name = 'pages'
            if name == 'large': self.reply({'ok': True, 'messages': [{'text': 'x' * 90000}]})
            elif name == 'oversize': self.reply({'ok': True, 'messages': [{'text': 'PRIVATE_RADIO_CONTENT' * 20000}]})
            elif name == 'truncated': self.reply(b'{"ok":true,"secret":"PRIVATE_RADIO_CONTENT', raw=True)
            elif name == 'invalid-ok': self.reply({'ok': 'true', 'nested': {'ok': True}, 'secret': 'PRIVATE_RADIO_CONTENT'})
            elif name == 'nested-catalog': self.reply({'ok': True, 'sessions': [{'sessionId': 'fixture-a', 'tracks': rows}]})
            elif name == 'error': self.reply({'ok': False, 'error': 'PRIVATE_RADIO_CONTENT' * 10000}, 400)
            elif name == 'token-error': self.reply({'ok': False, 'error': token}, 401)
            elif name == 'auth-error': self.reply({'ok': False, 'error': 'Unauthorized'}, 401)
            elif name == 'expired': self.reply({'ok': False, 'code': 'snapshot_expired', 'error': 'Snapshot expired'}, 409)
            elif name == 'native-error-code': self.reply({'ok': False, 'errorCode': 'incomplete_response', 'error': 'Incomplete response'}, 502)
            elif name == 'legacy-expired': self.reply({'ok': False, 'status': 409, 'error': 'Snapshot expired'}, 409)
            elif name == 'pages':
                offset = int(query.get('offset', ['0'])[0])
                if offset:
                    assert query.get('snapshot') == ['fixed-snapshot']
                self.reply({'ok': True, 'page': {'snapshot': 'fixed-snapshot', 'offset': offset,
                    'nextOffset': offset + 64 if offset + 64 < len(rows) else None, 'total': len(rows)},
                    'aircraft': rows[offset:offset + 64], 'positions': rows[offset:min(offset + 64, 256)],
                    'tracks': rows[offset:offset + 64],
                    **({'tracking': {'mapRfPositions': 256}, 'centerLat': 99, 'token': token} if not offset else {})})
            else:
                sessions = [{'sessionId': 'fixture-a', 'state': 'stopped', 'label': 'Test receiver',
                    'localDecodeEnabled': False, 'networkEnabled': False, 'trackCount': len(rows)}]
                data = {'ok': True}
                if url.path.endswith('/sessions'): data['sessions'] = sessions
                elif url.path.endswith('/status') and url.path == '/v1/status':
                    data['state'] = {'frequencyMHz': 100, 'mode': 'WFM', 'bandwidthHz': 180000,
                        'devices': [], 'capabilities': {'selectListenDevice': True}, 'sstv': {'sessions': sessions}}
                elif url.path == '/v1/inmarsat/status': data['inmarsat'] = {'state': 'stopped', 'messages': 0}
                elif url.path == '/v1/inmarsat/bandplans': data['plans'] = [{'id': 'test', 'name': 'Test', 'channels': [{'freqHz': 1546000000}]}]
                elif url.path.endswith('/messages'): data['messages'] = []
                elif url.path.endswith('/map-status'): data.update(positions=[], aircraft=[], tracks=[])
                self.reply(data)

        def do_POST(self):
            assert self.headers.get('Authorization') == 'Bearer ' + token
            body = json.loads(self.rfile.read(int(self.headers['Content-Length'])))
            if self.path == '/fixture-scenario':
                scenario['name'] = body['name']
                self.reply({'posted': len(scenario['posted'])})
                return
            scenario['posted'].append(body)
            assert 'admissionTicket' not in body and 'clientId' not in body
            if scenario['name'] in ('post-too-large', 'browser-post-too-large'):
                self.reply({'ok': False, 'errorCode': 'response_too_large', 'error': 'Read paged view'}, 413)
                return
            if scenario['name'] == 'browser-post-oversize':
                self.reply({'ok': True, 'tracks': [{'secret': 'PRIVATE_RADIO_CONTENT' * 20000}]})
                return
            if scenario['name'] == 'browser-post-ack':
                self.reply({'ok': True, 'refreshRequired': True})
                return
            if scenario['name'] == 'browser-post-omitted':
                self.reply({'ok': True, 'stateOmitted': True, 'status': 200})
                return
            self.reply({'ok': True})

        def reply(self, value, status=200, raw=False):
            data = value if raw else json.dumps(value, ensure_ascii=False).encode()
            self.send_response(status)
            self.send_header('Content-Type', 'application/json')
            self.send_header('Content-Length', str(len(data)))
            self.end_headers()
            try:
                for start in range(0, len(data), 1024): self.wfile.write(data[start:start + 1024])
            except (BrokenPipeError, ConnectionResetError, ConnectionAbortedError): pass

        def log_message(self, *args): pass

    server = ThreadingHTTPServer(('127.0.0.1', 0), Handler)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    with socket.socket() as probe:
        probe.bind(('127.0.0.1', 0)); port = probe.getsockname()[1]
    base = f'http://127.0.0.1:{port}/'
    env = dict(os.environ, FUBAR_SDR_TOWN_TOKEN=token, SDRTOWN_CONTROL_DLL=str(args.dll.resolve()))
    startup = subprocess.STARTUPINFO(); startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    duration = '75' if args.browser else '15'
    with (args.output / 'fubar.log').open('w') as log:
        proc = subprocess.Popen([str(args.fubar.resolve()), '--cli', '--web-only', '--port', str(port),
            '--sdr-town-port', str(server.server_port), '--allow-radio-control', '--duration', duration],
            env=env, stdout=log, stderr=log, startupinfo=startup)
        result = {'ok': False, 'browser': False, 'processId': proc.pid, 'url': base}

        def request(path, body=None):
            req = urllib.request.Request(base + 'api/sdr-town/' + path,
                data=None if body is None else json.dumps(body).encode(), headers={'Content-Type': 'application/json'})
            try: response = urllib.request.urlopen(req, timeout=15)
            except urllib.error.HTTPError as error: response = error
            with response:
                raw = response.read(); status = response.status
            return status, json.loads(raw), raw

        try:
            deadline = time.monotonic() + 10
            while True:
                try:
                    code, data, _ = request('inmarsat-messages'); break
                except urllib.error.URLError:
                    assert time.monotonic() < deadline and proc.poll() is None
                    time.sleep(.1)
            assert code == 200 and len(data['messages'][0]['text']) == 90000
            for name in ['oversize', 'truncated', 'invalid-ok', 'error', 'token-error', 'auth-error', 'expired', 'native-error-code', 'legacy-expired']:
                scenario['name'] = name
                code, data, raw = request('inmarsat-map-status')
                assert code == 502 and not data['ok'] and len(raw) < 512, (name, code, len(raw))
                assert b'PRIVATE_RADIO_CONTENT' not in raw and token.encode() not in raw
                if name == 'expired': assert data['code'] == 'snapshot_expired'
                if name == 'native-error-code': assert data['errorCode'] == 'incomplete_response' and data['code'] == 'incomplete_response'
            scenario['name'] = 'pages'
            for prefix in ['inmarsat-map-status?', 'inmarsat-sessions?sessionId=fixture-a&view=map&',
                           'aircraft-sessions?sessionId=fixture-a&view=map&']:
                combined = []
                offset = 0
                while True:
                    code, data, raw = request(prefix + 'offset=' + str(offset) + ('&snapshot=fixed-snapshot' if offset else ''))
                    assert code == 200 and len(raw) < 128 * 1024
                    assert 'centerLat' not in data and 'token' not in data
                    combined.extend(data['aircraft'])
                    offset = data['page']['nextOffset']
                    if offset is None: break
                assert combined == rows
            for suffix in ['offset=1', 'offset=-1', 'offset=4097', 'offset=0&offset=1',
                           'offset=64&snapshot=..%2Fprivate', 'offset=64&snapshot=' + 'a' * 97]:
                assert request('inmarsat-map-status?' + suffix)[0] == 400, suffix
            scenario['name'] = 'nested-catalog'
            code, data, raw = request('aircraft-sessions')
            assert code == 502 and not data['ok'] and len(raw) < 512
            scenario['name'] = 'ui'
            code, data, raw = request('aircraft-sessions')
            assert code == 200 and data['sessions'][0]['trackCount'] == 512 and len(raw) < 1024
            assert 'tracks' not in data['sessions'][0]
            code, lease, _ = request('control-session', {'action': 'take', 'clientId': 'caller'})
            ticket = lease['admissionTicket']
            assert code == 200 and lease['session']['canControl']
            assert request('satcom-control', {'action': 'stop', 'clientId': ticket})[0] == 423
            assert request('satcom-control', {'action': 'stop', 'admissionTicket': 'a' * 32})[0] == 423
            assert request('satcom-control', {'action': 'stop', 'admissionTicket': ticket})[0] == 200
            scenario['name'] = 'post-too-large'
            code, failure, _ = request('aircraft-map-control', {'action': 'refresh', 'admissionTicket': ticket})
            assert code == 400 and failure['errorCode'] == 'response_too_large'
            scenario['name'] = 'ui'
            assert request('inmarsat-control', {'admissionTicket': ticket, 'outputDirectory': 'C:\\private'})[0] == 400
            waiting = []
            for i in range(96):
                code, data, raw = request('control-session', {'action': 'take', 'clientId': 'id-' + str(i)})
                assert len(raw) < 1800 and ticket.encode() not in raw
                assert len(data['session']['queue']) <= 8 and data['session']['queueLength'] <= 32
                if code == 200: waiting.append(data['admissionTicket'])
                else: assert code == 429
            assert len(waiting) == 32
            assert request('control-session', {'action': 'take', 'admissionTicket': waiting[0]})[1]['session']['queuePosition'] == 1
            for _ in range(2): assert request('control-session', {'action': 'extend', 'admissionTicket': ticket})[1]['ok']
            assert not request('control-session', {'action': 'extend', 'admissionTicket': ticket})[1]['ok']
            assert request('control-session', {'action': 'release', 'admissionTicket': ticket})[1]['ok']
            assert request('control-session', {'action': 'status', 'admissionTicket': waiting[0]})[1]['session']['canControl']
            for queued in waiting: request('control-session', {'action': 'release', 'admissionTicket': queued})
            time.sleep(1.1)
            burst_ticket = request('control-session', {'action': 'take'})[1]['admissionTicket']
            results = [request('satcom-control', {'action': 'stop', 'admissionTicket': burst_ticket})[0] for _ in range(24)]
            assert 200 in results and 429 in results, results
            time.sleep(.3)
            assert request('control-session', {'action': 'release', 'admissionTicket': burst_ticket})[1]['ok']
            assert scenario['posted']
            if args.browser:
                time.sleep(1.1)
                browser = subprocess.run(['node', str(Path(__file__).with_name('test_web_browser.cjs')), base,
                    str(args.output.resolve()), 'fixture-a', 'synthetic', f'http://127.0.0.1:{server.server_port}/'],
                    env=env, timeout=60, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
                (args.output / 'browser.log').write_text(browser.stdout, encoding='utf-8')
                print(browser.stdout, flush=True)
                assert browser.returncode == 0, browser.stdout
                result['browser'] = True
            assert proc.wait(timeout=int(duration) + 10) == 0
            result['ok'] = True
        finally:
            if proc.poll() is None: proc.kill(); proc.wait(timeout=10)
            server.shutdown(); server.server_close(); thread.join(timeout=5)
            result['exitCode'] = proc.returncode
            (args.output / 'result.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
            content = (args.output / 'fubar.log').read_text(errors='replace').replace(token, '[test-token]')
            (args.output / 'fubar.sanitized.log').write_text(content, encoding='utf-8')
    print('PASS: real loopback DLL, 512 Unicode rows, fragmented/oversize/truncated/errors, paging, 96 IDs, leases, auth and privacy')


if __name__ == '__main__': main()
