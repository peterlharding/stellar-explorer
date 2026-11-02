# Voice commands in the Unreal Engine 5.8 spike

A log of part 4 of the spike: speaking to the app ("Stellar, go to Delta Pavonis") on the Mac and the Vision Pro.
As with [VISIONOS.md](VISIONOS.md) and [VIVE.md](VIVE.md), it records what failed as well as what worked, with the evidence for each, as material for an article.
The questions it set out to settle are in the [README](README.md#part-4-voice-commands); the feature is F9 of the [direction notes](../../docs/notes/2026-10-30-DIRECTION.md#f9-talking-to-the-application).

The setup, on 2 November 2026: Unreal Engine 5.8.4 from the Epic Games Launcher, Xcode 27.2, an M1 Max Mac on macOS 27.2 and an Apple Vision Pro.
The app is the graduated spike in [`src/app`](../../src/app/README.md), on the branch `feature-voice`.

## How it is built

- **The platform hears; the core understands.** The app hands the microphone to the platform's recogniser and gets text back (`VoiceRecognizer.cpp`): on Apple platforms the `Speech` framework, on the device, in the system's language (`en_AU` here).
  The core turns the text into a command and a star (`stc_voice_parse`, `src/core/src/voice.cpp`), the same on every platform and tested without the engine.
- **Spoken names come from the pipeline.** The catalogue writes designations as catalogues do (`del Pav`, `alf1 Cen`, `61 Cyg`); the pipeline now adds their spoken forms to the `identifier` table ("Delta Pavonis", "Alpha 1 Centauri" and "Alpha Centauri", "61 Cygni"), 4,622 of them, beside the 550 proper names.
- **Matching with tolerance.** A name heard is matched exactly, then within about one edit in four letters, then, after a command word only, by how it sounds: its consonants, loosely (no "r", d and t alike).
  Of two stars with one name the brighter wins ("Alpha Centauri" is Rigil Kentaurus).
- **The recogniser is told what to expect:** the command phrases, the wake word and 200 star names, the brightest and the nearest in turn, each by its proper and Bayer names.
- **Two ways to be heard,** either or both (`Listening` in `DefaultGame.ini`): push to talk (hold V on the desktop, a grip button on the Vive, a pinch of the hand that does not point on the Vision Pro), or the wake word, "Stellar", acted on once the words stop for 0.8 s.
  What was heard, and what was done, shows just above the panel.
- **The commands:** "show" or "go to" a star by name; "go there" (to the star kept or pointed at); "back off" and "closer" (three times as far from it, or a third as far); "return to the Sun"; "let go".

## 1. The Mac cannot listen from the editor

- **Evidence.** Asking for speech recognition without a usage description ends the process, and the editor's own `Info.plist` has a microphone description but none for speech recognition.
- **What worked.** The app checks for both descriptions before asking, and in the editor says "Voice needs a packaged build" above the panel.
  A packaged Mac build carries its own `Info.plist` (`Build/Mac/Resources/Info.Template.plist`) with both.

## 2. A packaged Mac build had not been possible since the move to visionOS

- **Evidence.** `./spike.sh package` failed: "OpenXRVisionOS.uplugin is referenced via StellarSpike.uproject with a mismatched 'SupportedTargetPlatforms' field".
  The plugin supports only visionOS, and the project enabled it for the Mac, which the Mac editor needs but a Mac game refuses.
- **What worked.** `"SupportedTargetPlatforms": ["VisionOS"]` on the project's reference: the Mac game leaves the plugin out, while the editor, which loads plugins for every platform it can target, keeps it.

## 3. In the app sandbox SQLite cannot open the catalogue

- **Evidence.** The packaged app logged "Cannot open catalogue" for a file that was there and readable, from the external disk and from the internal one alike.
  The same file opened at once outside the app, and the same app re-signed without the sandbox loaded the stars and started listening.
- **What worked.** Entitlements of the project's own with audio input and no sandbox (`Build/Mac/Resources/StellarSpike.entitlements`); a personal tool not sold through the App Store has no need of one ([ADR 0009](../../docs/decisions/0009-personal-tool.md)).

## 4. On the desktop the mouse fought the window manager

Not voice, but found testing it: in the packaged app the window jerked when dragged and the cursor vanished.

- **Cause, in three parts.** The engine's default captured, hid and locked the mouse from the first click; with the mouse left free instead, the engine's mouse look turned the view whenever the cursor crossed the window, button or not; and the game's own input saw only the first press of either button after launch (a debug log showed Slate with the right button held and the game input without it).
- **What worked.** No capture (`Config/DefaultInput.ini`); the engine's mouse-look axes removed from the pawn; and the pawn and the pointer reading the buttons and the cursor from Slate: hold the right button and drag to look, click to keep the star in the middle of the view.
  Desktop stars were also too faint: exposure 0.1, judged against 0.04 and 0.2 on Orion, replaces the material's 0.04.

## 5. On the Mac, push to talk works (2 November)

Holding V, in the packaged app:

| Said | Heard | Result |
| --- | --- | --- |
| Take me to Sirius | Take me to Sirius | Sirius |
| Take me to Delta Pavonis | Take me to Delta Peronis | Delta Leonis, wrongly |
| Delta Pavonis | PAVONISPavoneS | nothing |
| Take me to Alcyone | Take me to Alcione | Alcyone |
| Take me to Sol | Take me to Seoul | the Sun |
| Take me to Alpha Centauri (twice) | as said | Rigil Kentaurus |
| Take me to Bellatrix | as said | Bellatrix |
| Take me to Betelgeuse | Take me to Battle girls, then as said | nothing, then Betelgeuse |
| Stellar show me Vega (wake word) | as said | Vega |

The final words came 0.02 to 0.27 s after letting go of V, and the app acted in the same frame; the wake word acts 0.8 s after the words stop, by design.

Two fixes came from this.
"Peronis" is two letters from both "Pavonis" and "Leonis", and the brighter won; among names equally far in letters, the one whose consonants are closer now wins (p-r-n-s is nearer p-v-n-s than l-n-s).
And Delta Pavonis was not among the 100 names the recogniser was told of, all of them the brightest; the hints became the brightest and the nearest in turn, 200 of them.

Also noticed: "Alpha Centauri" takes you to the right place but names Rigil Kentaurus on the panel; the name of a system should show as the system, with its members.

## 6. On the Vision Pro, push to talk hears nothing (2 November)

- **Evidence.** The first test heard one word, "Test", in four holds; the others ended within 0.06 s of starting, because hand tracking lost the held pinch for a frame or two while PLH spoke.
  After a 0.3 s grace on letting go, holds lasted 0.07 to 2.12 s, and every one of about thirty ended with "No speech detected", while the wake word, on the same recogniser and microphone, heard the same speaker well.
- **Not yet known.** Why a request started by a pinch hears nothing on visionOS when the same code works on the Mac.
- **What was done.** On the Vision Pro the wake word is the only way to listen (`Config/VisionOS/VisionOSGame.ini`); push to talk stays for the Mac's V and the Vive's grip.

Also seen: no permission prompt appeared in the headset, though the app went from "Asking for the microphone" to "Listening" (permission granted, perhaps carried over from an earlier install).

## 7. The wake word, and what the recogniser writes (2 November)

Five rounds in the headset, each fixing what the last showed:

- **Commands without the wake word were ignored, silently.** "Go to Sirius" was heard cleanly six times and nothing happened. Now the panel says `"Go to Sirius": say "Stellar" first`.
- **The wake word with a pause after it was acted on too soon:** "Stellar", a pause, then the command, was taken as "Stellar" alone and thrown away. Now the wake word alone waits up to 3 s for its command.
- **Apple's recogniser writes "Siri" before "Sirius",** its own assistant's name first, corrected a moment later; the app acted on "Siri". Now a name that matches no star waits up to 3 s for the recogniser to finish.
- **A command restarted mid-sentence** ("Go to Siri go to Sirius") was read as one long star name. Now the last command said counts.
- **"Stellar" is heard as "Stella"** half the time, and as "Still a", "Still I" or "Start a". "Stella" and the like are accepted; one or two words that sound like it count when a command follows straight after.

With the wake word heard, by the last two rounds: 23 of 23 for commands that need no name (19 "go there", 4 "back off"), and names right about half the time.
Heard right: Sirius, Alnilam, Rigel, Saiph, Deneb, Atria, Vega, Alpha Centauri, Delta Pavonis (after the fixes in section 5), Fomalhaut, and Alcyone from "Alcione".
Heard wrong: Betelgeuse as "Battle girls" and "Battle Goose", Sadr as "Satter", and once "22 Sydney".
The looser sound matching now takes "Battle girls" to Betelgeuse and "Satter" to Sadr, and was checked on the whole catalogue against everyday phrases ("go to bed", "hello there", "find my keys"), which match no star.

## 8. Naming is the weak spot, so point instead

PLH's verdict on the star names: poor.
His fix was a command that needs no name: point at a star, or keep one, and say "Stellar, go there".
It worked every time it was heard, and "back off" and "closer" followed in the same way, moving along the line from the star at hand.
Typing a name, which cannot be misheard, comes with the desktop console (F10 in the direction notes).

## 9. A bug in `spike.sh install`

- **Evidence.** Installs on the headset failed with `CoreDeviceError 3002`, and succeeded when `devicectl` was given the app's path directly.
- **Cause.** `install` took the first `.app` under `Saved/Archive`, and once a Mac package existed that was `Archive/Mac/StellarSpike.app`.
- **What worked.** It installs `Saved/Archive/VisionOS/StellarSpike.app`, and nothing else.

## What it settles

- **Recogniser.** Apple's, on the device, is good enough on the Mac and the Vision Pro for commands, and hears in Australian English offline; star names are its weakness, and biasing it towards them (contextual strings) helps but does not cure it.
- **Starting to listen.** The wake word on the Vision Pro; push to talk on the desktop. The pinch is ruled out for now.
- **Names.** Spoken forms from the pipeline, tolerant matching in the core, and commands that need no name ("go there") carry most of the load; typing in the console will carry the rest.
- **Offline.** Yes, with the platforms' recognisers; spoken questions (turned into catalogue queries) would need a network and are left for later.

## Still open

- **Whisper,** the on-device model compared against the platforms' recognisers, and whether it builds for visionOS.
- **Windows and the Vive:** no recogniser there yet (Windows' own, or Whisper).
- **Why push to talk hears nothing on the Vision Pro.**
- **The Alpha Centauri system** shown as a system on the panel.
- **The colours of the stars,** which PLH found not quite right in the headset; what is wrong is not yet pinned down.
