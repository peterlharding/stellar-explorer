#!/bin/sh
#
#
#
# -----------------------------------------------------------------------------

# macOS

build_macos () {
    cmake -S src/core -B src/core/build-mac -G Xcode -DCMAKE_OSX_ARCHITECTURES=arm64 \
        && cmake --build src/core/build-mac --config Release
}

# visionOS

build_visionos() {
    cmake -S src/core -B src/core/build-visionos -G Xcode \
        -DCMAKE_SYSTEM_NAME=visionOS \
        -DCMAKE_OSX_SYSROOT=xros \
        -DCMAKE_OSX_ARCHITECTURES=arm64 \
        -DSTELLAR_CORE_TESTS=OFF \
        && cmake --build src/core/build-visionos --config Release
}


# -----------------------------------------------------------------------------
#  On the Mac:
#
#  1. git checkout devel && git pull (tip should be 4890dc4)
#  2. Ensure data/catalogue.sqlite exists — make catalogue if not
#  3. Build the core libs (from src/core/README.md):
#     - macOS: cmake -S src/core -B src/core/build-mac -G Xcode -DCMAKE_OSX_ARCHITECTURES=arm64 && cmake --build src/core/build-mac --config Release
#     - visionOS: cmake -S src/core -B src/core/build-visionos -G Xcode -DCMAKE_SYSTEM_NAME=visionOS -DCMAKE_OSX_SYSROOT=xros -DCMAKE_OSX_ARCHITECTURES=arm64 -DSTELLAR_CORE_TESTS=OFF && cmake --build src/core/build-visionos --config Release
#  4. Build/run from src/app: ./spike.sh build, ./spike.sh run, then ./spike.sh visionos to package

