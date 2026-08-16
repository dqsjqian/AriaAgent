// AriaAgent — SettingsModule: plugin for the settings feature.
#pragma once

#include "module_api/IModule.h"

namespace settings {

class SettingsModule final : public module_api::IModule {
public:
    std::string id() const override { return "settings"; }
    std::shared_ptr<aria::binding::ViewModel>
        create_view_model(module_api::ModuleContext& ctx) override;
};

std::shared_ptr<module_api::IModule> make_settings_module();

} // namespace settings
