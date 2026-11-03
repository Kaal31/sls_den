#!/usr/bin/env python3
"""Report POSIX-only includes in sources configured for the Windows CMake build.

Informational in prototype CI: this is a dependency inventory, not a separate
security gate. Only compile-database sources are reported, not dormant PS5 files.
"""
import json
import pathlib
import re
import sys

EXCLUSIVE = re.compile(
    r'^\s*#\s*include\s*[<"]'
    r'(?:unistd\.h|dirent\.h|sys/(?:file|statvfs|mman|mount|wait|socket|'
    r'user|sysctl|resource|uio|times|select)\.h|pthread\.h|dlfcn\.h|'
    r'arpa/inet\.h|netinet/in\.h|pwd\.h|grp\.h|termios\.h)'
    r'[>"]',
)
OTHER_POSIX = re.compile(r'^\s*#\s*include\s*[<"](?:sys/stat\.h|sys/types\.h|fcntl\.h)[>"]')
INCLUDE = re.compile(r'^\s*#\s*include\s*"([^"]+)"')


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: audit_windows_posix.py <compile_commands.json>", file=sys.stderr)
        return 2
    database = pathlib.Path(sys.argv[1]).resolve()
    repo = pathlib.Path(__file__).resolve().parent.parent
    entries = json.loads(database.read_text(encoding="utf-8"))
    sources = set()
    for entry in entries:
        path = pathlib.Path(entry["file"])
        if not path.is_absolute():
            path = pathlib.Path(entry["directory"]) / path
        path = path.resolve()
        if path.is_relative_to(repo) and path.is_file():
            sources.add(path)

    # Also scan project-local headers directly included by the compiled units.
    pending = list(sources)
    scanned = set()
    findings = []
    while pending:
        path = pending.pop()
        if path in scanned:
            continue
        scanned.add(path)
        try:
            lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
        except OSError:
            continue
        for number, line in enumerate(lines, 1):
            if EXCLUSIVE.match(line):
                findings.append((path.relative_to(repo).as_posix(), number, line.strip(), "POSIX-only"))
            elif OTHER_POSIX.match(line):
                findings.append((path.relative_to(repo).as_posix(), number, line.strip(), "portability-review"))
            match = INCLUDE.match(line)
            if match:
                candidate = (repo / "src" / match.group(1)).resolve()
                if candidate.is_relative_to(repo) and candidate.is_file() and candidate not in scanned:
                    pending.append(candidate)
    print(f"Windows compilation graph: {len(sources)} source files, {len(scanned)} local sources/headers")
    print(f"Potential POSIX dependencies: {len(findings)}")
    print("Note: conditional includes may be legitimate. Review platform preprocessor branches.")
    for file, number, directive, kind in sorted(findings):
        print(f"{kind}: {file}:{number}: {directive}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
