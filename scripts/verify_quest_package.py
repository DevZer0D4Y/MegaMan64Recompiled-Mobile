#!/usr/bin/env python3
"""Verify Quest APK launch metadata, ARM64 libraries, JNI exports and loader dependencies."""
import argparse
from pathlib import Path
import struct
import subprocess
import zipfile
from verify_mobile_package import verify


def elf(data):
    if data[:5] != b'\x7fELF\x02' or int.from_bytes(data[18:20], 'little') != 183:
        raise ValueError('Expected an ARM64 ELF image')
    offset = struct.unpack_from('<Q', data, 32)[0]
    size, count = struct.unpack_from('<HH', data, 54)
    for i in range(count):
        header = struct.unpack_from('<IIQQQQQQ', data, offset + i * size)
        if header[0] == 1 and header[7] < 16384:
            raise ValueError('Native library does not support 16 KB pages')
    offset = struct.unpack_from('<Q', data, 40)[0]
    size, count = struct.unpack_from('<HH', data, 58)
    sections = [struct.unpack_from('<IIQQQQIIQQ', data, offset + i * size) for i in range(count)]
    needed, exports = set(), set()
    for section in sections:
        if section[1] not in {6, 11}:
            continue
        strings = sections[section[6]]
        table = data[strings[4]:strings[4] + strings[5]]
        def string(index):
            return table[index:table.index(b'\0', index)].decode()
        for cursor in range(section[4], section[4] + section[5], section[9]):
            if section[1] == 6:
                tag, value = struct.unpack_from('<qQ', data, cursor)
                if tag == 1:
                    needed.add(string(value))
            else:
                name, info, _, index, _, _ = struct.unpack_from('<IBBHQQ', data, cursor)
                if index != 0 and (info >> 4) in {1, 2}:
                    exports.add(string(name))
    return needed, exports


def check(path, aapt):
    verify(path)
    system = {'libc.so', 'libm.so', 'libdl.so', 'liblog.so', 'libandroid.so', 'libEGL.so',
              'libGLESv1_CM.so', 'libGLESv2.so', 'libGLESv3.so', 'libvulkan.so', 'libOpenSLES.so', 'libaaudio.so', 'libz.so'}
    with zipfile.ZipFile(path) as archive:
        libraries = {Path(name).name: elf(archive.read(name)) for name in archive.namelist()
                     if name.startswith('lib/arm64-v8a/') and name.endswith('.so')}
        if not {'libmain.so', 'libSDL2.so', 'libc++_shared.so', 'libquest_vr.so', 'libopenxr_loader.so'} <= libraries.keys():
            raise ValueError('Missing Quest or game native library')
        for name, (needed, _) in libraries.items():
            missing = needed - system - libraries.keys()
            if missing:
                raise ValueError(f'{name}: missing runtime dependencies {sorted(missing)}')
        if 'megaman64_quest_surface_enabled' not in libraries['libmain.so'][1]:
            raise ValueError('Missing Vulkan/OpenXR presentation gate')
        for method in ('nativeStart', 'nativeStop', 'nativeMode', 'nativeSurfaceEnabled'):
            if 'Java_com_devz_megaman64_QuestActivity_' + method not in libraries['libquest_vr.so'][1]:
                raise ValueError('Missing Quest JNI method: ' + method)
        dex = b''.join(archive.read(name) for name in archive.namelist() if name.endswith('.dex'))
        for descriptor in (b'Lcom/devz/megaman64/QuestActivity;', b'Lorg/libsdl/app/QuestSurface;'):
            if descriptor not in dex:
                raise ValueError('Missing Quest Java class')
    badging = subprocess.check_output([str(aapt), 'dump', 'badging', str(path)], text=True)
    if "package: name='com.devz.megaman64.quest'" not in badging or "launchable-activity: name='com.devz.megaman64.QuestActivity'" not in badging:
        raise ValueError('Incorrect Quest package or launch activity')
    manifest = subprocess.check_output([str(aapt), 'dump', 'xmltree', str(path), 'AndroidManifest.xml'], text=True)
    for marker in ('org.khronos.openxr.intent.category.IMMERSIVE_HMD', 'com.oculus.supportedDevices',
                   'android.hardware.vr.headtracking', 'org.khronos.openxr.runtime_broker',
                   'org.khronos.openxr.permission.OPENXR', 'org.khronos.openxr.OpenXRRuntimeService'):
        if marker not in manifest:
            raise ValueError('Missing Quest runtime manifest entry: ' + marker)
    print('Quest package verified: launch metadata, loader, JNI exports, dependencies and 16 KB ELF pages')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('apk', type=Path)
    parser.add_argument('--aapt', type=Path, required=True)
    args = parser.parse_args()
    check(args.apk, args.aapt)
