// AriaAgent — TrajectoryModule: plugin for the tool-chain feature.
#pragma once

#include "module_api/IModule.h"

namespace trajectory {

class TrajectoryModule final : public module_api::IModule {
public:
    std::string id() const override { return "trajectory"; }
    std::shared_ptr<aria::binding::ViewModel>
        create_view_model(module_api::ModuleContext& ctx) override;
};

std::shared_ptr<module_api::IModule> make_trajectory_module();

} // namespace trajectory
