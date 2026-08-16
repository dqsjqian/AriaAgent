# ============================================================================
#  AriaAgent core — pure C++, zero Qt. Builds two static libs:
#    ariaagent_core         agent engine + tools + session store (no UI, no Qt)
#    ariaagent_viewmodel    framework-agnostic ViewModel (aria Property/Signal)
#
#  Any platform shell (Qt desktop, iOS UIKit, Android) links these and
#  provides its own views + platform services. Nothing in this directory may
#  include Qt headers or link Qt targets.
# ============================================================================

# nlohmann/json + cpp-httplib come from Aria's vendored third_party.
set(NLOHMANN_DIR ${ARIA_DIR}/third_party/nlohmann_json/include)
set(CPPHTTPLIB_DIR ${ARIA_DIR}/third_party/cpp-httplib)

find_package(OpenSSL REQUIRED)   # HTTPS transport for the LLM client

# ── ariaagent_core: engine + tools ───────────────────────────────────────────────
file(GLOB CORE_SRC CONFIGURE_DEPENDS agent/*.cpp)
add_library(ariaagent_core STATIC ${CORE_SRC})
target_include_directories(ariaagent_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_include_directories(ariaagent_core SYSTEM PUBLIC
    ${CPPHTTPLIB_DIR}
    ${NLOHMANN_DIR}
)
target_compile_definitions(ariaagent_core PUBLIC
    CPPHTTPLIB_OPENSSL_SUPPORT
)
target_link_libraries(ariaagent_core PUBLIC
    aria::binding      # transitively pulls runtime/async/core/abi
    aria::async
    OpenSSL::SSL
    OpenSSL::Crypto
)
if(WIN32)
    target_link_libraries(ariaagent_core PUBLIC ws2_32 crypt32)
endif()

# ── ariaagent_viewmodel: framework-agnostic VM ───────────────────────────────────
add_library(ariaagent_viewmodel STATIC
    viewmodel/chat_view_model.cpp
)
target_include_directories(ariaagent_viewmodel PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(ariaagent_viewmodel PUBLIC
    ariaagent_core
    aria::binding
    aria::runtime
)
