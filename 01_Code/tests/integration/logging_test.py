"""Production executables must never write runtime logs, including on shutdown."""
import argparse
import json
from pathlib import Path
import subprocess
from web_mission_test import ROOT,Run


def session(directory):
    r=Run(directory)
    console=None
    try:
        console=subprocess.Popen([str(ROOT/'bin/c_client.exe'),'--config',str(r.path),
                                  '--role','observer','--run-id','must_not_create_logs'],
                                 stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
        r.connect()
        assert r.state()['logging_enabled']==False
        assert bool(r.state()['log_directory'])==False
        assert r.api('/api/missions/upload',r.draft())[0]==202
        r.wait(lambda:r.state()['mission'].get('verified'))
        assert r.api('/api/commands',{'command':'ARM','request_id':'arm_no_log'})[0]==202
        r.wait(lambda:r.state()['telemetry']['heartbeat']['armed'])
        assert r.api('/api/disconnect',{})[0]==200
    finally:
        r.close()
        if console:
            try:console.communicate(timeout=8)
            except subprocess.TimeoutExpired:console.kill();console.communicate();raise
    assert console.returncode==0
    files=list((directory/'runs').rglob('*')) if (directory/'runs').exists() else []
    assert not (directory/'runs').exists(),files
    return {'file_logging':False,'mission_and_command':'passed','console_observer_exit':console.returncode}


def main():
    parser=argparse.ArgumentParser();parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=True)
    for exe in ('gateway','web_client','c_client'):
        command=[str(ROOT/'bin'/f'{exe}.exe')]
        for value in ('on','off','invalid'):
            bad=subprocess.run([*command,'--log',value],capture_output=True,timeout=5)
            assert bad.returncode!=0,exe
    command=[str(ROOT/'bin/c_client.exe'),'--print-config','--run-id','ignored_run']
    data=json.loads(subprocess.check_output(command,timeout=5))
    assert not data['effective_client']['logging']['enabled']
    results=[session(args.output/'no_file_logging')]
    (args.output/'results.json').write_text(json.dumps(results,indent=2),encoding='utf-8')
    print('PASS logging: no runtime option; no log directory; mission/ARM work')
    return 0


if __name__=='__main__':raise SystemExit(main())
