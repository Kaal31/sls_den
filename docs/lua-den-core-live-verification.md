# Lua Den core integration and live verification

## Status (2026-11-01)

**Important:** passing a Windows preview build is not evidence that games register, patches apply, or Windows titles launch on a PS5. No physical PS5 live execution has been observed in this integration session.

| Stage | Current evidence | Exit criterion |
| --- | --- | --- |
| Public Steam catalog discovery | HTTPS worker, parser and UI search results wired | PS5 system keyboard search + real network result + correct game ID |
| Select/inspect result | UI displays catalog IDs; detail page incomplete | Dedicated game details with actionable origin and entitlement state |
| Add/register game | No complete public-result-to-registered-game pipeline | Persist registration origin separately from Steam ownership; show in unified Library with ADDED badge |
| Provider manifest discovery | Resolver, key fields, source fallbacks and offline fixture tests | Live HTTP fetch with sanitized diagnostics, failure handling and compatible metadata |
| Authorized content installation | Existing Playden Steam installer and authorized depot planner | Explicit Steam entitlement and manifest verification; never use third-party GIDs to expand access |
| Native runtime preparation | Playden runtime/bottle services exist | PS5 kit admission, title registration, first launch and return-to-Library verified on hardware |
| Fix discovery/listing | Unchained/SLSDeck reviewed, no PS5 live fix workflow connected | Per-game choices, source, requirements, applicability status and progress |
| Fix application and removal | Not yet connected | Safe file operation, hash/backup journal, rollback and verified Un-fix |
| Offline resilience | Badge caches persist; source behavior varies | Disconnect/restart retains registrations, metadata, saves, and verified fixes; no endless spinner |

## Essential test routes

1. **Owned title:** Steam sign in → refresh entitlements → search → select → install authorized content → prepare title → launch → return.
2. **Not-owned public title:** discover → select → report unowned; no install/ownership claims or unauthorized depot access.
3. **Added local title:** import files from supported folder → validate executables → create persistent local registration → prepare → launch → restore after restart. The ADDED badge needs a provenance record.
4. **Provider failure:** source times out, malformed archive, invalid ZIP path, missing token, HTTP 429; UI must show a terminal actionable error with retry.
5. **Fix path:** list available fixes for an installed game → require explicit consent → back up every target → apply a verified game-version-compatible fix → log changes → restore originals; refuse path traversal, symlink targets and mismatched hashes.
6. **Offline restart:** sign out/network down → Library still available from cache → per-game settings and metadata remain → queued online work fails cleanly.

## Collect from PS5

Firmware and runtime-kit revision, game app ID, source selection, exact steps, whether game is Steam-owned/imported, and redacted `runtime.log`, `install.log`, `launch.log`; remove login tokens, Steam session data and secret provider keys.

## Implementation policy

Keep store-wide search independent from Steam entitlements. A source-discovered title is not a license. Provider manifests are metadata until content access is independently authorized. No unreviewed executable patch may be applied automatically. Native PS5 capabilities must be tested on console; Windows Actions can validate portable logic and offline UI only.
