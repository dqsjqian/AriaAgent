// AriaAgent — TodoModule implementation.
#include "todo/module/TodoModule.h"

#include "agent/todo_store.hpp"
#include "todo/viewmodel/todo_vm.hpp"

namespace todo {

std::shared_ptr<aria::binding::ViewModel>
TodoModule::create_view_model(module_api::ModuleContext& ctx) {
    return std::make_shared<TodoVm>(ctx.service<agent::TodoStore>());
}

std::shared_ptr<module_api::IModule> make_todo_module() {
    return std::make_shared<TodoModule>();
}

} // namespace todo
