#!/bin/sh
# Build and run the Unreal spike on a Mac, or on Windows from WSL.
#   ./spike.sh build      compile the editor target
#   ./spike.sh material   generate the star material from Shaders/
#   ./spike.sh run        fly the catalogue in a window (WASD and mouse)
#   ./spike.sh measure    circle the Sun for 30 s, log frame times, take a screenshot
#   ./spike.sh look RA,DEC  screenshot the sky from the Sun, e.g. 5.6,0 for Orion
#   ./spike.sh vive       run in the headset through SteamVR (Windows)
#   ./spike.sh package    cook a standalone build so shaders ship (no first-run wait)
#   ./spike.sh visionos   package for Apple Vision Pro (needs CodeSigningTeam in Config/UserEngine.ini)
#   ./spike.sh install UDID   install the packaged app on a paired Vision Pro
# Under WSL the checkout must be on a Windows drive (/mnt/c, /mnt/d), where Unreal's
# Windows tools can read it; UE_ROOT overrides where the engine is.
set -eu
HERE="$(cd "$(dirname "$0")" && pwd)"

if grep -qi microsoft /proc/version 2>/dev/null; then
	# WSL: run Unreal's Windows programs directly, passing them Windows paths.
	PLATFORM=Win64
	UE="${UE_ROOT:-/mnt/c/Program Files/Epic Games/UE_5.8}"
	EDITOR="$UE/Engine/Binaries/Win64/UnrealEditor.exe"
	EDITOR_CMD="$UE/Engine/Binaries/Win64/UnrealEditor-Cmd.exe"
	BUILD_TOOL="$UE/Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.exe"
	UAT="$UE/Engine/Binaries/DotNET/AutomationTool/AutomationTool.exe"
	STAGE=Windows
	native() { wslpath -w "$1"; }
	case "$HERE" in
	/mnt/[a-z]/*) ;;
	*)
		echo "spike.sh: under WSL the checkout must be on a Windows drive (/mnt/c, /mnt/d), not in $HERE" >&2
		exit 1
		;;
	esac
else
	PLATFORM=Mac
	UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
	EDITOR="$UE/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor"
	EDITOR_CMD="$UE/Engine/Binaries/Mac/UnrealEditor-Cmd"
	BUILD_TOOL="$UE/Engine/Build/BatchFiles/Mac/Build.sh"
	UAT="$UE/Engine/Build/BatchFiles/RunUAT.sh"
	STAGE=Mac
	native() { echo "$1"; }
fi
PROJECT="$(native "$HERE/StellarSpike.uproject")"

# Write the commit being built into BuildStamp.inl, which the game logs at start, so a log
# names the code it came from. "+changes" marks a working tree that differs from the commit,
# and the time tells apart builds of one commit with different changes.
stamp() {
	COMMIT="$(git -C "$HERE" rev-parse --short=12 HEAD)"
	BRANCH="$(git -C "$HERE" branch --show-current)"
	if [ -n "$(git -C "$HERE" status --porcelain)" ]; then
		COMMIT="$COMMIT+changes"
	fi
	STAMP="$HERE/Source/StellarSpike/BuildStamp.inl"
	{
		echo "#define STELLAR_SPIKE_COMMIT TEXT(\"$COMMIT\")"
		echo "#define STELLAR_SPIKE_BRANCH TEXT(\"${BRANCH:-detached HEAD}\")"
		echo "#define STELLAR_SPIKE_BUILT TEXT(\"$(date '+%Y-%m-%d %H:%M %z')\")"
	} > "$STAMP"
}

# The Mac-only commands, refused elsewhere rather than failing halfway.
mac_only() {
	if [ "$PLATFORM" != Mac ]; then
		echo "spike.sh: $1 needs a Mac with Xcode" >&2
		exit 1
	fi
}

case "${1:-}" in
build)
	stamp
	"$BUILD_TOOL" StellarSpikeEditor "$PLATFORM" Development -Project="$PROJECT" -WaitMutex
	;;
material)
	"$EDITOR_CMD" "$PROJECT" -run=pythonscript \
		-script="$(native "$HERE/Scripts/make_star_material.py")" -unattended -nosplash -nop4
	;;
run)
	"$EDITOR" "$PROJECT" -game -windowed -ResX=1600 -ResY=900 -log
	;;
measure)
	"$EDITOR" "$PROJECT" -game -windowed -ResX=1600 -ResY=900 -Autopilot -QuitAfter=30 \
		-ExecCmds="r.VSync 0, t.MaxFPS 0" -log
	;;
look)
	"$EDITOR" "$PROJECT" -game -windowed -ResX=1600 -ResY=900 -LookAt="$2" -QuitAfter=6 -log
	;;
vive)
	# -vr starts OpenXR, which uses SteamVR when SteamVR is the active OpenXR runtime.
	"$EDITOR" "$PROJECT" -game -vr -log
	;;
package)
	# Cook and package a standalone build for this desktop platform, so the
	# shaders ship in the package and the first run no longer spends ~27 min
	# compiling them, and it runs without the editor. The catalogue travels as a
	# plain file in Content/Data (DefaultGame.ini stages it); staging merges into
	# what is there, so start clean. Needs the generated materials: run
	# ./spike.sh material first.
	rm -rf "$HERE/Saved/StagedBuilds/$STAGE" "$HERE/Saved/Archive/$STAGE"
	stamp
	mkdir -p "$HERE/Content/Data"
	cp "$HERE/../../data/catalogue.sqlite" "$HERE/Content/Data/catalogue.sqlite"
	"$UAT" BuildCookRun -project="$PROJECT" -platform="$PLATFORM" \
		-clientconfig=Development -build -cook -stage -pak -iostore -package -archive \
		-archivedirectory="$(native "$HERE/Saved/Archive")" -nocompileeditor -utf8output
	;;
visionos)
	mac_only visionos
	# The catalogue travels inside the package as a plain file (DefaultGame.ini).
	# -pak puts the cooked content in the app; without it a Development build
	# streams it from the Mac's Zen server and stalls on the headset.
	# Staging and archiving merge into what is there, so start clean.
	rm -rf "$HERE/Saved/Archive/VisionOS" "$HERE/Saved/StagedBuilds/VisionOS"
	stamp
	mkdir -p "$HERE/Content/Data"
	cp "$HERE/../../data/catalogue.sqlite" "$HERE/Content/Data/catalogue.sqlite"
	"$UE/Engine/Build/BatchFiles/RunUAT.sh" BuildCookRun -project="$PROJECT" -platform=VisionOS \
		-clientconfig=Development -build -cook -stage -pak -iostore -package -archive \
		-archivedirectory="$HERE/Saved/Archive" -nocompileeditor -utf8output
	;;
install)
	mac_only install
	APP="$(find "$HERE/Saved/Archive" -name '*.app' -maxdepth 3 | head -1)"
	xcrun devicectl device install app --device "$2" "$APP"
	;;
*)
	sed -n '2,12p' "$0"
	exit 2
	;;
esac
