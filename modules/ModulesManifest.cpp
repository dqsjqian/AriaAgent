// AriaAgent — ModulesManifest implementation.
#include "ModulesManifest.h"

#include "chat/module/ChatModule.h"
#include "sessions/module/SessionsModule.h"
#include "settings/module/SettingsModule.h"
#include "todo/module/TodoModule.h"
#include "trajectory/module/TrajectoryModule.h"

namespace {

void add_module(ViewModelProvider& provider, aria::SubscriptionBag& registrations,
                std::shared_ptr<module_api::IModule> module) {
    const auto id = module->id();
    registrations += provider.add(id,
        [module = std::move(module)](module_api::ModuleContext& context) {
            return module->create_view_model(context);
        });
}

} // namespace

void populate_modules(ViewModelProvider& provider, aria::SubscriptionBag& registrations) {
    add_module(provider, registrations, chat::make_chat_module());
    add_module(provider, registrations, sessions::make_sessions_module());
    add_module(provider, registrations, trajectory::make_trajectory_module());
    add_module(provider, registrations, todo::make_todo_module());
    add_module(provider, registrations, settings::make_settings_module());
}
