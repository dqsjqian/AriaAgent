# ============================================================================
#  AriaAgent — DeepSeek Agent 工具框架 GUI, 基于 Aria (C++20 MVVM)
#
#  Layered on Aria via add_subdirectory (source embed, no install needed).
#  Aria is vendored as a git submodule at third_party/aria.
#  Uses:
#    - aria::core / aria::async      reactive core + coroutines
#    - aria::qt6                     Qt6 adapter (UI binding)
#    - cpp-httplib (vendored by Aria) DeepSeek REST client
#    - OpenSSL (system)              HTTPS transport
# ============================================================================

cmake_minimum_required(VERSION 3.20)
project(AriaAgent LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Qt6 (MSYS2 UCRT64 install, auto-detected via CMAKE_PREFIX_PATH) ────────
find_package(Qt6 REQUIRED COMPONENTS Widgets)
set(CMAKE_AUTOMOC ON)

# ── Aria submodule (must be checked out: git submodule update --init) ───────
set(ARIA_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/aria)
if(NOT EXISTS ${ARIA_DIR}/CMakeLists.txt)
    message(FATAL_ERROR
        "Aria submodule missing. Run:\n"
        "  git submodule update --init --recursive")
endif()

# Tests/examples/benchmark of Aria itself are not wanted here.
set(ARIA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ARIA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(ARIA_BUILD_BENCHMARK OFF CACHE BOOL "" FORCE)
set(ARIA_BUILD_QT6 ON CACHE BOOL "" FORCE)
add_subdirectory(${ARIA_DIR} aria)
# Aria's vendored nlohmann/json — reused by the LLM client.
set(NLOHMANN_DIR ${ARIA_DIR}/third_party/nlohmann_json/include)
# Use Aria's vendored cpp-httplib (0.53.1 since Aria cc19d76) — supports
# Client::Post ContentReceiver for true token-by-token SSE streaming.
set(CPPHTTPLIB_DIR ${ARIA_DIR}/third_party/cpp-httplib)

# OpenSSL for HTTPS transport (MSYS2 system package).
find_package(OpenSSL REQUIRED)

# ── Sources ─────────────────────────────────────────────────────────────────
file(GLOB_RECURSE AGENT_SRC CONFIGURE_DEPENDS src/agent/*.cpp)
file(GLOB_RECURSE UI_SRC    CONFIGURE_DEPENDS src/ui/*.cpp)

add_executable(aria_agent WIN32 MACOSX_BUNDLE
    src/main.cpp
    ${AGENT_SRC}
    ${UI_SRC}
)

target_include_directories(aria_agent PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_include_directories(aria_agent SYSTEM PRIVATE
    ${CPPHTTPLIB_DIR}
    ${NLOHMANN_DIR}
)

target_compile_definitions(aria_agent PRIVATE
    CPPHTTPLIB_OPENSSL_SUPPORT
)

target_link_libraries(aria_agent PRIVATE
    aria::qt6          # transitively pulls binding/runtime/async/core/abi
    Qt6::Widgets
    Qt6::Core
    OpenSSL::SSL
    OpenSSL::Crypto
    aria::warnings
)

if(WIN32)
    target_link_libraries(aria_agent PRIVATE ws2_32 crypt32)
endif()
