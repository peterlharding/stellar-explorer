# 0010. A public repository, with book-based scenarios kept outside it

Date: 2026-10-25
Status: Accepted

## Context

The GitHub repository is public.
Scenarios that model journeys from Alastair Reynolds' books carry names, places and events from published fiction, which is someone else's work.
A git-ignored folder keeps those files out of commits, but not out of harm: they have no history and no backup.

## Decision

The public repository holds the scenario mechanism and a neutral example scenario only.
Book-based scenarios live in a separate private repository, checked out into `scenarios/private/`, which this repository ignores.

## Alternatives considered

- **A private main repository.** Removes the problem but hides the code, which has no reason to be hidden.
- **A git-ignored folder with no repository behind it.** Keeps the files out of the public repository but gives them no history or backup.

## Consequences

- The app loads scenarios from `scenarios/` and `scenarios/private/` alike.
- The example scenario in the public repository uses invented names attached to real stars.
- Before every commit, `git status` is checked for scenario files outside `scenarios/private/`. See [RELEASING.md, Content safety](../../RELEASING.md#content-safety).
