#!/bin/sh
# Build and run the Unreal app locally on Windows (from WSL), on the monitor.
# A convenience wrapper over src/app/stellar.sh; see src/app/README.md.
# -----------------------------------------------------------------------------
set -eu

HERE="$(cd "$(dirname "$0")" && pwd)"
cd "$HERE/../src/app"

./stellar.sh build          # compile (quick no-op if unchanged)
./stellar.sh material       # generate the star material (harmless to re-run)
./stellar.sh run            # on the monitor, windowed, with a -log console
