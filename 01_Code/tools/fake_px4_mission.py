"""Independent MAVLink wire peer for LOCAL protocol demos; not an autopilot."""
import argparse
import json
import socket
import struct
import threading
import time

EXTRAS = {0:50,1:124,30:39,33:104,39:254,40:230,42:28,43:132,44:221,46:11,47:153,51:196,73:38,76:152,77:143,242:104,245:130}


def crc(data):
    result = 0xffff
    for value in data:
        result ^= value
        for _ in range(8):
            result = (result >> 1) ^ 0x8408 if result & 1 else result >> 1
    return result


def packet(msgid, payload, seq=0, sysid=1, compid=1):
    header = bytes([len(payload),0,0,seq%256,sysid,compid]) + msgid.to_bytes(3,'little')
    return b'\xfd' + header + payload + struct.pack('<H',crc(header+payload+bytes([EXTRAS[msgid]])))


def frames(data):
    offset = 0
    while offset+12 <= len(data):
        if data[offset] != 253:
            offset += 1
            continue
        length = data[offset+1]+12
        if offset+length > len(data):
            return
        raw = data[offset:offset+length]
        msgid = int.from_bytes(raw[7:10],'little')
        if msgid in EXTRAS and crc(raw[1:-2]+bytes([EXTRAS[msgid]])) == int.from_bytes(raw[-2:],'little'):
            yield msgid, raw[10:-2].ljust(256,b'\0'), raw[5], raw[6]
        offset += length


class FakePX4:
    def __init__(self, host, port, fault=''):
        if host not in ('127.0.0.1','localhost'):
            raise ValueError('Fake peer is restricted to localhost')
        self.target=(host,port)
        self.sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM)
        self.sock.bind(('127.0.0.1',0));self.sock.settimeout(.015)
        self.stop_event=threading.Event();self.thread=None
        self.seq=0;self.armed=False;self.height=0.0;self.mode='HOLD'
        self.mission=[];self.receiving={};self.count=0;self.gcs=(254,190)
        self.commands=[];self.received_items=[];self.fault=fault;self.fault_used=False
        self.boot=time.monotonic();self.flight_step=-1;self.flight_due=0;self.current=0
        self.rx_count=0
        self.home=(160740000,1081499000,10000)
        self.readback_count=0

    def send(self,msgid,payload):
        self.sock.sendto(packet(msgid,payload,self.seq),self.target);self.seq=(self.seq+1)%256

    def request(self,seq):
        self.send(51,struct.pack('<HBBB',seq,*self.gcs,0))

    def ack(self):
        self.send(47,struct.pack('<BBBB',*self.gcs,0,0))

    def telemetry(self):
        boot=int((time.monotonic()-self.boot)*1000)
        custom=(4<<16)|((4 if self.mode=='MISSION' else 6 if self.mode=='LAND' else 3)<<24)
        self.send(0,struct.pack('<IBBBBB',custom,2,12,81|(128 if self.armed else 0),4 if self.armed else 3,3))
        status=bytearray(31);struct.pack_into('<H',status,14,12300);status[30]=85;self.send(1,status)
        self.send(30,struct.pack('<Iffffff',boot,.003,.02,1.57,0,0,0))
        self.send(33,struct.pack('<IiiiihhhH',boot,160740000,1081499000,10000+int(self.height*1000),int(self.height*1000),0,0,0,9000))
        self.send(242,struct.pack('<iii10f',*self.home,0,0,0,1,0,0,0,0,0,0))
        self.send(245,bytes([0,2 if self.height>0 else 1]))
        self.send(42,struct.pack('<HHBBIII',self.current,len(self.mission),0,1 if self.mode=='MISSION' else 2,0,0,0))

    def receive(self,msgid,payload,sysid,compid):
        self.rx_count+=1;self.gcs=(sysid,compid)
        if msgid==44:
            self.count=struct.unpack_from('<H',payload)[0];self.receiving={}
            if self.fault=='arm_during_upload':
                self.armed=True;self.telemetry()
            if self.fault=='timeout':return
            if self.fault=='drop_request' and not self.fault_used:
                self.fault_used=True;return
            if self.fault=='wrong_target' and not self.fault_used:
                self.send(51,struct.pack('<HBBB',0,253,190,0));self.fault_used=True;return
            self.request(0)
        elif msgid==73:
            seq=struct.unpack_from('<H',payload,28)[0]
            self.received_items.append(seq)
            if seq != len(self.receiving):
                if len(self.receiving)==self.count:self.ack()
                else:self.request(len(self.receiving))
                return
            self.receiving[seq]=bytes(payload[:38])
            if len(self.receiving)<self.count:self.request(len(self.receiving))
            else:
                self.mission=[self.receiving[i] for i in range(self.count)];self.current=0
                if self.fault=='reject':self.send(47,struct.pack('<BBBB',*self.gcs,1,0))
                elif self.fault=='drop_ack' and not self.fault_used:self.fault_used=True
                else:self.ack()
        elif msgid==43:
            self.readback_count+=1
            if self.readback_count==2 and self.fault in ('home_small_start','home_large_start','home_vertical_start'):
                self.home=(160740002,1081499000,10700) if self.fault=='home_small_start' else (
                    (160740200,1081499000,10000) if self.fault=='home_large_start' else (160740000,1081499000,12100))
                self.telemetry()
            self.send(44,struct.pack('<HBBB',len(self.mission),*self.gcs,0))
        elif msgid==51:
            seq=struct.unpack_from('<H',payload)[0]
            if self.readback_count==2 and self.fault=='home_drift_start':
                self.home=(160740000+(seq+1)*30,1081499000,10000)
                self.telemetry()
            if seq<len(self.mission):
                item=bytearray(self.mission[seq]);item[32:34]=bytes(self.gcs)
                if self.fault=='mismatch' and seq==1:struct.pack_into('<f',item,24,99)
                self.send(73,item)
        elif msgid==76:
            cmd=struct.unpack_from('<H',payload,28)[0];params=struct.unpack_from('<7f',payload)
            self.commands.append(cmd);result=0
            if cmd==400:
                self.armed=bool(params[0])
                if self.armed and self.fault=='home_small_arm':self.home=(160740002,1081499000,10700)
            elif cmd==22:
                if not self.armed:result=2
                else:self.height=10
            elif cmd==21:self.height=0;self.armed=False;self.mode='LAND';self.flight_step=-1
            elif cmd==300:
                if not self.armed or not self.mission:result=2
                else:self.mode='MISSION';self.height=10;self.flight_step=0;self.flight_due=time.monotonic()+.4
            else:result=3
            if self.fault!='drop_start_ack' or cmd!=300:
                self.send(77,struct.pack('<HBBiBB',cmd,result,100,0,*self.gcs))
            self.telemetry()

    def run(self):
        next_telemetry=0
        while not self.stop_event.is_set():
            now=time.monotonic()
            if now>=next_telemetry:self.telemetry();next_telemetry=now+.1
            if self.flight_step>=0 and now>=self.flight_due:
                self.current=self.flight_step
                self.send(42,struct.pack('<HHBBIII',self.current,len(self.mission),0,1,0,0,0))
                self.send(46,struct.pack('<H',self.current))
                self.flight_step+=1;self.flight_due=now+.4
                if self.flight_step>=len(self.mission):self.flight_step=-1;self.height=0;self.armed=False;self.mode='HOLD'
            try:
                data,_=self.sock.recvfrom(65536)
                for f in frames(data):self.receive(*f)
            except socket.timeout:pass
            except OSError:
                if not self.stop_event.is_set():raise

    def start(self):
        self.thread=threading.Thread(target=self.run,daemon=True);self.thread.start();return self

    def close(self):
        self.stop_event.set()
        if self.thread:self.thread.join(timeout=2)
        self.sock.close()


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--config',default='config/gateway.web-demo.example.json')
    args=parser.parse_args()
    from pathlib import Path
    cfg=json.loads(Path(args.config).read_text(encoding='utf-8'))
    peer=FakePX4('127.0.0.1',cfg['network']['udp_bind']['port']).start()
    print('LOCAL FAKE PX4: giao thuc mo phong; khong dieu khien drone. Ctrl+C de dung.',flush=True)
    try:
        while True:time.sleep(.5)
    except KeyboardInterrupt:pass
    finally:peer.close()
