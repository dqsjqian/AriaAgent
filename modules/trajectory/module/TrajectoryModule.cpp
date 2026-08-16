// AriaAgent — TrajectoryModule implementation.
#include "trajectory/module/TrajectoryModule.h"

#include "trajectory/services/tool_trace_store.hpp"
#include "trajectory/viewmodel/trajectory_vm.hpp"

namespace trajectory {

std::shared_ptr<aria::binding::ViewModel>
TrajectoryModule::create_view_model(module_api::ModuleContext& ctx) {
    return std::make_shared<TrajectoryVm>(ctx.service<agent::ToolTraceStore>());
}

std::shared_ptr<module_api::IModule> make_trajectory_module() {
    return std::make_shared<TrajectoryModule>();
}

} // namespace trajectory
