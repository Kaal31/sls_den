# SLS Den live Windows → PS5 integration contract

This records actual wiring in branch `windows-preview`. **Building a module is not proof of live functionality.**

## Shared functional pipeline (Windows and PS5)

1. Authentication: `src/steam/backend/auth_client.*`, session repository/vault and CM renewal.
2. Owned library: `src/steam/backend/library_client.*`, `entitled_library.*` and `LibraryService`. Public Steam Store search is discovery **only**; a search hit never grants ownership.
3. Details and depot entitlements: CM app details service and licensed-depot checks.
4. Install queue: `SteamInstallRunner` with real authenticated CM/CDN sessions, storage-volume discovery, downloaded bytes, pause/resume/cancel, and reboot recovery.
5. Game preparation and launch: platform-owned runtime interfaces; Windows must not reuse PS5 executable registration or pretend a fixture runtime is real.
6. Manifest providers: `src/sls/provider_registry.h` and `provider_discovery.h`. These are distinct from Fixes providers.
7. Fixes providers: `src/sls/fix_source_registry.h` and `fix_source_discovery.h`; 404, non-200, timeout or exceptions produce **no** visible results from that provider, without suppressing successful others. Catalogue matches never automatically become installable.
8. UI: `src/homebrew/simulation.*` and `controller.*`; do not fork separate business logic into the Windows preview.

## Present verification / blockers

- `playden-windows-offline.exe`: simulator; no Steam login or real download.
- `playden-windows-online.exe`: public Steam Store search plus simulator; no owned-library login, real download or launch.
- `src/homebrew/host.cpp`: full service wiring exists, but Windows `--live-network` is denied pending private credential storage and secure publication checks.
- `windows-playden/ntfs_security.*`: private ACL/lease primitives exist; their presence alone does not verify their integration with all credential, session, manifest, and game-writing paths.
- CI: `windows-playden-cmake.yml` now explicitly compiles `playden-homebrew` to find Windows full-host portability failures.

## Release gates

- Verify Windows private data root, session permissions, storage isolation, lock semantics, write-then-rename publication, safe installation roots, reparse-point/symlink rejection and cleanup in automated tests.
- Pass live transport integration tests with test credentials and a genuine owned game; do not store credentials in workflow logs/artifacts.
- Verify entitlement before any real download, authentic depot/chunk validation, cancellation/retry/recovery and storage disconnect behavior.
- Verify source preferences and account tokens are passed to real source adapters, rather than disconnected UI-only fields.
- End-to-end native UI: login → owned library → choose install drive → download → verify → prepare → launch where supported → uninstall.
- Port the **shared** controller, source discovery and safety invariants to PS5. Rebuild only platform-specific credential storage, filesystem, runtime registration/launch and transport adapters. Repeat verification on physical PS5.

Until all relevant gates pass, artifact names and release notes must state which features are simulated.
