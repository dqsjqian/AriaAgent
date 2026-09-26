# ============================================================================
#  AriaAgent — provider-agnostic C++20 Agent tooling framework GUI
#
#  Architecture (mirrors AiTools Workbench):
#    core/           pure C++ — zero Qt. Engine + ViewModel, platform-agnostic.
#                    mobile shells (iOS/Android) reuse this verbatim.
#    platform/qt     Qt6 desktop shell — the ONLY place allowed to see Qt.
#    build/deps/aria   pinned Aria fetch (tools/ci/fetch_aria.py)
#
#  One core, many shells. Platform switch:
#    -DARIA_TARGET_QT=ON   Qt desktop (default)
#    (future: -DARIA_TARGET_IOS=ON / _ANDROID=ON)
# ============================================================================
cmake_minimum_required(VERSION 3.20)
project(AriaAgent LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
enable_testing()
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

if(MSVC)
    add_compile_options(/utf-8)
endif()

# ── Platform target ─────────────────────────────────────────────────────────
option(ARIA_TARGET_QT "Build the Qt6 desktop app (Win/Mac/Linux)" ON)
if(NOT ARIA_TARGET_QT)
    message(FATAL_ERROR "Only ARIA_TARGET_QT is implemented; other shells planned")
endif()

# ── Aria (pinned fetch; no git submodule) ───────────────────────────────────
set(ARIA_DIR ${CMAKE_CURRENT_SOURCE_DIR}/build/deps/aria CACHE PATH
    "Pinned Aria source tree, created by tools/ci/fetch_aria.py")
if(NOT EXISTS ${ARIA_DIR}/CMakeLists.txt)
    message(FATAL_ERROR
        "Aria not found at ${ARIA_DIR}.\n"
        "Run once before configuring:\n"
        "  python tools/ci/fetch_aria.py\n"
        "(no git submodules -- Aria is fetched at a pinned commit SHA)")
endif()

set(ARIA_BUILD_TESTS     OFF CACHE BOOL "" FORCE)
set(ARIA_BUILD_EXAMPLES  OFF CACHE BOOL "" FORCE)
set(ARIA_BUILD_BENCHMARK OFF CACHE BOOL "" FORCE)
if(ARIA_TARGET_QT)
    set(ARIA_BUILD_QT6 ON CACHE BOOL "" FORCE)
endif()
add_subdirectory(${ARIA_DIR} aria)

# ── Pure-C++ core (agent engine + services + module API) ─────────────────────
add_subdirectory(core)
add_subdirectory(tests)

# ── Business modules (plugin pattern; see modules/CMakeLists.txt) ───────────
add_subdirectory(modules)

# ── Platform shell ──────────────────────────────────────────────────────────
if(ARIA_TARGET_QT)
    add_subdirectory(modules/app/platforms/qt)
endif()
