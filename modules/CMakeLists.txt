# ============================================================================
#  AriaAgent modules — one static lib per feature module (plugin pattern).
#
#  Each module owns its ViewModel(s) and optionally services. Modules never
#  reference each other's ViewModels — cross-module communication goes
#  through services registered in the ServiceHub (see ModuleContext).
#
#  ModulesManifest.cpp (ariaagent_modules) is the explicit assembly list
#  the app shell links against.
# ============================================================================

set(MODULES_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

# ── Feature modules (one lib each: viewmodel + module factory) ──────────────
add_library(ariaagent_mod_sessions    STATIC
    sessions/viewmodel/session_list_vm.cpp
    sessions/module/SessionsModule.cpp
)
add_library(ariaagent_mod_trajectory  STATIC
    trajectory/viewmodel/trajectory_vm.cpp
    trajectory/module/TrajectoryModule.cpp
)
add_library(ariaagent_mod_todo        STATIC
    todo/viewmodel/todo_vm.cpp
    todo/module/TodoModule.cpp
)
add_library(ariaagent_mod_settings    STATIC
    settings/viewmodel/settings_vm.cpp
    settings/services/settings_store.cpp
    settings/module/SettingsModule.cpp
)
add_library(ariaagent_mod_chat        STATIC
    chat/viewmodel/chat_view_model.cpp
    chat/module/ChatModule.cpp
)
add_library(ariaagent_mod_app         STATIC app/viewmodel/app_text.cpp)

foreach(m IN ITEMS sessions trajectory todo settings chat app)
    target_include_directories(ariaagent_mod_${m} PUBLIC ${MODULES_ROOT})
    target_link_libraries(ariaagent_mod_${m} PUBLIC ariaagent_core)
endforeach()

# chat consumes ToolTraceStore (trajectory module service header) — link the
# trajectory lib so the include/symbol graph is explicit.
target_link_libraries(ariaagent_mod_chat PUBLIC ariaagent_mod_trajectory)

# ── Assembly list (explicit, so static libs aren't stripped) ────────────────
add_library(ariaagent_modules STATIC ModulesManifest.cpp)
target_include_directories(ariaagent_modules PUBLIC ${MODULES_ROOT})
target_link_libraries(ariaagent_modules PUBLIC
    ariaagent_mod_chat
    ariaagent_mod_sessions
    ariaagent_mod_trajectory
    ariaagent_mod_todo
    ariaagent_mod_settings
    ariaagent_mod_app
)
