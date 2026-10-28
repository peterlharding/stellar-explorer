"""Downloads the source catalogues into the raw data directory.

A manifest beside the files records where each came from, when, and its SHA-256,
so a build can say exactly which download it was made from.
"""

import hashlib
import json
import logging
import shutil
import tempfile
import urllib.request
from dataclasses import asdict, dataclass
from datetime import UTC, datetime
from pathlib import Path

from stellar_pipeline import __version__
from stellar_pipeline.sources import SourceSpec

log = logging.getLogger(__name__)

MANIFEST = "manifest.json"
_CHUNK = 1 << 20


class FetchError(Exception):
    pass


@dataclass(frozen=True)
class Download:
    url: str
    filename: str
    sha256: str
    retrieved_at: str


def sha256_of(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        while chunk := stream.read(_CHUNK):
            digest.update(chunk)
    return digest.hexdigest()


def read_manifest(raw_dir: Path) -> dict[str, Download]:
    path = raw_dir / MANIFEST
    if not path.exists():
        return {}
    entries = json.loads(path.read_text())
    return {key: Download(**entry) for key, entry in entries.items()}


def _write_manifest(raw_dir: Path, manifest: dict[str, Download]) -> None:
    path = raw_dir / MANIFEST
    text = json.dumps({key: asdict(entry) for key, entry in sorted(manifest.items())}, indent=2)
    path.write_text(text + "\n")


def fetch(spec: SourceSpec, raw_dir: Path, *, refresh: bool = False) -> Download:
    """Download one source unless a verified copy is already present."""
    raw_dir.mkdir(parents=True, exist_ok=True)
    manifest = read_manifest(raw_dir)
    target = raw_dir / spec.filename
    known = manifest.get(spec.key)

    if known and target.exists() and not refresh:
        actual = sha256_of(target)
        if actual == known.sha256 and spec.sha256 in (None, actual):
            log.info("%s: already downloaded (%s)", spec.key, known.retrieved_at)
            return known
        log.warning("%s: local copy does not match its manifest entry, downloading again", spec.key)

    log.info("%s: downloading %s", spec.key, spec.download_url)
    request = urllib.request.Request(
        spec.download_url, headers={"User-Agent": f"stellar-explorer-pipeline/{__version__}"}
    )
    with tempfile.NamedTemporaryFile(dir=raw_dir, delete=False) as temporary:
        temporary_path = Path(temporary.name)
        try:
            with urllib.request.urlopen(request, timeout=300) as response:
                shutil.copyfileobj(response, temporary, _CHUNK)
        except BaseException:
            temporary_path.unlink()
            raise

    actual = sha256_of(temporary_path)
    if spec.sha256 is not None and actual != spec.sha256:
        temporary_path.unlink()
        message = f"{spec.key}: downloaded file has SHA-256 {actual}, expected {spec.sha256}"
        raise FetchError(message)

    temporary_path.replace(target)
    download = Download(
        url=spec.download_url,
        filename=spec.filename,
        sha256=actual,
        retrieved_at=datetime.now(UTC).isoformat(timespec="seconds"),
    )
    manifest[spec.key] = download
    _write_manifest(raw_dir, manifest)
    return download
