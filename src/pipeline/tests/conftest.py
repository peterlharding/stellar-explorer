import gzip
import shutil
from pathlib import Path

import pytest

from stellar_pipeline import fetch, sources
from stellar_pipeline.build import Inputs

FIXTURES = Path(__file__).parent / "fixtures"


@pytest.fixture
def inputs(tmp_path: Path) -> Inputs:
    """The fixture excerpts laid out as `stellar-pipeline fetch` would leave them."""
    raw = tmp_path / "raw"
    raw.mkdir()
    hyg = raw / sources.HYG.filename
    with (FIXTURES / "hyg.csv").open("rb") as source, gzip.open(hyg, "wb") as target:
        shutil.copyfileobj(source, target)
    hipparcos = raw / sources.HIPPARCOS.filename
    with (FIXTURES / "hip2.dat").open("rb") as source, gzip.open(hipparcos, "wb") as target:
        shutil.copyfileobj(source, target)
    exoplanets = raw / sources.EXOPLANETS.filename
    shutil.copy(FIXTURES / "pscomppars.csv", exoplanets)
    downloads = {
        spec.key: fetch.Download(
            url=spec.download_url,
            filename=spec.filename,
            sha256=fetch.sha256_of(raw / spec.filename),
            retrieved_at="2026-10-25T00:00:00+00:00",
        )
        for spec in sources.ALL
    }
    return Inputs(hyg=hyg, hipparcos=hipparcos, exoplanets=exoplanets, downloads=downloads)
