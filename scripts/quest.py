#!/usr/bin/env python3
"""Fetch the pinned OpenXR loader and build the separate Quest presentation library."""
import argparse
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[1]
VERSION = '1.1.63'
SHA256 = '622419d2f6741c3443a3beb4779af0764318edd01830de967f24c741ebcded73'
URL = f'https://repo.maven.apache.org/maven2/org/khronos/openxr/openxr_loader_for_android/{VERSION}/openxr_loader_for_android-{VERSION}.aar'
DEPENDENCY = ROOT / 'android/deps/openxr'

def prepare():
    DEPENDENCY.parent.mkdir(parents=True, exist_ok=True)
    archive = DEPENDENCY.parent / f'openxr-{VERSION}.aar'
    if not archive.exists():
        urllib.request.urlretrieve(URL, archive)
    if hashlib.sha256(archive.read_bytes()).hexdigest() != SHA256:
        raise RuntimeError('OpenXR archive checksum mismatch; delete the download and retry')
    with zipfile.ZipFile(archive) as content:
        for item in content.infolist():
            if Path(item.filename).is_absolute() or '..' in Path(item.filename).parts:
                raise RuntimeError('Invalid OpenXR archive path')
        content.extractall(DEPENDENCY)
    print('Pinned OpenXR Android loader prepared')

def build(ndk, jobs):
    if not (DEPENDENCY / 'jni/arm64-v8a/libopenxr_loader.so').exists():
        prepare()
    toolchain = ndk / 'build/cmake/android.toolchain.cmake'
    if not toolchain.is_file():
        raise RuntimeError('Set ANDROID_NDK_HOME or pass --ndk with the Android NDK path')
    output = ROOT / 'build-quest-bridge'
    subprocess.run(['cmake', '-S', str(ROOT / 'android/quest/native'), '-B', str(output), '-G', 'Ninja',
                    '-DCMAKE_TOOLCHAIN_FILE=' + str(toolchain), '-DANDROID_ABI=arm64-v8a',
                    '-DANDROID_PLATFORM=android-26', '-DANDROID_STL=c++_shared',
                    '-DCMAKE_BUILD_TYPE=Release', '-DOPENXR_ROOT=' + str(DEPENDENCY)], check=True)
    subprocess.run(['cmake', '--build', str(output), '--parallel', str(jobs)], check=True)
    target = ROOT / 'android/quest/libs/arm64-v8a'
    target.mkdir(parents=True, exist_ok=True)
    shutil.copy2(output / 'libquest_vr.so', target / 'libquest_vr.so')
    shutil.copy2(DEPENDENCY / 'jni/arm64-v8a/libopenxr_loader.so', target / 'libopenxr_loader.so')
    print('Quest libraries staged; build Android flavor assembleQuestDebug or assembleQuestRelease')

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prepare', action='store_true')
    parser.add_argument('--build', action='store_true')
    parser.add_argument('--ndk', type=Path, default=Path(os.environ.get('ANDROID_NDK_HOME', 'missing-ndk')))
    parser.add_argument('--jobs', type=int, default=2)
    args = parser.parse_args()
    if args.prepare:
        prepare()
    if args.build:
        build(args.ndk.resolve(), max(1, args.jobs))
    if not args.prepare and not args.build:
        parser.error('Use --prepare or --build')
