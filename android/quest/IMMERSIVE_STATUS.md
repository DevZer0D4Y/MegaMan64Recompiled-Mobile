# Immersive VR conversion — development status

This branch is work in progress toward the requested first-person Quest conversion. **There is no playable immersive APK yet.** The published Quest preview remains a virtual-screen app. It does not have tracked game arms, motion-aimed weapons, or a stereoscopic game world.

The intended immersive APK will have a separate application ID (`com.devz.megaman64.quest.immersive`) and independent ROM/config/save storage. Do not relabel the existing preview as the immersive version.

## Implemented foundation

- OpenXR grip and aim pose actions for both controllers, with separate action spaces.
- Predicted-display-time head pose, individual eye poses and asymmetric FOVs, grip/aim poses, triggers, squeeze values and thumbsticks.
- Versioned, fixed-layout C tracking ABI between the XR producer and game/render consumers.
- Coherent snapshots, quaternion normalization, invalid/stale-pose rejection, tracking-loss trigger suppression, and independent Android-resumed/OpenXR-focused activation gates.
- Reference-space/recenter generations for consumers to reset their world origin.
- CPU view/projection and controller aim-ray math using each eye's actual pose and FOV; this is not yet connected to game rendering.

Validation: Android ARM64 compilation of the bridge and tracking module; host AddressSanitizer/UndefinedBehaviorSanitizer tests for ABI compatibility, stale/lost tracking, invalid data, concurrent publication, suspension races, distinct eye transforms, projection boundaries/depth, and aim direction. No headset testing has occurred.

Run the host checks without a ROM or headset:

```sh
c++ -std=c++17 -pthread -fsanitize=address,undefined -Iinclude src/main/immersive_tracking.cpp tests/vr_tracking.cpp -o /tmp/vr-tracking-tests
/tmp/vr-tracking-tests
```

These tests verify tracking transport and camera math, not stereo game rendering or weapon gameplay.

## Required before an immersive APK

1. Replay each RT64 game frame for two separate eye render targets, using the same game state and tracking snapshot. Present real per-eye images through an OpenXR projection layer. Do not duplicate a mono image into both eyes.
2. Attach the head view to a verified player head/world anchor, calibrate metres to game units, and handle collision, teleports, menus and cutscenes.
3. Render tracked left/right arms and the equipped weapon, with a verified player-body hiding rule that does not hide unrelated actors.
4. Connect the controller aim pose to actual weapon muzzle, projectile direction and hit detection. Preserve the game's ammo, damage, upgrades and save identity.
5. Provide appropriate VR locomotion/turning and interaction handling, then validate on the target Quest.

## Source investigation

RT64's `ProjectionProcessor::processScene` currently uploads one modified view/projection per game projection. `WorkloadQueue::threadRenderFrame` already supports re-rendering interpolated frames into override render targets; this is a possible stereo integration point, but framebuffer dependencies and presentation synchronization must be preserved. `PresentQueue` currently composites one color target.

The project's `PlayerState` exposes position/yaw and equipped buster parts, but most actor fields remain unknown. `func_8003B2B4_166B4` receives the player state for shadow placement. `D_802059A0_1E0DA0` is the tagged player-projectile actor array; its velocity/muzzle/collision fields are not mapped by the current structs. The tagged model functions `func_80084870_5FC70` and `func_8008498C_5FD8C` are leads for player model rendering. These observations are not sufficient to safely rewrite weapon physics or claim first-person rendering is working.
