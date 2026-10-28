#!/bin/sh
# Build and run the Unreal spike on a Mac.
#   ./spike.sh build      compile the editor target
#   ./spike.sh material   generate the star material from Shaders/
#   ./spike.sh run        fly the catalogue in a window (WASD and mouse)
#   ./spike.sh measure    circle the Sun for 30 s, log frame times, take a screenshot
#   ./spike.sh look RA,DEC  screenshot the sky from the Sun, e.g. 5.6,0 for Orion
#   ./spike.sh visionos   package for Apple Vision Pro (needs CodeSigningTeam in Config/UserEngine.ini)
#   ./spike.sh install UDID   install the packaged app on a paired Vision Pro
set -eu
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
HERE="$(cd "$(dirname "$0")" && pwd)"
PROJECT="$HERE/StellarSpike.uproject"
EDITOR="$UE/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor"

case "${1:-}" in
build)
	"$UE/Engine/Build/BatchFiles/Mac/Build.sh" StellarSpikeEditor Mac Development -Project="$PROJECT" -WaitMutex
	;;
material)
	"$UE/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PROJECT" -run=pythonscript \
		-script="$HERE/Scripts/make_star_material.py" -unattended -nosplash -nop4
	;;
run)
	"$EDITOR" "$PROJECT" -game -windowed -ResX=1600 -ResY=900 -log
	;;
measure)
	"$EDITOR" "$PROJECT" -game -windowed -ResX=1600 -ResY=900 -Autopilot -QuitAfter=30 \
		-ExecCmds="r.VSync 0, t.MaxFPS 0" -log
	;;
visionos)
	# The catalogue travels inside the package as a plain file (DefaultGame.ini).
	# -pak puts the cooked content in the app; without it a Development build
	# streams it from the Mac's Zen server and stalls on the headset.
	# Staging and archiving merge into what is there, so start clean.
	rm -rf "$HERE/Saved/Archive/VisionOS" "$HERE/Saved/StagedBuilds/VisionOS"
	mkdir -p "$HERE/Content/Data"
	cp "$HERE/../../data/catalogue.sqlite" "$HERE/Content/Data/catalogue.sqlite"
	"$UE/Engine/Build/BatchFiles/RunUAT.sh" BuildCookRun -project="$PROJECT" -platform=VisionOS \
		-clientconfig=Development -build -cook -stage -pak -iostore -package -archive \
		-archivedirectory="$HERE/Saved/Archive" -nocompileeditor -utf8output
	;;
install)
	APP="$(find "$HERE/Saved/Archive" -name '*.app' -maxdepth 3 | head -1)"
	xcrun devicectl device install app --device "$2" "$APP"
	;;
look)
	"$EDITOR" "$PROJECT" -game -windowed -ResX=1600 -ResY=900 -LookAt="$2" -QuitAfter=6 -log
	;;
*)
	sed -n '2,10p' "$0"
	exit 2
	;;
esac
