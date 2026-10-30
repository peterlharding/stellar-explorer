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
- **What didn't work either.** Updating the Mac to macOS 27.2 and restarting it: the Screen Mirroring icon in the menu bar was still highlighted afterwards, so the setting outlives a restart.
  Stopping Mirror My View on the headset: it disconnected the headset's stream to the Mac but left the Dell blank.
- **A wrong turn.** I took the blank Dell to be the Mac blanking its displays for Mac Virtual Display, the Mac's screen sent into the headset.
  It was not the headset at all.
- **The real cause.** A plain left-click on the menu-bar Screen Mirroring icon (a right-click only shows its menu-bar options) showed "Mirror or Extend to: DELL U3821DW", "Connected", with a black preview and a "Choose Content" button.
  macOS had made the Dell itself the target of a Screen Mirroring session, as if it were an AirPlay television, with nothing chosen to show on it.
  It fits `system_profiler` reporting the monitor, over DisplayPort, as "Television: Yes".
  Apple Vision Pro was listed in the same panel as another target, not the active one.
  The session came back after it had once been cleared, around when the headset came back online after its update.
- **What worked.** "Choose Content", then Extend: the Dell became an ordinary extended display again.
- **If it recurs.** Check System Settings → Displays → DELL U3821DW → Use as: Extended display.

## 12. Stars only near opaque objects: the compositor needs something behind them (29 October)

After both updates (macOS 27.2, visionOS 27.2), and without mirroring, the view was still black.
Each step below separated the possible causes.

- **Is anything reaching the eyes?** A `-TestObject` option put a lit 20 cm cube 1 m ahead.
  The cube was visible, so frames reach the display.
  Stars showed only in rectangular patches around the cube, patches that moved with it as the head turned.
  A camera log every two seconds showed head tracking working, though no tracking permission prompt appeared.
- **What didn't work.** Turning off OpenXR's depth layer (`xr.OpenXRAllowDepthLayer=0`): the visionOS plugin fails a check in `FOXRVisionOSSession::XrEndFrame`, which requires depth for both eyes, and the app closes when it goes immersive.
- **What didn't work.** MSAA off with the Vision Pro device profile's own `r.Mobile.XRMSAAMode=2`: no change.
- **The decisive test.** Unreal can save the frame it renders, before the compositor sees it (`-QuitAfter` takes a screenshot; it lands in the app's `Documents/StellarSpike/Saved/Screenshots/IOS/`).
  That frame had stars over the whole view.
  So the stars were rendered and the compositor was dropping them.
- **A test that tested nothing.** A black sky sphere made from the engine's sphere mesh, turned inside out with a negative scale, changed nothing in the headset, at 1,200 pc or at 20 pc.
  Black on black, its absence was invisible; the screenshot showed it: as many stars as without it.
  Unreal detects a negative scale and flips its culling to compensate, so the sphere still showed only its outside.
- **What worked.** A sphere generated in code with each triangle in both windings, opaque, unlit and so black.
  At 20 pc it hid every star beyond it, which the screenshot confirmed, and the headset showed the remaining stars all around.
  At 1,200 pc, beyond the furthest star, it hides nothing, and the headset showed the whole sky.
- **The cause.** The visionOS compositor shows only pixels with something opaque behind them.
  Additive stars write no depth and leave the background's alpha and depth empty, and the compositor treats those pixels as empty, black in full immersion.
  Near the cube there was opaque depth, which is why stars showed in patches there.
  The app now always draws the sky sphere in a headset.
- **Still to judge.** White arcs at the outer lower edges of each eye's view, which move as the head turns and vanish when looked at.
  They are not in Unreal's frame, even brightened five stops; probably glare in the lenses from the bright Sun near the edge of the view.

## 13. Tuning by eye, and what the launch options are for (29 October)

Values that only a person in the headset can judge were made launch options, set by writing the app's `Documents/uecommandline.txt` with `devicectl device copy to`, so each try needs no rebuild:
`-StarExposure=`, `-StarSize=`, `-LimitingMag=`, `-TestObject`, `-SkySphere=<pc>`, `-QuitAfter=<s>` (which saves Unreal's frame as a screenshot) and `-PointerTest`.

- **Brightness.** The Mac's exposure (0.04) was too dim in the headset; 0.12 better, 0.25 right. The app now uses 0.25 in a headset.
- **Blur.** The compositor hands Unreal 1888 × 1792 pixels per eye (`Resizing VR buffer to 3776 by 1792`), well under the panels' resolution, because the engine's SwiftUI code configures the compositor layer with foveation off (`isFoveationEnabled = false` in `SwiftMain.swift`).
  `vr.PixelDensity=1.5` was accepted and changed nothing: the compositor sets the size.
  Turning foveation on means changing precompiled engine code; not done.
- **Faint stars fade when stared at.** Troxler fading, a property of human vision, not of the app.
- **Hands.** visionOS showed the viewer's hands (`UpperLimbVisibility` automatic). Hidden with `UpperLimbVisibility=0` under `[/Script/VisionOSRuntimeSettings.VisionOSRuntimeSettings]` in `DefaultEngine.ini`, in favour of a pointer.
- **A system control** of two concentric circles stays in view and is distracting. SwiftUI can hide some system overlays (`persistentSystemOverlays(.hidden)`), but the immersive space is declared in the engine's precompiled SwiftUI code. Not done.

## 14. A pointer: point at a star and pinch to name it (29 October)

- **What the plugin offers.** The OpenXR visionOS plugin defines an `/interaction_profiles/epic/visionos_controller` profile with a pinch (`select`) and aim and grip poses per hand.
  But nothing registers that profile with Unreal's input system, and Unreal's OpenXR input only suggests bindings for profiles it knows (simple controller, Vive, Touch and the like), so the pinch reaches no input key.
  Gaze is not offered at all; visionOS keeps it from apps.
- **What worked.** Unreal's OpenXR hand-tracking plugin (`OpenXRHandTracking`), built for visionOS in the launcher engine, reading the runtime's `XR_EXT_hand_tracking`: every joint of both hands, in world space, needing no input bindings.
  The ray runs from an estimated shoulder (15 cm below and 18 cm beside the head) through the index knuckle; a pinch is the thumb tip and index tip closer than 2.5 cm, ending when they part beyond 4 cm.
  On the Mac the same code takes the view's centre and the left mouse button, so it can be checked with screenshots before it goes on the headset.
- **A trap: untracked hands read as pinching.** The first build quit two seconds after starting, having selected two stars in its second frame.
  `IHandTracker::GetKeypointState` returns the last joints it received even for a hand not being tracked, and before any, all joints at one point: a thumb-to-index gap of zero, a pinch, on both hands, which is also the exit gesture.
  `GetAllKeypointStates` reports whether the hand is tracked; the pointer now uses that, and counts a pinch only after the hand has been seen open since tracking began.
- **A trap: the engine's text is lit.** The label did not show. Its material, `DefaultTextMaterialOpaque`, is lit (`MSM_DEFAULT_LIT`, the text colour as base colour), and nothing lights this scene.
  A material of my own that sampled the font gave solid blocks, not letters.
  What worked: the material script copies the engine's text material and makes the copy unlit, with the text colour as emission, keeping Epic's handling of the font.
- **A bug the pointer exposed.** `-LookAt` had put the camera 0.001 units behind the Sun, not in front as its comment said, so the Sun was dead ahead, hidden only by the near clipping plane; the pointer duly selected it. Fixed, and the pointer now ignores stars nearer than the near plane.
- **From the first session in the headset.**
  The rays followed the hands; selection worked but intermittently; a label for a star below the viewer was edge-on, because it turned only to face the viewer's direction horizontally; the rays were too bright; the two-handed exit worked.
- **Changed for the next session.**
  A pinch now selects what the ray pointed at 0.15 s before, since pinching moves the hand and the ray with it; and the log records each pinch's measured gap, for tuning the thresholds.
  The label faces the viewer square on, below the star as seen from there.
  The ray is a line of 18 small dim dots from 10 cm to 3 m, each the same apparent size.

## 15. A panel instead of a label, and a steadier ray (30 October)

- **From the second session.** Selection was more reliable but still often missed, and pinching visibly moved the ray off the star.
  The ray could hardly be seen: the dots that answered "too bright" were 1.5 mm across and nearly black.
- **What the log showed, once read properly.** Every pinch that registered selected a star; my first search of the log matched only names starting with a capital, so "22 Eri" and "tau Pup" looked like failures.
  The gaps logged at each pinch (1.7 to 2.5 cm) are the first readings under the 2.5 cm threshold, so they say nothing about pinches that never got that close.
  In a third of pinches the star under the ray at the pinch differed from the one 0.15 s before: the hand does move.
- **Changes.**
  - The ray passes through the middle of the hand (the `Palm` joint) instead of the index knuckle, since the palm moves less as the fingers close, and its direction is smoothed over about 50 ms.
  - A pinch selects what the ray was on before thumb and index began to close (the last frame they were 4.5 cm apart, up to half a second back), not a fixed 0.15 s before.
  - A pinch starts below 3 cm, not 2.5, and the log now records hands that closed without reaching that ("Near pinch ... at closest"), for tuning it.
  - The dots are twice the size and brighter.
- **The panel.** The label beside the star is replaced by a panel with an outline and a clear interior, 1 m away at the bottom right of the view, turning with the head a little behind it.
  It describes the star the ray is on as it moves, so reading a star's details no longer needs a pinch; a pinch keeps a star, which the panel describes when nothing else is pointed at.
  The previous approach is kept on the `pointer-label` branch, for comparison.
- **A trap: the tonemapper washes out overlay colours.** On the Mac the text came out white and the amber selection ring pale cream.
  Unreal's filmic tonemapper desaturates bright colours towards white, so the overlays are now drawn at 40% of their intended colour.
  And `UTextRenderComponent`'s colour reaches the material as vertex colour without the sRGB-to-linear conversion, so the text colour is passed as linear (`ToFColor(false)`) to match the rings.

## 16. Keeping a star, and why the ray was black (30 October)

- **From the third session.** The panel was a lot better; taps still sometimes failed to select; a selected star's details should stay put long enough to read; and the ray was black, seen only as it hid the stars behind it.
- **A trap: a material on instanced meshes in a cooked build.** The dots are instances of one sphere (`UInstancedStaticMeshComponent`), and a material needs the "used with instanced static meshes" usage flag to draw them.
  The editor sets the flag the first time it meets that use, so on the Mac nothing looked wrong; a cooked build cannot, and silently draws the dots with the default lit material, which is black in an unlit scene, with no warning in the device log.
  The rings use the same material on procedural meshes, which need no flag, so they showed.
  What worked: the material script sets `used_with_instanced_static_meshes`.
- **What the log said about the taps.** A relaxed pointing hand rests with thumb and index 4 to 4.5 cm apart, which is where the pinch's release threshold was.
  So a pinch could fail to end, and then the next tap had no start to detect; and a pinch that took "before the fingers closed" to mean "the last time they were 4.5 cm apart" could reach back half a second, to while the ray was still sweeping.
  Near misses at 3.3 to 3.9 cm were each followed within two seconds by a pinch that worked: tries that stopped short of the 3 cm threshold.
- **Changes.**
  - A pinch starts below 3.5 cm, and ends once thumb and index part 1 cm from the closest they came, or beyond 4.5 cm.
  - A pinch keeps the star the ray was on as the fingers began to close: the last frame they were within 1 mm of their widest in the half second before, whatever the hand's resting gap.
  - A kept star holds the panel: pointing elsewhere does nothing, and two pinches within 0.6 s let it go. A line at the foot of the panel says what a pinch will do.
- **From the fourth session.** Much better, and the ray visible; but a ray on each hand made no sense.
  Now only the pointing hand has a ray: `PointingHand=Right` in `DefaultGame.ini`, or `-PointingHand=Left` on the command line, for left-handed viewers.
  The other hand is tracked only for the two-handed exit.
  While a star is kept the ray is hidden, and it returns when two pinches let the star go.
- **Telling builds apart.** Four headset builds in a morning, all reporting Unreal's defaults (version 1.0, build 58902052, which is the engine's changelist), left only file times to say which code a session ran.
  `spike.sh` now writes the commit, with "+changes" for uncommitted work, the branch and the build time into a git-ignored `BuildStamp.inl`; the game logs it at start, and the panel shows it when there is nothing to describe, as at start-up.
  The splash window would be the natural place, but it is the engine's own SwiftUI code (`SwiftMain.swift`, with fixed text), compiled into the launcher engine and holding the app's entry point, so a project cannot change or replace it.
- **From the fifth session: contact bounce.** A pinch kept a star for an instant, and then the ray came back.
  The log showed why: 0.06 to 0.2 s after nearly every pinch that kept a star, a second pinch began, which counted as the double pinch that lets the star go.
  As thumb and index touch they hide each other from the headset's cameras, and the measured gap jumps by a centimetre or more for a frame or two, enough to end the pinch and start another.
  Two fixes: a pinch now ends only once the fingers have stayed apart for 0.15 s, and for a second after a star is kept or let go further pinches are ignored (`GestureGateSeconds` in `DefaultGame.ini`, or `-GestureGate=`), with each ignored pinch logged.
- **From the sixth session: keeping is solid, letting go is not.** The pinches made to let a star go came 0.53 to 0.75 s apart, mostly outside the 0.6 s window; the new debounce adds 0.15 s to every pinch's release.
  And a pinch could begin the moment the last ended, with the fingers barely apart (the log's "widest 1.9 cm"), adding pinches nobody made.
  Now two pinches within 1 s let a star go (`DoublePinchSeconds`, or `-DoublePinch=`), and each pinch must close by 1 cm from the widest the fingers opened since the last.
- **From the seventh session: quick pinches refused.** Letting go needed slow, deliberate pinches, and a run of several pinches counted only the last two.
  Between quick pinches the fingers open only partway, and requiring each pinch to close by 1 cm from that opening refused pinches that reached 2.2 cm, well inside the threshold; the log's near misses show them.
  Now a pinch need only close by 0.6 cm, and the release debounce is 0.1 s rather than 0.15 s, since the jumps it guards against last a frame or two.
- **The ray waits to be asked for.** At start the ray was already on; PLH wanted an explicit action to begin.
  In a headset the ray is now off until a first pinch, which is followed by the same one-second gate as keeping a star, so that pinch cannot also keep one.

## Harmless noise in the logs

- "Ensure condition failed: ViewController" in `FIOSApplication::HandlePhysicalKeyboardConnectionChanged`: a handled ensure when a keyboard is reported at start-up, with no UIKit view controller on visionOS.
- "you still need to add "remote-notification" to the list of your supported UIBackgroundModes".
- "This app is in LANDSCAPE mode".

## Still open

- The visionOS build renders with the mobile renderer on `SF_METAL_ES3_1_IOS`, which supports neither instanced stereo nor multiview ("bIsMobileMultiViewEnabled=0"): each eye is drawn separately.
  Even so it holds about 95 frames a second with every star.
- Sharpness: foveation is off in the engine's precompiled SwiftUI code (section 13).
- The system's concentric-circle control in view (section 13).
- White arcs at the outer lower edge of each eye, not in Unreal's frame; probably lens glare from the Sun.
- One ray, hidden while a star is kept (section 16), not yet tried in the headset.
