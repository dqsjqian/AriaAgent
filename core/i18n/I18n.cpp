// AriaAgent — global i18n facade implementation.
#include "i18n/I18n.h"

namespace agent::i18n {

namespace {

services::II18nService* g_backend = nullptr;

} // namespace

std::string_view lang_code(Lang lang) {
    switch (lang) {
        case Lang::ZhCN:  return "zh-CN";
        case Lang::En:    return "en";
        case Lang::System:
        default:          return {};
    }
}

void set_backend(services::II18nService* backend) {
    g_backend = backend;
}

std::string language() {
    return g_backend ? g_backend->language().get() : std::string{};
}

void set_language(const std::string& lang) {
    if (g_backend) g_backend->set_language(lang);
}

aria::Subscription on_language_changed(
    std::function<void(const std::string&)> fn) {
    if (!g_backend) return {};
    return g_backend->language().observe(
        [fn = std::move(fn)](const std::string& lang, const std::string&) {
            fn(lang);
        });
}

namespace detail {

std::string resolve(std::string_view key, Lang lang) {
    if (!g_backend) return std::string(key);
    if (lang == Lang::System) return g_backend->tr(kAppModule, key);
    const std::string code = std::string(lang_code(lang));
    return g_backend->tr_in(code, kAppModule, key);
}

} // namespace detail

} // namespace agent::i18n
