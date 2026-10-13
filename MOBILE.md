# Mobile use

## Touch controls and controllers

Gameplay and menu pads have separate saved layouts. Tap EDIT, drag controls, adjust size/opacity/color, then tap DONE. Reset affects the selected profile. GAME PAD returns from the menu pad; MENU PAD provides menu navigation. The launcher uses its menu pad until a game has started.

SDL-compatible USB/Bluetooth gamepads are supported. Pair the controller through the device settings, then use Setup controls to configure bindings. Controller Back cancels menus; controller removal releases its inputs. Physical controller compatibility depends on the device/OS and should be tested with your controller.

## Saves

The USA save identity remains `megaman.n64.us.1.0`. FlashRAM saves are 128 KiB. Writes are serialized, flushed, and atomically replaced, with a backup of the previous save. Backgrounding the app requests a save flush. Avoid uninstalling the app without a backup.

On iOS, open Mega Man 64 in Files and back up the `config/saves` directory. Restore that directory into the new app's Documents/config folder while the app is closed. Preserve save filenames.

On Android, files live in the app's private files directory under `config/saves`. The system file picker imports ROMs; it does not expose private saves. Backup/restore requires access to that app's private storage (for example, appropriate developer/device backup tools). A different package ID cannot read the previous package's private files. Keep the old app installed until your save backup is safe.

## Rendering and lifecycle

The mobile patches include iOS Metal argument-buffer fixes and screen-size restoration after screenshots/notifications. Android uses supported RGBA surface formats, handles replacement native windows, validates surface dimensions, and reserves Vulkan descriptor-pool capacity for variable bindings. Startup failures are recorded in `megaman64-startup.log` when file access permits.

Gameplay and rendering were confirmed by the tester on iOS and an Adreno 710 Android device before the publication cleanup. The new icon/IDs and compiler path mapping are packaging changes; this release still needs installation verification on each device. Other Vulkan GPUs may have different requirements.

## IPA signing

The IPA contains one `Payload/MegaMan64Recompiled.app` bundle and no provisioning profile or personal signing certificate. Import it into eSign or another signing tool and sign it with your own certificate/profile. The bundle ID is `com.devz.megaman64`.
