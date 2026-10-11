"""Real COM enumeration and no-capture startup, including audio-less CI hosts."""
import subprocess
import sys
import socket

exe = sys.argv[1]
for _ in range(3):
    result = subprocess.run([exe, '--cli', '--list-devices'], capture_output=True, timeout=20)
    assert result.returncode in (0, 1), (result.returncode, result.stderr)
with socket.socket() as probe:
    probe.bind(('127.0.0.1', 0))
    port = probe.getsockname()[1]
result = subprocess.run([exe, '--cli', '--web-only', '--port', str(port), '--duration', '0.3'],
                        capture_output=True, timeout=20)
assert result.returncode == 0, (result.returncode, result.stderr)
assert b'[web]' in result.stdout, (result.stdout, result.stderr)
print('PASS: enumeration cleanup and no-capture startup')
