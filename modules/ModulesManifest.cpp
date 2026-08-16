// AriaAgent — ModulesManifest implementation.
#include "ModulesManifest.h"

#include "chat/module/ChatModule.h"
#include "sessions/module/SessionsModule.h"
#include "settings/module/SettingsModule.h"
#include "todo/module/TodoModule.h"
#include "trajectory/module/TrajectoryModule.h"

void populate_modules(module_api::ModuleRegistry& registry) {
    registry.add(chat::make_chat_module());
    registry.add(sessions::make_sessions_module());
    registry.add(trajectory::make_trajectory_module());
    registry.add(todo::make_todo_module());
    registry.add(settings::make_settings_module());
}
