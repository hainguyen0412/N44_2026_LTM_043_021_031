"""Validate mission schema through the actual client CLI, without sockets."""
import json
from pathlib import Path
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[2]
base=json.loads((ROOT/'config/gateway.mission.example.json').read_text(encoding='utf-8'))
cases=[]
def case(name,change,valid=False):
    value=json.loads(json.dumps(base));change(value);cases.append((name,value,valid))
case('schema_2_valid',lambda j:None,True)
case('schema_1_valid',lambda j:(j.update(schema_version=1),j.pop('mission'),j['commands'].update(allowed_command_ids=[400,22,21])),True)
case('v1_mission_section',lambda j:j.update(schema_version=1))
case('disabled_start',lambda j:j['mission'].update(enabled=False))
case('too_many_items',lambda j:j['mission'].update(max_items=101))
case('zero_timeout',lambda j:j['mission'].update(timeout_ms=0))
case('too_many_attempts',lambda j:j['mission'].update(max_attempts=6))
case('unknown_key',lambda j:j['mission'].update(unknown=True))
case('duplicate_command',lambda j:j['commands'].update(allowed_command_ids=[400,22,21,21]))
(ROOT/'output/test_results').mkdir(parents=True,exist_ok=True)
with tempfile.TemporaryDirectory(prefix='mission_config_',dir=ROOT/'output/test_results') as tmp:
    for name,value,valid in cases:
        path=Path(tmp)/(name+'.json');path.write_text(json.dumps(value),encoding='utf-8')
        result=subprocess.run([str(ROOT/'bin/c_client.exe'),'--config',str(path),'--print-config'],capture_output=True,text=True,encoding='utf-8',timeout=5)
        assert (result.returncode==0)==valid,(name,result.stdout,result.stderr)
        if valid:json.loads(result.stdout)
        print('PASS',name)
