// AriaAgent — BaseVm: base class for all business ViewModels.
//
// Ported from AiTools Workbench (core/module_api/BaseVm.h). Centralizes the
// cross-cutting VM capability: auto-refreshing UI text on language change.
//
// Usage (write freely in VMs; text auto-refreshes on language change):
//
//   // Static text: bind a Property to an i18n key (runs once now).
//   text(title, "chat_title");
//
//   // Dynamic text: register a recompute closure (re-runs on language change).
//   localize([this]{
//       status.set(agent::i18n::str("count_prefix") + std::to_string(n));
//   });
//
// Theme color changes deliberately do NOT belong here — that is the View
// layer's job (each platform's native skinning), so no presentation detail
// leaks into cross-platform logic.
#pragma once

#include <aria/binding/view_model.hpp>
#include <aria/property.hpp>
#include <aria/subscription.hpp>

#include "i18n/I18n.h"

#include <functional>
#include <string>
#include <utility>
#include <vector>

class BaseVm : public aria::binding::ViewModel {
public:
    BaseVm() {
        // Language change -> re-run all localization closures. This
        // subscription must live for the VM's whole lifetime (not in the
        // view bag), otherwise text stops refreshing after a tab switch.
        lang_sub_ = agent::i18n::on_language_changed(
            [this](const std::string&) { relocalize_all_(); });
    }
    ~BaseVm() override = default;

protected:
    /// Register a localization closure: runs once immediately to set the
    /// initial value, and re-runs automatically on language change.
    void localize(std::function<void()> fn) {
        fn();
        localizers_.push_back(std::move(fn));
    }

    /// Convenience: bind a text Property to a global i18n key.
    /// Equivalent to localize([&]{ prop.set(agent::i18n::str(key)); }).
    void text(aria::Property<std::string>& prop, const std::string& key) {
        localize([&prop, key]() { prop.set(agent::i18n::str(key)); });
    }

private:
    void relocalize_all_() {
        for (auto& fn : localizers_) fn();
    }

    std::vector<std::function<void()>> localizers_;
    aria::Subscription lang_sub_;   ///< lives with the VM, not in the view bag
};
