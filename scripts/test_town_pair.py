"""Exercise the real FUBAR -> DLL -> authenticated Town GUI chain without RF."""
import argparse
import json
import os
from pathlib import Path
import secrets
import socket
import subprocess
import time
import urllib.request
import urllib.error


def port():
    with socket.socket() as s:
        s.bind(('127.0.0.1', 0))
        return s.getsockname()[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--town', type=Path, required=True)
    parser.add_argument('--fubar', type=Path, required=True)
    parser.add_argument('--output', type=Path, default=Path('build/pair-test'))
    parser.add_argument('--duration', type=int, default=45)
    parser.add_argument('--dll', type=Path, help='Packaged DLL to verify (default: beside Town)')
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    town, web = port(), port()
    token = secrets.token_hex(32)
    env = dict(os.environ, FUBAR_SDR_TOWN_TOKEN=token,
               SDRTOWN_CONTROL_DLL=str(args.dll.resolve() if args.dll else args.town.resolve().parent/'SdrTownControl.dll'))
    base = f'http://127.0.0.1:{web}'
    startup = subprocess.STARTUPINFO()
    startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    with (args.output/'town.log').open('w') as tlog, (args.output/'fubar.log').open('w') as flog:
        t = subprocess.Popen([str(args.town.resolve()), '--allow-multiple', '--no-remote-diagnostics',
            '--gui-dry-run', '--control-port', str(town), '--control-token', token,
            '--control-auth-required', '--gui-exit-after-ms', str(args.duration*1000)], stdout=tlog, stderr=tlog, startupinfo=startup)
        f = subprocess.Popen([str(args.fubar.resolve()), '--cli', '--web-only', '--port', str(web),
            '--sdr-town-port', str(town), '--allow-radio-control', '--duration', str(args.duration)],
            env=env, stdout=flog, stderr=flog, startupinfo=startup)
        def request(path, body=None):
            req=urllib.request.Request(base+'/api/sdr-town/'+path,
                data=None if body is None else json.dumps(body).encode(), headers={'Content-Type':'application/json'})
            try:
                with urllib.request.urlopen(req,timeout=8) as r: return r.status,json.load(r)
            except urllib.error.HTTPError as e:
                data=e.read().decode()
                return e.code,json.loads(data) if data.startswith('{') else data
        try:
            deadline=time.monotonic()+25
            while True:
                try:
                    status,state=request('status')
                    if status==200 and state.get('ok'): break
                except (OSError,ValueError): pass
                assert f.poll() is None and t.poll() is None, f'Paired process exited: FUBAR={f.poll()}, Town={t.poll()}'
                assert time.monotonic()<deadline, 'Pair did not connect'
                time.sleep(.2)
            print('PAIR_URL='+base, flush=True)
            assert request('sstv-file?path=C%3A%2FUsers%2Fprivate.png')[0]==404
            assert request('satcom-arm',{'action':'disarm'})[0]==423
            status,lease=request('control-session',{'action':'take','clientId':'qa_owner','name':'Pair QA'})
            assert status==200 and lease['session']['canControl'],lease
            def post(path,body):
                return request(path,dict(body,clientId='qa_owner'))
            for body in ({'outputDirectory':'C:/private'}, {'config':{'sstvSaveDir':'C:/private'}}, {'force':True}):
                assert post('sstv-live',body)[0]==400
            cases=[]
            for workflow in ('satcom','inmarsat','aircraft'):
                for suffix in ('a','b'):
                    name='pair_'+suffix
                    code,opened=post(workflow+'-sessions',{'sessionId':name,'action':'open'})
                    if code != 200 or not opened['ok']:
                        # Diagnose an ambiguous bridge result without repeating a command.
                        started=time.monotonic()
                        probe=urllib.request.Request(f'http://127.0.0.1:{town}/v1/{workflow}/sessions',
                            headers={'Authorization':'Bearer '+token})
                        try:
                            with urllib.request.urlopen(probe,timeout=15) as r:
                                direct=json.load(r)
                            print('NATIVE_AFTER_FAILURE',workflow,'elapsed',time.monotonic()-started,
                                  'sessions',[s.get('sessionId') for s in direct.get('sessions',[])],flush=True)
                        except Exception as e:
                            print('NATIVE_AFTER_FAILURE',workflow,type(e).__name__,str(e).replace(token,'[test-token]'),flush=True)
                    assert code==200 and opened['ok'],(workflow,opened)
                for suffix in ('a','b'):
                    name='pair_'+suffix
                    views=('map','messages') if workflow=='inmarsat' else ('status',)
                    for view in views:
                        code,result=request(workflow+'-sessions?sessionId='+name+'&view='+view)
                        assert code==200 and result['ok'],(workflow,view,result)
                        assert 'outputDirectory' not in json.dumps(result)
                        cases.append(workflow+'/'+name+'/'+view)
                assert not request(workflow+'-sessions?sessionId=missing&view=status')[1]['ok']
            sat=request('satcom-status')[1]['satcom']
            links=[d for s in sat['catalogue']['satellites'] for d in s['downlinks']]
            assert any(d['role']=='sstv' and d['freqHz']>400e6 for d in links),links
            assert not post('satcom-arm',{'action':'arm','satId':'iss','downlinkId':'iss-sstv'})[1]['ok'], 'dry-run must not arm RF'
            for name in ('pair_a','pair_b'):
                assert post('sstv-sessions',{'sessionId':name})[1]['ok']
            modes=request('status')[1]['state']['sstv']['modes']
            assert len([m for m in modes if m['live']])==50
            assert request('inmarsat-sessions?sessionId=..%2Fescape&view=map')[0]==400
            assert not post('inmarsat-sessions',{'sessionId':'pair_a','action':'configure','config':{'baud':99}})[1]['ok']
            assert not post('inmarsat-control',{'action':'set','baud':99})[1]['ok']
            code,tuned=post('tune',{'frequencyMHz':145.8,'mode':'NFM','bandwidthKHz':15,'squelchDb':-98})
            # Dry-run applies monitor settings but cannot start a physical radio.
            assert not tuned['ok'],tuned
            assert request('status')[1]['state']['squelchDb']==-98
            assert post('tune',{'frequencyMHz':145.8,'squelchDb':-200})[0]==400
            assert post('satcom-control',{'action':'stop'})[1]['ok']
            for workflow in ('satcom','inmarsat','aircraft'):
                for suffix in ('a','b'):
                    assert post(workflow+'-sessions',{'sessionId':'pair_'+suffix,'action':'close'})[1]['ok']
            (args.output/'result.json').write_text(json.dumps({'ok':True,'url':base,'cases':cases},indent=2))
            # Keep available for browser checks, then let both apps shut down normally.
            f.wait(timeout=args.duration+15); t.wait(timeout=20)
            assert f.returncode==0 and t.returncode==0,(f.returncode,t.returncode)
        finally:
            for process in (f,t):
                if process.poll() is None: process.kill(); process.wait(timeout=15)
            for name in ('town','fubar'):
                content=(args.output/(name+'.log')).read_text(errors='replace').replace(token,'[test-token]')
                (args.output/(name+'.sanitized.log')).write_text(content,encoding='utf-8')
                print(f'{name} startup/shutdown log tail:\n'+ '\n'.join(content.splitlines()[-40:]),flush=True)
    text=(args.output/'town.log').read_text(errors='replace')
    assert 'Started real Soapy streaming' not in text
    print('PASS: real paired DLL/HTTP, sessions, privacy, ownership refusal and graceful no-RF shutdown')


if __name__=='__main__': main()
