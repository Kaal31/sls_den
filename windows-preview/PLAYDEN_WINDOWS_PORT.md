# Native Playden UI on Windows — port boundary

The existing `windows-preview/main.cpp` is an intentionally separate Win32 source manager. It is not the Playden UI.

To replace it with the real Playden simulator, the port should reuse:
- `src/homebrew/simulation.cpp` for navigation, game-library model and controller actions.
- `src/homebrew/view.cpp` for the real renderer and source-setting rows.
- `src/homebrew/mock_console.cpp` for offline PS5-service doubles.
- The pinned `third_party/ps5-homebrew-ui` library without modifying its source.
- `src/sls/` for provider data and validation.

## Current Windows blockers
- `cmake/HomebrewUI.cmake` unconditionally rejects non-Apple host builds.
- The host uses POSIX headers/services and a Unix-style filesystem/signal/process model.
- `ps5-homebrew-ui/host/platform_host.cpp` and macOS SDL/OpenGL assumptions require a Windows host implementation.
- The CMake target graph includes native libraries and integration tests that must be examined individually for Windows.
- Fonts/assets must be packaged with the executable and located relative to its directory.
- The mock-console fixture runner uses Unix temporary path and low-level file APIs; translate safely to Windows before enabling installation tests.

## Acceptance target
1. Build `playden-homebrew.exe` with MSVC on `windows-2022` using SDL2 and OpenGL.
2. Run the real home/library/details/settings UI, controller input, and visible SLS Den provider management.
3. Run a headless fixture replay and save a state snapshot and screenshot.
4. Only then enable the full mock-console install/prepare cases in Windows CI.
5. Package an artifact independently of the Win32 fallback preview.
6. Do not touch `sls-manifest-integration` until the Windows host is validated.

The separate Win32 shell remains the currently validated Windows executable. Fixes in that shell are not evidence that the real Playden renderer has been ported.
