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

# Dependencies share Aria's resolved versions and lock. An explicit
# ARIA_DEP_<NAME>_VERSION wins; otherwise the lock keeps builds reproducible.
include("${ARIA_DIR}/cmake/ariaFetchPinned.cmake")

# Aria may already provide these targets. Read its actual configured sources,
# never an inherited downloader cache variable belonging to a different target.
get_directory_property(_license_json DIRECTORY "${ARIA_DIR}" DEFINITION _aria_json_license_source)
get_directory_property(_license_mira DIRECTORY "${ARIA_DIR}" DEFINITION _aria_mira_license_source)
get_directory_property(_license_bundled_ssl DIRECTORY "${ARIA_DIR}" DEFINITION ARIA_BUNDLED_OPENSSL)
set(_license_openssl "")
if(_license_bundled_ssl)
    get_directory_property(_license_openssl DIRECTORY "${ARIA_DIR}" DEFINITION OPENSSL_SOURCE_DIR)
endif()

# Aria's core no longer downloads JSON; only its optional HTTP adapter needs it.
# AriaAgent uses JSON directly for model messages, tools, settings and storage.
if(NOT TARGET nlohmann_json::nlohmann_json)
    aria_fetch_pinned_archive(NAME json)
    set(_license_json "${ARIA_PINNED_JSON_SOURCE_DIR}")
    add_library(ariaagent_json INTERFACE)
    target_include_directories(ariaagent_json SYSTEM INTERFACE
        "${ARIA_PINNED_JSON_SOURCE_DIR}/include")
    add_library(nlohmann_json::nlohmann_json ALIAS ariaagent_json)
endif()

if(TARGET Mira::transport AND TARGET Mira::http AND TARGET Mira::tls)
    # A complete transport supplied by the parent is an explicit override.
elseif(TARGET Mira::transport OR TARGET Mira::http OR TARGET Mira::tls)
    message(FATAL_ERROR "AriaAgent requires parent Mira targets transport, http, and tls together")
else()
    aria_fetch_pinned_archive(NAME Mira)
    set(_license_mira "${ARIA_PINNED_MIRA_SOURCE_DIR}")
    # Use the same locked TLS dependency as Aria's HTTP adapter. Explicit parent
    # OpenSSL targets are honored by this helper before any source resolution.
    include("${ARIA_DIR}/cmake/BuildOpenSSL.cmake")
    if(ARIA_BUNDLED_OPENSSL)
        set(_license_openssl "${OPENSSL_SOURCE_DIR}")
    endif()
    set(MIRA_BUILD_TESTS    OFF)
    set(MIRA_BUILD_EXAMPLES OFF)
    set(MIRA_ENABLE_WEBSOCKET OFF)
    set(MIRA_ENABLE_HTTP2   OFF)
    set(MIRA_ENABLE_HTTP3   OFF)
    set(MIRA_ENABLE_TLS     ON)   # LLM endpoints are HTTPS; Mira owns the OpenSSL wiring
    add_subdirectory(${ARIA_PINNED_MIRA_SOURCE_DIR} Mira)
endif()

# ── ariaagent_core: engine + tools + i18n + services ───────────────────────
file(GLOB CORE_SRC CONFIGURE_DEPENDS
    agent/*.cpp
    i18n/*.cpp
    services/*.cpp
)
add_library(ariaagent_core STATIC ${CORE_SRC})
foreach(component json mira openssl)
    set_property(TARGET ariaagent_core PROPERTY ARIA_LICENSE_${component}_SOURCE "${_license_${component}}")
endforeach()
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
