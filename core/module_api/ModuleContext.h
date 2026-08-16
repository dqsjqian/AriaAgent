// AriaAgent — ModuleContext: injected into each module.
//
// Modules obtain stable services through this; they never new services
// themselves and never depend on concrete sibling ViewModels — all
// cross-module communication goes through the ServiceHub.
#pragma once

#include "services/ServiceHub.h"

namespace module_api {

class ModuleContext {
public:
    explicit ModuleContext(ServiceHub& hub) : hub_(hub) {}

    ServiceHub& services() { return hub_; }

    /// Fetch a stable service by type (registered in the ServiceHub).
    template <typename T>
    T& service() { return hub_.service<T>(); }

private:
    ServiceHub& hub_;
};

} // namespace module_api
