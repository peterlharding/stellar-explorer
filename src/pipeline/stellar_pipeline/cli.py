"""Command line: `stellar-pipeline fetch` downloads the sources, `build` makes the catalogue."""

import argparse
import logging
import sys
from pathlib import Path

from stellar_pipeline import build, fetch, sources

# src/pipeline/stellar_pipeline/cli.py -> the repository root.
_REPOSITORY = Path(__file__).resolve().parents[3]
DEFAULT_DATA = _REPOSITORY / "data"


def _fetch(arguments: argparse.Namespace) -> int:
    raw = arguments.data / "raw"
    for spec in sources.ALL:
        refresh = arguments.refresh and spec.sha256 is None
        fetch.fetch(spec, raw, refresh=refresh)
    return 0


def _build(arguments: argparse.Namespace) -> int:
    raw = arguments.data / "raw"
    manifest = fetch.read_manifest(raw)
    missing = [spec.key for spec in sources.ALL if spec.key not in manifest]
    if missing:
        print(f"Not downloaded yet: {', '.join(missing)}. Run `stellar-pipeline fetch`.")
        return 1
    inputs = build.Inputs(
        hyg=raw / sources.HYG.filename,
        hipparcos=raw / sources.HIPPARCOS.filename,
        exoplanets=raw / sources.EXOPLANETS.filename,
        downloads=manifest,
    )
    target = arguments.data / "catalogue.sqlite"
    report = build.build(inputs, target)
    print(f"Built {target}")
    print(f"  stars from HYG: {report.stars}")
    print(f"  planet hosts not in HYG, added from the archive: {report.stars_from_archive}")
    print(f"  planets: {report.planets}")
    matched = ", ".join(
        f"{method} {count}" for method, count in sorted(report.host_matches.items())
    )
    print(f"  planet hosts matched by: {matched}")
    return 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="stellar-pipeline", description=__doc__)
    parser.add_argument(
        "--data",
        type=Path,
        default=DEFAULT_DATA,
        help="data directory: downloads go in raw/, the catalogue beside it (default: %(default)s)",
    )
    parser.add_argument("-v", "--verbose", action="store_true", help="log progress")
    commands = parser.add_subparsers(dest="command", required=True)
    fetch_command = commands.add_parser("fetch", help="download the source catalogues")
    fetch_command.add_argument(
        "--refresh",
        action="store_true",
        help="download sources that change over time (the exoplanet archive) again",
    )
    fetch_command.set_defaults(run=_fetch)
    commands.add_parser("build", help="build catalogue.sqlite").set_defaults(run=_build)

    arguments = parser.parse_args(argv)
    logging.basicConfig(
        level=logging.INFO if arguments.verbose else logging.WARNING,
        format="%(levelname)s %(name)s: %(message)s",
    )
    result: int = arguments.run(arguments)
    return result


if __name__ == "__main__":
    sys.exit(main())
