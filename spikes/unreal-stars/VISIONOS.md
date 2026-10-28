# Getting an Unreal Engine 5.8 app running on Apple Vision Pro

A log of what it took to get the Unreal spike from compiling on a Mac to rendering in full immersion on a Vision Pro.
It records what failed as well as what worked, with the evidence that pointed at each cause.
It is kept as the work goes, as material for an article.

The setup: Unreal Engine 5.8.4 installed from the Epic Games Launcher (not built from source), on an M1 Max running macOS 27.0.1 with Xcode 27.1 and the visionOS 27.0 SDK.
The app is C++ only: a game mode spawns a star field of 113,012 stars, read from a 215 MB SQLite catalogue, into the engine's empty map.
It uses the OpenXR and OpenXR visionOS plugins, forward shading and 4× MSAA.

## 1. Installing the visionOS platform (28 October)

The iOS and visionOS target platforms are optional downloads in the Epic Games Launcher.

- **What didn't work.** Looking for them under "Unreal Engine → Library → 5.8 → Options", a menu path that does not exist.
  They are under the engine's Installation Options, Target Platforms.
- **What didn't work.** Queuing iOS, tvOS and visionOS together.
  While a download is queued, the options dialog cannot be opened to untick tvOS.
  The download sat for 40 minutes without progress.
- **What didn't work.** Retrying with visionOS alone: "Plugin download and install failed".
  visionOS needs iOS.
- **The cause of the stalls.** The launcher will not change an engine while any of that engine's editors is running.
  It does not say so; the install just never starts.
  Two editor processes left over from earlier work were holding it.
- **What worked.** Closing every Unreal Editor, then installing iOS first and visionOS after it.

## 2. Enabling the OpenXR visionOS plugin (28 October)

- **What didn't work.** Enabling the `OpenXRVisionOS` plugin in the `.uproject` before the visionOS platform was installed.
  The editor would not start: "Missing Modules: OXRVisionOSSettings".
- **What worked.** Disabling the plugin until the platform component was installed, then enabling it again.

## 3. Compiling and signing (28 October)

- **Toolchain.** Unreal 5.8 states "Xcode 15.3 or later".
  It compiled and linked for visionOS with Xcode 27.1 and the visionOS 27.0 SDK without changes.
- **Signing.** The build stopped at code signing until the developer's team was set.
  The team ID goes in `Config/UserEngine.ini`, which is git-ignored, under `[/Script/MacTargetPlatform.XcodeProjectSettings]` as `CodeSigningTeam=`.
  The shared settings, `CodeSigningPrefix` and `bUseAutomaticCodeSigning=True`, are in `DefaultEngine.ini`.
  With those, `RunUAT BuildCookRun -platform=VisionOS ... -package -archive` produced a signed 589 MB `.app`.

## 4. Pairing the headset (29 October)

Vision Pro has no USB port of its own; the Developer Strap replaces the right audio strap and gives a USB-C data link to the Mac.
Developer Mode is in Settings → Privacy & Security on the headset.

- **What didn't work.** Xcode's Devices and Simulators window showed the headset as paired, but `xcrun devicectl device install app` failed: "The device must be paired before it can be connected".
  `devicectl list devices` showed it as `available`, not `available (paired)`.
- **What worked.** `xcrun devicectl manage pair --device <UDID>`, after which the state read `available (paired)`.

## 5. Installing: the provisioning profile (29 October)

- **What didn't work.** Installing the package built the night before.
  "This provisioning profile cannot be installed on this device" (`0xe8008012`).
  The embedded profile (`security cms -D -i embedded.mobileprovision`) listed five devices, none of them the headset: it was made before the headset was registered with the team.
- **Why rebuilding alone doesn't fix it.** Unreal runs `xcodebuild` with `-allowProvisioningUpdates` but not `-allowProvisioningDeviceRegistration`, and against a generic visionOS destination, so it never registers a new device.
- **What worked.** Running the same `xcodebuild` command Unreal logs, against the headset (`-destination id=<UDID>`) and with `-allowProvisioningDeviceRegistration`.
  That registered the headset and regenerated the team profile; every later Unreal package picks it up.

## 6. The app starts, shows a grey window, and closes (29 October)

The symptom: a grey window titled "Unreal SwiftMain Splash Screen!", reading "Waiting for configuration...", with a greyed-out "Go Immersive!" button and a three-second countdown.
At zero the window closes and nothing else appears.
Starting the app again afterwards does nothing, because the process is still alive with no window.

The console (`devicectl device process launch --console`) showed only the first lines of start-up and then `SwiftUIView Open()`; no Unreal log file was ever written.

Reading the engine source explained the window: on visionOS the app is a SwiftUI app (`Engine/Source/Runtime/Launch/Private/Apple/SwiftMain.swift`).
The window waits for the engine to send its immersion settings, which it does once its configuration files are loaded; after three seconds it opens the immersive space anyway and closes itself.
So the engine was stalling before it had read its configuration.

### First cause found: the content was not in the app

- **Evidence.** The package contained a `cookeddata/ue.projectstore` file pointing at a Zen server on the Mac (port 8558, the Mac's LAN addresses) and almost no cooked content.
  A Development build made without `-pak` streams its content from the Mac's Zen store.
- **What worked.** Adding `-pak -iostore` to `BuildCookRun`, so the cooked content is inside the app.
- **A trap.** The archive directory is merged into, not replaced: the stale `ue.projectstore` from the earlier build stayed in the new package.
  Delete `Saved/Archive/VisionOS` and `Saved/StagedBuilds/VisionOS` before packaging.
- **Not the whole story.** With the content in the app, the symptom was unchanged.

### What didn't work: debugging from the command line

- `lldb` with `device select <UDID>` and `device process attach --pid <pid>` printed nothing and left no stopped process to inspect ("Process must be launched").
  An attach left hanging also left the app unresponsive on the headset.
- Pushing a command line with `-stdout` to the app's `Documents/uecommandline.txt` produced no more output either, which itself said the engine had not reached the point where it reads its command line.

### Second cause found: nothing starts the engine's game thread

- **Evidence.** In `IOSAppDelegate.cpp`, `application:didFinishLaunchingWithOptions:` starts the game thread only `#if !UE_IOS_SCENE_LIFECYCLE`.
  With the scene-based lifecycle, `IOSSceneDelegate`'s `scene:willConnectToSession:options:` starts it instead.
  `BaseEngine.ini` in 5.8 sets `bUseSceneBasedLifecycle=True`.
  But in a SwiftUI app SwiftUI owns the scenes and their delegates; the final `Info.plist` had an empty scene configuration, and `IOSSceneDelegate` is never called.
  The engine never starts, and nothing reports it.
- **What didn't work.** Setting `bUseSceneBasedLifecycle=False` for the project.
  The project's own code rebuilt with `UE_IOS_SCENE_LIFECYCLE 0`, but only the project's ten files were compiled: a launcher install links the engine from precompiled objects, and `Module.ApplicationCore.cpp.o` for visionOS was built with the scene lifecycle on (it contains `-[IOSSceneDelegate scene:willConnectToSession:options:]`).
  The setting cannot change it.
- **What worked.** Doing in the project what UIKit would have done: `Source/StellarSpike/VisionOSStartup.cpp` observes `UISceneWillConnectNotification` and hands the first window scene to an `IOSSceneDelegate`.
  After that the window showed "ImmersiveStyle: FullImmersionStyle() UpperLimbVisibility: automatic", the button became active, and the engine started.

## 7. The first frame crashes in OpenXR (29 October)

- **Evidence.** The Unreal log (copied off the headset with `devicectl device copy from --domain-type appDataContainer`) and the crash report (`--domain-type systemCrashLogs`) both showed a failed `check` in `FOpenXRHMD::AllocateDepthTexture`, called from the mobile renderer's scene textures.
- **Cause.** The engine's `VisionPro` device profile sets `r.Mobile.XRMSAAMode=2`: use the headset's depth swapchain directly as the multisampled scene depth.
  The 5.8 OpenXR plugin always allocates that swapchain with one sample (`NumSamplesExpected = 1`), so with MSAA on, `check(DepthSwapchainDesc.NumSamples == NumSamples)` fails.
- **What worked.** `Config/DefaultDeviceProfiles.ini` overriding the `VisionPro` profile with `r.Mobile.XRMSAAMode=0`, where the multisampled scene depth is resolved into the one-sample swapchain.
  4× MSAA stays on.

## 8. The catalogue loads no stars (29 October)

- **Evidence.** "Loaded 0 stars", with no error, from a database that opened and a query that prepared.
  The code ignored the result of `Execute`.
- **Cause.** The catalogue's `star` view pivots its measurement table with a `GROUP BY`, which needs a temporary table; SQLite puts that in a file, and on the Mac it can, but in the visionOS app sandbox it cannot.
- **What worked.** `PRAGMA temp_store = MEMORY` after opening, and treating an `Execute` result of `INDEX_NONE` as an error.
  All 113,012 stars then loaded, in 2.97 s.
- **For the real app.** Ship a render table built from the preferred values rather than querying a pivoting view on the device.

## 9. The app is killed when the immersive space opens (29 October)

- **Evidence.** The console: `NSInternalInconsistencyException`, "This app failed to request an authorization. The NSWorldSensingUsageDescription / NSHandsTrackingUsageDescription ... keys may be missing from this app's Info.plist."
  The OpenXR runtime starts an ARKit session for head and hand tracking, and visionOS requires a reason for each.
- **What didn't work.** Putting `AdditionalPlistData` in `Config/VisionOS/VisionOSEngine.ini`.
  The `Info.plist` template is generated from the iOS configuration hierarchy, which does not read `Config/VisionOS`.
- **What worked.** `AdditionalPlistData` under `[/Script/IOSRuntimeSettings.IOSRuntimeSettings]` in `DefaultEngine.ini`, with both usage descriptions.
  The OpenXR session then went through `READY`, `SYNCHRONIZED`, `VISIBLE` and `FOCUSED`: the app was rendering in full immersion.

## 10. Meanwhile, the launcher broke the Mac build (29 October)

- **Evidence.** After adding `VisionOSStartup.cpp`, the Mac editor build failed in under two seconds: "Expecting to find a type to be declared in a module rules named 'FabToolset' in 'UE5Rules'".
  The launcher had updated the engine's Fab plugin at 06:27 that morning, adding a `FabToolset` module, but the engine's precompiled rules assembly (`Engine/Intermediate/Build/BuildRules/UE5Rules.dll`, from the day before) did not include it.
  Builds kept working until a new source file made UnrealBuildTool read the rules again.
- **What didn't work.** Moving the stale rules assembly aside so it would be rebuilt: an installed engine refuses to build without it ("Precompiled rules assembly ... does not exist").
- **What worked.** Disabling the Fab plugin, which the project does not use, in the `.uproject`.

## 11. Black in the headset, and a Mac display taken hostage (29 October)

The headset was running a visionOS 27.2 beta; the Mac, macOS 27.0.1.

- **The black view.** With Mirror My View sending the headset's view to the Mac, the app opened into full immersion and showed only black.
  The log showed it running normally: 30,696 frames in 5 minutes 22 seconds, about 95 a second, all 113,012 stars loaded, and no shader or material errors.
  Stopping the mirroring invalidated the compositor layer (`cp_layer_renderer_state_invalidated`), and the OpenXR session asked the app to exit, as it should.
  Why no stars were visible is not yet known; the next look is without mirroring.
- **The hostage display.** Afterwards the Mac's Dell U3821DW (38", 3840×1600) stayed black with "Choose to mirror from the … menu", unusable: the pointer could not give it focus, so neither clicks nor keys reached it.
  macOS still saw the monitor on its cable but no longer listed it as a display.
  The window covering it, found with `CGWindowListCopyWindowInfo`, was Control Center's "Screen Mirroring shield", sized to that monitor.
- **What didn't work.**
  Looking for a Stop control in the Mac's Screen Mirroring settings: there was none.
  Unplugging and reconnecting the monitor.
  Turning off AirPlay Receiver.
  Restarting Control Center (`killall ControlCenter`): it came back and put the shield straight up again, so the state lives elsewhere.
  Restarting the headset.
- **Next.** Updating the headset to the visionOS 27.2 release candidate and the Mac to macOS 27.2, which restarts the Mac and should clear the shield.

## Paused (29 October)

Work stopped here to update both machines.
On resuming:

1. Check the Dell U3821DW is a display again (`system_profiler SPDisplaysDataType` lists it).
2. Check Xcode still works after the macOS update, and whether it wants an update or a new visionOS SDK; open it once so it can install its components.
3. Reconnect the headset; Xcode may "prepare" it again for the new visionOS, which takes a few minutes.
   Check `xcrun devicectl list devices` shows it `available (paired)`; if not, `xcrun devicectl manage pair --device 00008112-000A088C11C1A01E`.
4. If the app is gone, `./spike.sh install 00008112-000A088C11C1A01E`; if the SDK changed, rebuild first with `./spike.sh visionos`.
5. Launch it without mirroring and look for the Sun, a bright white blob about 1.5 m ahead.
   If the view is still black, add a large bright test object a metre ahead to tell "nothing renders" from "the stars do not show".

## Harmless noise in the logs

- "Ensure condition failed: ViewController" in `FIOSApplication::HandlePhysicalKeyboardConnectionChanged`: a handled ensure when a keyboard is reported at start-up, with no UIKit view controller on visionOS.
- "you still need to add "remote-notification" to the list of your supported UIBackgroundModes".
- "This app is in LANDSCAPE mode".

## Still open

- The visionOS build renders with the mobile renderer on `SF_METAL_ES3_1_IOS`, which supports neither instanced stereo nor multiview ("bIsMobileMultiViewEnabled=0"): each eye is drawn separately.
- Frame rate, comfort and how the stars look in the headset.
- Input: OpenXR warns that no interaction profiles are bound, so no input actions work yet.
