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
# sibling project, four-library source linkage.
include(ariaFetchPinned)

aria_fetch_pinned_archive(
    NAME json
    VERSION 3.12.0
    URL "https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz"
    SHA256 42f6e95cad6ec532fd372391373363b62a14af6d771056dbfc86160e6dfff7aa
)

aria_fetch_pinned_archive(
    NAME continuo
    VERSION 0.1.0
    URL "https://github.com/dqsjqian/continuo/releases/download/v0.1.0/continuo-0.1.0.tar.gz"
    SHA256 35eedfbacf2418218d83d6201113a5753e8919dbab02d1cec7f486443aa8b43a
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
target_include_directories(ariaagent_core SYSTEM PUBLIC
    ${ARIA_PINNED_JSON_SOURCE_DIR}/include
)
target_link_libraries(ariaagent_core PUBLIC
    aria::binding      # transitively pulls runtime/async/core/abi
    aria::async
    continuo::transport
    continuo::http
    continuo::tls
)
if(WIN32)
    target_link_libraries(ariaagent_core PUBLIC ws2_32 crypt32)
endif()
