// AriaAgent — AppText implementation.
#include "app/viewmodel/app_text.hpp"

#include "i18n/AppLocale.h"

#include <stdexcept>
#include <utility>

namespace {

constexpr const char* kKeys[] = {
    // window / branding
    "window_title", "app_name", "app_subtitle",
    // sidebar
    "new_chat", "settings", "delete_session", "workspace",
    // top bar / panels
    "trajectory", "todo", "panel_title", "collapse_panel",
    // input bar
    "input_placeholder", "attach_tooltip", "send", "stop",
    "attach_dialog_title", "attach_all_files",
    // workspace trust levels
    "ws_read_only", "ws_workspace_write", "ws_full_access", "ws_tooltip",
    // phases (error flash) + feedback menu
    "phase_error", "copy_message", "feedback_helpful", "feedback_not_helpful",
    // approval prompt (injected by the shell, text owned here)
    "approve_title", "approve_body",
};

} // namespace

AppText::AppText(std::shared_ptr<aria::i18n::LocaleContext> locale)
    : locale_(std::move(locale)), bindings_(locale_, std::string(agent::kTextDomain)) {
    if (!locale_) throw std::invalid_argument("AppText requires a locale context");
    // Aria refreshes this cache on language and catalog revision changes.
    bindings_.localize([this] { refresh(); });
}

void AppText::refresh() {
    for (const char* key : kKeys) {
        cache_[key] = locale_->tr(agent::kTextDomain, key);
    }
}

const std::string& AppText::text(const char* key) const {
    static const std::string kEmpty;
    auto it = cache_.find(key);
    return it == cache_.end() ? kEmpty : it->second;
}
