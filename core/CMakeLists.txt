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

# The selected SDK environment owns these libraries and their license texts.
find_package(nlohmann_json CONFIG REQUIRED)
find_package(Mira CONFIG REQUIRED COMPONENTS transport http tls)

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
