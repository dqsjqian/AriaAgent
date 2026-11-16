#pragma once

#include <aria/binding/view_model.hpp>
#include <aria/i18n/i18n.hpp>

#include "i18n/AppLocale.h"

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

// The Agent's app-domain convenience layer. Aria owns lookup, dependency
// tracking, language notifications, and localization subscription lifetime.
class BaseVm : public aria::binding::ViewModel {
public:
    explicit BaseVm(std::shared_ptr<aria::i18n::LocaleContext> locale)
        : locale_(std::move(locale)), texts_(locale_, std::string(agent::kTextDomain)) {}
    ~BaseVm() override = default;

protected:
    void localize(std::function<void()> fn) { texts_.localize(std::move(fn)); }
    void clear_text_bindings() { texts_.clear(); }
    void text(aria::Property<std::string>& target, std::string key) {
        texts_.text(target, std::move(key));
    }
    [[nodiscard]] std::string tr(std::string_view key) const {
        return locale_->tr(agent::kTextDomain, key);
    }
    [[nodiscard]] const std::shared_ptr<aria::i18n::LocaleContext>& locale_context() const {
        return locale_;
    }

private:
    std::shared_ptr<aria::i18n::LocaleContext> locale_;
    aria::i18n::TextBindings texts_;
};
