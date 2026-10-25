# Release notes

One file per release, named `v<version>.md` (for example `v0.1.0.md`). The file is used as the body of the GitHub release, so it must read well on its own. The process is in [../RELEASING.md](../RELEASING.md).

Write for someone exploring with the app, not for developers. Say what they can now do, on which platform, and anything they must do before upgrading.

## Structure

```markdown
# Stellar Explorer <version>

Two or three sentences: what this release is for and who it matters to.

## Highlights

- What is new, one line each, most noticeable first. Say which platforms (desktop, Vive, Vision Pro) each applies to.

## Fixed

- Bugs fixed, described by what the user saw. Omit the section if there are none.

## Data

- Source catalogues and their versions (for example HYG 4.2, NASA Exoplanet Archive retrieved YYYY-MM-DD).
- Whether the catalogue database must be rebuilt or downloaded again.
- Whether `user.sqlite` is migrated on first run, and where the backup copy is written.

## Under the hood

- Changes with no visible effect that a developer would want to know about. Keep it short.

## Not tested

- Any platform or headset a change in this release was not run on. Omit the section if everything was tested.
```

## Rules

- Link to the changelog for the full list; do not repeat it.
- Use absolute links to anything outside this file, because the text is also shown on GitHub's release page.
- Catalogue attribution belongs in Data whenever a database is distributed with the release.
