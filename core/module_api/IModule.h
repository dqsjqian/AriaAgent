// AriaAgent — application-specific feature factory contract.
//
// Each feature module implements this and exports a make_<mod>_module()
// factory. The app layer lists them explicitly in the modules manifest
// (no global-constructor auto-registration — static libs get stripped by
// the linker otherwise). The manifest adapts these factories to Aria's
// ViewModelProvider, which owns registration lifetime and cached instances.
#pragma once

#include <memory>
#include <string>

#include <aria/binding/view_model.hpp>

#include "module_api/ModuleContext.h"

namespace module_api {

class IModule {
public:
    virtual ~IModule() = default;

    /// Module id (e.g. "chat").
    [[nodiscard]] virtual std::string id() const = 0;

    /// Create this module's ViewModel (injecting services via ctx).
    [[nodiscard]] virtual std::shared_ptr<aria::binding::ViewModel>
        create_view_model(ModuleContext& ctx) = 0;
};

} // namespace module_api
