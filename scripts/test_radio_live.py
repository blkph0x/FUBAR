"""Explicit, bounded physical-RX smoke through FUBAR -> DLL -> Town GUI.

Requires a named free radio. No transmitter, diagnostics upload or IQ recording.
Persists the operator-requested listening selection, like a normal web tune.
"""
import argparse
import json
import os
from pathlib import Path
import subprocess
import time
import urllib.request
from test_town_pair import port


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--town',type=Path,required=True)
    p.add_argument('--fubar',type=Path,required=True)
    p.add_argument('--device-key',required=True)
    p.add_argument('--frequency-mhz',type=float,required=True)
    p.add_argument('--output',type=Path,default=Path('build/live-radio-test'))
    args=p.parse_args();args.output.mkdir(parents=True,exist_ok=True)
    town,web=port(),port()
    env=dict(os.environ,SDRTOWN_CONTROL_DLL=str(args.town.resolve().parent/'SdrTownControl.dll'))
    env.pop('FUBAR_SDR_TOWN_TOKEN',None);env.pop('SDR_TOWN_CONTROL_TOKEN',None)
    startup=subprocess.STARTUPINFO();startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    with (args.output/'town.log').open('w') as tl,(args.output/'fubar.log').open('w') as fl:
        t=subprocess.Popen([str(args.town.resolve()),'--no-remote-diagnostics','--control-port',str(town),
            '--gui-exit-after-ms','35000'],env=env,stdout=tl,stderr=tl,startupinfo=startup)
        f=subprocess.Popen([str(args.fubar.resolve()),'--cli','--web-only','--port',str(web),
            '--sdr-town-port',str(town),'--allow-radio-control','--duration','35'],env=env,stdout=fl,stderr=fl,startupinfo=startup)
        ticket=''
        def request(path,body=None):
            req=urllib.request.Request(f'http://127.0.0.1:{web}/api/sdr-town/'+path,
                data=None if body is None else json.dumps(dict(body,admissionTicket=ticket)).encode(),
                headers={'Content-Type':'application/json'})
            with urllib.request.urlopen(req,timeout=12) as r:return json.load(r)
        try:
            deadline=time.monotonic()+20
            while True:
                try:
                    state=request('status')
                    if state.get('ok'):break
                except OSError:pass
                assert time.monotonic()<deadline,'Pair did not connect'
                time.sleep(.2)
            assert any(d['stableKey']==args.device_key for d in state['state']['devices']),'Requested radio unavailable'
            lease=request('control-session',{'action':'take','name':'Local radio test'})
            assert lease['session']['canControl']
            ticket=lease['admissionTicket']
            tuned=request('tune',{'deviceKey':args.device_key,'frequencyMHz':args.frequency_mhz,
                'mode':'WFM','bandwidthKHz':180,'lpfKHz':15,'audioLpfEnabled':True})
            assert tuned['ok'],tuned
            deadline=time.monotonic()+15
            while True:
                state=request('status')['state']
                runtime=next(d['runtime'] for d in state['workflowDevices'] if d['deviceKey']==args.device_key)
                if runtime=='live hardware':break
                assert time.monotonic()<deadline,('No hardware readiness',runtime)
                time.sleep(.3)
            assert state['activeDeviceKey']==args.device_key
            assert state['frequencyHz']==args.frequency_mhz*1e6
            time.sleep(4)
            again=request('tune',{'deviceKey':args.device_key,'frequencyMHz':args.frequency_mhz+.1,'mode':'WFM'})
            assert again['ok'],again
            time.sleep(2)
            final=request('tune',{'deviceKey':args.device_key,'frequencyMHz':args.frequency_mhz,'mode':'WFM'})
            assert final['ok'],final
            (args.output/'result.json').write_text(json.dumps({'ok':True,'runtime':runtime,
                'frequencyMHz':args.frequency_mhz,'secondTuneMHz':args.frequency_mhz+.1},indent=2))
            t.wait(timeout=45);f.wait(timeout=10)
            assert t.returncode==0 and f.returncode==0
        finally:
            for proc in (f,t):
                if proc.poll() is None:proc.kill();proc.wait()
    log=(args.output/'town.log').read_text(errors='replace')
    assert 'Started real Soapy streaming' in log,'No actual hardware stream in log'
    print('PASS: keyed web tune, real hardware readiness, repeated retune and graceful shutdown')


if __name__=='__main__':main()
