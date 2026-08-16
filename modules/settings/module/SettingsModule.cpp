// AriaAgent — SettingsModule implementation.
#include "settings/module/SettingsModule.h"

#include "settings/services/settings_store.hpp"
#include "settings/viewmodel/settings_vm.hpp"

namespace settings {

std::shared_ptr<aria::binding::ViewModel>
SettingsModule::create_view_model(module_api::ModuleContext& ctx) {
    return std::make_shared<SettingsVm>(ctx.service<agent::SettingsStore>());
}

std::shared_ptr<module_api::IModule> make_settings_module() {
    return std::make_shared<SettingsModule>();
}

} // namespace settings
