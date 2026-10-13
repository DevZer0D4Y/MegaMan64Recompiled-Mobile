#!/usr/bin/env python3
"""Check tracked source for private data and generated game/build artifacts."""
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
entries = subprocess.check_output(['git', 'ls-files', '--stage', '-z'], cwd=ROOT).split(b'\0')
forbidden_suffixes = {'.z64', '.v64', '.n64', '.rom', '.sav', '.sra', '.eep', '.fla', '.elf', '.o', '.a', '.so', '.dylib', '.apk', '.ipa', '.log', '.keystore', '.jks', '.p12', '.pfx', '.mobileprovision', '.pem', '.key'}
forbidden_dirs = {'.git', '.gradle', '.cxx', '__pycache__', 'RecompiledFuncs', 'RecompiledPatches', 'deps', 'dist', 'work', 'outputs', 'build'}
# Split marker literals so the scanner can inspect its own source.
private_markers = [b'/' + b'Users/', b'/' + b'Volumes/', b'C:' + b'\\Users\\']
secrets = re.compile(rb'(?:gh[pousr]_|github_pat_)[A-Za-z0-9_]{20,}|-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----')
rom_headers = {bytes.fromhex('80371240'), bytes.fromhex('37804012'), bytes.fromhex('40123780')}
errors = []
count = 0
for entry in entries:
    if not entry:
        continue
    meta, name = entry.split(b'\t', 1)
    mode = meta.split()[0]
    if mode == b'160000':  # Pinned upstream dependency, not a vendored working tree.
        continue
    relative = Path(name.decode())
    path = ROOT / relative
    count += 1
    if mode == b'120000' or path.is_symlink():
        errors.append(f'{relative}: symlink forbidden in public export')
        continue
    if relative.suffix.lower() in forbidden_suffixes or any(part in forbidden_dirs or part.startswith('build-') for part in relative.parts) or relative.parts[0] in {'config', 'saves'} or relative.name in {'.DS_Store', 'local.properties', '.env'} or relative.name.startswith('._') or relative.as_posix() == 'rsp/aspMain.cpp':
        errors.append(f'{relative}: private/generated artifact')
    data = path.read_bytes()
    if data[:4] in rom_headers or any(marker in data for marker in private_markers) or secrets.search(data):
        errors.append(f'{relative}: ROM, private path, or credential marker')
if errors:
    print('\n'.join(errors), file=sys.stderr)
    sys.exit(1)
print(f'{count} tracked source files checked; no detected private artifacts or ROMs')
