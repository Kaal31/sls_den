# SLS Den → Playden integration contract testing

## Scope

The Windows test target `sls_playden_handoff_test` compiles the same
`ResolveInstallHandoff` function used by `SteamInstallRunner`. It calls
real source parsing, provider fallback and Steam-depot authorization logic,
then records the result at the Playden installer handoff boundary. All Steam
entitlements and provider responses in this target are **synthetic**.

The C++20 credential-aware boundary `ResolveCredentialHandoff` exercises
provider-key lookup and transport separately from URL templates. It does not
expose credentials to logs or grant Steam entitlements.

### Included deterministic cases

- Ordered enabled sources and fallback after a failed source
- Disabled providers never fetched
- Successful metadata observation and app/depot identity
- Unauthorized depot and third-party GID override attempts
- Missing license, malformed response and transport failure
- Keyed provider receiving a credential separately; missing-key rejection
- All install handoffs preserving Steam-authorized order and GIDs

## CI

The `Windows Application Build and UI` workflow builds and runs the
`sls_playden_handoff_test.exe` target even when `BUILD_TESTING=OFF`.
Run locally with the same configured Windows toolchain:

```powershell
cmake --build build/windows-offline --target sls_playden_handoff_test
./build/windows-offline/sls_playden_handoff_test.exe
```

## Not yet demonstrated by this contract test

- A live authenticated Steam session, refresh and expiry handling
- A real CM entitlement response being delivered to `SteamInstallRunner`
- Network operation of keyed providers (requires host-specific credential
  retrieval and verified provider-specific transport)
- Actual CDN chunk downloading, file installation, or PS5 launch
- A full `SteamInstallRunner` invocation using fake CM/CDN transports:
  the contract test starts at the already authorized depot boundary

These require separate transport-backed/integration tests, not claims that
this fixture proves live behavior. A live opt-in smoke test should use an
owned, legally accessible test title and never emit tokens or secrets.

## Architecture

The SLS provider metadata is **advisory**. Source metadata cannot grant
Steam ownership or substitute its manifest GIDs. Only Playden's Steam
authorization path can construct the authoritative depot list. A Windows
PS5-runtime adapter cannot prove a physical PS5 game launch.
