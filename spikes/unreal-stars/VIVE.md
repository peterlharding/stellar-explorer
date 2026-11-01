# Getting the Unreal Engine 5.8 spike running on an HTC Vive Pro 2

A log of what it took to get the Unreal spike from running on the desktop and the Vision Pro to running in an HTC Vive Pro 2 on Windows, with its controllers pointing and selecting.
As with [VISIONOS.md](VISIONOS.md), it records what failed as well as what worked, with the evidence that pointed at each cause, and is kept as the work goes, as material for an article.

The setup: Unreal Engine 5.8.4 installed from the Epic Games Launcher (not built from source), on Windows 11 with Visual Studio 2022's C++ game workload.
PLH works in WSL2 (mirrored networking) with the checkout on the Windows drive at `D:\src\Projects\stellar-explorer` (`/mnt/d/...` in WSL), because Unreal and Visual Studio are slow with, and sometimes break on, paths inside WSL's own filesystem.
`spike.sh` runs under WSL and calls Unreal's Windows programs (`UnrealBuildTool.exe`, `UnrealEditor.exe`) with paths translated by `wslpath`; `./spike.sh vive` starts the game in the headset.
The headset ran through HTC's VIVE OpenXR runtime.
It is the same C++ code and the same 215 MB SQLite catalogue as the other two targets.

## 1. Building the catalogue from WSL times out (30 October)

- **Evidence.** `make catalogue` hung fetching the Hipparcos source and failed with a connection timeout (`Errno 110`) to `cdsarc.cds.unistra.fr`; `Test-NetConnection cdsarc.cds.unistra.fr -Port 443` from PowerShell reported `TcpTestSucceeded: False`, while `codeberg.org` succeeded.
- **What didn't work.** A Windows Firewall outbound rule for the host: PowerShell was not elevated (`New-NetFirewallRule: Access is denied`), and it was moot anyway — `Get-NetFirewallProfile` showed `DefaultOutboundAction` as `NotConfigured`, that is, allowed, so the firewall was not the block.
- **Cause.** The route from this WSL box to the CDS host in Strasbourg, not a firewall: the same build runs on the Mac, which is behind the same external firewall.
- **What worked.** A mirror. VizieR mirrors the catalogue byte for byte, so the pipeline now tries the primary (`download_url`) and falls back to any `mirror_urls`, here Harvard's `vizier.cfa.harvard.edu` at `/ftp/cats/I/311/hip2.dat.gz`. The sha256 pin confirms the mirror's file is identical, and `make gate` then passed 67/67. (A pipeline fix, committed separately; see `src/pipeline`.)

## 2. The visionOS plugin stops the Windows editor from starting (30 October)

- **Evidence.** The editor aborted at start-up: "Unable to load plugin 'OpenXRVisionOS' ... missing on disk". Its modules are built only for Apple platforms.
- **What worked.** Gating the plugin to where it exists, with a `PlatformAllowList` in the `.uproject`: `OpenXRVisionOS` is enabled only for `Mac`, `IOS` and `VisionOS`. The Vision Pro build is unaffected, and the Windows editor starts.

## 3. OpenXR refuses to start: API version 1.1 (30 October–1 November)

- **Evidence.** With the headset connected the app fell back to a flat window. The log showed `xrCreateInstance` failing: "invalid API version 1.1. Max supported version is 1.0".
- **Cause.** The active OpenXR runtime was HTC's VIVE runtime, which supports only OpenXR 1.0; Unreal 5.8 requests 1.1 when it is enabled, so the instance is never created and the headset never starts.
- **What worked.** `Config/Windows/WindowsEngine.ini` setting `bIsOpenXR1_1Enabled=False` under `[/Script/OpenXRHMD.OpenXRHMDSettings]`, so Unreal requests 1.0, which the runtime accepts; 1.0 stays enabled. Windows-only, so the Vision Pro build is unaffected. The headset then started and the whole catalogue rendered in the Vive, all 113,012 stars (the desktop mirror held about 85 fps while the first-run shaders were still compiling).

## 4. The pointer takes the hand path, not the controllers (1–2 November)

- **Symptom.** The app rendered in the headset, but the panel showed the pinch wording and said nothing about the controllers, though a controller was held and tracked.
- **Cause.** The pointer chose hand tracking because the HTC VIVE runtime reports a *valid hand tracker* even when controllers are in use, and the hand path was tried first — it is all the Vision Pro has.
- **What worked.** Check the motion controllers first, and use them when either is tracked (`AnyControllerTracked`), falling back to hand tracking only when neither is. The pawn gained left and right `UMotionControllerComponent`s on the `LeftAim` / `RightAim` sources. Gated to Windows, because on the Vision Pro the aim pose comes from the hands and must not be mistaken for a controller. The panel now says "pull the trigger" rather than "pinch" once a controller is seen.

## 5. The trigger reads zero (2 November)

- **Evidence.** With the controller path taken, pulling the trigger did nothing; the logged trigger value stayed at 0.00.
- **What didn't work.** The generic `MotionController_Right_TriggerAxis` / `_Left_` keys: they do not exist in Unreal 5.8, so they always read zero.
- **Cause.** OpenXR delivers a controller's trigger value only when an input action is bound to it, and the key names are device-specific — a Vive wand and a Valve Index controller report different keys. Nothing was bound.
- **What worked.** Two things together. `Config/Windows/WindowsInput.ini` binds both devices' trigger axes per hand (`Vive_Right_Trigger_Axis` and `ValveIndex_Right_Trigger_Axis` to `PointerTriggerRight`, and the left likewise), under `[/Script/Engine.InputSettings]`. And the pointer polls every candidate key for the hand — a comma-separated list in `RightTriggerKey` / `LeftTriggerKey` — and takes the largest value, with hysteresis around the press threshold (press above 0.6, release below 0.4) so a trigger held near it does not flicker. `-RightTriggerKey=` / `-LeftTriggerKey=` override the keys without a rebuild, and `-LogInput` lists every input key the engine registered, so a new controller's exact key can be found. The trigger then selected stars (μ Per, Capella).

## 6. The wrong hand points (2 November)

- **Symptom.** Selection worked, but with the wrong controller: the held controller was the left, yet the ray stayed on the right, the configured pointing hand.
- **Cause.** `PointingHand` (right by default) fixed the pointer to a hand whose controller might be asleep on the desk.
- **What worked.** Point with whichever controller is tracked — the configured hand when its controller is up, otherwise the other — derived from the stable `PointingHand` rather than the mutable pointer index. A single controller now works in either hand. (A first attempt derived the preference from the reassigned pointer index and drifted; fixed.)

## Harmless and still open

- **First-run shader compile.** The first launch on this machine spent about 27 minutes compiling shaders before the view appeared; later launches are quick. Fixed: `./spike.sh package` cooks a standalone Win64 build that ships the shaders, so it starts without the wait (built and run, confirmed 2 November 2026).
- **No controller shown in view.** Most Vive apps show the controller, or a wand, in the field of view; this spike shows only the ray. A simple wand or rod is in the [backlog](../../docs/BACKLOG.md#interaction) as an application feature.
- **The controller's other inputs are unused.** The Vive controller has a menu button, a circular trackpad with a click, and two grip buttons besides the trigger; only the trigger is read. Mapping the rest to named actions is in the [backlog](../../docs/BACKLOG.md#interaction).
- **Exposure not tuned.** The Vision Pro's star exposure is tuned (0.25); the Vive's is not yet, and a clean in-headset frame-rate figure is still to take (the first run was busy compiling shaders).
