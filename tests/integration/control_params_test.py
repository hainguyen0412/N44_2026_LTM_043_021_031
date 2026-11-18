"""Inspect actual controller COMMAND_LONG bytes against a loopback ACK peer."""
import argparse
import json
import math
from pathlib import Path
import socket
import struct
import subprocess
import time
from command_retry_test import ROOT, extract_frames, heartbeat
from command_ack_test import ack_packet


def run_case(client, output, mode, verbose=False):
    frames = []; pending = bytearray(); process = None
    log_path = output / (mode + ('_verbose' if verbose else '') + '.log')
    with socket.socket() as server, log_path.open('wb') as log:
        server.bind(('127.0.0.1', 0)); server.listen(1); server.settimeout(5)
        process = subprocess.Popen([str(client), '127.0.0.1', str(server.getsockname()[1]),
            '--role', 'controller', '--mode', mode, '--ack-timeout-ms', '2000',
            *(['--verbose'] if verbose else [])],
            cwd=ROOT, stdin=subprocess.PIPE, stdout=log, stderr=log)
        try:
            connection, _ = server.accept()
            with connection:
                connection.settimeout(.05)
                telemetry_seq = 0

                def collect(seconds, expected=None):
                    nonlocal telemetry_seq
                    deadline = time.monotonic() + seconds
                    while time.monotonic() < deadline:
                        connection.sendall(heartbeat((1, 1), telemetry_seq))
                        telemetry_seq = (telemetry_seq + 1) % 256
                        try: raw = connection.recv(4096)
                        except socket.timeout: continue
                        assert raw, 'Controller closed unexpectedly'
                        pending.extend(raw)
                        for frame in extract_frames(pending):
                            if int.from_bytes(frame[7:10], 'little') != 76: continue
                            value = struct.unpack('<7fHBBB', frame[10:-2].ljust(33, b'\0'))
                            assert value[8:11] == (1, 1, 0), value
                            assert frame[5:7] == bytes([255, 190]), 'Wrong sender ID'
                            frames.append(value)
                            connection.sendall(ack_packet(command=value[7]))
                        if expected is not None and len(frames) >= expected: return
                    if expected is not None: raise AssertionError('Command did not arrive')

                def send(command):
                    process.stdin.write((command + '\n').encode()); process.stdin.flush()

                for bad in ('takeoff nope', 'takeoff nan', 'takeoff inf', 'takeoff 1e999',
                            'takeoff 123 extra', 'arm force', 'land extra'):
                    send(bad)
                collect(.3)
                assert not frames, 'Malformed console input sent a command'
                for number, command in enumerate(('arm', 'disarm', 'takeoff', 'takeoff 123.5', 'land'), 1):
                    send(command); collect(2, number); collect(.1)
                assert [v[7] for v in frames] == [400, 400, 22, 22, 21]
                assert frames[0][:7] == (1., 0., 0., 0., 0., 0., 0.)
                assert frames[1][:7] == (0.,) * 7
                for index in (2, 3, 4):
                    assert frames[index][:3] == (0., 0., 0.)
                    assert all(math.isnan(v) for v in frames[index][3:6]), 'Navigation target must be unspecified'
                assert math.isnan(frames[2][6]), 'Default takeoff must select PX4 takeoff height'
                assert frames[3][6] == 123.5, 'Explicit takeoff altitude must remain AMSL'
                assert math.isnan(frames[4][6]), 'LAND must not request zero AMSL'
                send('quit'); process.wait(timeout=10)
                assert process.returncode == 0
        finally:
            if process and process.poll() is None: process.terminate(); process.wait(timeout=3)
            if process and process.stdin: process.stdin.close()
    text = log_path.read_text(encoding='utf-8', errors='replace')
    assert text.count('Lenh khong hop le; khong gui.') == 7
    assert text.count('ack_accepted') == 5
    if verbose and mode != 'B0': assert 'HEARTBEAT type=' in text
    else:
        assert 'HEARTBEAT type=' not in text, 'Controller console flooded by telemetry'
        assert 'COMMAND_ACK command=' not in text, 'Controller duplicated raw ACK output'
    return {'mode': mode, 'verbose': verbose, 'commands': 5, 'invalid_inputs_rejected': 7, 'passed': True}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT/'output/test_results'/('control_params_'+str(time.time_ns())))
    args = parser.parse_args(); args.output.mkdir(parents=True, exist_ok=False)
    results = []
    for mode in ('B0', 'B1', 'B2'):
        results.append(run_case(ROOT/'build/test_support/bin/c_client.exe', args.output, mode))
        print('PASS', mode, 'ARM/DISARM/TAKEOFF/LAND params, CRC, ACK and invalid input', flush=True)
    results.append(run_case(ROOT/'build/test_support/bin/c_client.exe', args.output, 'B1', True))
    print('PASS B1 verbose restores telemetry display', flush=True)
    (args.output/'result.json').write_text(json.dumps({'simulated': True, 'results': results}, indent=2), encoding='utf-8')


if __name__ == '__main__': main()
