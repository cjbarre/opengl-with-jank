#!/usr/bin/env python3
"""Check a development command or relocated release with a server and client."""
import argparse
import os
from pathlib import Path
import pty
import threading
import signal
import subprocess
import tempfile
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--cwd', default='.')
parser.add_argument('--graphics', action='store_true')
parser.add_argument('--standalone', action='store_true')
parser.add_argument('--logs', default=None)
parser.add_argument('command', nargs=argparse.REMAINDER)
args = parser.parse_args()
command = args.command[1:] if args.command[:1] == ['--'] else args.command
if not command:
    parser.error('provide a command after --')
logs = Path(args.logs or tempfile.mkdtemp(prefix='sca-smoke-')).resolve()
logs.mkdir(parents=True, exist_ok=True)
env = os.environ.copy()
if args.graphics:
    env['SCA_SMOKE_TEST'] = '1'
if args.standalone:
    env['PATH'] = '/usr/bin:/bin'
    for key in ['LD_LIBRARY_PATH', 'DYLD_LIBRARY_PATH', 'JANK_EXTRA_FLAGS']:
        env.pop(key, None)
processes = []
files = []
readers = []

def copy_output(master, output):
    try:
        while True:
            data = os.read(master, 65536)
            if not data:
                break
            output.write(data)
            output.flush()
    except OSError:
        pass  # PTYs report EIO when the child closes the slave.
    finally:
        os.close(master)

def start(mode):
    path = logs / (mode + '.log')
    output = path.open('wb')
    master, slave = pty.openpty()
    files.append(output)
    modes = [mode] if args.graphics else ['net-test', mode]
    proc = subprocess.Popen(command + modes, cwd=args.cwd, env=env,
                            stdin=subprocess.DEVNULL, stdout=slave, stderr=subprocess.STDOUT,
                            start_new_session=True)
    os.close(slave)
    reader = threading.Thread(target=copy_output, args=(master, output), daemon=True)
    reader.start()
    readers.append(reader)
    processes.append(proc)
    return proc, path

def wait_for(proc, path, text):
    deadline = time.monotonic() + 900
    while time.monotonic() < deadline:
        content = path.read_text(errors="replace")
        if proc.poll() is not None:
            raise RuntimeError(f'{path}: exited {proc.returncode}\n{content}')
        if text in content:
            return
        time.sleep(0.5)
    raise RuntimeError(f'{path}: timed out waiting for {text}\n{path.read_text(errors="replace")}')

try:
    server, server_log = start('server')
    wait_for(server, server_log, 'Server ready.' if args.graphics else 'Server listening.')
    client, client_log = start('client')
    if args.graphics:
        assert client.wait(timeout=900) == 0, client_log.read_text(errors="replace")
        readers[-1].join(timeout=5)
        output = client_log.read_text(errors="replace")
        assert 'Received welcome!' in output, output
        assert 'Completed 120 client frames.' in output and 'Client finished.' in output, output
    else:
        assert client.wait(timeout=900) == 0, client_log.read_text(errors="replace")
        readers[-1].join(timeout=5)
        output = client_log.read_text(errors="replace")
        assert 'Received: {:type :echo' in output and 'Client finished.' in output, output
    assert server.poll() is None, server_log.read_text(errors="replace")
    print(f'Passed: {command}; logs: {logs}', flush=True)
finally:
    for proc in reversed(processes):
        try:
            os.killpg(proc.pid, signal.SIGTERM)
        except ProcessLookupError:
            pass
    for proc in processes:
        try:
            proc.wait(timeout=10)
        except subprocess.TimeoutExpired:
            os.killpg(proc.pid, signal.SIGKILL)
            proc.wait()
    for reader in readers:
        reader.join(timeout=5)
    for output in files:
        output.close()
