#!/usr/bin/env python3
"""Prepare Android dependencies/host tools, or check game source prerequisites."""
import argparse
import os
from pathlib import Path
import platform
import shlex
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
HOST = ROOT / 'build-android-host'
DEPENDENCIES = (
    ('SDL', 'https://github.com/libsdl-org/SDL.git', 'release-2.32.10'),
    ('freetype', 'https://github.com/freetype/freetype.git', 'VER-2-13-2'),
)
GENERATED = (
    'RecompiledFuncs/recomp_overlays.inl',
    'RecompiledPatches/patches.c', 'RecompiledPatches/patches_bin.c',
    'RecompiledPatches/patches_bin.h', 'RecompiledPatches/recomp_overlays.inl',
    'RecompiledPatches/funcs.h', 'rsp/aspMain.cpp',
)

def run(*command, cwd=ROOT):
    subprocess.run(command, cwd=cwd, check=True)

def check():
    missing = [path for path in GENERATED if not (ROOT / path).is_file()]
    if not list((ROOT / 'RecompiledFuncs').glob('*.c')):
        missing.append('RecompiledFuncs/*.c')
    if missing:
        print('Android game build blocked: missing generated sources:', file=sys.stderr)
        for path in missing:
            print('  ' + path, file=sys.stderr)
        print('Generate these on the host from MegaMan64.us.rev1.z64; see android/README.md.', file=sys.stderr)
        return False
    print('Generated game sources are present.')
    return True

def prepare():
    for name, url, tag in DEPENDENCIES:
        destination = ROOT / 'android' / 'deps' / name
        if not destination.exists():
            run('git', 'clone', '--depth', '1', '--branch', tag, url, str(destination))
        actual = subprocess.check_output(['git', '-C', str(destination), 'rev-parse', 'HEAD'], text=True).strip()
        expected = subprocess.check_output(['git', '-C', str(destination), 'rev-parse', tag + '^{commit}'], text=True).strip()
        if actual != expected:
            raise RuntimeError(f'{name} must be checked out at {tag}')
    patches = (
        ('lib/rt64', 'android/patches/rt64-mobile.patch'),
        ('lib/rt64/src/contrib/plume', 'ios/patches/plume-ios.patch'),
        ('lib/N64ModernRuntime', 'android/patches/runtime-rom-loading.patch'),
        ('lib/N64ModernRuntime', 'android/patches/runtime-mobile-saves.patch'),
        ('lib/N64ModernRuntime', 'ios/patches/runtime-ios.patch'),
        ('lib/N64ModernRuntime/N64Recomp/lib/sljit', 'ios/patches/sljit-ios.patch'),
    )
    for repo, filename in patches:
        patch = ROOT / filename
        repository = ROOT / repo
        applicable = subprocess.run(['git', 'apply', '--check', str(patch)], cwd=repository, capture_output=True)
        if applicable.returncode == 0:
            run('git', 'apply', str(patch), cwd=repository)
        else:
            applied = subprocess.run(['git', 'apply', '--reverse', '--check', str(patch)], cwd=repository, capture_output=True)
            if applied.returncode:
                raise RuntimeError(f'{filename} does not match this checkout; preserve local edits and resolve manually')
    repository = ROOT / 'lib/rt64'
    run('cmake', '-S', str(repository / 'src/tools/file_to_c'), '-B', str(HOST / 'file_to_c'), '-DCMAKE_BUILD_TYPE=Release')
    run('cmake', '--build', str(HOST / 'file_to_c'), '--config', 'Release')
    machine = 'arm64' if platform.machine() in ('arm64', 'aarch64') else 'x64'
    system = platform.system()
    if system not in ('Darwin', 'Linux'):
        raise RuntimeError('Host helper supports macOS and Linux; Windows requires explicit hostDxc/hostFileToC Gradle properties')
    base = repository / 'src/contrib/dxc'
    binary = base / 'bin' / machine / ('dxc-macos' if system == 'Darwin' else 'dxc-linux')
    if not binary.exists():
        raise RuntimeError('DXC missing: initialize recursive submodules')
    loader = 'DYLD_LIBRARY_PATH' if system == 'Darwin' else 'LD_LIBRARY_PATH'
    wrapper = HOST / 'dxc-host'
    wrapper.write_text('#!/bin/sh\nexport ' + loader + '=' + shlex.quote(str(base / 'lib' / machine)) + '\nexec ' + shlex.quote(str(binary)) + ' "$@"\n')
    wrapper.chmod(0o755)
    print('\nHost tools ready. After generating game sources, build with:')
    print('android/gradlew --project-dir android assembleDebug ' + shlex.quote('-PhostDxc=' + str(wrapper)) + ' ' + shlex.quote('-PhostFileToC=' + str(HOST / 'file_to_c/file_to_c')))

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prepare', action='store_true', help='Fetch pinned dependencies, apply RT64 patch, build host file_to_c')
    parser.add_argument('--check', action='store_true', help='Check ROM-generated game prerequisites without downloading anything')
    args = parser.parse_args()
    if args.prepare:
        prepare()
    if args.check or not args.prepare:
        return 0 if check() else 2
    return 0

if __name__ == '__main__':
    try:
        sys.exit(main())
    except (subprocess.CalledProcessError, RuntimeError) as error:
        print(str(error), file=sys.stderr)
        sys.exit(1)
