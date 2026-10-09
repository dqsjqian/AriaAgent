#!/usr/bin/env python3
"""Unified build pipeline for AriaAgent (Windows/macOS/Linux).

Complete pipeline: deps -> build -> test -> bench -> package.
Powered by aria_deps.build_kit (pip install aria-deps).

Usage:
    python scripts/build.py                 # Full pipeline: deps + build + test
    python scripts/build.py deps            # Only resolve/download dependencies
    python scripts/build.py build           # Only configure + compile
    python scripts/build.py test            # Only run tests
    python scripts/build.py bench           # Only run benchmarks
    python scripts/build.py package         # Only create release package
    python scripts/build.py all             # Everything
    python scripts/build.py clean           # Remove all build output

Project-specific:
    --qt-dir PATH   Qt6 install prefix (overrides QT_DIR env and auto-detect)

Environment:
    QT_DIR          Qt6 install prefix (else auto-detected)
    MSYS2_ROOT      MSYS2 install root (Windows only)
    JOBS            Parallel build jobs (default: CPU count)
    ARIA_SOURCE     Local Aria repository path (skips download)
"""
from __future__ import annotations

import argparse
import os
import sys
from pathlib import Path

try:
    from aria_deps.build_kit import Pipeline, deploy_qt_dlls
except ImportError:
    print("Error: aria-deps is required. Install it with:", file=sys.stderr)
    print("    pip install aria-deps", file=sys.stderr)
    print("Or from source: pip install git+https://github.com/dqsjqian/AriaDeps.git",
          file=sys.stderr)
    sys.exit(1)

ROOT = Path(__file__).resolve().parents[1]


def extra_args(parser: argparse.ArgumentParser):
    parser.add_argument("--qt-dir", type=Path,
                        help="Qt6 install prefix (overrides QT_DIR)")


def aria_dep(args) -> list[str] | None:
    """Pinned Aria source: local override or fetch script."""
    if os.environ.get("ARIA_SOURCE"):
        print(f"[build] Using local Aria: {os.environ['ARIA_SOURCE']}")
        return None
    fetch = ROOT / "tools" / "ci" / "fetch_aria.py"
    if fetch.is_file():
        return [sys.executable, str(fetch)]
    return None


def main(argv=None) -> int:
    pipeline = Pipeline(
        name="aria-agent",
        root=ROOT,
        deps=[
            ("locked", [sys.executable, str(ROOT / "tools/ci/dependencies.py"), "resolve"]),
            ("aria", aria_dep),
        ],
        qt_required=True,
        post_build=[deploy_qt_dlls("aria_agent.exe")],
        extra_args=extra_args,
    )
    return pipeline.run(argv)


if __name__ == "__main__":
    sys.exit(main())
