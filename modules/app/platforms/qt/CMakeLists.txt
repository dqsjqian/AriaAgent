# ============================================================================
#  AriaAgent — app module, Qt desktop shell.
#
#  The ONLY place allowed to see Qt. Composes the module VMs (via the
#  ServiceHub + ModulesManifest) and owns the window / theme / markdown view.
#  The exe lands in ${CMAKE_BINARY_DIR}/bin beside aria's DLLs so
#  scripts/deploy-dlls.ps1 works unchanged.
# ============================================================================

set(REPO_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../../..)

include("${ARIA_DIR}/cmake/ariaDependencies.cmake")
aria_find_qt6(REQUIRED COMPONENTS Widgets)

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
    ariaagent_modules      # all feature modules + manifest; transitively links core
    aria::qt6              # transitively pulls binding/runtime/async/core/abi
    Qt6::Widgets
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

# Keep known licenses beside the app on every build, including document-only
# changes. Qt's corresponding source and runtime notices remain separate work.
foreach(component json mira openssl)
    get_target_property(${component}_source ariaagent_core ARIA_LICENSE_${component}_SOURCE)
endforeach()
set(license_manifest "${CMAKE_CURRENT_BINARY_DIR}/licenses-$<CONFIG>.cmake")
file(GENERATE OUTPUT "${license_manifest}" CONTENT
"set(license_output [==[$<TARGET_FILE_DIR:aria_agent>/licenses]==])
set(agent_source [==[${REPO_ROOT}]==])
set(aria_source [==[${ARIA_DIR}]==])
set(json_source [==[${json_source}]==])
set(mira_source [==[${mira_source}]==])
set(openssl_source [==[${openssl_source}]==])
include([==[${REPO_ROOT}/cmake/StageAgentLicenses.cmake]==])
")
add_custom_target(ariaagent_licenses
    COMMAND ${CMAKE_COMMAND} -P "${license_manifest}"
    VERBATIM)
add_dependencies(aria_agent ariaagent_licenses)
