#!/usr/bin/env python3
"""Verify mobile archives contain required native code and no N64 ROMs."""
import argparse
import plistlib
from pathlib import Path
import zipfile

ROM_SUFFIXES = {'.z64', '.v64', '.n64', '.rom'}
ROM_HEADERS = {bytes.fromhex('80371240'), bytes.fromhex('37804012'), bytes.fromhex('40123780')}

def verify(path):
    with zipfile.ZipFile(path) as archive:
        damaged = archive.testzip()
        if damaged:
            raise ValueError(f'Damaged archive entry: {damaged}')
        names = archive.namelist()
        for name in names:
            if name.endswith('/'):
                continue
            if Path(name).suffix.lower() in ROM_SUFFIXES:
                raise ValueError(f'ROM file forbidden in package: {name}')
            if Path(name).suffix.lower() in {'.log', '.sav', '.sra', '.eep', '.fla', '.mobileprovision', '.p12', '.key', '.keystore', '.jks'} or Path(name).name == '.DS_Store' or Path(name).name.startswith('._'):
                raise ValueError(f'Private data or metadata forbidden in package: {name}')
            data = archive.read(name)
            if b'/' + b'Users/' in data or b'/' + b'Volumes/' in data:
                raise ValueError(f'Personal filesystem path in package: {name}')
            with archive.open(name) as file:
                if file.read(4) in ROM_HEADERS:
                    raise ValueError(f'N64 ROM data forbidden in package: {name}')
        if path.suffix == '.apk':
            for library in ('libmain.so', 'libSDL2.so', 'libc++_shared.so'):
                data = archive.read('lib/arm64-v8a/' + library)
                if data[:5] != b'\x7fELF\x02' or int.from_bytes(data[18:20], 'little') != 183:
                    raise ValueError(f'Expected ARM64 ELF library: {library}')
            if 'classes.dex' not in names:
                raise ValueError('Missing Android bytecode')
        elif path.suffix == '.ipa':
            plists = [name for name in names if name.startswith('Payload/') and name.count('/') == 2 and name.endswith('/Info.plist')]
            if len(plists) != 1:
                raise ValueError('Expected one app bundle')
            info = plistlib.loads(archive.read(plists[0]))
            executable = plists[0].rsplit('/', 1)[0] + '/' + info['CFBundleExecutable']
            data = archive.read(executable)
            if data[:4] != bytes.fromhex('cffaedfe') or int.from_bytes(data[4:8], 'little') != 0x0100000c:
                raise ValueError('Expected ARM64 Mach-O executable')
        else:
            raise ValueError('Expected .apk or .ipa')
    print(f'{path.name}: structure verified; no N64 ROM files or headers')

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('packages', nargs='+', type=Path)
    args = parser.parse_args()
    for package in args.packages:
        verify(package)
