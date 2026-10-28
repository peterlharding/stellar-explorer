"""Checks that relative links between the Markdown files resolve, anchors included.

Run from anywhere: `python3 scripts/check_docs.py`. Exits non-zero on a broken link.
"""

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SKIP_DIRECTORIES = {".git", ".venv", "data", "node_modules", ".godot"}

_LINK = re.compile(r"\]\(([^)\s]+)\)")
_HEADING = re.compile(r"^#{1,6}\s+(.*?)\s*#*$", re.MULTILINE)
_FENCE = re.compile(r"^```.*?^```", re.MULTILINE | re.DOTALL)


def anchor(heading: str) -> str:
    """GitHub's anchor for a heading: lower case, punctuation dropped, spaces to hyphens."""
    text = re.sub(r"[`*_]", "", heading).strip().lower()
    text = re.sub(r"[^\w\- ]", "", text)
    return text.replace(" ", "-")


def anchors(path: Path) -> set[str]:
    text = _FENCE.sub("", path.read_text())
    return {anchor(heading) for heading in _HEADING.findall(text)}


def markdown_files() -> list[Path]:
    return sorted(
        path
        for path in ROOT.rglob("*.md")
        if not SKIP_DIRECTORIES.intersection(path.relative_to(ROOT).parts)
    )


def broken_links(path: Path) -> list[str]:
    problems = []
    text = _FENCE.sub("", path.read_text())
    for link in _LINK.findall(text):
        if re.match(r"^[a-z]+:", link) or "<" in link:
            # External, or a template such as release_notes/v<version>.md.
            continue
        target_part, _, fragment = link.partition("#")
        target = (path.parent / target_part).resolve() if target_part else path
        if not target.exists():
            problems.append(f"{link}: no such file")
        elif fragment and target.suffix == ".md" and fragment not in anchors(target):
            problems.append(f"{link}: no heading #{fragment}")
    return problems


def main() -> int:
    failures = 0
    for path in markdown_files():
        for problem in broken_links(path):
            print(f"{path.relative_to(ROOT)}: {problem}")
            failures += 1
    if failures:
        print(f"{failures} broken link(s)")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
