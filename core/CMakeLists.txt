# ============================================================================
#  AriaAgent core — pure C++, zero Qt.
#    ariaagent_core    agent engine + tools + session store + i18n +
#                      ServiceHub (DI) + module_api (BaseVm/IModule/Registry)
#
#  Any platform shell (Qt desktop, iOS UIKit, Android) links this and
#  provides its own views + platform services. Nothing in this directory may
#  include Qt headers or link Qt targets.
#
#  Transport: Mira (hash-pinned release archive) — the same coroutine-
#  native C++23 networking library behind Aria's HTTP adapter. The LLM
#  client resolves, connects, handshakes TLS (always verified) and streams
#  response bodies through Mira; cpp-httplib is gone.
# ============================================================================

# Third-party dependencies, hash-pinned (the Mira way — no vendored
# sources, no submodules). Aria is fetched at an exact commit and added before
# this directory. Use its dependency verifier for our JSON and Mira pins.
include(ariaFetchPinned)

# Aria's core no longer downloads JSON; only its optional HTTP adapter needs it.
# AriaAgent uses JSON directly for model messages, tools, settings and storage.
if(NOT TARGET nlohmann_json::nlohmann_json)
    aria_fetch_pinned_archive(
        NAME json
        VERSION 3.12.0
        URL "https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz"
        SHA256 42f6e95cad6ec532fd372391373363b62a14af6d771056dbfc86160e6dfff7aa
    )
    add_library(ariaagent_json INTERFACE)
    target_include_directories(ariaagent_json SYSTEM INTERFACE
        "${ARIA_PINNED_JSON_SOURCE_DIR}/include")
    add_library(nlohmann_json::nlohmann_json ALIAS ariaagent_json)
endif()

aria_fetch_pinned_archive(
    NAME Mira
    VERSION 0.4.0
    URL "https://github.com/dqsjqian/Mira/releases/download/v0.4.0/Mira-0.4.0.tar.gz"
    SHA256 3290abda456d4897103f160e0905d09c5b29dfc973ec15024b8f76ce705dd856
)
set(MIRA_BUILD_TESTS    OFF)
set(MIRA_BUILD_EXAMPLES OFF)
set(MIRA_ENABLE_HTTP2   OFF)
set(MIRA_ENABLE_HTTP3   OFF)
set(MIRA_ENABLE_TLS     ON)   # LLM endpoints are HTTPS; Mira owns the OpenSSL wiring
add_subdirectory(${ARIA_PINNED_MIRA_SOURCE_DIR} Mira)

# ── ariaagent_core: engine + tools + i18n + services ───────────────────────
file(GLOB CORE_SRC CONFIGURE_DEPENDS
    agent/*.cpp
    i18n/*.cpp
    services/*.cpp
)
add_library(ariaagent_core STATIC ${CORE_SRC})
target_include_directories(ariaagent_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(ariaagent_core PUBLIC
    aria::binding      # transitively pulls runtime/async/core/abi
    aria::async
    nlohmann_json::nlohmann_json
    Mira::transport
    Mira::http
    Mira::tls
)
if(WIN32)
    target_link_libraries(ariaagent_core PUBLIC ws2_32 crypt32)
endif()
