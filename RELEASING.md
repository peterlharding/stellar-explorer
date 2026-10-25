# Releasing Stellar Explorer

This document describes the release process for Stellar Explorer.
Follow these steps for every release so that the version number, changelog, release notes, and git tag stay in sync.

Nothing has been released yet: `VERSION` is `0.0.0` and the repository holds documentation only. Steps that depend on tooling the project does not have yet are marked *planned*, and [Not yet in place](#not-yet-in-place) lists what is missing. Update this file in the same change that adds each piece.

## Versioning

The project uses [Semantic Versioning](https://semver.org/spec/v2.0.0.html): `MAJOR.MINOR.PATCH`.

- MAJOR: incompatible or sweeping changes.
- MINOR: new, backward-compatible features.
- PATCH: backward-compatible bug fixes only.

**One release version for the whole repo, as a label and not a guarantee.** The pipeline, the core library and the Godot front end ship together under one number. Five things are versioned five different ways here, and conflating them is the mistake to avoid:

| Thing | Versioned by | Changes |
| --- | --- | --- |
| Release | semver in `VERSION` | Every release |
| Database schema | Integer `SCHEMA_VERSION`, stored in each database as SQLite `PRAGMA user_version` | Only when tables or columns change; ordinal, never semver |
| Core interface | Integer `CORE_API_VERSION`, exported by the core library | Only on a breaking change to what the front end calls |
| Tour and scenario files | Integer `format` field in each file | Only on a breaking change to the file layout |
| Catalogue data | The source catalogue versions recorded in the `source` table (for example HYG 4.2), plus the pipeline build date | Whenever the pipeline is re-run against new sources |

The release version does **not** enforce compatibility. Checks do, and they run regardless of what the version says (*planned*: each arrives with the code it guards):

- The core refuses to open a `catalogue.sqlite` or `user.sqlite` whose `user_version` is not the `SCHEMA_VERSION` it was built for.
- The Godot front end reads `CORE_API_VERSION` when it loads the core and refuses to start on a mismatch.
- A tour or scenario file with an unknown `format` is rejected with a message, not half-loaded.
- The About panel reports the release version, schema version, core interface version and the source catalogue versions, so a bug report can say exactly what was running.

**When to split the release version:** the first time one part ships on its own schedule, for example a catalogue database published as a download that several app versions use, or a Vision Pro build released apart from the desktop build. Until then one version is simpler and costs nothing; after then it asserts something untrue.

Until `1.0.0` the database schema, the core interface and the tour and scenario formats are not stable.

## Where versions live

Today the version string lives in four places:

- `VERSION` at the repo root, which is **canonical**.
- `CHANGELOG.md` at the repo root, following [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
- `release_notes/v<version>.md`, one user-facing file per release.
- The git tag `v<version>` (annotated).

As components land, each gains a version field that has no reason to agree with the others unless something makes it:

- `version` in `src/pipeline/pyproject.toml`.
- The core library's build file: `project(... VERSION ...)` in `src/core/CMakeLists.txt` (the core is C++, [ADR 0008](docs/decisions/0008-core-in-cpp.md)).
- `config/version` in `src/app/project.godot`, and the bundle version fields in the Godot export presets for macOS and visionOS.

*Planned:* when the second of these appears, add `scripts/version.py` to write them together and a `make version-check` target that fails on drift. From then on, do not edit version fields by hand.

## Release steps

1. **Pick the version number** based on the nature of the changes (see Versioning above).

2. **Bump the version.** Today that is one file:

   ```sh
   echo "<version>" > VERSION
   git diff   # expect only VERSION to change
   ```

   *Planned:* `make version-set VERSION=<version>`, once more than one file carries the version.

3. **Cut the changelog.** In `CHANGELOG.md`:
   - Move the items under `## [Unreleased]` into a new `## [<version>] - YYYY-MM-DD` section (today's date).
   - Leave a fresh, empty `## [Unreleased]` at the top.
   - Add a `See [release_notes/v<version>.md](release_notes/v<version>.md) for details.` line under the new heading.
   - Update the link references at the bottom of the file: point `[Unreleased]` at `compare/v<version>...HEAD` and add a `[<version>]` tag link.

4. **Write the release notes** at `release_notes/v<version>.md`.
   See `release_notes/README.md` for the structure: a short intro, Highlights, any Fixed section, Data, and Under the hood.
   Write them for someone exploring with the app, not for developers.

5. **Run every check** and confirm there are no warnings and no failures. As each component lands its checks join this list (*planned:* one `make check` that runs them all):

   - Pipeline: `pytest`, with warnings treated as errors.
   - Core: the unit tests, including the fixed reference cases (known star distances, known relativistic travel times, known sky positions at a date).
   - Front end: the Godot project opens and exports for every target without errors or warnings.
   - Documentation: links between the files in `docs/` resolve.

   Do not cut a release on a failing or flaky suite.
   Fix the test or the code first, even when the failure looks unrelated to what the release contains.

6. **Check it in the headsets.** Automated tests cannot see a dropped frame. If the release touches rendering, input or performance:

   - Run it on the Vive and confirm it holds the headset's refresh rate with the full star budget.
   - Run it on the Vision Pro, fully immersive, and confirm the same.
   - Record in the release notes any platform a change was not tested on.

7. **Check the data.** If the release changes the schema or the pipeline:

   - Confirm `SCHEMA_VERSION` was bumped in the pipeline and in the core together.
   - Rebuild `catalogue.sqlite` from scratch with the pipeline. The catalogue is never migrated in place; it is rebuilt.
   - `user.sqlite` holds things that cannot be rebuilt (saved queries, scenarios, progress). If its schema changed, the release must migrate it forward on open, after writing a backup copy beside it. Say so in the release notes.
   - List the source catalogue versions in the release notes under Data.
   - Any database published with the release carries the attribution and licence of its sources. HYG and AT-HYG are CC BY-SA 4.0, so a database derived from them is published under the same licence.

8. **Commit** the version bump, changelog, and release notes together:

   ```sh
   git add -A
   git status   # nothing unexpected: no databases, downloaded catalogues, private scenarios, signing keys or .env files
   git commit -m "Release <version>"
   ```

9. **Push the commit, not the tag.**

   ```sh
   git push origin main
   ```

   There is no CI yet, so the local run in steps 5 to 7 is the gate. *Planned:* once CI exists, wait for it to pass on exactly this commit before tagging:

   ```sh
   gh run watch --exit-status \
     "$(gh run list --commit "$(git rev-parse HEAD)" -L 1 --json databaseId --jq '.[0].databaseId')"
   ```

   A tag is a claim that this commit is the release, and a pushed tag should never move.
   So it goes on only after the checks have passed on exactly that commit.

   If a check fails after the push, the release commit is on `main` but nothing claims it.
   Fix forward in a new commit, cut the changelog and notes again if the fix belongs in the release, and repeat this step on the new commit.

10. **Tag** the commit, with an annotated tag, and push the tag:

    ```sh
    git tag -a v<version> -m "Stellar Explorer <version>" <commit>
    git push origin v<version>
    ```

    Name the commit rather than tagging `HEAD`, so a commit made meanwhile cannot end up tagged by accident.
    If a tag ever does go out on a bad commit, leave it where it is and release the next patch version; moving a published tag rewrites what anyone who fetched it already has.

11. **Publish the GitHub release** from the tag, with the release notes as the body:

    ```sh
    gh release create v<version> --title "Stellar Explorer <version>" \
      --notes-file release_notes/v<version>.md --verify-tag
    ```

    `--verify-tag` refuses to publish if the tag is not on GitHub, rather than creating one on whatever `main` is by then.

    Treat creating a release as a one-shot.
    If the repository has immutable releases enabled, a release cannot be amended, and deleting it leaves the tag name reserved, so it cannot be recreated.
    Get the notes right first.

    If the previous version was tagged but never published, readers of this release have not seen its notes.
    Publish the two together, this release's first, with relative links made absolute.

12. **Build and attach the artefacts** (*planned*). There are none yet. When there are, build them from the tag, never from a working tree:

    - Desktop builds for Windows, Linux and macOS, exported from Godot. The Vive runs the Windows or Linux desktop build with OpenXR enabled; it is not a separate artefact.
    - The Vision Pro build, exported from Godot and built in Xcode on a Mac.
    - `catalogue.sqlite`, with a note of its sources and licence, if the database is distributed apart from the app.

## Commit conventions

Use a short prefix on commit messages so history is easy to scan.

- `Release <version>` for the single commit that cuts a release (step 8 above).
- `feat: <summary>` for a new feature, `fix: <summary>` for a bug fix.
- `refactor: <summary>` for restructuring that does not change behaviour.
- `docs: <summary>` for documentation and process changes that are not part of a release.
- `test: <summary>` for test-only changes.
- `build: <summary>` for build tooling, dependencies, CI, export presets and configuration.
- `data: <summary>` for a change to the database schema or to what the pipeline ingests, so data changes are greppable in history.
- `content: <summary>` for tours, missions and the example scenario.

Documentation or process changes (this file, README, the `docs/` tree) are committed on their own with a `docs:` message rather than being folded into a release commit.

Architecture decisions go in `docs/decisions/` as numbered ADRs, committed with `docs:`. An ADR is added when a decision is settled, not when it is proposed; open questions live in `docs/PLAN.md` until then.

## Content safety

Three kinds of file must never be committed.

- **Other people's fiction.** Scenario files that carry names, events or text from published books live in a separate private repository, checked out into `scenarios/private/`, which is git-ignored here ([ADR 0010](docs/decisions/0010-public-repository-private-scenarios.md)). This repository holds the scenario mechanism and a neutral example only.
- **Catalogues and built databases.** Downloaded source catalogues and built `.sqlite` files live in `data/`, which is git-ignored. They are large, the pipeline can rebuild them, and they carry licence terms of their own (see step 7).
- **Signing material and secrets.** Apple certificates, provisioning profiles, `.p8` and `.p12` keys, and `.env` files are git-ignored.

Check `git status` before every commit, and never force-add an ignored file.

## Not yet in place

Each of these is added with the phase in [docs/PLAN.md](docs/PLAN.md#roadmap) that first needs it.

- `scripts/version.py`, `make version-set` and `make version-check` (when a second file carries the version).
- `make check`, running every suite in step 5 (with the first code, phase 1).
- CI on push, and the wait-for-CI part of step 9 (phase 1 or 2).
- The schema guard in the core and the `CORE_API_VERSION` check in the front end (phases 1 and 2).
- Build artefacts and step 12 (phase 2 for desktop, phase 3 for the headsets).

## Notes

- `CHANGELOG.md` is maintained by hand; there is no generator.
  Add to `## [Unreleased]` as part of each change, not only at release time.
- Tags are annotated (`git tag -a`) so they carry a message and tagger, and can be verified and listed with release metadata.
- Keep one commit per release (`Release <version>`) so each tag anchors to a distinct, accurate point in history.
- Build output (Godot's `.godot/` cache, export directories, `.venv/`, compiled core binaries) is git-ignored. Nothing it produces is committed.
- The database schema is **not** part of the version scheme. A release does not have a "schema version"; it has a required `SCHEMA_VERSION`, recorded in the code and checked when a database is opened.
- Lockfiles **are** committed once they exist (`uv.lock` for the pipeline), and checks install from them. Changing a dependency means committing the regenerated lockfile in the same change.
- Godot's `*.import` files and `export_presets.cfg` **are** committed; the `.godot/` directory is not. Keep signing identities out of `export_presets.cfg`.
