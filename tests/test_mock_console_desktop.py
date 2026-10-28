# Copyright (C) 2026 Playden contributors
# SPDX-License-Identifier: GPL-3.0-or-later
"""Exercise the real SDL executable and verify its captured evidence."""
import json
from pathlib import Path
import subprocess
import sys

executable, replay, build = sys.argv[1:]
build = Path(build)
state = build / "mock-console-state.json"
capture = build / "mock-console.png"
subprocess.run([executable, "--mock-console", "--headless", "--replay", replay,
                "--snapshot", str(state), "--capture", str(capture)], check=True, timeout=90)
snapshot = json.loads(state.read_text())
if not snapshot.get("mock_console") or snapshot.get("hardware_runtime_tested") is not False:
    raise RuntimeError("desktop snapshot lacks explicit mock/hardware boundary")
runtime = snapshot["mock_runtime"]
if "invalid_manifest" not in snapshot.get("mock_source_status", ""):
    raise RuntimeError("desktop flow did not exercise corrupt-source fallback")
if len(runtime) != 1 or not runtime[0]["ready"] or runtime[0]["play_status"] != "Mock validation passed":
    raise RuntimeError(f"desktop install/play flow failed: {runtime}")
if "no hardware runtime tested" not in runtime[0]["detail"]:
    raise RuntimeError("desktop outcome does not state runtime limit")
if capture.read_bytes()[:8] != b"\x89PNG\r\n\x1a\n":
    raise RuntimeError("desktop renderer did not capture a PNG")
print("PASS: actual desktop source selection, installation, preparation and Play validation; PNG captured; no hardware runtime tested")
