// AriaAgent — SessionsModule: plugin for the sessions feature.
#pragma once

#include "module_api/IModule.h"

namespace sessions {

class SessionsModule final : public module_api::IModule {
public:
    std::string id() const override { return "sessions"; }
    std::shared_ptr<aria::binding::ViewModel>
        create_view_model(module_api::ModuleContext& ctx) override;
};

std::shared_ptr<module_api::IModule> make_sessions_module();

} // namespace sessions
