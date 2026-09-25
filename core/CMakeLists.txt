# ============================================================================
#  AriaAgent core — pure C++, zero Qt.
#    ariaagent_core    agent engine + tools + session store + i18n +
#                      ServiceHub (DI) + module_api (BaseVm/IModule/Registry)
#
#  Any platform shell (Qt desktop, iOS UIKit, Android) links this and
#  provides its own views + platform services. Nothing in this directory may
#  include Qt headers or link Qt targets.
#
#  Transport: Continuo (hash-pinned release archive) — the same coroutine-
#  native C++23 networking library behind Aria's HTTP adapter. The LLM
#  client resolves, connects, handshakes TLS (always verified) and streams
#  response bodies through Continuo; cpp-httplib is gone.
# ============================================================================

# Third-party dependencies, hash-pinned (the Continuo way — no vendored
# sources, no submodules). Aria itself stays a git submodule by design:
# sibling project, four-library source linkage. Aria is added before this
# directory and already provides nlohmann_json::nlohmann_json from its own
# pinned fetch; this file only pins Continuo.
include(ariaFetchPinned)

aria_fetch_pinned_archive(
    NAME continuo
    VERSION 0.1.2
    URL "https://github.com/dqsjqian/continuo/releases/download/v0.1.2/continuo-0.1.2.tar.gz"
    SHA256 508c56092c004bda7e7e606e2c266fab8a918b38bab46c96b37604862681dafa
)
set(CONTINUO_BUILD_TESTS    OFF)
set(CONTINUO_BUILD_EXAMPLES OFF)
set(CONTINUO_ENABLE_HTTP2   OFF)
set(CONTINUO_ENABLE_HTTP3   OFF)
set(CONTINUO_ENABLE_TLS     ON)   # LLM endpoints are HTTPS; Continuo owns the OpenSSL wiring
add_subdirectory(${ARIA_PINNED_CONTINUO_SOURCE_DIR} continuo)

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
    continuo::transport
    continuo::http
    continuo::tls
)
if(WIN32)
    target_link_libraries(ariaagent_core PUBLIC ws2_32 crypt32)
endif()
