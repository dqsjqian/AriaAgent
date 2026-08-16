# ============================================================================
#  AriaAgent Qt shell — the only directory allowed to touch Qt.
#  Links the pure-C++ core; provides QApplication + QtDispatcher + views.
# ============================================================================
find_package(Qt6 REQUIRED COMPONENTS Core Widgets)
set(CMAKE_AUTOMOC ON)

add_executable(aria_agent WIN32 MACOSX_BUNDLE
    main.cpp
    main_window.cpp
    settings_dialog.cpp
    markdown_render.cpp
)

target_include_directories(aria_agent PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(aria_agent PRIVATE
    ariaagent_viewmodel      # pulls ariaagent_core + aria::binding transitively
    aria::qt6           # Qt adapter for aria reactive binding
    Qt6::Widgets
    Qt6::Core
    aria::warnings
)

if(WIN32)
    target_link_libraries(aria_agent PRIVATE ws2_32 crypt32)
endif()

# Put the exe beside Aria's bin/libaria_*.dll so deploy-dlls.ps1 (which
# copies from ${BuildDir}/bin) finds everything in one place.
set_target_properties(aria_agent PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# ── Stage i18n resources next to the exe (and into .app bundles) ───────────
set(ARIA_I18N_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../i18n)
if(WIN32)
    add_custom_command(TARGET aria_agent POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${ARIA_I18N_SRC_DIR}" "$<TARGET_FILE_DIR:aria_agent>/i18n"
        VERBATIM)
elseif(APPLE)
    add_custom_command(TARGET aria_agent POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${ARIA_I18N_SRC_DIR}" "$<TARGET_BUNDLE_CONTENT_DIR:aria_agent>/Resources/i18n"
        VERBATIM)
endif()
