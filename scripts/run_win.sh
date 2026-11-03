#!/bin/sh
# Build and run the Unreal app locally on Windows (from WSL), on the monitor.
# A convenience wrapper over src/app/spike.sh; see src/app/README.md.
# -----------------------------------------------------------------------------
set -eu

HERE="$(cd "$(dirname "$0")" && pwd)"
cd "$HERE/../src/app"

./spike.sh build          # compile (quick no-op if unchanged)
./spike.sh material       # generate the star material (harmless to re-run)
./spike.sh run            # on the monitor, windowed, with a -log console
