// AriaAgent — ModuleRegistry: runtime registry of assembled modules.
//
// Filled explicitly by the modules manifest (ModulesManifest.cpp) — see
// IModule.h for why auto-registration is avoided.
#pragma once

#include "module_api/IModule.h"

#include <algorithm>
#include <memory>
#include <vector>

namespace module_api {

class ModuleRegistry {
public:
    /// Add a module instance (called by the manifest).
    void add(std::shared_ptr<IModule> m) { modules_.push_back(std::move(m)); }

    /// All registered modules, in registration order.
    [[nodiscard]] const std::vector<std::shared_ptr<IModule>>& all() const {
        return modules_;
    }

private:
    std::vector<std::shared_ptr<IModule>> modules_;
};

} // namespace module_api
