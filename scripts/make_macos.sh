#!/bin/sh
# Build the core libraries the Unreal app links on a Mac (see src/core/README.md):
#   scripts/make_macos.sh            both, the Mac's then the Vision Pro's
#   scripts/make_macos.sh mac        macOS (arm64) into src/core/build-mac, and run the tests
#   scripts/make_macos.sh visionos   visionOS (device) into src/core/build-visionos
# Run it after pulling a change to src/core: the app quits at start on a core built for
# another interface version. Then build and run the app from src/app with spike.sh
# (build, run, visionos to package); the catalogue must exist first (make catalogue).
# -----------------------------------------------------------------------------
set -eu

# The real path, symlinks resolved (pwd -P), as spike.sh uses: the build directories record it.
HERE="$(cd "$(dirname "$0")" && pwd -P)"
cd "$HERE/.."

build_macos() {
	cmake -S src/core -B src/core/build-mac -G Xcode -DCMAKE_OSX_ARCHITECTURES=arm64
	cmake --build src/core/build-mac --config Release
	ctest --test-dir src/core/build-mac -C Release --output-on-failure
}

# Without the tests, whose executables are device binaries that cannot run on the Mac.
build_visionos() {
	cmake -S src/core -B src/core/build-visionos -G Xcode \
		-DCMAKE_SYSTEM_NAME=visionOS \
		-DCMAKE_OSX_SYSROOT=xros \
		-DCMAKE_OSX_ARCHITECTURES=arm64 \
		-DSTELLAR_CORE_TESTS=OFF
	cmake --build src/core/build-visionos --config Release
}

case "${1:-all}" in
mac) build_macos ;;
visionos) build_visionos ;;
all)
	build_macos
	build_visionos
	;;
*)
	echo "usage: scripts/make_macos.sh [mac|visionos|all]" >&2
	exit 2
	;;
esac
