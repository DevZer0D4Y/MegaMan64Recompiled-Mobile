# Quest VR preview

Preview 2 (`0.9.1-quest-preview.2`, Android version code 4) corrects the upside-down image by flipping the OpenXR layer's image sampling vertically. It keeps the same application ID and signing identity, so it installs over preview 1 and preserves that app's ROM/config/save storage. The runtime must support `XR_FB_composition_layer_image_layout`; this extension is checked and enabled before use.

This is an experimental standalone Meta Quest build with OpenXR headset tracking and Quest Touch controller input. The game is presented on a 3.2 × 1.8 metre screen, anchored three metres ahead of the viewer. Both eyes see the same game image on that screen; the compositor handles the screen's position as you move your head.

**This is a virtual-screen VR mod. The game world is not rendered in stereoscopic 3D, and head motion does not steer the game's camera.** RT64 currently renders one game view. Full 3D VR requires a separate per-eye render path, camera tracking, and HUD/cutscene handling.

The app ID is `com.devz.megaman64.quest`, so it coexists with the normal Android app and has independent ROM/config/save storage. It requires a compatible Quest runtime with Horizon OS v62 or newer, OpenXR, OpenGL ES 3, Vulkan, and the Android Surface swapchain extension. The manifest targets Quest 2, Quest Pro, Quest 3, and Quest 3S. Actual device compatibility and performance have not been verified.

## Install and import ROM

Install the Quest APK using ADB or your usual sideloading tool, then launch Mega Man 64 Quest from Unknown Sources. Import your own supported USA ROM with the launcher. If the headset lacks an Android document picker, copy the ROM to the app's external files directory and restart:

```sh
adb shell mkdir -p /sdcard/Android/data/com.devz.megaman64.quest/files
adb push /path/to/your/MegaMan64.us.rev1.z64 /sdcard/Android/data/com.devz.megaman64.quest/files/
```

No ROM is distributed. The USA save identity is unchanged; the VR app's private storage is separate. See MOBILE.md before trying to transfer saves from another app ID.

## Quest Touch controls

| Quest input | Game input / function |
|---|---|
| Left stick | Movement; menu navigation |
| Right A / B | N64 A / B; menu Confirm / Back |
| Left X | C-Up |
| Left Y or right stick click | N64 Start; Y applies app menu settings |
| Right stick | C buttons |
| Left trigger | N64 Z |
| Right trigger or right grip | N64 R |
| Left grip | N64 L |
| Left controller Menu | App options / Back |
| Left stick click | Recenter the VR screen ahead of your current head position |

The Quest system button remains controlled by the OS. Controller pointers/hands are not drawn; use stick-and-button navigation. Gameplay keeps the game's original camera controls. App menus and cutscenes stay on the same VR screen. Touch overlays are omitted from this variant.

## Build

Follow BUILDING.md to generate the game sources and prepare the mobile dependency patches. The updated Plume patch includes a presentation gate: it stops Android Surface submissions before ending an XR session and consumes pending Vulkan semaphores without posting hidden frames.

```sh
python3 scripts/quest.py --prepare
python3 scripts/quest.py --build --ndk "$ANDROID_NDK_HOME"
android/gradlew -p android assembleQuestDebug \
  -PhostDxc="$PWD/build-android-host/dxc-host" \
  -PhostFileToC="$PWD/build-android-host/file_to_c/file_to_c"
```

The bridge uses the checksum-pinned Khronos OpenXR Android loader 1.1.63, licensed under Apache-2.0; see OPENXR-LICENSE.txt. `android/quest/libs` and downloaded dependencies are ignored. Release APKs require private signing, like the regular Android build.

## Validation status

A host test exercises 100 in-flight-frame suspension/drain/resume handshakes using the actual presentation gate. ARM64 compilation, native linkage, Java/Dex generation, package IDs/manifest metadata, private-path/ROM scans, signing, and archive structure are checked during packaging. A physical Quest is still required to verify first launch, controller navigation, Vulkan-to-OpenXR Surface presentation, file picker returns, sleep/resume, recentering, and frame rate. Run the packaging check with your Android SDK's `aapt`:

```sh
python3 scripts/verify_quest_package.py path/to/Quest.apk --aapt "$ANDROID_HOME/build-tools/35.0.0/aapt"
```

Startup errors are shown in a dialog and tagged `MegaManQuest` in logcat. Do not treat the preview as a validated full 3D VR conversion.

References: [Meta's OpenXR Android setup](https://developers.meta.com/vr/documentation/native/android/mobile-openxr/) and [Khronos Android Surface swapchains](https://registry.khronos.org/OpenXR/specs/1.1/man/html/xrCreateSwapchainAndroidSurfaceKHR.html).
