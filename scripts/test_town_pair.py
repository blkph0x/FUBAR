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
    parser.add_argument('--automatic-pairing', action='store_true', help='Use default protected local pairing without supplied tokens')
    parser.add_argument('--browser', action='store_true', help='Exercise production browser safe controls before closing test sessions')
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    town, web = port(), port()
    token = secrets.token_hex(32)
    env = dict(os.environ, FUBAR_SDR_TOWN_TOKEN=token,
               SDRTOWN_CONTROL_DLL=str(args.dll.resolve() if args.dll else args.town.resolve().parent/'SdrTownControl.dll'))
    if args.automatic_pairing:
        env.pop('FUBAR_SDR_TOWN_TOKEN',None)
        env.pop('SDR_TOWN_CONTROL_TOKEN',None)
    base = f'http://127.0.0.1:{web}'
    startup = subprocess.STARTUPINFO()
    startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    result={'ok':False,'url':base,'cases':[],'browser':False,'functionalChecksPassed':False,'exitCodes':{}}
    with (args.output/'town.log').open('w') as tlog, (args.output/'fubar.log').open('w') as flog:
        auth=[] if args.automatic_pairing else ['--control-token',token]
        t = subprocess.Popen([str(args.town.resolve()), '--allow-multiple', '--no-remote-diagnostics',
            '--gui-dry-run', '--control-port', str(town), *auth,
            '--control-auth-required', '--gui-exit-after-ms', str(args.duration*1000)], env=env,stdout=tlog, stderr=tlog, startupinfo=startup)
        f = subprocess.Popen([str(args.fubar.resolve()), '--cli', '--web-only', '--port', str(web),
            '--sdr-town-port', str(town), '--allow-radio-control', '--duration', str(args.duration)],
            env=env, stdout=flog, stderr=flog, startupinfo=startup)
        result['processIds']={'town':t.pid,'fubar':f.pid}
        result['controlPort']=town
        print('PAIR_PROCESSES='+json.dumps(result['processIds']),flush=True)
        def request(path, body=None):
            req=urllib.request.Request(base+'/api/sdr-town/'+path,
                data=None if body is None else json.dumps(body).encode(), headers={'Content-Type':'application/json'})
            try:
                with urllib.request.urlopen(req,timeout=15) as r: return r.status,json.load(r)
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
            try:
                urllib.request.urlopen(f'http://127.0.0.1:{town}/v1/status',timeout=5)
                raise AssertionError('Native status accepted an unauthenticated caller')
            except urllib.error.HTTPError as e:
                assert e.code==401,e.code
            assert request('sstv-file?path=C%3A%2FUsers%2Fprivate.png')[0]==404
            assert request('satcom-arm',{'action':'disarm'})[0]==423
            status,lease=request('control-session',{'action':'take','name':'Pair QA'})
            assert status==200 and lease['session']['canControl'],lease
            def post(path,body):
                time.sleep(.26)  # Respect the production per-ticket command budget.
                return request(path,dict(body,admissionTicket=lease['admissionTicket']))
            for body in ({'outputDirectory':'C:/private'}, {'config':{'sstvSaveDir':'C:/private'}}, {'force':True}):
                assert post('sstv-live',body)[0]==400
            cases=result['cases']
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
                        code,view_result=request(workflow+'-sessions?sessionId='+name+'&view='+view)
                        assert code==200 and view_result['ok'],(workflow,view,view_result)
                        assert 'outputDirectory' not in json.dumps(view_result)
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
            initial=request('status')[1]['state']
            assert initial['capabilities']['selectListenDevice'], initial['capabilities']
            radios=initial['devices']
            assert radios, 'Pair fixture needs an enumerated radio or Town stub device'
            key=radios[0]['stableKey']
            # Missing identity must fail without changing settings; recovery is
            # an explicit selection, not an ownership bypass or radio-0 fallback.
            for route,body in [('tune',{'frequencyMHz':146.1,'mode':'NFM','squelchDb':-81}),
                               ('mode',{'mode':'AM'}),
                               ('p25-control',{'frequencyMHz':420.35,'autoFollow':True})]:
                bad=post(route,dict(body,deviceKey='missing-radio-for-pair-test'))[1]
                assert not bad['ok'] and 'available radio' in bad['error'],bad
                unchanged=request('status')[1]['state']
                for field in ('frequencyHz','mode','squelchDb','listenDeviceKey'):
                    assert unchanged[field]==initial[field],(route,field,unchanged[field],initial[field])
            for badkey in (None,42,{},'', 'x'*513):
                assert post('tune',{'frequencyMHz':145.8,'deviceKey':badkey})[0]==400
            code,tuned=post('tune',{'frequencyMHz':145.8,'mode':'NFM','bandwidthKHz':15,'squelchDb':-98,'deviceKey':key})
            assert code==200 and tuned.get('ok'),tuned
            applied=request('status')[1]['state']
            assert applied['squelchDb']==-98 and applied['frequencyHz']==145800000
            assert applied['activeDeviceKey']==key and applied['listenDeviceAvailable'],applied
            assert post('mode',{'mode':'AM','deviceKey':key})[1]['ok']
            assert request('status')[1]['state']['mode']=='AM'
            assert post('tune',{'frequencyMHz':145.8,'mode':'NFM','deviceKey':key})[1]['ok']
            assert not any(d.get('streaming') for d in applied['devices']), 'Dry-run opened RF'
            assert post('tune',{'frequencyMHz':145.8,'squelchDb':-200})[0]==400
            assert post('satcom-control',{'action':'stop'})[1]['ok']
            if args.browser:
                assert post('control-session',{'action':'release'})[1]['ok']
                browser=subprocess.run(['node',str(Path(__file__).with_name('test_web_browser.cjs')),
                    base+'/',str(args.output.resolve()),'pair_a','paired'],env=env,stdout=subprocess.PIPE,
                    stderr=subprocess.STDOUT,text=True,timeout=65)
                (args.output/'browser.log').write_text(browser.stdout,encoding='utf-8')
                print(browser.stdout,flush=True)
                assert browser.returncode==0,('Browser checks failed',browser.returncode)
                result['browser']=True
                status,lease=request('control-session',{'action':'take','name':'Pair QA cleanup'})
                assert status==200 and lease['session']['canControl'],lease
                final_state=request('status')[1]['state']
                assert not any(d.get('streaming') for d in final_state['devices']), 'Browser opened RF'
            for workflow in ('satcom','inmarsat','aircraft'):
                for suffix in ('a','b'):
                    assert post(workflow+'-sessions',{'sessionId':'pair_'+suffix,'action':'close'})[1]['ok']
            result['functionalChecksPassed']=True
            (args.output/'result.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
            print('FUNCTIONAL_CHECKS_PASS; waiting for normal process exits',flush=True)
            # Keep available for browser checks, then let both apps shut down normally.
            f.wait(timeout=args.duration+15); t.wait(timeout=20)
            assert f.returncode==0 and t.returncode==0,(f.returncode,t.returncode)
            result['ok']=True
        finally:
            for process in (f,t):
                if process.poll() is None: process.kill(); process.wait(timeout=15)
            result['exitCodes']={'town':t.returncode,'fubar':f.returncode}
            (args.output/'result.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
            print('EXIT_CODES='+json.dumps(result['exitCodes']),flush=True)
            for name in ('town','fubar'):
                content=(args.output/(name+'.log')).read_text(errors='replace').replace(token,'[test-token]')
                (args.output/(name+'.sanitized.log')).write_text(content,encoding='utf-8')
                print(f'{name} startup/shutdown log tail:\n'+ '\n'.join(content.splitlines()[-40:]),flush=True)
    text=(args.output/'town.log').read_text(errors='replace')
    assert 'Started real Soapy streaming' not in text
    print('PASS: real paired DLL/HTTP, sessions, privacy, ownership refusal and graceful no-RF shutdown')


if __name__=='__main__': main()
