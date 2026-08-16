// AriaAgent — ChatModule: plugin for the conversation feature.
#pragma once

#include "module_api/IModule.h"

namespace chat {

class ChatModule final : public module_api::IModule {
public:
    std::string id() const override { return "chat"; }
    std::shared_ptr<aria::binding::ViewModel>
        create_view_model(module_api::ModuleContext& ctx) override;
};

std::shared_ptr<module_api::IModule> make_chat_module();

} // namespace chat
