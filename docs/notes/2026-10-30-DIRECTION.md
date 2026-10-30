# Direction discussion, 30 October 2026

Notes from a discussion between PLH and Claude on how the project moves forward after the Unreal spike, and which features would improve the application.
As points are settled they are folded into [../PLAN.md](../PLAN.md) (phases, gates, open questions) and [../BACKLOG.md](../BACKLOG.md) (features not yet scheduled); settled engine and architecture choices become ADRs.
Folded so far: the roadmap changes (items 1 to 3) into the plan, and every feature into the backlog.

Each point is marked **Open**, **Agreed** or **Deferred** under its heading.

## Where things stand

- The Unreal spike ([../../spikes/unreal-stars/README.md](../../spikes/unreal-stars/README.md)) renders all 113,012 catalogue stars at 120 fps on the Mac and about 95 fps in full immersion on the Vision Pro.
  The history, written for an article, is in [VISIONOS.md](../../spikes/unreal-stars/VISIONOS.md): sixteen sections, eight fixes to reach full immersion and the rest on making the stars visible, tuning, and the pointer.
- In the headset a hand pointer names stars: one ray on a configurable hand, a panel at the bottom right of the view that follows the head, pinch to keep a star, two pinches to let it go, both hands held to exit.
  Keeping is solid; letting go is good enough for now but not perfect.
- The plan still describes a Godot front end ([ADR 0007](../decisions/0007-stack-core-sqlite-godot.md)); the spike's evidence points to Unreal, which would be ADR 0012, superseding 0007.
- Not yet tried: the Vive Pro 2 on Windows (part 3 of the spike).
- Known limits of the launcher engine on visionOS: no foveation, so the image is softer than it could be; the system's concentric-circle control cannot be hidden; the splash window cannot be changed; no multiview, so each eye is drawn separately.
  All four are in engine code that ships precompiled, so they need either a source-built engine or Epic to change them.

## Agenda

1. **The engine decision.** Is the spike's evidence enough for ADR 0012 (Unreal), or must the Vive part of the spike come first?
2. **A source-built engine.** Building Unreal from source would unlock foveation, the system control, the splash window and faster fixes; it costs a large download, long builds and keeping up with Epic's releases.
3. **The roadmap after Godot.** Which of the seven phases change, and does phase 3's go/no-go on the Vision Pro still mean anything now that the spike has passed most of it?
4. **From spike to application.** What carries over from the spike (star rendering, the pointer, the panel, the build stamp, the lessons in VISIONOS.md), and where the engine-independent core ([ADR 0008](../decisions/0008-core-in-cpp.md)) sits in an Unreal project.
5. **Features.** PLH's ideas for improving the application, collected into the backlog.
6. **The open questions already in the plan:** Vision Pro standalone or Mac-rendered, the Vive's graphics card, and display units.
7. **Working practice.** Feature branches merged into `main` ([../../RELEASING.md](../../RELEASING.md#branches)); when the spike branch is merged; when the first versioned release is due.

## Discussion

### 1. The engine decision

**Agreed.**

Unreal is the engine.
ADR 0012, superseding [ADR 0007](../decisions/0007-stack-core-sqlite-godot.md), is written after the Vive test, so it can cite that evidence too.

### 2. A source-built engine

**Agreed, not yet.**

Stay on the launcher engine.
Of its four limits only foveation affects what the viewer sees; switch to a source build if sharpness proves to matter in use.
The project's code does not depend on which build of the engine it runs on.

### 3. The roadmap

**Agreed.**

Phase 3 changes from a go/no-go on Godot for the Vision Pro to: the Vive holds its refresh rate, and the Vision Pro build reaches feature parity with the desktop.
Phase 2 becomes the spike grown into the real application, on the C++ core.

### The Vive test

**Agreed, on PLH's Windows 11 PC.**

The Vive test runs on PLH's Windows 11 PC, from a checkout of this repository.
To get there:

- The spike's work must be on GitHub: done, `feature-star-panel` was pushed on 30 October.
- The setup and commands are in the spike's README, [Part 3](../../spikes/unreal-stars/README.md#part-3-htc-vive-pro-2-on-windows-not-started); the points below are why it is set up that way.
- Unreal 5.8.4 for Windows, Visual Studio with the C++ game workload, and SteamVR for the Vive.
- PLH works in WSL on Windows, so `spike.sh` runs there too: under WSL it calls Unreal's Windows programs (`UnrealBuildTool.exe`, `UnrealEditor.exe`) with paths translated by `wslpath`, and has a `vive` command that starts the game in the headset through SteamVR.
- The checkout lives on the Windows drive, at `D:\src\Projects\stellar-explorer` (`/mnt/d/src/Projects/stellar-explorer` in WSL): Unreal and Visual Studio are slow with, and sometimes break on, paths inside WSL's own filesystem.
  The script refuses to run from there.
- `.gitattributes` keeps shell scripts and the Makefile in LF, whichever git checks them out.
- The catalogue builds natively in WSL (`make catalogue`, with Python and `uv` installed there), straight into `data/`, where the spike looks for it.
- `Config/UserEngine.ini` (Apple signing) is not needed on Windows.
- The pointer knows only hand tracking, so on the Vive it needs a controller path (the ray from the controller, the trigger for the pinch); it is written and tested on the PC, after a first session that checks the stars and the frame rate.
- `feature-star-panel` is merged into `main` after the Vive test, so `main` gets the whole spike at once, followed by ADR 0012.

### 4. From spike to application

**Deferred until after the Vive test,** when the spike is complete on both headsets and what carries over can be judged on both.

### 6. The plan's open questions

**Open, with evidence gathered.**

- **Vision Pro standalone or rendered on a Mac.** The spike runs standalone on the Vision Pro at about 95 fps with every star, which suggests standalone is enough; Mac rendering would matter only if the scale-up (phase 7) outgrows the headset.
  To settle with ADR 0012.
- **The Vive's graphics card.** Answered on the PC during the Vive test.
- **Display units.** F1 offers light-years or parsecs as a setting; which is the default is still open.

### 7. Working practice

**Agreed in part.**

- The spike branch, `feature-star-panel`, is merged into `main` after the Vive test (see the Vive test, below).
- When the first versioned release is due: **deferred** until the application, not the spike, has something to release (phase 2).

### A fix to the spike: the ray starts off

**Agreed, and done in the spike.**

At start the ray was already on.
Selection should begin with an explicit action, so in a headset the ray is now off until a first pinch; the panel says "Pinch to start pointing".
The cycle is: idle, a pinch starts pointing, a pinch keeps a star (the ray goes), two pinches let it go (the ray returns).
For the application: the same rule, and the settings screen may want a way back to idle.

## Features raised

Each one is copied into [../BACKLOG.md](../BACKLOG.md) with the phase it belongs to.

### F1. A settings screen in the headset

**Agreed in outline.**

PLH's idea, from playing Half-Life: Alyx: a settings screen that can be called up at any time to adjust the application, on both the Vive and the Vision Pro.
On the Vive a controller button opens it; the Vision Pro has no buttons, so it needs a gesture.

Points raised:

- **Opening it on the Vision Pro.** Turning the palm up is taken: since visionOS 2 the system uses a palm-up look-and-tap for the Home View and Control Center, even in full immersion, so the app should not use it.
  Candidates: a settings button on the head-following panel, pointed at and pinched like a star (works the same on both headsets, and is discoverable); a pinch held for a second on the hand that does not point; a small button on that hand's wrist.
- **Opening it on the Vive.** The controller's menu button, with the panel's button as well, so the two headsets work alike.
- **What it holds.** First candidates, all of them settings the spike already has as configuration or command-line options: the pointing hand; the gesture gate and double-pinch timings; star brightness, size and faintest magnitude shown; display units (light-years or parsecs, an open question in the plan); and Quit, which could replace the two-handed exit gesture.
- **How it behaves.** It opens in front of the viewer and stays put in the world, unlike the panel, so it can be read and pointed at steadily; settings apply at once and are saved between sessions.
- **How it is built.** A screen of buttons and sliders needs Unreal's UI system (UMG widgets in the world, driven by the ray), not the text components the panel uses now; moving the panel to UMG as well would give one look and sharper text.

Agreed:

- **The way in** is a gear on the head-following panel, pointed at and pinched, on both headsets.
- **Plus a gesture:** a pinch held for about a second on the hand that does not point; on the Vive, the controller's menu button.
  The two-handed exit would clash with it (both hands pinched would open the screen first), so Quit moves onto the settings screen and the two-handed exit goes.

Still open: whether the screen holds anything beyond the first list, such as comfort options, the panel's position or the text size.

### F2. A radius of visibility

**Agreed in outline.**

PLH's idea: control the sphere of space that is rendered, with a slider for its radius, so as to see only nearby stars.

Points raised:

- **Cheap to build.** The star shader already computes each star's distance from the viewer every frame, for its brightness; hiding stars beyond a radius is one more comparison on the GPU, with nothing rebuilt, so the slider can move smoothly.
- **Centred where?** On the viewer, so the sphere travels with them (the natural reading), or on a chosen point such as the Sun or a kept star, which shows a neighbourhood from outside.
- **Faint nearby stars.** Most nearby stars are red dwarfs too faint to see by eye: Proxima Centauri, at 1.3 pc, is magnitude 11, and the spike shows nothing fainter than 6.5. A radius alone would mostly empty the sky. So "see the nearby stars" probably also wants a way to show every star inside the sphere, however faint, for instance by lifting the magnitude limit inside it or by drawing stars at a size set by their luminosity.
- **The edge.** A hard edge makes stars pop in and out as the viewer moves; a fade over the outer tenth of the radius avoids that. The boundary could be drawn faintly while the slider moves, to show where it is.
- **The scale.** Distances in the catalogue run from 1.3 pc to over 1,000 pc, so the slider wants a logarithmic scale, with the radius and the number of stars inside it shown as it moves ("20 pc, 1,214 stars").
- **Where the slider lives.** On the settings screen (F1), or somewhere quicker to reach, since it may be adjusted often.

Agreed:

- **Centre.** On the viewer first; later also on a chosen point, such as "everything within 10 pc of the Sun".
- **Brightness.** The faintest magnitude shown can be adjusted alongside the radius, so that faint nearby stars can be brought into view.
  A setting that lifts the limit far enough to show every star inside the sphere would save adjusting the two together.
- **Controls.** A logarithmic slider on both headsets; on the Vision Pro, also a two-handed stretch; on the Vive, also the controller's trackpad.

To watch when it is built:

- **Gestures must not collide.** The stretch starts with both hands pinched, and the settings screen opens on a pinch held with the hand that does not point.
  They can be told apart: the stretch needs both hands pinched and moving, the settings gesture only the one hand, held still.
  The two-handed exit is already gone (F1).
- **Which Vive controllers.** The Vive wands have a trackpad; Valve Index controllers, often used with a Vive Pro 2, have a thumbstick and a small track button. The Vive test should note which are in use.

### F3. Galactic orientation: a grid and an axis triad

**Agreed in outline.**

PLH's idea: show which way is galactic north, with a galactic latitude and longitude grid, say in red, that can be turned on and off; and an axis triad like Blender's navigation gizmo or a CAD system's, showing the orientation in 3D space.

Points raised:

- **The grid is a set of directions, not a place.** Galactic longitude and latitude are angles, so the grid is drawn at infinity, around the viewer wherever they are, like the sky sphere; it does not shift as the viewer moves.
  The catalogue is in ICRS, and the galactic frame is a fixed rotation of it, so the grid costs nothing to compute.
- **What it marks.** The galactic equator drawn stronger than the other lines, galactic north and south poles marked, and the galactic centre (towards Sagittarius A*) marked with its longitude zero; lines every 30° in longitude and 15° in latitude, labelled.
- **Other frames.** The same mechanism draws the equatorial grid (right ascension and declination) and the ecliptic, each in its own colour and with its own switch.
- **The axis triad.** Three short arrows showing the galactic axes: towards the galactic centre, the direction of the Sun's orbit, and galactic north. In a headset, a fixed spot in the view would be distracting, so candidates are beside the panel or on the wrist of the hand that does not point.
- **Comfort.** Thin, dim lines, so the grid frames the stars rather than covering them; it is on and off from the settings screen (F1).

Agreed:

- **The axis triad's place.** On the wrist of the hand that does not point on the Vision Pro, and on the controller in the hand that does not point on the Vive, so a glance at that hand shows the orientation.
  On the Vision Pro the hands are hidden in full immersion, so the triad is drawn at the tracked wrist on its own.
- **The grids.** As above; there was no objection to the equatorial and ecliptic grids alongside the galactic one, each with its own switch.

This makes the hand that does not point a carrier of tools: the triad (F3), the settings gesture (F1) and a half of the stretch (F2).

### F3, continued: the plane of the ecliptic

**Agreed.**

PLH's idea: show the plane of the ecliptic, the plane of the Solar System, and be able to turn it on and off.

Points raised:

- **A line at infinity, or a plane in space.** The ecliptic grid in F3 is drawn at infinity, a great circle around the viewer wherever they are.
  The plane itself passes through the Sun: drawn as a faint translucent disc centred on the Sun, with rings every few parsecs, it shows from anywhere how the Solar System is tilted, and from far away it is seen edge-on or at a slant, as a place rather than a direction.
- **The galactic plane the same way.** A disc through the Sun parallel to the galactic plane (the Sun sits about 20 pc above the true mid-plane) would show the thin, flat layer most of the catalogue's stars lie in.

Agreed: both planes, the ecliptic and the galactic, are drawn as discs, each with its own switch.

### F4. The galaxy as a background

**Agreed in outline.**

PLH's idea: paint the Milky Way, and perhaps the Magellanic Clouds, on the background, for more global context.

Points raised:

- **There is already a place to paint it.** The headset needed an opaque black sphere 1,200 pc out behind the stars (the compositor shows nothing without opaque content behind them); giving that sphere a texture of the sky costs nearly nothing.
- **The double-counting trap.** All-sky images of the Milky Way include the bright stars, which the catalogue also draws in 3D.
  Fly a few parsecs and the 3D stars move while their painted copies stay put.
  The background needs an image with the resolved stars removed, leaving the glow of the distant, unresolved stars, the dust lanes and the Clouds; or the catalogue's stars subtracted from it.
- **Candidate sources,** each to be checked for its licence and recorded in the `source` table like the catalogue: ESA's Gaia all-sky maps, ESO's Milky Way panorama, Axel Mellinger's all-sky panorama, and Planck's dust maps (which carry no stars at all).
- **How far it stays true.** The Milky Way is 30 kpc across and the catalogue reaches about 1 kpc, so the background stays a fair approximation anywhere in the catalogue; it is the view from the Sun, and the app could say so.
- **The Magellanic Clouds,** at 50 and 62 kpc, and the Andromeda galaxy, at 770 kpc, are effectively at infinity at this scale; they can be part of the painted background or marked and labelled on it.
- **Brightness and switch.** On and off, and its brightness adjustable, on the settings screen (F1); it must stay dim enough not to hide faint stars.

Agreed: the background is made from ESA's Gaia all-sky map.
To check before it is used:

- **Its licence.** ESA publishes its Gaia images under a Creative Commons licence with attribution to ESA/Gaia/DPAC; the exact terms of the chosen image are recorded in the `source` table, as for the catalogue.
- **The stars in it.** Gaia's maps are built from the light of the stars Gaia measured, so the brightest catalogue stars are in the image too; how visible they are at the map's resolution decides whether they must be subtracted.

### F5. Markers for the Sun and the nearest star

**Agreed.**

PLH's idea: highlight the Sun, or the star the viewer is closest to.

Points raised:

- **Why the Sun needs it.** From 10 pc the Sun is magnitude 4.8, one faint star among many; from 100 pc it is 9.8 and not drawn at all with the spike's limit of 6.5. A marker is the only way to find home once the viewer has travelled.
- **The nearest star changes as the viewer moves,** and is often a faint red dwarf; a marker on it says "this is your neighbourhood", and the panel can name it with its distance.
- **One marker or two.** Either a marker on whichever is nearer, or (my suggestion) both: the Sun always marked as home, and the nearest star marked in its own way, each with its own switch.
- **Finding it when it is out of view.** When the Sun is behind the viewer, an arrow at the edge of the panel can point the way to it.
- **A style of its own,** distinct from the rings for the star pointed at (blue) and the kept star (amber), such as a small diamond or crosshair that stays the same size on screen.

Agreed: both markers, each with its own switch: the Sun as home, with an arrow on the panel when it is out of view, and the nearest star, named on the panel.

### F1, continued: grouping the settings

**Agreed.**

The settings screen is grouped as **Sky** (the grids, the planes, the background), **Markers** (the Sun, the nearest star) and **Controls** (the pointing hand, the gesture timings), with the brightness, the radius and the magnitude limit under Sky or a group of their own when the screen is designed.

### F6. Examining a small region closely

**Agreed.**

PLH's idea: select a small region of the sky and examine it more closely, so as to pick out fainter stars; show it as a cubic volume containing its stars, with the rest of the sky hidden. The Pleiades are the example.

Points raised:

- **A region of sky is a cone, not a box.** The Pleiades lie about 136 pc away and cover about 2° of sky, but the cone towards them also holds foreground and background stars.
  A cube needs a centre and a size as well as a direction: for instance centred on a kept star (Alcyone, for the Pleiades) with an edge of about 20 pc.
- **Defining it.** Either keep a star and set the cube's size with the same kind of slider as the radius (F2), or sweep out a rectangle on the sky with the ray and then choose the depth.
- **Showing it.** Two ways: fly there and hide everything outside the cube, or lift the cube out as a model held at arm's length, a metre across, that the hands can turn and the ray can point into.
  The model makes depth visible (a cluster looks like a cluster) and makes faint stars big enough to point at.
- **Faint stars inside.** Inside the cube stars would be drawn by luminosity rather than by how bright they look from far away, so every star in it is visible and can be selected; the rest of the sky goes dark behind it.
- **What the catalogue can show.** The catalogue is mostly Hipparcos, complete to about magnitude 7 to 9, so the Pleiades' brighter members are there but most of its several hundred fainter members are not; Gaia (phase 7) would add them.
- **An honest view of depth.** Hipparcos distances at 136 pc are uncertain by around 10 pc or more each, so in 3D the cluster is stretched along the line of sight.
  The Pleiades are also a famous case: Hipparcos put the cluster at about 120 pc, against about 135 pc from other methods, a disagreement that lasted until radio interferometry and Gaia settled it near 136 pc.
  The catalogue's distances for these stars come from Hipparcos ([ADR 0011](../decisions/0011-astrometry-from-hipparcos.md)), so the app will show the cluster where Hipparcos put it unless that is corrected.
  Checked: the catalogue puts the nine named Pleiades between 114 and 125 pc (Alcyone 123.6, Merope 116.6), not at 136.

Agreed: the region is lifted out as a model at arm's length, not flown to.

PLH's view on defining it: for a cluster such as the Pleiades the size should come from the cluster's own size; for a plain patch of sky, either select and drag to mark it out, or select a star and set a radius around it.

Proposed, one mechanism with two ways in:

1. **Around a kept star (first).** Keep a star, then choose "Examine" on the panel: a cube centred on the star, its edge set with a slider like the radius in F2.
   If the star belongs to a known cluster, the cube is centred on the cluster and sized from its catalogued radius instead, so keeping Alcyone and choosing Examine gives the Pleiades.
   This needs a catalogue of open clusters with their centres, distances, radii and member stars, added to the pipeline as a new source; Hunt and Reffert's 2023 catalogue of clusters from Gaia DR3 and Cantat-Gaudin and others' 2020 catalogue are the candidates.
2. **Sweeping a patch (second).** Pinch, hold and drag across empty sky to mark out a rectangle; the region is that patch of sky between a near and a far distance, set on the panel, so a model of a plain area includes only the depth the viewer asks for.
   This changes the pinch: today a star is kept the moment a pinch starts, so a pinch would have to wait for its release, or for the hand to start moving, before deciding whether it keeps or sweeps.

The first is simpler, reuses what exists, and covers the Pleiades; the second follows when plain areas are wanted.

Agreed: in that order, around a kept star first and sweeping a patch second.
  As a scientific exhibit ([ADR 0003](../decisions/0003-scientific-exhibit-with-provenance.md)) the model could show each star's distance uncertainty as a short line through it.

### F7. Screenshots and recordings

**Agreed in outline.**

PLH's idea: take screenshots and record images from inside the app, perhaps with a way to mark out a region of the star field to capture.

Points raised:

- **What the headsets offer already.** The Vision Pro records what the wearer sees from Control Center, and Xcode can capture from it over the Developer Strap; SteamVR takes screenshots of the Vive's view, and the desktop can be recorded with OBS.
  These show the wearer's view, panel and all, at the headset's resolution, and on the Vision Pro without foveation they are no sharper than the headset itself.
- **An in-app camera.** Unreal can render a second, separate view to an image at any resolution and field of view, without the panel or the ray, so a capture can be sharper and wider than what the eyes see, with the overlays (grids, planes, labels) included or left out as chosen.
  The spike already writes Unreal screenshots; this extends that.
- **Marking out a region.** A viewfinder: pinch, hold and drag to sweep a rectangle on the sky, the same gesture as sweeping a patch for the close-up model (F6), then choose Capture or Examine on the panel. One gesture, two uses.
- **Provenance on every image** ([ADR 0003](../decisions/0003-scientific-exhibit-with-provenance.md)): where the viewer was, which way they looked, the field of view, the overlays, the catalogue and the build, stored inside the image file or beside it, so any picture can be recreated.
- **Recordings.** Video of what the wearer sees is best left to the platforms (Control Center, SteamVR, OBS); smooth video along a planned path belongs with the tours in phase 6, where the app renders it frame by frame.
- **Where images go.** The app's own folder, copied off with the tools used today; or, on the Vision Pro, the Photos library, which needs one more permission.

Agreed:

- **Purpose, at first:** sharing images to demonstrate the software. So a capture shows the sky as the viewer has set it up (the overlays that are on stay on), without the panel or the ray; labels and scientific charts can come later.
- **On the Vision Pro,** images go into Photos, which needs the add-to-Photos permission in the app's Info.plist.
- **On the Vive: open.** The natural default on Windows is a folder under the user's Pictures (`Pictures\Stellar Explorer`); to settle with the Vive test.

### F8. Travelling to a star

**Agreed.**

PLH's idea: move to a selected star and see its neighbourhood from there, with a visible anchor back to the Sun.

Points raised:

- **Not the ships of phase 5.** This moves the viewer instantly, as an observer; travel with relativistic time and ship clocks is phase 5's, and stays separate.
- **Getting there comfortably.** Smooth motion through space in a headset makes many people queasy, because the eyes see movement the body does not feel.
  Two ways: a quick fade out and in at the new place (comfortable, the usual default in VR), or a fast flight with the edges of the view darkened (the parallax as the stars swing past is the most striking thing this app can show). Offer both, with the fade as default, on the settings screen (F1).
- **Where to arrive.** At the star itself the star would fill the sky, so arrive a little way from it, as if from one of its planets: the star bright but bearable, the rest of the sky as seen from that system.
- **The anchor back to the Sun.** The Sun marker (F5) already marks home from anywhere, with an arrow when it is out of view; add its distance on the panel, an optional faint line from the viewer towards the Sun, and a "Return to the Sun" action.
- **A trail.** A history of the stars visited, with "Back" to the previous one, and optionally the path drawn as a line through space.
- **What it shows.** From another star the constellations are distorted and the Sun is a faint star; with the grids (F3) the viewer can see that the galactic directions have not changed, only the near stars have moved.

Agreed:

- **The way in:** a "Go there" button on the panel when a star is kept.
- **Flight is the default,** with the edges of the view darkened for comfort, and the fade as the option.
- **Where it leads.** PLH ultimately wants the flight to mimic real interstellar travel, with the relativistic effects at the chosen fraction of light speed: stars crowding towards the direction of travel, blue shift ahead and red shift behind.
  This is the bridge view already in the design ([ARCHITECTURE.md](../ARCHITECTURE.md), phase 5), so the observer's flight is its first step, built so that a speed can be added later rather than as a separate mechanism.

What the relativistic view involves, for when phase 5 is planned:

- **Aberration.** A star at angle θ from the direction of travel appears at θ′, with cos θ′ = (cos θ + β)/(1 + β cos θ), where β is the speed as a fraction of light's; at 0.9 c the half of the sky ahead crowds to within about 26° of the direction of travel, and the sky behind empties.
- **Doppler shift.** Each star's light is shifted by the factor D = 1/(γ(1 − β cos θ)). A black body at temperature T, Doppler-shifted, is a black body at D·T, and the catalogue holds each star's temperature, so the shifted colour can be computed exactly in that approximation.
- **Beaming.** Brightness changes too, by about D⁴ for the total light, so the stars ahead blaze and those behind fade; the magnitude limit decides which appear and vanish.
- **Time.** At 0.9 c a 4 pc crossing takes about 14.5 years by the Sun's clock and about 6.3 years aboard; the flight is shown in seconds, with the three clocks of the design ([ARCHITECTURE.md](../ARCHITECTURE.md#three-clocks): master, ship and seen time) to keep the times honest.
- **Cost.** All of it is per-star arithmetic in the shader the stars already use, so it fits the frame budget the spike has measured.

### F9. Talking to the application

**Agreed as a feature; the details deferred.**

PLH's idea, raised after the session paused: speak commands such as "Show the Pleiades", "Show Alpha Centauri" and "Go to Delta Pavonis".

Points raised:

- **Voice is one more way to reach the same actions.** Keep a star, Go there (F8), Examine (F6), Return to the Sun, take a picture (F7), switch a grid on (F3): if each is a named command that the panel, the settings screen, the gestures and the voice all call, voice adds no new behaviour, only a new way in.
- **The names are the hard part.** The catalogue stores names as catalogues write them, not as people say them.
  Checked: Delta Pavonis is stored as `del Pav`; Alpha Centauri appears only as its proper names, Rigil Kentaurus and Toliman.
  The pipeline would add spoken forms to the `identifier` table (Greek letters and constellation genitives written out: "Delta Pavonis", "Alpha Centauri A"), and the recogniser would match against those with some tolerance.
- **Not only stars.** "The Pleiades" is a cluster, so it needs the cluster catalogue already in the backlog for F6; galaxies and the Magellanic Clouds (F4) would need names too.
- **Recognising speech.** Unreal has no speech recognition of its own.
  Candidates: each platform's own (Apple's Speech framework on the Vision Pro, Windows speech recognition on the PC), or one on-device model on both, such as Whisper through a third-party Unreal plugin, which keeps it offline and the same everywhere.
  Whether a plugin builds for visionOS has to be checked.
- **When it listens.** Always listening risks acting on stray speech; a wake word ("Stellar, go to Delta Pavonis") or a gesture that opens the microphone avoids that.
  Either way the panel shows what it heard before acting, so a misheard name can be caught.
- **Permissions.** Microphone and speech recognition each need a usage description in the Vision Pro's Info.plist, like hand tracking.
- **Later, questions as well as commands.** The catalogue is meant to be queryable ([ADR 0004](../decisions/0004-extensible-queryable-catalogue.md)), with an SQL console in phase 4.
  A language model could turn "show me the red giants within 50 parsecs" into a query and a selection; that needs a network connection and is a step beyond fixed commands.

Deferred, to decide when F9 is planned: a wake word or a gesture to start listening, and whether it must work offline (which decides between the platforms' recognisers and an on-device model, and whether spoken questions are in reach).
Agreed now: actions are built as named commands from the start, so that voice can call them later.

#### A voice spike after the Vive spike

**Agreed.**

PLH asked for a spike on voice once the Vive spike is done, since speaking could make the application more accessible: to anyone for whom pinches or controllers are awkward, and to anyone who knows a star's name but not where it is.

What the spike would settle, as part 4 of [spikes/unreal-stars](../../spikes/unreal-stars/README.md):

- **Recognition on both headsets.** A handful of fixed commands ("Show", "Go to", "Return to the Sun", "Take a picture") and a few dozen star names, including hard ones ("Delta Pavonis", "Alpha Centauri", "the Pleiades").
- **Both routes compared:** each platform's own recogniser against one on-device model (Whisper through an Unreal plugin), for accuracy on star names, delay from speaking to acting, and whether it builds for visionOS at all.
- **Starting to listen:** a wake word against a gesture, tried in the headset.
- **The answers to the deferred questions above,** with evidence, as the Vive spike gives ADR 0012 its evidence.

