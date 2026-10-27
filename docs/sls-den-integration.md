# SLS Den manifest integration

This branch introduces an independent C++17 parser for declarative LuaTools-style metadata.

- Reads `addappid(depot_id, ...)` and `setManifestid(depot_id, "gid")`.
- No Lua execution, unlocker installation, Steam client hooks, ownership changes, or changes to download authorization.
- The parser does not yet feed Playden's downloader or library UI. Steam licenses and depot checks are unchanged.
- Build registration: `cmake/SlsManifest.cmake`; host test: `sls_manifest_metadata`.
- PS5 hardware behavior has not been tested.
