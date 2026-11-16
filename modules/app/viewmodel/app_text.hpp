// AriaAgent — AppText: the single source of user-facing UI strings.
//
// Lives in `core` (VM layer) so that Views never touch the i18n service
// directly — they read text from a ViewModel-owned provider, exactly like
// AiTools' SettingsVm exposes `title` / `hint` / `languageLabel` Properties.
//
//   - refresh() rebuilds every entry from the i18n table for the current
//     language (auto-subscribed to language changes).
//   - text("key") returns the cached, localized string.
//
// Pure C++, zero Qt.
#pragma once

#include <string>
#include <unordered_map>

#include <aria/i18n/i18n.hpp>
#include <memory>

class AppText {
public:
    explicit AppText(std::shared_ptr<aria::i18n::LocaleContext> locale);

    /// Re-read every entry from the i18n table for the current language.
    /// Auto-invoked on language change (subscribed in the ctor).
    void refresh();

    /// Localized string for `key` (cached; refresh() re-pulls).
    const std::string& text(const char* key) const;
    const std::string& text(const std::string& key) const { return text(key.c_str()); }

private:
    std::shared_ptr<aria::i18n::LocaleContext> locale_;
    std::unordered_map<std::string, std::string> cache_;
    aria::i18n::TextBindings bindings_;
};
