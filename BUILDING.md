# Building from source

Clone recursively so each dependency is checked out at its pinned revision:

```sh
git clone --recursive https://github.com/DevZer0D4Y/MegaMan64Recompiled-Mobile.git
cd MegaMan64Recompiled-Mobile
python3 scripts/android.py --prepare
```

The preparation script fetches SDL 2.32.10 and FreeType 2.13.2, applies mobile patches to RT64, Plume, N64ModernRuntime, and SLJIT, and builds host `file_to_c` and a DXC wrapper. It is shared by Android and iOS. Patches are intentionally stored in the parent repository: uncommitted dependency edits alone would not survive a clone.

## Generate game code locally

Place your own USA ROM in the root as `MegaMan64.us.rev1.z64`. Its SHA-1 is `f24fe0aff01aec018e2dd558ec4f076cf328129f`. Generated sources and ROMs are ignored and must not be committed.

Install CMake, Ninja, Python 3, and LLVM/LLD with the MIPS backend. Apple Clang alone cannot compile the MIPS patches. Build the host tools and generate game and RSP code:

```sh
cmake -S lib/N64ModernRuntime/N64Recomp -B build-host/recompiler -DCMAKE_BUILD_TYPE=Release
cmake --build build-host/recompiler --target N64RecompCLI RSPRecomp --parallel
build-host/recompiler/N64Recomp us.rev1.toml
build-host/recompiler/RSPRecomp aspMain.us.rev1.toml
make -C patches CC=clang LD=ld.lld
build-host/recompiler/N64Recomp patches.toml
build-android-host/file_to_c/file_to_c patches/patches.bin mm_patches_bin RecompiledPatches/patches_bin.c RecompiledPatches/patches_bin.h
python3 scripts/android.py --check
```

Use explicit LLVM executable paths if your default `clang` lacks MIPS. With CMake 4, older dependency projects may require `-DCMAKE_POLICY_VERSION_MINIMUM=3.5`. Do not substitute recompiler revisions without checking symbol/config compatibility.

## Android

Install JDK 17, Android SDK 35, NDK `27.2.12479018`, and SDK CMake `3.22.1`. Set `ANDROID_HOME` or create an untracked `android/local.properties` with `sdk.dir` pointing to your SDK. From the repository root:

```sh
android/gradlew -p android assembleDebug \
  -PhostDxc="$PWD/build-android-host/dxc-host" \
  -PhostFileToC="$PWD/build-android-host/file_to_c/file_to_c"
```

The APK appears at `android/app/build/outputs/apk/debug/app-debug.apk`. For release packaging, use `assembleRelease`, align with `zipalign -P 16`, then sign using a privately stored key with `apksigner`. Signing keys/passwords must remain outside the repository. Only `arm64-v8a` is supported.

## iOS

Use macOS with Xcode and the iPhoneOS SDK. Generate the game sources above, then build the host Metal shader converter:

```sh
cmake -S ios/host-tools -B build-ios-host -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-ios-host --target spirv_cross_msl_ios
cmake -S . -B build-ios -G Ninja \
  -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphoneos \
  -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=16.0 \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  -DRECOMP_METAL_SDK=iphoneos -DRECOMP_METAL_SOURCE_SHADERS=ON \
  -DRECOMP_HOST_DXC="$PWD/build-android-host/dxc-host" \
  -DRECOMP_HOST_FILE_TO_C="$PWD/build-android-host/file_to_c/file_to_c" \
  -DRECOMP_HOST_SPIRV_CROSS_MSL="$PWD/build-ios-host/spirv_cross_msl_ios"
cmake --build build-ios --target MegaMan64Recompiled --parallel
python3 scripts/package_ios.py build-ios/MegaMan64Recompiled.app MegaMan64-iOS.ipa
```

Source-shader mode compiles Metal shaders on the device and does not require the optional offline Metal compiler. First launch can take longer. The IPA needs device signing. Do not include AppleDouble `._` files, ROMs, saves, or provisioning profiles in public packages.

## Publication checks

```sh
python3 scripts/check_public_source.py
python3 scripts/verify_mobile_package.py path/to/app.apk
python3 scripts/verify_mobile_package.py path/to/app.ipa
```

Compiler prefix mapping removes local checkout paths from diagnostic strings. Release executables should be stripped and the finished archives scanned again; clean source alone does not guarantee a clean binary. CI checks the public source without needing a ROM. Full mobile builds require your own ROM and installed SDKs.
