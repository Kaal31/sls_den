# Lua Den: discovery and authentication roadmap

## Current supported settings
- Manual Hubcap (`moapikey`) and lua.tools access-token entry remain independent masked credential editors.
- Windows uses Credential Manager. PS5 needs its own secure-storage adapter; Windows-only credential storage does not make the PS5 path complete.
- A manually entered token may expire or be revoked. Never label a stored value as verified merely because it exists.
- Steam account ownership and installation entitlements are handled separately from catalog discovery.

## Later: PS5 browser-assisted sign-in (not implemented)
Reference: https://github.com/abhi245y/ps5-discord
- It demonstrates the PS5 system browser launcher (`sceSystemServiceLaunchWebBrowser`), native HTTP, QR code rendering, and encrypted per-user Discord session storage.
- It does **not** provide a lua.tools/Hubcap authentication grant, nor does a Discord user token automatically authenticate either provider.
- Add browser-assisted login **on top of** manual token/key entry, not instead of it, when a provider-supported OAuth redirect, device authorization, or explicitly documented callback is known.
- Confirm redirects/callbacks and user approval with provider documentation. Do not extract Discord account tokens or copy other apps' session stores.
- Repository is AGPL-3.0; assess license impact before copying components.

## Public catalog search: Unchained reference
Reference: https://github.com/Kaal31/Unchained
- `luatools/SteamStoreSearchClient.kt`: public Steam store search; it requests `/search/results/?term=...&start=...&count=...&infinite=1`, parses returned `results_html` and `total_count`, and extracts app IDs/names.
- `ui/screen/library/components/LibrarySearchBar.kt`: toggled search bar, live query, result count, clear, keyboard and D-pad focus behavior.
- `ui/screen/library/NonOwnedGameScreen.kt`: non-owned result detail with store artwork and a separate action.
- Playden's current Library query is scoped to known owned/imported games; it must stay that way.

## Planned UI progression
1. Add a **Search Steam Store** action alongside existing Library search, not replacing it. Use the PS5 system keyboard and an explicit source label.
2. Results should be isolated under a `Discover` heading with app ID, title, header image where available, and loading/empty/error states. Paginate, debounce, cache transiently, and never perform HTTP calls on the render thread.
3. Selecting a result opens a public detail sheet, not an owned game entry. Install/launch actions stay disabled unless Playden independently confirms lawful access through its existing entitlement pipeline.
4. Keep user-owned game search, library filters, favorites, storage and launch behavior unchanged.
5. Add offline fixture tests, controller focus tests, keyboard tests, HTML parser robustness checks and PS5 smoke checks before enabling networking.

## Source and security boundaries
Do not infer licenses or installability from Steam store search results. Limit query length, result count, response bytes, and timeouts. The Steam HTML parser is a compatibility risk: changes upstream may require updating fixtures. Prefer official supported search APIs if one becomes available.
