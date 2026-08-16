// AriaAgent — ViewModelProvider: lazily creates and caches module VMs.
//
// This is the stand-in for what Aria's binding layer will eventually offer
// (a ViewModelLocator). Until then, the view controller (e.g. MainWindow)
// requests VMs by module id through this provider — creation happens on
// first use, never up-front in the entry point.
#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include "module_api/ModuleContext.h"
#include "module_api/ModuleRegistry.h"

class ViewModelProvider {
public:
    ViewModelProvider(ServiceHub& hub, module_api::ModuleRegistry& modules)
        : hub_(hub), modules_(modules) {}

    /// Get (or lazily create + cache) the ViewModel of module `id`.
    std::shared_ptr<aria::binding::ViewModel> view_model(const std::string& id) {
        auto it = cache_.find(id);
        if (it != cache_.end()) return it->second;

        for (const auto& m : modules_.all()) {
            if (m->id() != id) continue;
            module_api::ModuleContext ctx(hub_);
            auto vm = m->create_view_model(ctx);
            cache_[id] = vm;
            return vm;
        }
        return nullptr;
    }

    /// Typed convenience: lazily create module `id`'s VM as type T.
    template <typename T>
    std::shared_ptr<T> view_model_as(const std::string& id) {
        return std::static_pointer_cast<T>(view_model(id));
    }

    /// Pre-create every module VM (eager warm-up; optional).
    void warm_up() {
        for (const auto& m : modules_.all()) view_model(m->id());
    }

private:
    ServiceHub& hub_;
    module_api::ModuleRegistry& modules_;
    std::unordered_map<std::string, std::shared_ptr<aria::binding::ViewModel>> cache_;
};
