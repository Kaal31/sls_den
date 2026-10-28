# Desktop and mock-console harness (Layers 1–2)

Build on macOS using the normal host instructions, then run:

```sh
build/homebrew-host/playden-homebrew --mock-console
build/homebrew-host/playden-homebrew --mock-console --headless \
  --replay fixtures/mock-console/install-play.replay \
  --snapshot build/homebrew-host/mock-console-state.json \
  --capture build/homebrew-host/mock-console.png
ctest --preset homebrew-host --output-on-failure
```

This is the existing SDL/OpenGL desktop application, renderer, navigation model and controller mapping.
The optional mode uses live service request queues with offline doubles instead of the synthetic UI clock.
It displays **MOCK CONSOLE**. It never opens a real Steam session, reads a real account/profile,
contacts a console, launches Wine, or invokes a game process. `--scenario` is incompatible with this mode.
The existing `--simulation` scenarios continue to work separately.

## Fixture flow

The synthetic Steam owned-games response in `fixtures/mock-console/steam.json` provides a game,
executable path and inert payload. In the game's Details screen, **Manifest source** cycles among
unavailable, corrupt and valid fixtures. Install uses the real SLS source resolver and inert manifest
parser, ordered fallback, and `PlanAuthorizedDepots`. The mock Steam authorization answer is supplied
independently: depot 221, manifest GID 123456789. The third-party fixture deliberately names a different
GID. That value must never replace the authorized GID. No real game files, keys or credentials are included.

Install writes and checks fixture bytes, then prepares a private mock bottle and mock title metadata.
Title registration uses the production `ps5::TitleRegistry` protocol/parser with an injected in-memory
ShadowMount transport. Play checks the installed bytes, private bottle, title identity, chosen executable
and registration, then reports **Mock validation passed** while keeping title activity Stopped.

Each run owns a canonical, newly created `playden-mock-ps5-XXXXXX` directory beneath the host temporary
directory. Virtual paths such as `/data/playden-games/220`, `/data/playden/games/220`,
`/data/playden/kits/mock` and `/data/homebrew/FAKE78220` resolve beneath that directory. Traversal,
relative paths and symlinks are rejected. The directory is removed on normal exit, including handled
errors; forced termination can leave it behind. No caller-supplied destination is recursively removed.

## Acceptance and evidence

`mock_console` checks the real UI's source-selection, Install and Play requests; unavailable/corrupt
source fallback; unentitled depot denial; preservation of authorized GIDs; full storage; interrupted
download and verified-prefix resume; missing/corrupt game and runtime artifacts; unavailable, pending
and conflicting registrations; launch preparation; sandbox escapes; and temporary-directory cleanup.
`mock_console_desktop` runs the actual SDL executable with the checked-in replay and captures a PNG
and JSON state. macOS Actions builds and runs the full host CTest suite and uploads evidence even on failure.

The fixture installer and preparation writer are doubles, not `SteamInstallRunner` or prospero-win.
The full host suite additionally exercises production installer/runtime components under their existing
tests. The fixed mock account, instant service operations and one fixture game do not implement live
source configuration, real Steam login/CDN transfers, production kit admission, every queue operation,
real permissions/privilege behavior, SDK execution, CPU/GPU emulation, or console acceptance.
**No PS5 or PS4 hardware runtime was tested.**
