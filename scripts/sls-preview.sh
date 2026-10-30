#!/usr/bin/env bash
# Build and run the actual Playden desktop interface with a mock PS5 console.
# Requires macOS, Xcode Command Line Tools, CMake/Ninja, and SDL2.
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$root"
if [[ $(uname -s) != Darwin ]]; then
  echo "Interactive Playden host UI currently requires macOS." >&2
  exit 1
fi
scripts/build-homebrew.sh
exec build/homebrew-host/playden-homebrew --mock-console "$@"
