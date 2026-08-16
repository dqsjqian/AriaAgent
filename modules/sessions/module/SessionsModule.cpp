// AriaAgent — SessionsModule implementation.
#include "sessions/module/SessionsModule.h"

#include "agent/session_store.hpp"
#include "sessions/viewmodel/session_list_vm.hpp"

namespace sessions {

std::shared_ptr<aria::binding::ViewModel>
SessionsModule::create_view_model(module_api::ModuleContext& ctx) {
    return std::make_shared<SessionListVm>(ctx.service<agent::SessionStore>());
}

std::shared_ptr<module_api::IModule> make_sessions_module() {
    return std::make_shared<SessionsModule>();
}

} // namespace sessions
