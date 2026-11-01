# ============================================================================
#  AriaAgent — provider-agnostic C++23 Agent tooling framework GUI
#
#  Architecture (mirrors AiTools Workbench):
#    core/           pure C++ — zero Qt. Engine + ViewModel, platform-agnostic.
#                    mobile shells (iOS/Android) reuse this verbatim.
#    modules/*/platforms/qt  Qt6 views for the app and settings modules.
#    shared Aria SDK  ForgeDeps workspace build/aria/<target>
#
#  One core, many shells. Platform switch:
#    -DARIA_TARGET_QT=ON   Qt desktop (default)
#    (future: -DARIA_TARGET_IOS=ON / _ANDROID=ON)
# ============================================================================
cmake_minimum_required(VERSION 3.21)
project(AriaAgent VERSION 0.2.0 LANGUAGES CXX)

set(ARIA_DEPENDENCIES_FILE "${CMAKE_CURRENT_SOURCE_DIR}/dependencies.json" CACHE FILEPATH
    "Shared dependency requests and resolved versions")

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

# Aria and its HTTPS dependencies are imported from the shared workspace.
set(ARIA_DIR "" CACHE PATH "Optional editable Aria source for the shared SDK recipe")
set(ARIA_SDK_SOURCE "${ARIA_DIR}")
set(ARIA_SDK_QT ON)
set(ARIA_SDK_HTTP ON)
set(ARIA_SDK_TLS ON)
set(ARIA_SDK_SHARED OFF)
include(SharedAria)

# ── OpenSSL selection for Mira TLS/QUIC ─────────────────────────────────────
# Mira's TLS/QUIC engines call SSL_set1_ipaddr / SSL_set1_dnsname (OpenSSL
# 3.5+ APIs). Qt's bundled libssl lacks both symbols, and Qt sits first in
# CMAKE_PREFIX_PATH, so MiraConfig's find_package(OpenSSL) used to pick Qt's
# copy and llm_client_smoke failed to link. The ForgeDeps SDK environment
# already ships a pinned OpenSSL prefix with those symbols, so put it first:
# Mira then links the forged static libssl instead of Qt's shared one, while
# Qt itself still resolves from the later CMAKE_PREFIX_PATH entries. Qt
# Core/Gui/Widgets do not DT_NEEDED libssl, so this does not change the Qt
# runtime linkage of aria_agent.
foreach(_ariaagent_prefix IN LISTS ARIA_SDK_PREFIXES)
    if(_ariaagent_prefix MATCHES "/openssl/")
        list(REMOVE_ITEM CMAKE_PREFIX_PATH "${_ariaagent_prefix}")
        list(PREPEND CMAKE_PREFIX_PATH "${_ariaagent_prefix}")
        message(STATUS "AriaAgent: preferring forged OpenSSL prefix ${_ariaagent_prefix} over Qt's libssl")
    endif()
endforeach()
unset(_ariaagent_prefix)

# ── Pure-C++ core (agent engine + services + module API) ─────────────────────
add_subdirectory(core)
add_subdirectory(tests)

# ── Business modules (plugin pattern; see modules/CMakeLists.txt) ───────────
add_subdirectory(modules)

# ── Platform shell ──────────────────────────────────────────────────────────
if(ARIA_TARGET_QT)
    add_subdirectory(modules/app/platforms/qt)
endif()
