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
# Per socket operation (connect and each read), so an unreachable primary fails over to a
# mirror within this long rather than hanging; the sources are fast, so reads never need more.
_TIMEOUT_SECONDS = 30


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


def _download(spec: SourceSpec, raw_dir: Path) -> tuple[Path, str]:
    """Download to a temporary file, falling back to each mirror if the primary cannot be reached.

    Returns the temporary file and the URL it came from. A network or HTTP error on one URL moves
    on to the next; the SHA-256 is checked by the caller, so a mirror must serve the same file.
    """
    failures: list[str] = []
    for url in (spec.download_url, *spec.mirror_urls):
        log.info("%s: downloading %s", spec.key, url)
        request = urllib.request.Request(
            url, headers={"User-Agent": f"stellar-explorer-pipeline/{__version__}"}
        )
        with tempfile.NamedTemporaryFile(dir=raw_dir, delete=False) as temporary:
            temporary_path = Path(temporary.name)
            try:
                with urllib.request.urlopen(request, timeout=_TIMEOUT_SECONDS) as response:
                    shutil.copyfileobj(response, temporary, _CHUNK)
                return temporary_path, url
            except OSError as error:
                temporary_path.unlink(missing_ok=True)
                failures.append(f"{url}: {error}")
                log.warning("%s: could not download from %s (%s)", spec.key, url, error)
            except BaseException:
                temporary_path.unlink(missing_ok=True)
                raise
    raise FetchError(f"{spec.key}: all sources failed:\n  " + "\n  ".join(failures))


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

    temporary_path, used_url = _download(spec, raw_dir)

    actual = sha256_of(temporary_path)
    if spec.sha256 is not None and actual != spec.sha256:
        temporary_path.unlink()
        message = f"{spec.key}: downloaded file has SHA-256 {actual}, expected {spec.sha256}"
        raise FetchError(message)

    temporary_path.replace(target)
    download = Download(
        url=used_url,
        filename=spec.filename,
        sha256=actual,
        retrieved_at=datetime.now(UTC).isoformat(timespec="seconds"),
    )
    manifest[spec.key] = download
    _write_manifest(raw_dir, manifest)
    return download
