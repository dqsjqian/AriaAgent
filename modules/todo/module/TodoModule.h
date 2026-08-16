// AriaAgent — TodoModule: plugin for the todo-board feature.
#pragma once

#include "module_api/IModule.h"

namespace todo {

class TodoModule final : public module_api::IModule {
public:
    std::string id() const override { return "todo"; }
    std::shared_ptr<aria::binding::ViewModel>
        create_view_model(module_api::ModuleContext& ctx) override;
};

std::shared_ptr<module_api::IModule> make_todo_module();

} // namespace todo
