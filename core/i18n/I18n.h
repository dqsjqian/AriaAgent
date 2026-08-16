// AriaAgent — global i18n facade (core layer, zero UI dependencies).
//
// Simplified from AiTools Workbench's I18n.h: AriaAgent is a single app, so
// there is exactly one module ("app") instead of per-module inference. The
// lookup order stays the same: current language -> default (zh-CN) ->
// "[app/key]" placeholder.
//
// Usage:
//   auto s = agent::i18n::str("send");              // std::string
//   auto en = agent::i18n::str("send", Lang::En);   // force a language
//   prop.set(agent::i18n::str("title"));            // feed a Property
//
// Reactive switch: the ViewModel can subscribe via on_language_changed()
// and re-set its text Properties (see ChatViewModel::apply_language_()).
//
// The backend is injected once at startup (main.cpp for the Qt shell).
#pragma once

#include "i18n/II18nService.h"

#include "aria/subscription.hpp"

#include <functional>
#include <string>
#include <string_view>

namespace agent::i18n {

/// Language enum. System = follow global setting; others concrete.
enum class Lang { System, ZhCN, En };

[[nodiscard]] std::string_view lang_code(Lang lang);

/// Inject the backend (owned by the caller; must outlive all users).
void set_backend(services::II18nService* backend);

/// The single module name used by this app.
constexpr std::string_view kAppModule = "app";

namespace detail {

/// Core lookup: app -> default -> placeholder. Never crosses modules.
[[nodiscard]] std::string resolve(std::string_view key, Lang lang);

} // namespace detail

/// Main business entry: fetch text by key (current language).
[[nodiscard]] inline std::string str(std::string_view key,
                                     Lang lang = Lang::System) {
    return detail::resolve(key, lang);
}

// ── Current language / switch / subscribe ──────────────────────────────────
[[nodiscard]] std::string language();
void set_language(const std::string& lang);
[[nodiscard]] aria::Subscription on_language_changed(
    std::function<void(const std::string&)> fn);

} // namespace agent::i18n
