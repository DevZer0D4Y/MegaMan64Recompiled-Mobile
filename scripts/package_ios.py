#!/usr/bin/env python3
"""Package an existing iOS app bundle without Finder metadata or private data."""
import argparse
from pathlib import Path
import zipfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('app', type=Path)
parser.add_argument('output', type=Path)
args = parser.parse_args()
if args.app.suffix != '.app' or not (args.app / 'Info.plist').is_file():
    parser.error('Expected an existing .app bundle with Info.plist')
forbidden = {'.z64', '.v64', '.n64', '.rom', '.sav', '.sra', '.eep', '.fla', '.mobileprovision', '.p12', '.key', '.log'}
files = sorted(p for p in args.app.rglob('*') if p.is_file())
for path in files:
    if path.is_symlink() or path.suffix.lower() in forbidden or path.relative_to(args.app).parts[0] in {'config', 'saves'}:
        parser.error(f'Private data or unsupported link in bundle: {path.name}')
with zipfile.ZipFile(args.output, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as archive:
    for path in files:
        if path.name == '.DS_Store' or path.name.startswith('._'):
            continue
        archive.write(path, 'Payload/' + args.app.name + '/' + path.relative_to(args.app).as_posix())
print(args.output.name)
