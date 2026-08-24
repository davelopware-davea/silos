#!/usr/bin/env python3
"""Keep Alive's editor-only SilOS API bindings aligned with the API docs."""

from __future__ import annotations

import argparse
import hashlib
import re
import sys
from pathlib import Path


EXPERIMENT_DIR = Path(__file__).resolve().parent
REPOSITORY_ROOT = EXPERIMENT_DIR.parents[1]
STUB_PATH = REPOSITORY_ROOT / ".vscode" / "silos-api-stubs.lisp"
RUNTIME_CMAKE_PATH = EXPERIMENT_DIR / "runtime" / "CMakeLists.txt"
DOC_PATHS = (
    REPOSITORY_ROOT / "docs" / "design" / "API-BoundQueueStore.md",
    REPOSITORY_ROOT / "docs" / "design" / "API-BoundQueueMQTT.md",
    REPOSITORY_ROOT / "docs" / "design" / "API-Shell.md",
    REPOSITORY_ROOT / "docs" / "design" / "API-UI.md",
)
FINGERPRINT_PREFIX = ";;; API docs SHA-256: "
PUBLIC_HEADING = re.compile(
    r"^#{2,4} `((?:store|mqtt|app|ui)-[a-z0-9-]+|require|import)`\s*$",
    re.MULTILINE,
)
STUB_DEFINITION = re.compile(
    r"^\(def(?:un|macro) ([a-z][a-z0-9-]+)(?:\s|$)", re.MULTILINE
)
PUBLIC_BUILTIN = re.compile(
    r'\{ \\"((?:store|mqtt|app|ui)-[a-z0-9-]+|require|import)\\",\s*fn_'
)


def normalised_text(path: Path) -> str:
    return path.read_text(encoding="utf-8").replace("\r\n", "\n").replace("\r", "\n")


def api_docs_fingerprint() -> str:
    digest = hashlib.sha256()
    for path in DOC_PATHS:
        relative_path = path.relative_to(REPOSITORY_ROOT).as_posix()
        digest.update(relative_path.encode("utf-8"))
        digest.update(b"\0")
        digest.update(normalised_text(path).encode("utf-8"))
        digest.update(b"\0")
    return digest.hexdigest()


def documented_names() -> set[str]:
    return {
        name
        for path in DOC_PATHS
        for name in PUBLIC_HEADING.findall(normalised_text(path))
    }


def stubbed_names(stub_text: str) -> set[str]:
    return set(STUB_DEFINITION.findall(stub_text))


def implemented_names() -> set[str]:
    return set(PUBLIC_BUILTIN.findall(normalised_text(RUNTIME_CMAKE_PATH)))


def fail_for_name_drift(stub_text: str) -> None:
    documented = documented_names()
    stubbed = stubbed_names(stub_text)
    implemented = implemented_names()
    missing = sorted(documented - stubbed)
    unexpected = sorted(stubbed - documented)
    undocumented = sorted(implemented - documented)
    if not missing and not unexpected and not undocumented:
        return
    if missing:
        print(f"Alive API stubs missing documented names: {', '.join(missing)}", file=sys.stderr)
    if unexpected:
        print(f"Alive API stubs contain undocumented names: {', '.join(unexpected)}", file=sys.stderr)
    if undocumented:
        print(
            f"Prototype public built-ins missing from API docs and Alive stubs: {', '.join(undocumented)}",
            file=sys.stderr,
        )
    raise SystemExit(1)


def stored_fingerprint(stub_text: str) -> str | None:
    fingerprints = [
        line.removeprefix(FINGERPRINT_PREFIX).strip()
        for line in stub_text.splitlines()
        if line.startswith(FINGERPRINT_PREFIX)
    ]
    if len(fingerprints) != 1:
        return None
    return fingerprints[0]


def update_fingerprint(stub_text: str, expected: str) -> None:
    current = stored_fingerprint(stub_text)
    if current is None:
        print(f"Expected exactly one '{FINGERPRINT_PREFIX}' marker in {STUB_PATH}", file=sys.stderr)
        raise SystemExit(1)
    updated = stub_text.replace(
        f"{FINGERPRINT_PREFIX}{current}", f"{FINGERPRINT_PREFIX}{expected}", 1
    )
    STUB_PATH.write_text(updated, encoding="utf-8", newline="\n")
    print(f"Updated {STUB_PATH.relative_to(REPOSITORY_ROOT)} after API review.")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--update",
        action="store_true",
        help="accept the current API docs after reviewing and updating the stubs",
    )
    args = parser.parse_args()

    stub_text = normalised_text(STUB_PATH)
    fail_for_name_drift(stub_text)
    expected = api_docs_fingerprint()
    current = stored_fingerprint(stub_text)

    if args.update:
        update_fingerprint(stub_text, expected)
        return
    if current != expected:
        print("Alive API stubs have not been reviewed against the current API docs.", file=sys.stderr)
        print(f"Expected fingerprint: {expected}", file=sys.stderr)
        print(f"Stored fingerprint:   {current or '<missing>'}", file=sys.stderr)
        print(
            "Review .vscode/silos-api-stubs.lisp, then run "
            "'python experiments/browser-todo-prototype/check_alive_api_stubs.py --update'.",
            file=sys.stderr,
        )
        raise SystemExit(1)

    print(
        f"Alive API stubs match {len(documented_names())} documented public names, "
        f"cover {len(implemented_names())} implemented prototype built-ins, and match the API docs fingerprint."
    )


if __name__ == "__main__":
    main()
