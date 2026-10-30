# Stellar Explorer: Backlog

Features wanted but not yet scheduled.
The phases and their gates are in [PLAN.md](PLAN.md); a feature moves from here into a phase when the phase is planned in detail.
Each entry says where the idea came from and which phase it most likely belongs to.

## Interaction

- **A settings screen in the headset.** Called up at any time, on both headsets, from a gear on the head-following panel; also by a pinch held for about a second on the hand that does not point, or the Vive's menu button.
  Holds the pointing hand, gesture timings, star brightness and size, the faintest magnitude shown, display units and Quit, which replaces the two-handed exit gesture.
  Built with UMG widgets driven by the ray.
  From PLH, 30 October 2026; see [notes/2026-10-30-DIRECTION.md](notes/2026-10-30-DIRECTION.md#f1-a-settings-screen-in-the-headset).
  Grouped as Sky, Markers and Controls.
  Phase 2 for the screen, phase 3 for the headset controls.

- **A radius of visibility.** A slider that limits the stars drawn to those within a radius, on a logarithmic scale, with a soft edge and a count of the stars inside.
  Centred on the viewer first, later also on a chosen point such as the Sun; the faintest magnitude shown is adjustable alongside it.
  Controlled by a slider on both headsets, a two-handed stretch on the Vision Pro, and the trackpad on the Vive.
  From PLH, 30 October 2026; see [notes/2026-10-30-DIRECTION.md](notes/2026-10-30-DIRECTION.md#f2-a-radius-of-visibility).
  Phase 2.

- **Galactic orientation.** A galactic longitude and latitude grid, red, on and off, with the galactic equator, poles and centre marked; an axis triad showing the galactic axes, on the wrist of the hand that does not point (Vision Pro) or on that hand's controller (Vive).
  The same mechanism gives equatorial and ecliptic grids.
  From PLH, 30 October 2026; see [notes/2026-10-30-DIRECTION.md](notes/2026-10-30-DIRECTION.md#f3-galactic-orientation-a-grid-and-an-axis-triad).
  Phase 2.

- **The planes of the ecliptic and the galaxy.** Faint discs through the Sun, one in the plane of the Solar System and one parallel to the galactic plane, each on and off.
  From PLH, 30 October 2026; see [notes/2026-10-30-DIRECTION.md](notes/2026-10-30-DIRECTION.md#f3-continued-the-plane-of-the-ecliptic).
  Phase 2.

- **The galaxy as a background.** The Milky Way and the Magellanic Clouds painted on the background sphere from ESA's Gaia all-sky map, with any catalogue stars in it removed, on and off and dimmable.
  From PLH, 30 October 2026; see [notes/2026-10-30-DIRECTION.md](notes/2026-10-30-DIRECTION.md#f4-the-galaxy-as-a-background).
  Phase 2 or 3.

- **Markers for the Sun and the nearest star.** The Sun marked as home wherever the viewer is, with an arrow towards it when out of view; the nearest star marked and named on the panel; each on and off.
  From PLH, 30 October 2026; see [notes/2026-10-30-DIRECTION.md](notes/2026-10-30-DIRECTION.md#f5-markers-for-the-sun-and-the-nearest-star).
  Phase 2.

- **Examining a small region closely.** A cubic volume around a chosen star or a region of sky, with its stars drawn by luminosity so the faint ones can be selected, and the rest of the sky hidden; perhaps lifted out as a model at arm's length.
  The Pleiades are the test case.
  Defined around a kept star (sized from its cluster when it belongs to one), and later by sweeping a patch of sky.
  From PLH, 30 October 2026; see [notes/2026-10-30-DIRECTION.md](notes/2026-10-30-DIRECTION.md#f6-examining-a-small-region-closely).
  Phase 4.

- **Pinch release reliability.** Letting a kept star go with two pinches works but still misses some; tune from the headset logs, or add a second way to let go (a pinch of the other hand).
  From the Unreal spike, 30 October 2026.
  Phase 3.

## Voice

- **Talking to the application.** Spoken commands ("Show the Pleiades", "Go to Delta Pavonis") calling the same named actions as the panel and the gestures; spoken forms of star names added to the catalogue; a wake word or a gesture to listen; what was heard shown before acting.
  Later, spoken questions turned into catalogue queries.
  From PLH, 30 October 2026; see [notes/2026-10-30-DIRECTION.md](notes/2026-10-30-DIRECTION.md#f9-talking-to-the-application).
  A spike first, after the Vive spike, to settle recognition, offline or online, and how listening starts; the feature itself in phase 4, after the named actions exist.

## Travel

- **Travelling to a star.** Move the viewer to a kept star, by a fade or a fast flight with the view's edges darkened, arriving a little way from it; the Sun marker, its distance, a faint line towards it and "Return to the Sun" as the anchor home; a history with "Back".
  Flight is the default, from a "Go there" button on the panel; built as the first step of phase 5's relativistic bridge view, which adds aberration, Doppler colour and beaming at a chosen fraction of light speed.
  From PLH, 30 October 2026; see [notes/2026-10-30-DIRECTION.md](notes/2026-10-30-DIRECTION.md#f8-travelling-to-a-star).
  Phase 2.

## Capture

- **Screenshots and recordings.** An in-app camera rendering a chosen region at high resolution, without the panel or the ray, with overlays optional; the region marked out with the same sweep gesture as the close-up model (F6); provenance stored with every image.
  First for sharing images to demonstrate the software: the sky as set up, without the panel or the ray; saved to Photos on the Vision Pro, and on Windows (undecided) probably to the user's Pictures folder.
  Video of the wearer's view is left to the platforms; rendered flights belong with the tours.
  From PLH, 30 October 2026; see [notes/2026-10-30-DIRECTION.md](notes/2026-10-30-DIRECTION.md#f7-screenshots-and-recordings).
  Phase 2 for screenshots of the view, phase 4 for the region capture.

## Data

- **A catalogue of open clusters.** Centres, distances, radii and member stars, so a view can be sized to a cluster and its members picked out; candidates are Hunt and Reffert (2023, from Gaia DR3) and Cantat-Gaudin and others (2020).
  Needed by the close-up of a region (F6).
  From the direction discussion, 30 October 2026.
  Phase 4.

- **Spoken forms of names.** Star names as people say them ("Delta Pavonis" for `del Pav`, "Alpha Centauri A"), added to the `identifier` table so voice (F9) and search can find them.
  From the direction discussion, 30 October 2026.
  Phase 4.

## Known engine limits on Vision Pro

- **Foveation, the system control, the splash window, multiview.** Each needs a change to engine code that the launcher engine ships precompiled.
  See [notes/2026-10-30-DIRECTION.md](notes/2026-10-30-DIRECTION.md#agenda), item 2.
