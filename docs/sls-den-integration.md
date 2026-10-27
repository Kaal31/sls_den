# SLS Den — integration status

## Implemented on sls-manifest-integration
- Native, data-only LuaTools metadata parser (`src/sls/manifest_metadata.*`).
- Configurable HTTPS source registry and URL validation (`src/sls/source_catalog.*`).
- Entitlement-preserving depot selection planner (`src/sls/authorized_depot_plan.h`).
- Three host CTest targets registered through `cmake/SlsManifest.cmake`, included in the top-level CMake build.

## Boundaries
- No Lua execution or Steam client hooks.
- No Moon binary dependency.
- Sources are not fetched yet: the registry merely validates configured endpoint templates. Provider endpoints, authentication and real network behavior remain unverified.
- Depot planning intersects requested depot IDs with **already-authorized** Steam `DepotInfo` records; manifest GIDs from third parties are not used for installation.
- The existing `SteamInstallRunner`, `SelectDepots`, `DepotInstaller`, authentication and game launching remain unchanged.
- No Playden-native UI work is done yet.

## Validation
- The separately packaged early SLS manifest prototype built under Linux GCC and its CTest smoke test passed.
- The new integration-branch CTest targets have **not yet been run** in a full Playden checkout.
- No macOS graphical simulation, PS5 SDK build, or hardware launch test has been run.
- A Playden console launch after stages 3–5 requires installed PS5 hardware and compatible firmware. A host simulation cannot substitute for this.

## Next
1. Confirm source API schemas and integrate Playden's HTTP client with fixture-backed source tests.
2. Complete validation of Lua manifest syntax using fixtures, never executing source code.
3. Add UI for inspecting available source metadata and existing entitled depots.
4. Integrate only with Playden's supported authorized download path.
5. Run full host test suite, PS5 cross-compile, and finally launch acceptance tests on a jailbroken PS5.

## Source resolution module (latest)
- `src/sls/source_resolver.{h,cpp}`: ordered source fallback using a host-provided fetch callback.
- Payloads are capped at 1 MiB and parsed as *data*, never executed as Lua.
- API-key sources are excluded until authenticated transport is designed.
- Does not currently implement live HTTPS, archive unpacking, or endpoint-specific provider authentication.
- `tests/sls_source_resolver_test.cpp` covers first-provider invalid metadata, fallback, network failure and oversized responses.
- The new test is registered in CMake but has not been executed in a full Playden build environment.
- Playden's Steam downloader, DRM handling, permissions and launch behavior are unchanged.
