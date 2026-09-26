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
# sources, no submodules). Aria itself stays a git submodule by design:
# sibling project, four-library source linkage. Aria is added before this
# directory and already provides nlohmann_json::nlohmann_json from its own
# pinned fetch; this file only pins Mira.
include(ariaFetchPinned)

aria_fetch_pinned_archive(
    NAME Mira
    VERSION 0.2.0
    URL "https://github.com/dqsjqian/Mira/releases/download/v0.2.0/Mira-0.2.0.tar.gz"
    SHA256 3d833b6b45575b090a0a4de2a9283c23aab3081d15ada2ebde2b121af14e80bd
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
    nlohmann_json::nlohmann_json   # provided by Aria's pinned fetch above
    Mira::transport
    Mira::http
    Mira::tls
)
if(WIN32)
    target_link_libraries(ariaagent_core PUBLIC ws2_32 crypt32)
endif()
