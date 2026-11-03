# Vendored SQLite amalgamation

The core compiles SQLite in rather than linking a system copy ([ADR 0014](../../../../docs/decisions/0014-core-as-linked-library.md)), so the library is self-contained on every target.

- **Version:** SQLite 3.54.1 (amalgamation `3540100`)
- **Source:** <https://sqlite.org/2026/sqlite-amalgamation-3540100.zip>
- **sha256 (zip):** `0a9c8e34352dad46006aa82836d5383bff9dc6bc9ce5a60553c05aa30e6ad1f9`
- **Files kept:** `sqlite3.c`, `sqlite3.h` (the amalgamation; `shell.c` and `sqlite3ext.h` are not used)
- **Licence:** public domain (SQLite is dedicated to the public domain)

Built by CMake as its own target with warnings off (it is third-party code) and `SQLITE_OMIT_LOAD_EXTENSION` / `SQLITE_THREADSAFE=1`. To update, download a newer amalgamation, replace both files, and update the version and sha256 above.
