"""Verify and launch a Windows release in a different directory with spaces.

Uses only Python's standard library. HTTP checks never connect a MAVLink client
or send flight commands. All started processes belong to this check.
"""
import argparse
import hashlib
import http.client
import json
from pathlib import Path
import socket
import subprocess
import sys
import time
import zipfile


def check(archive, destination):
    destination.mkdir(parents=True, exist_ok=False)
    with zipfile.ZipFile(archive) as release:
        assert release.testzip() is None
        for entry in release.infolist():
            assert (destination/entry.filename).resolve().is_relative_to(destination.resolve())
        release.extractall(destination)
    root=next(p for p in destination.iterdir() if p.is_dir())
    subprocess.run([sys.executable,'-B',str(root/'tools/verify_package.py')],check=True,cwd=destination)
    help_result=subprocess.run([str(root/'bin/web_client.exe'),'--help'],check=True,cwd=destination,capture_output=True,text=True)
    assert '--web-port' in help_result.stdout
    with socket.socket() as probe:
        probe.bind(('127.0.0.1',0));port=probe.getsockname()[1]
    stop=destination/'web.stop'
    with (destination/'web.log').open('w',encoding='utf-8') as log:
        child=subprocess.Popen([str(root/'bin/web_client.exe'),'--web-port',str(port),'--stop-file',str(stop)],
                               cwd=destination,stdout=log,stderr=subprocess.STDOUT)
        def get(path):
            conn=http.client.HTTPConnection('127.0.0.1',port,timeout=2)
            conn.request('GET',path);response=conn.getresponse()
            result=(response.status,response.getheader('Content-Type'),response.read())
            conn.close();return result
        try:
            deadline=time.monotonic()+10
            while True:
                assert child.poll() is None,'web exited before startup'
                try:
                    status,_,body=get('/api/bootstrap')
                    if status==200:break
                except OSError:pass
                if time.monotonic()>deadline:raise AssertionError('web startup timeout')
                time.sleep(.05)
            for path in ('/','/app.js','/styles.css','/assets/campus_map.json','/assets/campus_2d.svg'):
                status,kind,body=get(path);assert status==200 and body,path
                if path.endswith('.svg'):assert kind=='image/svg+xml',kind
            state=json.loads(get('/api/state')[2]);assert not state['connected']
            assert not (root/'output/runs').exists(),'web startup must not create a MAVLink session'
        finally:
            stop.touch()
            try:child.wait(timeout=8)
            except subprocess.TimeoutExpired:child.kill();child.wait();raise
        assert child.returncode==0,child.returncode
    result={'archive':str(archive.resolve()),'sha256':hashlib.sha256(archive.read_bytes()).hexdigest(),
            'unpacked_root':str(root),'manifest':'passed','assets_http':'passed','no_automatic_tcp':'passed',
            'shutdown':'passed','browser_visual_review':'not_run'}
    (destination/'result.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
    print(json.dumps(result,indent=2),flush=True)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('archive',type=Path)
    parser.add_argument('--destination',type=Path,required=True)
    args=parser.parse_args()
    check(args.archive.resolve(),args.destination.resolve())
