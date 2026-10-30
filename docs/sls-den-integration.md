# SLS Den integration

The harness milestone at `787e63b` passed both mock/desktop acceptance tests and **154/154 host tests** on macOS Actions attempt 2:
https://github.com/Kaal31/sls_den/actions/runs/40758936205/attempts/2
The full suite took 864.44 seconds. That result predates the following source-settings integration; the new changes require their own validation.

## Optional source integration

Settings > Library > Manifest source edits one optional HTTPS URL template containing `{appid}`. Clear the field to disable it. No provider is bundled. The existing preferences worker persists the setting; old files default to Off. URL whitespace, control characters and userinfo are rejected.

The existing SteamInstallRunner consults configured metadata only after Steam license, Windows depot selection and missing-base-depot checks. A complete metadata match retains Steam's depot order and authorized manifest GIDs. Source pins never replace those GIDs. Unknown depots, incomplete coverage, malformed data or source outages retain the complete Steam plan. Cancellation stops before download. The same DepotInstaller still performs the transfer and verification.

Network requests and preference reads run on the installer worker, never the UI thread. Logs record the metadata decision category without URLs or payloads. No second downloader, Lua execution, Moon dependency or ownership change is introduced.

## Tests and limits

New tests cover complete/incomplete metadata, unknown depots, source failure, Steam order/GID preservation, preference migration/restart/clear, URL validation and the real Settings editor model. CI also attempts a PS5 cross-build/package because the console entry point and explicit archive list include the optional source integration.

One source template can currently be edited in the native UI. Multi-source fallback exists in the resolver and fixtures; a persistent multi-provider manager is not implemented. Provider-specific APIs, authenticated providers, ZIP responses and live third-party endpoints remain unverified. Fixture responses contain raw Lua-style text metadata.

See [mock-console harness](mock-console-harness.md) for temporary storage, fixture installation/resume, registration and launch-preparation tests, and [PS4 feasibility](ps4-feasibility.md) for the separate port assessment.

No real Steam game transfer, PS5 title registration or Wine/game execution was tested on console hardware. Host tests and cross-builds are not console acceptance.

## Multi-source configuration model (October 31)

`src/sls/source_list.{h,cpp}` adds a conservative, bounded provider list serializer/parser. It supports up to 16 ordered entries, custom HTTPS templates containing `{appid}`, enable flags, and duplicate-ID rejection. Its data-only round-trip test is registered as `sls_source_list` with CTest. It does not store credentials or introduce unverified built-in provider endpoints. The existing one-source Preferences/Settings UI is unchanged; persistence of a full provider list, UI management, archive/provider protocols and live network tests remain to be implemented. This commit does not change Steam entitlements or install-time depot selection.
