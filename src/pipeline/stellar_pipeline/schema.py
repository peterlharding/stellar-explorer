"""The catalogue database schema.

`SCHEMA_VERSION` is stored as SQLite's `PRAGMA user_version`. Bump it whenever
a table or column changes; the core refuses to open a database built for a
different version (RELEASING.md, "Versioning"). docs/DATA-MODEL.md describes
the tables for people.
"""

SCHEMA_VERSION = 1

DDL = """
CREATE TABLE build_info (
    key TEXT PRIMARY KEY,
    value TEXT NOT NULL
) WITHOUT ROWID;

CREATE TABLE source (
    id INTEGER PRIMARY KEY,
    key TEXT NOT NULL UNIQUE,
    name TEXT NOT NULL,
    version TEXT NOT NULL,
    licence TEXT NOT NULL,
    url TEXT NOT NULL,
    citation TEXT NOT NULL,
    download_url TEXT,
    retrieved_at TEXT,
    sha256 TEXT
);

CREATE TABLE layer (
    id INTEGER PRIMARY KEY,
    key TEXT NOT NULL UNIQUE,
    name TEXT NOT NULL,
    kind TEXT NOT NULL CHECK (kind IN ('observed', 'hypothetical', 'fiction'))
);

CREATE TABLE object_kind (
    key TEXT PRIMARY KEY,
    description TEXT NOT NULL
) WITHOUT ROWID;

CREATE TABLE quantity (
    key TEXT PRIMARY KEY,
    object_kind TEXT NOT NULL REFERENCES object_kind (key),
    unit TEXT,
    description TEXT NOT NULL
) WITHOUT ROWID;

CREATE TABLE derivation (
    id INTEGER PRIMARY KEY,
    key TEXT NOT NULL UNIQUE,
    version INTEGER NOT NULL,
    description TEXT NOT NULL
);

CREATE TABLE dataset (
    key TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    object_kind TEXT NOT NULL REFERENCES object_kind (key),
    layer_id INTEGER NOT NULL REFERENCES layer (id),
    description TEXT NOT NULL
) WITHOUT ROWID;

CREATE TABLE object (
    id INTEGER PRIMARY KEY,
    kind TEXT NOT NULL REFERENCES object_kind (key),
    name TEXT NOT NULL,
    parent_id INTEGER REFERENCES object (id),
    layer_id INTEGER NOT NULL REFERENCES layer (id),
    source_id INTEGER NOT NULL REFERENCES source (id)
);

CREATE TABLE identifier (
    catalogue TEXT NOT NULL,
    value TEXT NOT NULL COLLATE NOCASE,
    object_id INTEGER NOT NULL REFERENCES object (id),
    source_id INTEGER NOT NULL REFERENCES source (id),
    PRIMARY KEY (catalogue, value, object_id, source_id)
) WITHOUT ROWID;

CREATE TABLE measurement (
    object_id INTEGER NOT NULL REFERENCES object (id),
    quantity TEXT NOT NULL REFERENCES quantity (key),
    source_id INTEGER NOT NULL REFERENCES source (id),
    value REAL NOT NULL,
    err_lower REAL CHECK (err_lower >= 0),
    err_upper REAL CHECK (err_upper >= 0),
    limit_flag INTEGER NOT NULL DEFAULT 0 CHECK (limit_flag IN (-1, 0, 1)),
    epoch REAL,
    derivation_id INTEGER REFERENCES derivation (id),
    preferred INTEGER NOT NULL DEFAULT 0 CHECK (preferred IN (0, 1)),
    invalid INTEGER NOT NULL DEFAULT 0 CHECK (invalid IN (0, 1)),
    note TEXT,
    reference_label TEXT,
    reference_url TEXT,
    PRIMARY KEY (object_id, quantity, source_id),
    CHECK (NOT (preferred AND invalid))
) WITHOUT ROWID;

CREATE TABLE property (
    object_id INTEGER NOT NULL REFERENCES object (id),
    name TEXT NOT NULL,
    source_id INTEGER NOT NULL REFERENCES source (id),
    value TEXT NOT NULL,
    preferred INTEGER NOT NULL DEFAULT 0 CHECK (preferred IN (0, 1)),
    reference_label TEXT,
    reference_url TEXT,
    PRIMARY KEY (object_id, name, source_id)
) WITHOUT ROWID;
"""

# Indexes are created after the bulk load, which is faster than maintaining them during it.
# That each quantity has exactly one preferred value is checked by the build
# rather than by a unique index, which would add a sixth to the file's size.
INDEXES = """
CREATE INDEX object_kind_name ON object (kind, name);
CREATE INDEX object_parent ON object (parent_id) WHERE parent_id IS NOT NULL;
CREATE INDEX identifier_object ON identifier (object_id);
CREATE INDEX measurement_preferred_value
    ON measurement (quantity, value) WHERE preferred = 1;
"""

# Convenience views: one row per object, preferred values only.
VIEWS = """
CREATE VIEW star AS
SELECT
    o.id,
    o.name,
    o.parent_id AS primary_id,
    max(CASE WHEN m.quantity = 'ra' THEN m.value END) AS ra,
    max(CASE WHEN m.quantity = 'dec' THEN m.value END) AS dec,
    max(CASE WHEN m.quantity = 'parallax' THEN m.value END) AS parallax,
    max(CASE WHEN m.quantity = 'distance' THEN m.value END) AS distance,
    max(CASE WHEN m.quantity = 'pmra' THEN m.value END) AS pmra,
    max(CASE WHEN m.quantity = 'pmdec' THEN m.value END) AS pmdec,
    max(CASE WHEN m.quantity = 'rv' THEN m.value END) AS rv,
    max(CASE WHEN m.quantity = 'x' THEN m.value END) AS x,
    max(CASE WHEN m.quantity = 'y' THEN m.value END) AS y,
    max(CASE WHEN m.quantity = 'z' THEN m.value END) AS z,
    max(CASE WHEN m.quantity = 'vx' THEN m.value END) AS vx,
    max(CASE WHEN m.quantity = 'vy' THEN m.value END) AS vy,
    max(CASE WHEN m.quantity = 'vz' THEN m.value END) AS vz,
    max(CASE WHEN m.quantity = 'vmag' THEN m.value END) AS vmag,
    max(CASE WHEN m.quantity = 'absmag' THEN m.value END) AS absmag,
    max(CASE WHEN m.quantity = 'b_v' THEN m.value END) AS b_v,
    max(CASE WHEN m.quantity = 'teff' THEN m.value END) AS teff,
    (SELECT p.value FROM property AS p
        WHERE p.object_id = o.id AND p.name = 'spectral_type' AND p.preferred) AS spectral_type
FROM object AS o
LEFT JOIN measurement AS m ON m.object_id = o.id AND m.preferred
WHERE o.kind = 'star'
GROUP BY o.id;

CREATE VIEW planet AS
SELECT
    o.id,
    o.name,
    o.parent_id AS host_id,
    h.name AS host_name,
    max(CASE WHEN m.quantity = 'orbital_period' THEN m.value END) AS orbital_period,
    max(CASE WHEN m.quantity = 'semi_major_axis' THEN m.value END) AS semi_major_axis,
    max(CASE WHEN m.quantity = 'eccentricity' THEN m.value END) AS eccentricity,
    max(CASE WHEN m.quantity = 'inclination' THEN m.value END) AS inclination,
    max(CASE WHEN m.quantity = 'radius' THEN m.value END) AS radius,
    max(CASE WHEN m.quantity = 'mass' THEN m.value END) AS mass,
    max(CASE WHEN m.quantity = 'equilibrium_temperature' THEN m.value END)
        AS equilibrium_temperature,
    max(CASE WHEN m.quantity = 'discovery_year' THEN m.value END) AS discovery_year,
    (SELECT p.value FROM property AS p
        WHERE p.object_id = o.id AND p.name = 'discovery_method' AND p.preferred)
        AS discovery_method
FROM object AS o
JOIN object AS h ON h.id = o.parent_id
LEFT JOIN measurement AS m ON m.object_id = o.id AND m.preferred
WHERE o.kind = 'planet'
GROUP BY o.id;
"""

OBJECT_KINDS = (
    ("star", "A star, including the Sun"),
    ("planet", "A planet orbiting a star other than the Sun"),
)

LAYERS = (("catalogue", "Catalogue", "observed"),)

DATASETS = (
    ("stars", "Stars", "star", "catalogue", "Stars from HYG, Hipparcos and planet hosts"),
    ("exoplanets", "Exoplanets", "planet", "catalogue", "Confirmed planets around other stars"),
)

# (key, object kind, unit, description). A unit of None means dimensionless.
QUANTITIES = (
    ("ra", "star", "deg", "Right ascension, ICRS, at the measurement's epoch"),
    ("dec", "star", "deg", "Declination, ICRS, at the measurement's epoch"),
    ("parallax", "star", "mas", "Trigonometric parallax"),
    ("distance", "star", "pc", "Distance from the Sun"),
    ("pmra", "star", "mas/yr", "Proper motion in right ascension, including cos(dec)"),
    ("pmdec", "star", "mas/yr", "Proper motion in declination"),
    ("rv", "star", "km/s", "Radial velocity, positive away from the Sun"),
    ("x", "star", "pc", "Position towards RA 0h, Dec 0 (ICRS equatorial, Sun at the origin)"),
    ("y", "star", "pc", "Position towards RA 6h, Dec 0"),
    ("z", "star", "pc", "Position towards the north celestial pole"),
    ("vx", "star", "km/s", "Velocity along x"),
    ("vy", "star", "km/s", "Velocity along y"),
    ("vz", "star", "km/s", "Velocity along z"),
    ("vmag", "star", "mag", "Apparent visual (V) magnitude"),
    ("hpmag", "star", "mag", "Apparent Hipparcos (Hp) magnitude"),
    ("absmag", "star", "mag", "Absolute visual magnitude"),
    ("b_v", "star", "mag", "B-V colour index"),
    ("luminosity", "star", "L_sun", "Luminosity in the V band, in solar luminosities"),
    ("teff", "star", "K", "Effective temperature"),
    ("orbital_period", "planet", "d", "Orbital period"),
    ("semi_major_axis", "planet", "AU", "Orbit semi-major axis"),
    ("eccentricity", "planet", None, "Orbit eccentricity"),
    ("inclination", "planet", "deg", "Orbit inclination"),
    ("periastron_argument", "planet", "deg", "Argument of periastron"),
    ("periastron_time", "planet", "d", "Time of periastron, as a Julian date"),
    ("radius", "planet", "R_earth", "Planet radius, in Earth radii"),
    ("mass", "planet", "M_earth", "Planet mass, or minimum mass (see mass_provenance)"),
    ("equilibrium_temperature", "planet", "K", "Equilibrium temperature"),
    ("discovery_year", "planet", "yr", "Year of discovery"),
)
