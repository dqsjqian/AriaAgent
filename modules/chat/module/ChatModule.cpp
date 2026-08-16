// AriaAgent — ChatModule implementation.
#include "chat/module/ChatModule.h"

#include "agent/session_store.hpp"
#include "agent/tool_registry.hpp"
#include "chat/viewmodel/chat_view_model.hpp"
#include "trajectory/services/tool_trace_store.hpp"

namespace chat {

std::shared_ptr<aria::binding::ViewModel>
ChatModule::create_view_model(module_api::ModuleContext& ctx) {
    return std::make_shared<ChatViewModel>(ctx.service<agent::ToolRegistry>(),
                                           ctx.service<agent::SessionStore>(),
                                           ctx.service<agent::ToolTraceStore>());
}

std::shared_ptr<module_api::IModule> make_chat_module() {
    return std::make_shared<ChatModule>();
}

} // namespace chat
