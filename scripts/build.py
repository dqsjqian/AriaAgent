#!/usr/bin/env python3
"""Unified one-shot build runner for AriaAgent (Windows/macOS/Linux).

Replaces scripts/build.sh, scripts/build.ps1 and scripts/deploy-dlls.ps1.

Usage:
    python scripts/build.py                 # Release build
    python scripts/build.py debug           # Debug build
    python scripts/build.py run             # Debug build and launch
    python scripts/build.py release-run     # Release build and launch
    python scripts/build.py clean           # Remove all build output

Parameters:
    mode            Build mode (positional, default: release):
                    - release:     Release build, output to build/flavors/release
                    - debug:       Debug build, output to build/flavors/debug
                    - run:         Debug build then launch the executable
                    - release-run: Release build then launch the executable
                    - clean:       Delete the entire build/ directory
    --jobs N        Parallel build jobs (default: JOBS env or CPU count)
    --qt-dir PATH   Qt6 install prefix, overrides QT_DIR env and auto-detection.
                    Must contain lib/cmake/Qt6/Qt6Config.cmake (or cmake/Qt6/
                    on Windows/MSYS2 layout).
    --build-dir PATH
                    Override the build directory (default:
                    build/flavors/<release|debug>). Useful for CI matrices
                    or side-by-side compiler builds.

Environment:
    QT_DIR          Qt6 install prefix (else auto-detected: brew on macOS,
                    MSYS2 UCRT64 on Windows, /usr/lib/qt6 etc. on Linux)
    MSYS2_ROOT      MSYS2 install root (Windows only, for compiler/DLL lookup)
    JOBS            Parallel build jobs (default: CPU count)

Build steps:
    1. Verify pinned Aria via scripts/ci/fetch_aria.py
    2. Detect Qt6 (or fail with a clear error)
    3. CMake configure (Ninja if available) with CMAKE_PREFIX_PATH=<qt>
    4. cmake --build
    5. Windows only: deploy DLLs via windeployqt + objdump recursive copy,
       so the exe runs by double-click
    6. run/release-run: launch the built executable
"""
from __future__ import annotations

import argparse
import os
import platform
import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def run(cmd, **kwargs):
    print(f"[build] {' '.join(str(c) for c in cmd)}", flush=True)
    subprocess.run(cmd, check=True, **kwargs)


def find_qt_prefix() -> Path | None:
    """Locate Qt6 install prefix."""
    # QT_DIR first, then the env vars set by jurplel/install-qt-action on CI.
    for env_name in ("QT_DIR", "QT_INSTALL_DIR", "QT_ROOT_DIR"):
        env_dir = os.environ.get(env_name)
        if not env_dir:
            continue
        p = Path(env_dir)
        if (p / "lib" / "cmake" / "Qt6" / "Qt6Config.cmake").is_file():
            return p
        # Windows layout: cmake dir directly under prefix
        if (p / "cmake" / "Qt6" / "Qt6Config.cmake").is_file():
            return p
    return None

    system = platform.system()
    if system == "Darwin":
        brew = shutil.which("brew")
        if brew:
            try:
                out = subprocess.run(
                    [brew, "--prefix", "qt"], capture_output=True, text=True, timeout=15
                )
                if out.returncode == 0:
                    p = Path(out.stdout.strip())
                    if (p / "lib" / "cmake" / "Qt6" / "Qt6Config.cmake").is_file():
                        return p
            except (subprocess.TimeoutExpired, OSError):
                pass
    elif system == "Windows":
        # MSYS2 UCRT64 layout
        msys2_root = os.environ.get("MSYS2_ROOT")
        candidates = []
        if msys2_root:
            candidates.append(Path(msys2_root) / "ucrt64")
        candidates += [
            Path("C:/msys64/ucrt64"),
            Path("D:/msys64/ucrt64"),
            Path.home() / "msys64" / "ucrt64",
        ]
        for c in candidates:
            if (c / "lib" / "cmake" / "Qt6" / "Qt6Config.cmake").is_file():
                return c
    else:
        # Linux: check common locations
        for c in [Path("/usr/lib/qt6"), Path("/usr/local/qt6"), Path.home() / "opt" / "qt6"]:
            if (c / "lib" / "cmake" / "Qt6" / "Qt6Config.cmake").is_file():
                return c
    return None


def find_msys2_bin() -> Path | None:
    """Locate MSYS2 UCRT64 bin directory (Windows only)."""
    msys2_root = os.environ.get("MSYS2_ROOT")
    candidates = []
    if msys2_root:
        candidates.append(Path(msys2_root) / "ucrt64" / "bin")
    candidates += [
        Path("C:/msys64/ucrt64/bin"),
        Path("D:/msys64/ucrt64/bin"),
        Path.home() / "msys64" / "ucrt64" / "bin",
    ]
    for c in candidates:
        if (c / "g++.exe").is_file():
            return c
    # Fall back to objdump on PATH
    objdump = shutil.which("objdump.exe") or shutil.which("objdump")
    if objdump:
        return Path(objdump).parent
    return None


def deploy_dlls(build_dir: Path, msys2_bin: Path | None):
    """Deploy runtime DLLs next to the exe (Windows only).

    1. windeployqt -> Qt DLLs + plugins
    2. objdump-based recursive copy of MSYS2/aria DLLs
    System DLLs are intentionally skipped.
    """
    if platform.system() != "Windows":
        return

    bin_dir = build_dir / "bin"
    exe = bin_dir / "aria_agent.exe"
    if not exe.is_file():
        print(f"[deploy] exe not found: {exe}", file=sys.stderr)
        sys.exit(1)

    if msys2_bin is None:
        msys2_bin = find_msys2_bin()
    if msys2_bin is None or not (msys2_bin / "objdump.exe").is_file():
        print("[deploy] objdump not found, skipping DLL deployment", file=sys.stderr)
        return

    # 1. Qt deployment via windeployqt
    windeployqt = msys2_bin / "windeployqt.exe"
    if windeployqt.is_file():
        print("[deploy] windeployqt...")
        subprocess.run(
            [str(windeployqt), "--no-translations", "--no-system-d3d-compiler",
             "--no-opengl-sw", str(exe)],
            capture_output=True,
        )
    else:
        print("[deploy] windeployqt not found, skipping Qt plugins")

    # 2. Recursive dependency copy via objdump
    system_dll = re.compile(
        r"^(kernel32|user32|gdi32|advapi32|shell32|ole32|oleaut32|comdlg32|"
        r"ws2_32|crypt32|dwmapi|winmm|version|shcore|ucrtbase|vcruntime|winhttp|"
        r"wldap32|netapi32|secur32|authz|mpr|rpcrt4|userenv|usp10|uxtheme|"
        r"d3d11|d3d12|dxgi|ntdll|dwrite)",
        re.IGNORECASE,
    )

    def get_imports(path: Path) -> list[str]:
        out = subprocess.run(
            [str(msys2_bin / "objdump.exe"), "-p", str(path)],
            capture_output=True, text=True,
        )
        dlls = []
        for line in out.stdout.splitlines():
            m = re.search(r"DLL Name:\s*(.+)", line)
            if m:
                dlls.append(m.group(1).strip())
        return dlls

    # api-ms-win-crt-*.dll live in System32\downlevel on Win10/11
    windir = os.environ.get("WINDIR", r"C:\Windows")
    extra_sources = [(Path(windir) / "System32" / "downlevel", re.compile(r"^api-ms-win-", re.IGNORECASE))]

    print("[deploy] copying runtime dependencies...")
    queue = [exe]
    processed = set()
    copied = 0
    while queue:
        f = queue.pop(0)
        if f in processed:
            continue
        processed.add(f)
        for dll in get_imports(f):
            if system_dll.match(dll):
                continue
            dest = bin_dir / dll
            if dest.is_file():
                queue.append(dest)
                continue
            candidates = [msys2_bin, bin_dir]
            for extra_path, extra_re in extra_sources:
                if extra_re.match(dll):
                    candidates.append(extra_path)
            for src_dir in candidates:
                src = src_dir / dll
                if src.is_file():
                    shutil.copy2(src, dest)
                    print(f"  + {dll}")
                    copied += 1
                    queue.append(dest)
                    break
    print(f"[deploy] done. {copied} DLL(s) copied.")
    print(f"Now run: {exe}")


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "mode",
        nargs="?",
        choices=("release", "debug", "run", "release-run", "clean"),
        default="release",
        help="Build mode (default: release)",
    )
    parser.add_argument("--jobs", type=int, default=int(os.environ.get("JOBS", os.cpu_count() or 4)))
    parser.add_argument("--qt-dir", type=Path, help="Qt6 install prefix (overrides QT_DIR)")
    parser.add_argument("--build-dir", type=Path, help="Override build directory")
    args = parser.parse_args(argv)

    if args.mode == "clean":
        build_root = ROOT / "build"
        print(f"[build] Removing {build_root}")
        shutil.rmtree(build_root, ignore_errors=True)
        return 0

    build_type = "Debug" if args.mode in ("debug", "run") else "Release"
    # Unified directory scheme: build/unified/<platform>-<toolchain>-<config>-<arch>
    # AriaAgent is Qt-based; platform defaults to qt, toolchain by host.
    host = platform.system()
    toolchain = "msvc" if host == "Windows" else "native"
    arch = platform.machine()
    suffix = f"qt-{toolchain}-{build_type.lower()}-{arch}"
    build_dir = args.build_dir or (ROOT / "build" / "unified" / suffix)

    # Tool checks
    for tool in ("cmake", "git", "python3" if platform.system() != "Windows" else "python"):
        if not shutil.which(tool):
            print(f"[build] Error: {tool} is not installed or not on PATH", file=sys.stderr)
            return 1

    # Verify pinned Aria
    print("[build] Verifying pinned Aria (set ARIA_SOURCE for a local repository)...")
    fetch_aria = ROOT / "scripts" / "ci" / "fetch_aria.py"
    if fetch_aria.is_file():
        run([sys.executable, str(fetch_aria)])

    # Qt6 detection
    qt_prefix = args.qt_dir
    if qt_prefix is None and os.environ.get("QT_DIR"):
        qt_prefix = Path(os.environ["QT_DIR"])
    if qt_prefix is None:
        qt_prefix = find_qt_prefix()
    if qt_prefix is None:
        print("[build] Error: Qt6 was not found. Install Qt6 or set QT_DIR to its prefix.",
              file=sys.stderr)
        return 1
    print(f"[build] Qt6: {qt_prefix}")

    # Windows: ensure MSYS2 tools on PATH
    msys2_bin = None
    if platform.system() == "Windows":
        msys2_bin = find_msys2_bin()
        if msys2_bin:
            os.environ["PATH"] = str(msys2_bin) + os.pathsep + os.environ["PATH"]
            os.environ.setdefault("CC", "gcc")
            os.environ.setdefault("CXX", "g++")

    # Configure
    configure_cmd = ["cmake", "-S", str(ROOT), "-B", str(build_dir)]
    if shutil.which("ninja"):
        configure_cmd += ["-G", "Ninja"]
    configure_cmd += [
        f"-DCMAKE_BUILD_TYPE={build_type}",
        f"-DCMAKE_PREFIX_PATH={qt_prefix}",
    ]
    print(f"[build] Configuring {build_type}")
    run(configure_cmd)

    # Build
    print(f"[build] Building with {args.jobs} jobs")
    run(["cmake", "--build", str(build_dir), "--parallel", str(args.jobs)])

    # Windows: deploy DLLs
    if platform.system() == "Windows":
        deploy_dlls(build_dir, msys2_bin)

    # Verify executable
    exe_name = "aria_agent.exe" if platform.system() == "Windows" else "aria_agent"
    executable = build_dir / "bin" / exe_name
    if not executable.is_file():
        print(f"[build] Error: executable not found at {executable}", file=sys.stderr)
        return 1

    print(f"[build] Complete: {executable}")
    if args.mode in ("run", "release-run"):
        print("[build] Launching AriaAgent")
        subprocess.run([str(executable)])
    return 0


if __name__ == "__main__":
    sys.exit(main())
