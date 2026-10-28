"""Keeps every version field in step with VERSION, the canonical release version.

    python3 scripts/version.py check        fails if any field differs from VERSION
    python3 scripts/version.py set 0.1.0    writes the version everywhere, then re-locks

See RELEASING.md, "Where versions live". Add each new field to FIELDS.
"""

import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
VERSION_FILE = ROOT / "VERSION"
_SEMVER = re.compile(r"^\d+\.\d+\.\d+$")

# (file, pattern whose group 1 is the version)
FIELDS = (
    (ROOT / "src/pipeline/pyproject.toml", re.compile(r'^version = "([^"]+)"$', re.MULTILINE)),
)


def canonical() -> str:
    return VERSION_FILE.read_text().strip()


def check() -> int:
    expected = canonical()
    drift = 0
    for path, pattern in FIELDS:
        match = pattern.search(path.read_text())
        found = match[1] if match else None
        if found != expected:
            print(f"{path.relative_to(ROOT)}: {found}, but VERSION is {expected}")
            drift += 1
    return 1 if drift else 0


def set_version(version: str) -> int:
    if not _SEMVER.match(version):
        print(f"{version!r} is not MAJOR.MINOR.PATCH")
        return 1
    VERSION_FILE.write_text(version + "\n")
    for path, pattern in FIELDS:
        text = path.read_text()
        match = pattern.search(text)
        if match is None:
            print(f"{path.relative_to(ROOT)}: no version field found")
            return 1
        path.write_text(text[: match.start(1)] + version + text[match.end(1) :])
    # uv.lock records the pipeline package's own version.
    subprocess.run(["uv", "lock", "--quiet"], cwd=ROOT / "src/pipeline", check=True)
    return check()


def main(arguments: list[str]) -> int:
    if arguments == ["check"]:
        return check()
    if len(arguments) == 2 and arguments[0] == "set":
        return set_version(arguments[1])
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
