"""Builds catalogue.sqlite from the downloaded sources.

The database is built in a temporary file beside the target and moved into
place only after its integrity checks pass, so a failed build never leaves a
half-written catalogue behind.
"""

import logging
import sqlite3
from collections.abc import Iterable
from dataclasses import dataclass, field
from datetime import UTC, datetime
from pathlib import Path
from typing import Any

from stellar_pipeline import __version__, assemble, astrometry, exoplanets, hipparcos, hyg
from stellar_pipeline.crossmatch import HostMatches, HygIndex, match_hosts
from stellar_pipeline.exoplanets import ArchivePlanet
from stellar_pipeline.fetch import Download
from stellar_pipeline.records import ObjectRecord
from stellar_pipeline.schema import (
    DATASETS,
    DDL,
    INDEXES,
    LAYERS,
    OBJECT_KINDS,
    QUANTITIES,
    SCHEMA_VERSION,
    VIEWS,
)
from stellar_pipeline.sources import (
    EXOPLANETS,
    HIPPARCOS,
    HYG,
    PIPELINE_KEY,
    PIPELINE_URL,
    SourceSpec,
)

log = logging.getLogger(__name__)

_BATCH = 50_000


class BuildError(Exception):
    pass


@dataclass(frozen=True)
class Inputs:
    hyg: Path
    hipparcos: Path
    exoplanets: Path
    downloads: dict[str, Download]


@dataclass
class Report:
    stars: int = 0
    stars_from_archive: int = 0
    planets: int = 0
    host_matches: dict[str, int] = field(default_factory=dict)


class _Writer:
    """Buffers rows and writes them in batches."""

    def __init__(self, connection: sqlite3.Connection, layer_id: int) -> None:
        self._connection = connection
        self._layer_id = layer_id
        self._sources = dict(connection.execute("SELECT key, id FROM source"))
        self._derivations = dict(connection.execute("SELECT key, id FROM derivation"))
        self._next_id = 1
        self._rows: dict[str, list[tuple[Any, ...]]] = {
            "object": [],
            "identifier": [],
            "measurement": [],
            "property": [],
        }

    def add(self, record: ObjectRecord, parent_id: int | None = None) -> int:
        object_id = self._next_id
        self._next_id += 1
        source = self._sources
        self._rows["object"].append(
            (object_id, record.kind, record.name, parent_id, self._layer_id, source[record.source])
        )
        # Two sources may give the same identifier; one row per source is enough.
        seen: set[tuple[str, str, str]] = set()
        for identifier in record.identifiers:
            key = (identifier.catalogue, identifier.value.casefold(), identifier.source)
            if key in seen:
                continue
            seen.add(key)
            self._rows["identifier"].append(
                (identifier.catalogue, identifier.value, object_id, source[identifier.source])
            )
        for m in record.measurements:
            derivation = None if m.derivation is None else self._derivations[m.derivation]
            self._rows["measurement"].append(
                (object_id, m.quantity, source[m.source], m.value, m.err_lower, m.err_upper,
                 m.limit, m.epoch, derivation, int(m.preferred), int(m.invalid), m.note,
                 m.reference_label, m.reference_url)
            )  # fmt: skip
        for p in record.properties:
            self._rows["property"].append(
                (object_id, p.name, source[p.source], p.value, int(p.preferred),
                 p.reference_label, p.reference_url)
            )  # fmt: skip
        if len(self._rows["measurement"]) >= _BATCH:
            self.flush()
        return object_id

    def flush(self) -> None:
        for table, rows in self._rows.items():
            if rows:
                placeholders = ", ".join("?" * len(rows[0]))
                statement = f"INSERT INTO {table} VALUES ({placeholders})"
                self._connection.executemany(statement, rows)
                rows.clear()


def _create_schema(connection: sqlite3.Connection, downloads: dict[str, Download]) -> int:
    connection.executescript(DDL)
    connection.executemany("INSERT INTO object_kind VALUES (?, ?)", OBJECT_KINDS)
    connection.executemany("INSERT INTO layer (key, name, kind) VALUES (?, ?, ?)", LAYERS)
    layer_ids = dict(connection.execute("SELECT key, id FROM layer"))
    connection.executemany(
        "INSERT INTO dataset VALUES (?, ?, ?, ?, ?)",
        [(key, name, kind, layer_ids[layer], text) for key, name, kind, layer, text in DATASETS],
    )
    connection.executemany("INSERT INTO quantity VALUES (?, ?, ?, ?)", QUANTITIES)
    connection.executemany(
        "INSERT INTO derivation (key, version, description) VALUES (?, ?, ?)",
        [(key, version, text) for key, (version, text) in assemble.DERIVATIONS.items()],
    )
    for spec in (HYG, HIPPARCOS, EXOPLANETS):
        _insert_source(connection, spec, downloads[spec.key])
    connection.execute(
        "INSERT INTO source (key, name, version, licence, url, citation) VALUES (?, ?, ?, ?, ?, ?)",
        (
            PIPELINE_KEY,
            "Stellar Explorer pipeline",
            __version__,
            "MIT",
            PIPELINE_URL,
            "Values derived by the Stellar Explorer pipeline; see the derivation table.",
        ),
    )
    return int(layer_ids["catalogue"])


def _insert_source(connection: sqlite3.Connection, spec: SourceSpec, download: Download) -> None:
    # A source without a fixed version (the exoplanet archive) is versioned by retrieval date.
    version = spec.version or f"retrieved {download.retrieved_at}"
    connection.execute(
        "INSERT INTO source (key, name, version, licence, url, citation, download_url, "
        "retrieved_at, sha256) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)",
        (spec.key, spec.name, version, spec.licence, spec.url, spec.citation, download.url,
         download.retrieved_at, download.sha256),
    )  # fmt: skip


def _hosts(planets: Iterable[ArchivePlanet]) -> dict[str, ArchivePlanet]:
    """One row per host: the system-level columns are the same on every planet's row."""
    hosts: dict[str, ArchivePlanet] = {}
    for planet in planets:
        hosts.setdefault(planet.hostname, planet)
    return hosts


def _write(connection: sqlite3.Connection, inputs: Inputs, layer_id: int) -> Report:
    report = Report()
    writer = _Writer(connection, layer_id)

    log.info("reading Hipparcos")
    by_hip = {entry.hip: entry for entry in hipparcos.read(inputs.hipparcos)}

    log.info("reading the exoplanet archive")
    planets = list(exoplanets.read(inputs.exoplanets))
    hosts = _hosts(planets)

    log.info("matching planet hosts to HYG")
    index = HygIndex()
    for star in hyg.read(inputs.hyg):
        index.add(star, None if star.hip is None else by_hip.get(star.hip))
    matches: HostMatches = match_hosts(hosts.values(), index)
    report.host_matches = dict(matches.methods)
    host_for_hyg: dict[int, ArchivePlanet] = {}
    for hostname, hyg_id in matches.to_hyg.items():
        host_for_hyg.setdefault(hyg_id, hosts[hostname])

    log.info("assembling stars")
    object_for_hyg: dict[int, int] = {}
    primary_of: dict[int, int] = {}
    for star in hyg.read(inputs.hyg):
        entry = None if star.hip is None else by_hip.get(star.hip)
        record = assemble.star(star, entry, host_for_hyg.get(star.id))
        object_for_hyg[star.id] = writer.add(record)
        if star.primary_id is not None:
            primary_of[star.id] = star.primary_id
        report.stars += 1

    object_for_host: dict[str, int] = {
        hostname: object_for_hyg[hyg_id] for hostname, hyg_id in matches.to_hyg.items()
    }
    for hostname, host in hosts.items():
        if hostname not in object_for_host:
            object_for_host[hostname] = writer.add(assemble.host_star(host))
            report.stars_from_archive += 1

    log.info("assembling planets")
    for planet in planets:
        writer.add(assemble.planet(planet), parent_id=object_for_host[planet.hostname])
        report.planets += 1
    writer.flush()

    connection.executemany(
        "UPDATE object SET parent_id = ? WHERE id = ?",
        [(object_for_hyg[primary], object_for_hyg[companion])
         for companion, primary in primary_of.items() if primary in object_for_hyg],
    )  # fmt: skip
    return report


def _check(connection: sqlite3.Connection) -> None:
    problems = connection.execute("PRAGMA foreign_key_check").fetchall()
    if problems:
        message = f"foreign key violations, for example {problems[:5]}"
        raise BuildError(message)
    (integrity,) = connection.execute("PRAGMA integrity_check").fetchone()
    if integrity != "ok":
        message = f"integrity check failed: {integrity}"
        raise BuildError(message)
    unpreferred = connection.execute(
        "SELECT object_id, quantity FROM measurement WHERE NOT invalid "
        "GROUP BY object_id, quantity HAVING sum(preferred) != 1 LIMIT 5"
    ).fetchall()
    unpreferred += connection.execute(
        "SELECT object_id, name FROM property "
        "GROUP BY object_id, name HAVING sum(preferred) != 1 LIMIT 5"
    ).fetchall()
    if unpreferred:
        message = f"values without exactly one preferred choice: {unpreferred}"
        raise BuildError(message)


def build(inputs: Inputs, target: Path) -> Report:
    target.parent.mkdir(parents=True, exist_ok=True)
    temporary = target.with_name(target.name + ".building")
    temporary.unlink(missing_ok=True)
    connection = sqlite3.connect(temporary)
    try:
        connection.execute("PRAGMA foreign_keys = ON")
        connection.execute("PRAGMA journal_mode = OFF")
        connection.execute("PRAGMA synchronous = OFF")
        with connection:
            layer_id = _create_schema(connection, inputs.downloads)
            report = _write(connection, inputs, layer_id)
            connection.executescript(INDEXES)
            connection.executescript(VIEWS)
            built_at = datetime.now(UTC).isoformat(timespec="seconds")
            connection.executemany(
                "INSERT INTO build_info VALUES (?, ?)",
                [
                    ("schema_version", str(SCHEMA_VERSION)),
                    ("pipeline_version", __version__),
                    ("built_at", built_at),
                    ("reference_epoch", f"J{astrometry.REFERENCE_EPOCH}"),
                ],
            )
            connection.execute(f"PRAGMA user_version = {SCHEMA_VERSION}")
        _check(connection)
        connection.execute("ANALYZE")
        connection.execute("VACUUM")
    except BaseException:
        connection.close()
        temporary.unlink(missing_ok=True)
        raise
    connection.close()
    temporary.replace(target)
    return report
