#!/usr/bin/env bash
# Unified one-shot build runner for macOS / Linux.
#
# Usage:
#   ./scripts/build.sh             # Release build
#   ./scripts/build.sh debug       # Debug build
#   ./scripts/build.sh run         # Debug build and launch
#   ./scripts/build.sh release-run # Release build and launch
#   ./scripts/build.sh clean       # Remove all build output
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MODE="${1:-release}"
JOBS="${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)}"

case "$MODE" in
  debug|run)
    BUILD_TYPE="Debug"
    FLAVOR="debug"
    ;;
  release|release-run)
    BUILD_TYPE="Release"
    FLAVOR="release"
    ;;
  clean)
    echo "[build] Removing $ROOT/build"
    rm -rf "$ROOT/build"
    exit 0
    ;;
  *)
    echo "Unknown mode: $MODE" >&2
    echo "Valid modes: release | debug | run | release-run | clean" >&2
    exit 2
    ;;
esac

for tool in cmake git; do
  if ! command -v "$tool" >/dev/null 2>&1; then
    echo "[build] Error: $tool is not installed or not on PATH" >&2
    exit 1
  fi
done

if [[ ! -f "$ROOT/third_party/aria/CMakeLists.txt" ]]; then
  echo "[build] Initializing the Aria submodule..."
  git -C "$ROOT" submodule update --init third_party/aria
fi

QT_PREFIX="${QT_DIR:-}"
if [[ -z "$QT_PREFIX" ]] && command -v brew >/dev/null 2>&1; then
  QT_PREFIX="$(brew --prefix qt 2>/dev/null || true)"
fi
if [[ -z "$QT_PREFIX" || ! -f "$QT_PREFIX/lib/cmake/Qt6/Qt6Config.cmake" ]]; then
  echo "[build] Error: Qt6 was not found. Install Qt6 or set QT_DIR to its prefix." >&2
  exit 1
fi

BUILD_DIR="$ROOT/build/flavors/$FLAVOR"

CONFIGURE_ARGS=(-S "$ROOT" -B "$BUILD_DIR")
if command -v ninja >/dev/null 2>&1; then
  CONFIGURE_ARGS+=(-G Ninja)
fi
CONFIGURE_ARGS+=(
  -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
  -DCMAKE_PREFIX_PATH="$QT_PREFIX"
)

echo "[build] Configuring $BUILD_TYPE"
cmake "${CONFIGURE_ARGS[@]}"

echo "[build] Building with $JOBS jobs"
cmake --build "$BUILD_DIR" --parallel "$JOBS"

EXECUTABLE="$BUILD_DIR/bin/aria_agent"
if [[ ! -x "$EXECUTABLE" ]]; then
  echo "[build] Error: executable not found at $EXECUTABLE" >&2
  exit 1
fi

echo "[build] Complete: $EXECUTABLE"
if [[ "$MODE" == "run" || "$MODE" == "release-run" ]]; then
  echo "[build] Launching AriaAgent"
  "$EXECUTABLE"
fi
