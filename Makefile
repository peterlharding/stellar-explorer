# Stellar Explorer: every check, and the catalogue build.
# See RELEASING.md, step 5, for what `make check` must cover before a release.

PIPELINE := src/pipeline
RUN := cd $(PIPELINE) && uv run --locked

.PHONY: check lint typecheck test core docs version-check version-set fetch catalogue gate

## Every automated check. Must pass, with no warnings, before a release.
check: lint typecheck test core docs version-check

lint:
	$(RUN) ruff format --check . ../../scripts ../../spikes/unreal-stars/Scripts
	$(RUN) ruff check . ../../scripts ../../spikes/unreal-stars/Scripts

typecheck:
	$(RUN) mypy stellar_pipeline tests ../../scripts

test:
	$(RUN) pytest

## Build the engine-independent core and run its tests (ADR 0008; no engine needed).
core:
	cmake -S src/core -B src/core/build
	cmake --build src/core/build
	ctest --test-dir src/core/build --output-on-failure

docs:
	python3 -I scripts/check_docs.py

version-check:
	python3 -I scripts/version.py check

## Set the release version everywhere: make version-set VERSION=0.1.0
version-set:
	python3 -I scripts/version.py set $(VERSION)

## Download the source catalogues into data/raw (the exoplanet archive only once).
fetch:
	$(RUN) stellar-pipeline fetch

## Build data/catalogue.sqlite from the downloads.
catalogue: fetch
	$(RUN) stellar-pipeline build

## The phase 1 gate: the built catalogue against published values.
gate:
	$(RUN) pytest -m gate
