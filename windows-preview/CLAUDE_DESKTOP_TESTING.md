# Claude Desktop Windows GUI test handoff

This is a **human-approved local test workflow**, not a remote-control bridge. Claude Desktop computer use runs on the user's own Windows PC; ChatGPT cannot invoke or observe that session directly.

## Prerequisites
- A Windows 10/11 computer, the `sls-den-windows-preview` ZIP extracted, and `sls-den-preview.exe`.
- Claude Desktop with Computer use available and enabled in Settings > Capabilities (or Settings > System).
- Allow Claude access **only** to the SLS Den preview when Claude requests permission.
- Close sensitive apps/windows first. Do not enable unattended access or expose any local desktop-control port.

## Prompt to paste into Claude Desktop
Please test my locally extracted `sls-den-preview.exe` using Windows computer use. It is an **offline GUI preview**, not the full Playden/PS5 simulator.

Only interact with this app. Do not access my Steam account, browser credentials, terminals or other applications. Ask before writing outside the app's own temporary settings file. Do not download software or install dependencies.

Test:
1. Open the EXE and confirm window title, labels, field, list, and five buttons.
2. Add `https://example.org/manifests/{appid}.lua`; confirm it appears enabled.
3. Enter `http://example.org/{appid}`; confirm validation rejects it.
4. Add `https://backup.example.org/{appid}.lua`; confirm the two rows.
5. Disable/re-enable the first row, move the second source up/down, and remove a row.
6. Close and reopen the application; confirm the remaining source persists.
7. Check for frozen UI, rendering overflow, broken keyboard navigation, crashes and suspicious behavior.
8. Capture screenshots and record exact steps, expected vs actual, and reproducibility.

Deliver a Markdown report using `windows-preview/CLAUDE_TEST_REPORT_TEMPLATE.md` and screenshot filenames. Do not claim that Steam downloads or Playden simulation were tested.

## Sending results back
Create a GitHub issue in `Kaal31/sls_den` with label/title prefix `[Windows preview GUI]` and attach screenshots, **or** share the Markdown report and screenshots in ChatGPT. Only share data from the preview window. ChatGPT can then inspect the issue and reproduce or fix the code in `windows-preview`.

Do not put a Claude API key or any personal credentials in repository files or issue bodies.
