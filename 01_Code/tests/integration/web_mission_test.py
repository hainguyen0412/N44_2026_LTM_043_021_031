"""Real web executable + real gateway + independent UDP peer at localhost only."""
import argparse
import http.client
import json
import socket
import struct
import subprocess
import sys
import time
import traceback
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools'))
from fake_px4_mission import FakePX4,packet


def free_port(typ=socket.SOCK_STREAM):
    with socket.socket(socket.AF_INET,typ) as s:
        s.bind(('127.0.0.1',0));return s.getsockname()[1]


class Run:
    def __init__(self,directory,fault='',mission_enabled=True):
        self.directory=directory;directory.mkdir(parents=True)
        cfg=json.loads((ROOT/'config/gateway.mission.example.json').read_text(encoding='utf-8'))
        self.udp=free_port(socket.SOCK_DGRAM);self.observer=free_port();self.controller=free_port();self.http=free_port()
        for key,port in [('udp_bind',self.udp),('tcp_observer_bind',self.observer),('tcp_controller_bind',self.controller)]:cfg['network'][key]={'host':'127.0.0.1','port':port}
        cfg['logging']['root_directory']=str(directory/'runs')
        cfg['measurement'].update(warmup_seconds=0,duration_seconds=0,drain_timeout_ms=200)
        cfg['mission'].update(timeout_ms=250,max_attempts=2)
        if not mission_enabled:
            cfg['mission']['enabled']=False
            cfg['commands']['allowed_command_ids']=[400,22,21]
        cfg['commands'].update(ack_timeout_ms=300,max_total_attempts=2)
        self.path=directory/'config.json';self.path.write_text(json.dumps(cfg,indent=2),encoding='utf-8')
        self.stop_gateway=directory/'gateway.stop';self.stop_web=directory/'web.stop'
        self.logs=[];self.procs=[];self.peer=None;self.csrf='';self.owner=''
        try:
            for exe,stop,args in [('gateway',self.stop_gateway,[]),('web_client',self.stop_web,['--web-port',str(self.http)])]:
                log=(directory/(exe+'.log')).open('w',encoding='utf-8');self.logs.append(log)
                self.procs.append(subprocess.Popen([str(ROOT/'bin'/f'{exe}.exe'),'--config',str(self.path),'--run-id',directory.name,'--stop-file',str(stop),*args],cwd=ROOT,stdout=log,stderr=subprocess.STDOUT))
                if exe=='gateway':self.wait(lambda: 'READY' in (directory/'gateway.log').read_text(encoding='utf-8'),timeout=8)
            self.wait(lambda:self.get_bootstrap(),timeout=10)
            self.peer=FakePX4('127.0.0.1',self.udp,fault).start()
        except Exception:
            self.close();raise

    def get_bootstrap(self):
        try:
            code,data=self.api('/api/bootstrap');self.csrf=data.get('csrf','');return code==200 and self.csrf
        except OSError:return False

    def api(self,path,body=None,owner=True,csrf=True,headers=None):
        conn=http.client.HTTPConnection('127.0.0.1',self.http,timeout=4)
        h={'Content-Type':'application/json'}
        if csrf:h['X-CSRF-Token']=self.csrf
        if owner:h['X-Controller-Token']=self.owner
        h.update(headers or {})
        conn.request('GET' if body is None else 'POST',path,None if body is None else json.dumps(body),h)
        response=conn.getresponse();code=response.status;raw=response.read();conn.close()
        return code,json.loads(raw) if raw else {}

    def state(self):return self.api('/api/state')[1]

    def wait(self,fn,timeout=5):
        deadline=time.monotonic()+timeout
        while time.monotonic()<deadline:
            result=fn()
            if result:return result
            for p in self.procs:
                if p.poll() is not None:raise AssertionError('Child exited: '+str(p.returncode))
            time.sleep(.03)
        raise AssertionError('Timeout; state='+str(self.state()) if self.csrf else 'Startup timeout')

    def connect(self,role='controller'):
        code,result=self.api('/api/connect',{'role':role});assert code==200,(code,result)
        self.owner=result.get('controller_token','')
        self.wait(lambda:self.state().get('fresh') and 'home' in self.state()['telemetry'])

    def draft(self):
        m=self.api('/api/map')[1]
        return {'request_id':'upload_1','schema_version':1,'map_id':m['map_id'],'map_version':m['version'],'takeoff_altitude':10,
                'points':[{'x':205,'y':240,'alt':10,'hold':0},{'x':205,'y':156,'alt':10,'hold':0},{'x':180,'y':156,'alt':10,'hold':0},{'x':205,'y':220,'alt':10,'hold':0}]}

    def close(self):
        if self.peer:self.peer.close()
        for stop in (self.stop_web,self.stop_gateway):stop.touch(exist_ok=True)
        for p in reversed(self.procs):
            try:p.wait(timeout=8)
            except subprocess.TimeoutExpired:p.kill();p.wait();raise AssertionError('Child shutdown hung')
        for log in self.logs:log.close()


def happy(directory):
    r=Run(directory)
    try:
        assert r.api('/api/connect',{'role':'controller'},csrf=False)[0]==403
        assert r.api('/api/state',headers={'Origin':'https://external.invalid'})[0]==403
        r.connect('observer');assert r.state()['telemetry']['battery']['percent']==85
        assert r.api('/api/commands',{'command':'ARM','request_id':'denied'})[0]==403
        r.connect()
        assert r.api('/api/connect',{'role':'controller'},owner=False)[0]==409
        assert r.api('/api/commands',{'command':'ARM','request_id':'other_tab'},owner=False)[0]==403
        d=r.draft();assert r.api('/api/missions/validate',d)[0]==200
        wrong={**d,'map_version':'wrong'};assert r.api('/api/missions/upload',wrong)[0]==400
        code,result=r.api('/api/missions/upload',d);assert code==202,result
        assert r.api('/api/missions/upload',d)[0]==202
        r.wait(lambda:r.state()['mission'].get('verified'))
        assert len(r.peer.mission)==7
        code,operation=r.api('/api/operations/upload_1')
        assert code==200 and operation['mission']['verified']
        assert r.api('/api/operations/missing')[0]==404
        assert r.api('/api/operations/upload_1',owner=False)[0]==403
        assert r.api('/api/missions/start',{'request_id':'before_arm','upload_request_id':'upload_1'})[0]==400
        assert r.api('/api/commands',{'command':'ARM','request_id':'arm_1'})[0]==202
        r.wait(lambda:r.state()['telemetry']['heartbeat']['armed'])
        assert r.api('/api/missions/upload',{**d,'request_id':'armed_upload'})[0]==400
        assert r.api('/api/missions/start',{'request_id':'wrong_revision','upload_request_id':'other_upload'})[0]==400
        start={'request_id':'start_1','upload_request_id':'upload_1'};code,result=r.api('/api/missions/start',start);assert code==202,result
        assert r.api('/api/missions/start',start)[0]==202
        r.wait(lambda:r.state()['mission'].get('state')=='completed',timeout=7)
        assert r.peer.commands.count(300)==1,r.peer.commands
        assert r.peer.commands.count(400)==1,r.peer.commands
        assert r.api('/api/missions/start',{'request_id':'start_again','upload_request_id':'upload_1'})[0]==400
        s=r.state();assert abs(s['map_position']['x']-205)<.05;assert abs(s['map_position']['y']-288)<.05
        # SSE is an actual HTTP stream, not a fake timer in the frontend.
        conn=http.client.HTTPConnection('127.0.0.1',r.http,timeout=3);conn.request('GET','/api/events');resp=conn.getresponse();assert resp.status==200;assert resp.readline().startswith(b'data: ');resp.close();conn.close()
        assert r.api('/api/disconnect',{})[0]==200
        r.connect();assert not r.state()['mission'].get('verified',False)
        assert r.api('/api/missions/readback',{'request_id':'read_1'})[0]==202
        r.wait(lambda:r.state()['mission'].get('state')=='downloaded')
        return {'simulated':True,'mission_items':len(r.peer.mission),'commands':r.peer.commands,'state':r.state()}
    finally:r.close()


def failure_case(directory,fault,success):
    r=Run(directory,fault)
    try:
        r.connect();code,result=r.api('/api/missions/upload',r.draft());assert code==202,result
        if success:r.wait(lambda:r.state()['mission'].get('verified'))
        else:r.wait(lambda:not r.state().get('connected'))
        s=r.state()
        assert s['mission'].get('verified',False)==success,s
        assert 300 not in r.peer.commands
        if success:assert r.peer.fault_used
        return {'simulated':True,'fault':fault,'state':s,'uploaded_sequences':r.peer.received_items}
    finally:r.close()


def lost_start_ack(directory):
    r=Run(directory,'drop_start_ack')
    try:
        r.connect()
        assert r.api('/api/missions/upload',r.draft())[0]==202
        r.wait(lambda:r.state()['mission'].get('verified'))
        assert r.api('/api/commands',{'command':'ARM','request_id':'arm_lost_ack'})[0]==202
        r.wait(lambda:r.state()['telemetry']['heartbeat']['armed'])
        assert r.api('/api/missions/start',{'request_id':'start_lost_ack','upload_request_id':'upload_1'})[0]==202
        r.wait(lambda:r.state()['command'].get('event')=='final_unknown')
        time.sleep(.8)
        assert r.peer.commands.count(300)==1,r.peer.commands
        assert r.state()['mission']['consumed']
        assert r.api('/api/missions/start',{'request_id':'start_again','upload_request_id':'upload_1'})[0]==400
        return {'simulated':True,'commands':r.peer.commands,'state':r.state()}
    finally:r.close()

def home_change(directory,fault):
    r=Run(directory,fault)
    try:
        r.connect()
        assert r.api('/api/bootstrap')[1]['max_waypoints']==97
        assert r.api('/api/missions/upload',r.draft())[0]==202
        r.wait(lambda:r.state()['mission'].get('verified'))
        assert r.api('/api/commands',{'command':'ARM','request_id':'arm_home'})[0]==202
        r.wait(lambda:r.state()['telemetry']['heartbeat']['armed'])
        assert r.api('/api/missions/start',{'request_id':'start_home','upload_request_id':'upload_1'})[0]==202
        success=fault in ('home_small_start','home_small_arm')
        if success:
            r.wait(lambda:r.peer.commands.count(300)==1)
            assert r.peer.commands.count(300)==1
            # Map Home remains bound to the uploaded coordinates.
            assert abs(r.state()['map_home']['y']-288)<.05
        else:
            r.wait(lambda:not r.state()['connected'])
            assert r.peer.commands.count(300)==0
            assert 'Home PX4 da thay doi' in r.state()['mission']['message']
            assert not r.state()['mission']['verified']
        return {'fault':fault,'start_sent':r.peer.commands.count(300),'state':r.state()}
    finally:r.close()


def landing_approach(directory):
    r=Run(directory)
    try:
        r.connect()
        draft=r.draft()
        draft.update(takeoff_altitude=50,points=[{'x':250,'y':180,'alt':50,'hold':0}])
        assert r.api('/api/missions/upload',draft)[0]==202
        r.wait(lambda:r.state()['mission'].get('verified'))
        assert len(r.peer.mission)==4
        approach=r.peer.mission[-2];land=r.peer.mission[-1]
        assert struct.unpack_from('<H',approach,30)[0]==16
        assert struct.unpack_from('<H',land,30)[0]==21
        assert struct.unpack_from('<f',approach,24)[0]==50
        assert approach[16:24]==land[16:24]
        assert struct.unpack_from('<2f',approach)==(1.,1.)
        return {'items':4,'approach_altitude':50,'land_altitude':struct.unpack_from('<f',land,24)[0]}
    finally:r.close()


def gateway_permissions(directory,enabled=True):
    r=Run(directory,mission_enabled=enabled)
    try:
        time.sleep(.2)
        with socket.create_connection(('127.0.0.1',r.observer),timeout=2) as observer:
            observer.sendall(packet(44,struct.pack('<HBBB',3,1,1,0),sysid=254,compid=190))
            observer.settimeout(2)
            try:
                while observer.recv(8192):pass
            except ConnectionResetError:pass
        assert r.peer.count==0
        with socket.create_connection(('127.0.0.1',r.controller),timeout=2) as controller:
            bad=b''.join([
                packet(44,struct.pack('<HBBB',3,2,1,0),sysid=254,compid=190),
                packet(44,struct.pack('<HBBB',3,1,1,1),sysid=254,compid=190),
                packet(44,struct.pack('<HBBB',1001,1,1,0),sysid=254,compid=190),
                packet(44,struct.pack('<HBBB',3,1,1,0),sysid=253,compid=190)])
            controller.sendall(bad);time.sleep(.2);assert r.peer.count==0
            controller.sendall(packet(44,struct.pack('<HBBB',3,1,1,0),sysid=254,compid=190))
            if enabled:r.wait(lambda:r.peer.count==3)
            else:time.sleep(.3);assert r.peer.count==0
        return {'simulated':True,'mission_enabled':enabled,'peer_mission_count':r.peer.count}
    finally:r.close()


def main():
    p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);args=p.parse_args();args.output.mkdir(parents=True,exist_ok=True)
    results={};failed=False
    for name,fault,success in [('happy','',True),('retry_request','drop_request',True),('retry_ack','drop_ack',True),('wrong_target','wrong_target',True),('readback_mismatch','mismatch',False),('px4_rejected','reject',False),('mission_timeout','timeout',False),('gateway_permissions','',True),('mission_disabled','',False),('lost_start_ack','drop_start_ack',False),('arm_during_upload','arm_during_upload',False)]:
        try:
            value=happy(args.output/name) if name=='happy' else lost_start_ack(args.output/name) if name=='lost_start_ack' else gateway_permissions(args.output/name,success) if name in ('gateway_permissions','mission_disabled') else failure_case(args.output/name,fault,success)
            results[name]={'passed':True,'result':value};print('PASS',name,flush=True)
        except Exception as e:failed=True;results[name]={'passed':False,'error':str(e)};traceback.print_exc();print('FAIL',name,flush=True)
    for name in ('home_small_arm','home_small_start','home_large_start','home_vertical_start','home_drift_start','landing_approach'):
        try:
            value=landing_approach(args.output/name) if name=='landing_approach' else home_change(args.output/name,name)
            results[name]={'passed':True,'result':value};print('PASS',name,flush=True)
        except Exception as e:failed=True;results[name]={'passed':False,'error':str(e)};traceback.print_exc();print('FAIL',name,flush=True)
    (args.output/'results.json').write_text(json.dumps(results,ensure_ascii=False,indent=2),encoding='utf-8')
    return int(failed)


if __name__=='__main__':raise SystemExit(main())
