# Windows SLS Den preview (experimental)
This is a **standalone native Win32 preview of the SLS Den manifest-source manager**, not yet Playden's SDL/OpenGL interface and not a PS5 emulator.

## Run
Download the **sls-den-windows-preview** artifact from the Windows SLS Den Preview Actions workflow on the `windows-preview` branch. Extract and run `sls-den-preview.exe`. No installer is required. The application runs completely offline and does not connect to Steam.

## Features
Add ordered HTTPS sources containing `{appid}`, enable/disable, remove, and change priority. Sources persist only for this preview, in the Windows user temporary directory as `sls-den-preview.conf`. This is intentionally isolated from PS5/Playden preferences.

## Limits
The real Playden C++ UI is still macOS/PS5-only. This preview shares the C++ `sls/source_list` and `source_catalog` model with the project but uses a Win32 shell as a safe first Windows build target. Future work may port the Playden renderer and console doubles into a Windows SDL host. No downloads, account access, OS emulation or launch behavior is included.
