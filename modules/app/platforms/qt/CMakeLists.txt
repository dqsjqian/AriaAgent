# ============================================================================
#  AriaAgent — app module, Qt desktop shell.
#
#  The ONLY place allowed to see Qt. Composes the module VMs (via the
#  ServiceHub + ModulesManifest) and owns the window / theme / markdown view.
#  The exe lands in ${CMAKE_BINARY_DIR}/bin beside aria's DLLs so
#  scripts/deploy-dlls.ps1 works unchanged.
# ============================================================================

set(REPO_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../../..)

find_package(Qt6 REQUIRED COMPONENTS Widgets)
find_package(OpenSSL REQUIRED)

add_executable(aria_agent
    main.cpp
    main_window.cpp
    markdown_render.cpp
    ${REPO_ROOT}/modules/settings/platforms/qt/settings_dialog.cpp
)

set_target_properties(aria_agent PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    AUTOMOC ON
)

target_include_directories(aria_agent PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}                       # theme.hpp / markdown_render.hpp / main_window.hpp
    ${REPO_ROOT}/modules/settings/platforms/qt        # settings_dialog.hpp
)

target_link_libraries(aria_agent PRIVATE
    ariaagent_modules      # all feature modules + manifest
    ariaagent_core
    aria::qt6              # transitively pulls binding/runtime/async/core/abi
    Qt6::Widgets
    OpenSSL::SSL
    OpenSSL::Crypto
    aria::warnings
)

if(WIN32)
    target_link_libraries(aria_agent PRIVATE ws2_32 crypt32)
    # Windows application icon (replaces the default exe icon).
    target_sources(aria_agent PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/resources/ariaagent.rc)
endif()

# Stage the i18n resources next to the exe.
add_custom_command(TARGET aria_agent POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${REPO_ROOT}/i18n
            ${CMAKE_BINARY_DIR}/bin/i18n
    COMMENT "Staging i18n resources"
)
