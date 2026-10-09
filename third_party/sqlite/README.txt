SQLite 3.53.4, unmodified upstream amalgamation (sqlite3.c and sqlite3.h).
Source: https://sqlite.org/2026/sqlite-amalgamation-3530400.zip
Archive SHA3-256: 628a44cfe82c66aed1ccbbe85a562d2e33ebe64b3288981ed76285612227934e
License: public domain. https://sqlite.org/copyright.html

Compiled directly as C and linked into FileSpacer.exe; no SQLite DLL or CLI is required.
SQLITE_OMIT_LOAD_EXTENSION: no runtime extension loading.
SQLITE_DQS=0: SQL strings use standard single-quoted string literals.
Default thread safety is retained. MSVC runtime matches the application (/MT or /MTd).
No full-text-search or spatial-index extensions are enabled.
Update both upstream files together, and update the version/hash in this document.
